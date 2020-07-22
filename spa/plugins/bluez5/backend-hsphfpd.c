/* Spa hsphfpd backend
 *
 * Based on previous work for pulseaudio by: Pali Rohár <pali.rohar@gmail.com>
 * Copyright © 2020 Collabora Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>

#include <dbus/dbus.h>

#include <spa/support/log.h>
#include <spa/support/loop.h>
#include <spa/support/dbus.h>
#include <spa/support/plugin.h>
#include <spa/utils/type.h>

#include "defs.h"

struct spa_bt_backend {
	struct spa_bt_monitor *monitor;

	struct spa_log *log;
	struct spa_loop *main_loop;
	struct spa_dbus *dbus;
	DBusConnection *conn;

	struct spa_list endpoint_list;
	bool endpoints_listed;

	char *hsphfpd_service_id;

	unsigned int filters_added:1;
};

enum hsphfpd_volume_control {
	HSPHFPD_VOLUME_CONTROL_NONE = 1,
	HSPHFPD_VOLUME_CONTROL_LOCAL,
	HSPHFPD_VOLUME_CONTROL_REMOTE,
};

enum hsphfpd_profile {
	HSPHFPD_PROFILE_HEADSET = 1,
	HSPHFPD_PROFILE_HANDSFREE,
};

enum hsphfpd_role {
	HSPHFPD_ROLE_CLIENT = 1,
	HSPHFPD_ROLE_GATEWAY,
};

struct hsphfpd_endpoint {
	struct spa_list link;
	char *path;
	bool valid;
	bool connected;
	char *remote_address;
	char *local_address;
	enum hsphfpd_profile profile;
	enum hsphfpd_role role;
};

#define DBUS_INTERFACE_OBJECTMANAGER "org.freedesktop.DBus.ObjectManager"

#define HSPHFPD_SERVICE "org.hsphfpd"
#define HSPHFPD_APPLICATION_MANAGER_INTERFACE HSPHFPD_SERVICE ".ApplicationManager"
#define HSPHFPD_ENDPOINT_INTERFACE            HSPHFPD_SERVICE ".Endpoint"
#define HSPHFPD_AUDIO_AGENT_INTERFACE         HSPHFPD_SERVICE ".AudioAgent"

#define APPLICATION_OBJECT_MANAGER_PATH "/Profile/hsphfpd/manager"
#define HSPHFP_AUDIO_CLIENT_CVSD        "/Profile/hsphfpd/cvsd_agent"

#define HSPHFP_AGENT_CODEC_PCM          "PCM_s16le_8kHz"

#define APPLICATION_OBJECT_MANAGER_INTROSPECT_XML                              \
    DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE                                  \
    "<node>\n"                                                                 \
    " <interface name=\"" DBUS_INTERFACE_OBJECTMANAGER "\">\n"                 \
    "  <method name=\"GetManagedObjects\">\n"                                  \
    "   <arg name=\"objects\" direction=\"out\" type=\"a{oa{sa{sv}}}\"/>\n"    \
    "  </method>\n"                                                            \
    "  <signal name=\"InterfacesAdded\">\n"                                    \
    "   <arg name=\"object\" type=\"o\"/>\n"                                   \
    "   <arg name=\"interfaces\" type=\"a{sa{sv}}\"/>\n"                       \
    "  </signal>\n"                                                            \
    "  <signal name=\"InterfacesRemoved\">\n"                                  \
    "   <arg name=\"object\" type=\"o\"/>\n"                                   \
    "   <arg name=\"interfaces\" type=\"as\"/>\n"                              \
    "  </signal>\n"                                                            \
    " </interface>\n"                                                          \
    " <interface name=\"" DBUS_INTERFACE_INTROSPECTABLE "\">\n"                \
    "  <method name=\"Introspect\">\n"                                         \
    "   <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"                   \
    "  </method>\n"                                                            \
    " </interface>\n"                                                          \
    "</node>\n"

#define HSPHFPD_ERROR_INVALID_ARGUMENTS   HSPHFPD_SERVICE ".Error.InvalidArguments"
#define HSPHFPD_ERROR_ALREADY_EXISTS      HSPHFPD_SERVICE ".Error.AlreadyExists"
#define HSPHFPD_ERROR_DOES_NOT_EXISTS     HSPHFPD_SERVICE ".Error.DoesNotExist"
#define HSPHFPD_ERROR_NOT_CONNECTED       HSPHFPD_SERVICE ".Error.NotConnected"
#define HSPHFPD_ERROR_ALREADY_CONNECTED   HSPHFPD_SERVICE ".Error.AlreadyConnected"
#define HSPHFPD_ERROR_IN_PROGRESS         HSPHFPD_SERVICE ".Error.InProgress"
#define HSPHFPD_ERROR_IN_USE              HSPHFPD_SERVICE ".Error.InUse"
#define HSPHFPD_ERROR_NOT_SUPPORTED       HSPHFPD_SERVICE ".Error.NotSupported"
#define HSPHFPD_ERROR_NOT_AVAILABLE       HSPHFPD_SERVICE ".Error.NotAvailable"
#define HSPHFPD_ERROR_FAILED              HSPHFPD_SERVICE ".Error.Failed"
#define HSPHFPD_ERROR_REJECTED            HSPHFPD_SERVICE ".Error.Rejected"
#define HSPHFPD_ERROR_CANCELED            HSPHFPD_SERVICE ".Error.Canceled"

static struct hsphfpd_endpoint *endpoint_find(struct spa_bt_backend *backend, const char *path)
{
	struct hsphfpd_endpoint *d;
	spa_list_for_each(d, &backend->endpoint_list, link)
		if (strcmp(d->path, path) == 0)
			return d;
	return NULL;
}

static void endpoint_free(struct hsphfpd_endpoint *endpoint)
{
	spa_list_remove(&endpoint->link);
	free(endpoint->path);
	if (endpoint->local_address)
		free(endpoint->local_address);
	if (endpoint->remote_address)
		free(endpoint->remote_address);
}

static void append_audio_agent_object(DBusMessageIter *iter, const char *endpoint, const char *agent_codec) {
	const char *interface_name = HSPHFPD_AUDIO_AGENT_INTERFACE;
	DBusMessageIter object, array, entry, dict, codec, data;
	char *str = "AgentCodec";

	dbus_message_iter_open_container(iter, DBUS_TYPE_DICT_ENTRY, NULL, &object);
	dbus_message_iter_append_basic(&object, DBUS_TYPE_OBJECT_PATH, &endpoint);

	dbus_message_iter_open_container(&object, DBUS_TYPE_ARRAY, "{sa{sv}}", &array);

	dbus_message_iter_open_container(&array, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &interface_name);

	dbus_message_iter_open_container(&entry, DBUS_TYPE_ARRAY, "{sv}", &dict);

	dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &codec);
	dbus_message_iter_append_basic(&codec, DBUS_TYPE_STRING, &str);
	dbus_message_iter_open_container(&codec, DBUS_TYPE_VARIANT, "s", &data);
	dbus_message_iter_append_basic(&data, DBUS_TYPE_STRING, &agent_codec);
	dbus_message_iter_close_container(&codec, &data);
	dbus_message_iter_close_container(&dict, &codec);

	dbus_message_iter_close_container(&entry, &dict);
	dbus_message_iter_close_container(&array, &entry);
	dbus_message_iter_close_container(&object, &array);
	dbus_message_iter_close_container(iter, &object);
}

static DBusHandlerResult application_object_manager_handler(DBusConnection *c, DBusMessage *m, void *userdata)
{
	struct spa_bt_backend *backend = userdata;
	const char *path, *interface, *member;
	DBusMessage *r;

	path = dbus_message_get_path(m);
	interface = dbus_message_get_interface(m);
	member = dbus_message_get_member(m);

	spa_log_debug(backend->log, "dbus: path=%s, interface=%s, member=%s", path, interface, member);

	if (dbus_message_is_method_call(m, "org.freedesktop.DBus.Introspectable", "Introspect")) {
		const char *xml = APPLICATION_OBJECT_MANAGER_INTROSPECT_XML;

		if ((r = dbus_message_new_method_return(m)) == NULL)
			return DBUS_HANDLER_RESULT_NEED_MEMORY;
		if (!dbus_message_append_args(r, DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID))
			return DBUS_HANDLER_RESULT_NEED_MEMORY;
	} else if (dbus_message_is_method_call(m, DBUS_INTERFACE_OBJECTMANAGER, "GetManagedObjects")) {
		DBusMessageIter iter, array;

		if ((r = dbus_message_new_method_return(m)) == NULL)
			return DBUS_HANDLER_RESULT_NEED_MEMORY;

		dbus_message_iter_init_append(r, &iter);
		dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{oa{sa{sv}}}", &array);

		append_audio_agent_object(&array, HSPHFP_AUDIO_CLIENT_CVSD, HSPHFP_AGENT_CODEC_PCM);

		dbus_message_iter_close_container(&iter, &array);
	} else
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_connection_send(backend->conn, r, NULL))
		return DBUS_HANDLER_RESULT_NEED_MEMORY;
	dbus_message_unref(r);

  return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult hsphfpd_parse_endpoint_properties(struct spa_bt_backend *backend, struct hsphfpd_endpoint *endpoint, DBusMessageIter *i)
{
	DBusMessageIter element_i;
	struct spa_bt_device *d;

	dbus_message_iter_recurse(i, &element_i);
	while (dbus_message_iter_get_arg_type(&element_i) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter dict_i, value_i;
		const char *key;

		dbus_message_iter_recurse(&element_i, &dict_i);
		dbus_message_iter_get_basic(&dict_i, &key);
		dbus_message_iter_next(&dict_i);
		dbus_message_iter_recurse(&dict_i, &value_i);
		switch (dbus_message_iter_get_arg_type(&value_i)) {
			case DBUS_TYPE_STRING:
				{
					const char *value;
					dbus_message_iter_get_basic(&value_i, &value);
					if (strcmp(key, "RemoteAddress") == 0)
						endpoint->remote_address = strdup(value);
					else if (strcmp(key, "LocalAddress") == 0)
						endpoint->local_address = strdup(value);
					else if (strcmp(key, "Profile") == 0) {
						if (endpoint->profile)
							spa_log_warn(backend->log, "Endpoint %s received a duplicate '%s' property, ignoring", endpoint->path, key);
						else if (strcmp(value, "headset") == 0)
							endpoint->profile = HSPHFPD_PROFILE_HEADSET;
						else if (strcmp(value, "handsfree") == 0)
							endpoint->profile = HSPHFPD_PROFILE_HANDSFREE;
						else
							spa_log_warn(backend->log, "Endpoint %s received invalid '%s' property value '%s', ignoring", endpoint->path, key, value);
					} else if (strcmp(key, "Role") == 0) {
						if (endpoint->role)
							spa_log_warn(backend->log, "Endpoint %s received a duplicate '%s' property, ignoring", endpoint->path, key);
						else if (strcmp(value, "client") == 0)
							endpoint->role = HSPHFPD_ROLE_CLIENT;
						else if (strcmp(value, "gateway") == 0)
							endpoint->role = HSPHFPD_ROLE_GATEWAY;
						else
							spa_log_warn(backend->log, "Endpoint %s received invalid '%s' property value '%s', ignoring", endpoint->path, key, value);
					}
					spa_log_trace(backend->log, "  %s: %s (%p)", key, value, endpoint);
				}
				break;

			case DBUS_TYPE_BOOLEAN:
				{
					bool value;
					dbus_message_iter_get_basic(&value_i, &value);
					if (strcmp(key, "Connected") == 0)
						endpoint->connected = value;
					spa_log_trace(backend->log, "  %s: %d", key, value);
				}
				break;
		}

		dbus_message_iter_next(&element_i);
	}

	if (!endpoint->valid && endpoint->local_address && endpoint->remote_address && endpoint->profile && endpoint->role)
		endpoint->valid = true;

	d = spa_bt_device_find_by_address(backend->monitor, endpoint->remote_address, endpoint->local_address);
	if (!d) {
		spa_log_debug(backend->log, "No device for %s", endpoint->path);
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (!endpoint->valid || !endpoint->connected)
		return DBUS_HANDLER_RESULT_HANDLED;

	spa_log_debug(backend->log, "Transport %s available for hsphfpd", endpoint->path);

	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult hsphfpd_parse_interfaces(struct spa_bt_backend *backend, DBusMessageIter *dict_i)
{
	DBusMessageIter element_i;
	const char *path;

	spa_assert(backend);
	spa_assert(dict_i);

	dbus_message_iter_get_basic(dict_i, &path);
	dbus_message_iter_next(dict_i);
	dbus_message_iter_recurse(dict_i, &element_i);

	while (dbus_message_iter_get_arg_type(&element_i) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter iface_i;
		const char *interface;

		dbus_message_iter_recurse(&element_i, &iface_i);
		dbus_message_iter_get_basic(&iface_i, &interface);
		dbus_message_iter_next(&iface_i);

		if (strcmp(interface, HSPHFPD_ENDPOINT_INTERFACE) == 0) {
			struct hsphfpd_endpoint *endpoint;

			endpoint = endpoint_find(backend, path);
			if (!endpoint) {
				endpoint = calloc(1, sizeof(struct hsphfpd_endpoint));
				endpoint->path = strdup(path);
				spa_list_append(&backend->endpoint_list, &endpoint->link);
				spa_log_debug(backend->log, "Found endpoint %s", path);
			}
			hsphfpd_parse_endpoint_properties(backend, endpoint, &iface_i);
		} else
			spa_log_debug(backend->log, "Unknown interface %s found, skipping", interface);

		dbus_message_iter_next(&element_i);
	}

	return DBUS_HANDLER_RESULT_HANDLED;
}

static void hsphfpd_get_endpoints_reply(DBusPendingCall *pending, void *user_data)
{
	struct spa_bt_backend *backend = user_data;
	DBusMessage *r;
	DBusMessageIter i, array_i;

	r = dbus_pending_call_steal_reply(pending);
	if (r == NULL)
		return;

	if (dbus_message_get_type(r) == DBUS_MESSAGE_TYPE_ERROR) {
		spa_log_error(backend->log, "Failed to get a list of endpoints from hsphfpd: %s",
				dbus_message_get_error_name(r));
		goto finish;
	}

	if (strcmp(dbus_message_get_sender(r), backend->hsphfpd_service_id) != 0) {
		spa_log_error(backend->log, "Reply for GetManagedObjects() from invalid sender");
		goto finish;
	}

	if (!dbus_message_iter_init(r, &i) || strcmp(dbus_message_get_signature(r), "a{oa{sa{sv}}}") != 0) {
		spa_log_error(backend->log, "Invalid arguments in GetManagedObjects() reply");
		goto finish;
	}

	dbus_message_iter_recurse(&i, &array_i);
	while (dbus_message_iter_get_arg_type(&array_i) != DBUS_TYPE_INVALID) {
			DBusMessageIter dict_i;

			dbus_message_iter_recurse(&array_i, &dict_i);
			hsphfpd_parse_interfaces(backend, &dict_i);
			dbus_message_iter_next(&array_i);
	}

	backend->endpoints_listed = true;

finish:
	dbus_message_unref(r);
	dbus_pending_call_unref(pending);
}

static void hsphfpd_register_application_reply(DBusPendingCall *pending, void *user_data)
{
	struct spa_bt_backend *backend = user_data;
	DBusMessage *r;
	DBusMessage *m;
	DBusPendingCall *call;

	r = dbus_pending_call_steal_reply(pending);
	if (r == NULL)
		return;

	if (dbus_message_get_type(r) == DBUS_MESSAGE_TYPE_ERROR) {
		spa_log_error(backend->log, "RegisterApplication() failed: %s",
				dbus_message_get_error_name(r));
		goto finish;
	}

	backend->hsphfpd_service_id = strdup(dbus_message_get_sender(r));

	spa_log_debug(backend->log, "Registered to hsphfpd");

	m = dbus_message_new_method_call(HSPHFPD_SERVICE, "/",
			DBUS_INTERFACE_OBJECTMANAGER, "GetManagedObjects");
	if (m == NULL)
		goto finish;

	dbus_connection_send_with_reply(backend->conn, m, &call, -1);
	dbus_pending_call_set_notify(call, hsphfpd_get_endpoints_reply, backend, NULL);
	dbus_message_unref(m);

finish:
	dbus_message_unref(r);
        dbus_pending_call_unref(pending);
}

static int hsphfpd_register_application(struct spa_bt_backend *backend)
{
	DBusMessage *m;
	const char *path = APPLICATION_OBJECT_MANAGER_PATH;
	DBusPendingCall *call;

	spa_log_debug(backend->log, "Registering to hsphfpd");

	m = dbus_message_new_method_call(HSPHFPD_SERVICE, "/",
			HSPHFPD_APPLICATION_MANAGER_INTERFACE, "RegisterApplication");
	if (m == NULL)
		return -ENOMEM;

	dbus_message_append_args(m, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_INVALID);

	dbus_connection_send_with_reply(backend->conn, m, &call, -1);
	dbus_pending_call_set_notify(call, hsphfpd_register_application_reply, backend, NULL);
	dbus_message_unref(m);

	return 0;
}

static DBusHandlerResult hsphfpd_filter_cb(DBusConnection *bus, DBusMessage *m, void *user_data)
{
	const char *sender;
	struct spa_bt_backend *backend = user_data;
	DBusError err;

	dbus_error_init(&err);

	sender = dbus_message_get_sender(m);

	if (strcmp(sender, DBUS_SERVICE_DBUS) == 0) {
		if (dbus_message_is_signal(m, "org.freedesktop.DBus", "NameOwnerChanged")) {
			const char *name, *old_owner, *new_owner;

			if (!dbus_message_get_args(m, &err,
			                           DBUS_TYPE_STRING, &name,
			                           DBUS_TYPE_STRING, &old_owner,
			                           DBUS_TYPE_STRING, &new_owner,
			                           DBUS_TYPE_INVALID)) {
					spa_log_error(backend->log, "Failed to parse org.freedesktop.DBus.NameOwnerChanged: %s", err.message);
					goto finish;
			}

			if (strcmp(name, HSPHFPD_SERVICE) == 0) {
					if (old_owner && *old_owner) {
							struct hsphfpd_endpoint *endpoint;

							spa_log_debug(backend->log, "hsphfpd disappeared");
							if (backend->hsphfpd_service_id) {
								free(backend->hsphfpd_service_id);
								backend->hsphfpd_service_id = NULL;
							}
							backend->endpoints_listed = false;
							spa_list_consume(endpoint, &backend->endpoint_list, link)
								endpoint_free(endpoint);
					}

					if (new_owner && *new_owner) {
							spa_log_debug(backend->log, "hsphfpd appeared");
							hsphfpd_register_application(backend);
					}
			} else {
				spa_log_debug(backend->log, "Name owner changed %s", dbus_message_get_path(m));
			}
		}
	} else if (backend->hsphfpd_service_id && strcmp(sender, backend->hsphfpd_service_id) == 0) {
		if (dbus_message_is_signal(m, DBUS_INTERFACE_OBJECTMANAGER, "InterfacesAdded")) {
			DBusMessageIter arg_i;

			spa_log_warn(backend->log, "sender: %s", dbus_message_get_sender(m));

			if (!backend->endpoints_listed)
				goto finish;

			if (!dbus_message_iter_init(m, &arg_i) || strcmp(dbus_message_get_signature(m), "oa{sa{sv}}") != 0) {
					spa_log_error(backend->log, "Invalid signature found in InterfacesAdded");
					goto finish;
			}

			hsphfpd_parse_interfaces(backend, &arg_i);
		} else if (dbus_message_is_signal(m, DBUS_INTERFACE_OBJECTMANAGER, "InterfacesRemoved")) {
			const char *path;
			DBusMessageIter arg_i, element_i;

			if (!backend->endpoints_listed)
				goto finish;

			if (!dbus_message_iter_init(m, &arg_i) || strcmp(dbus_message_get_signature(m), "oas") != 0) {
					spa_log_error(backend->log, "Invalid signature found in InterfacesRemoved");
					goto finish;
			}

			dbus_message_iter_get_basic(&arg_i, &path);
			dbus_message_iter_next(&arg_i);
			dbus_message_iter_recurse(&arg_i, &element_i);

			while (dbus_message_iter_get_arg_type(&element_i) == DBUS_TYPE_STRING) {
					const char *iface;

					dbus_message_iter_get_basic(&element_i, &iface);

					if (strcmp(iface, HSPHFPD_ENDPOINT_INTERFACE) == 0) {
							struct hsphfpd_endpoint *endpoint;
							struct spa_bt_transport *transport = spa_bt_transport_find(backend->monitor, path);

							if (transport)
								spa_bt_transport_free(transport);

							spa_log_debug(backend->log, "Remove endpoint %s", path);
							endpoint = endpoint_find(backend, path);
							if (endpoint)
								endpoint_free(endpoint);
					}

					dbus_message_iter_next(&element_i);
			}
		} else if (dbus_message_is_signal(m, DBUS_INTERFACE_PROPERTIES, "PropertiesChanged")) {
			DBusMessageIter arg_i;
			const char *iface;
			const char *path;

			if (!backend->endpoints_listed)
				goto finish;

			if (!dbus_message_iter_init(m, &arg_i) || strcmp(dbus_message_get_signature(m), "sa{sv}as") != 0) {
					spa_log_error(backend->log, "Invalid signature found in PropertiesChanged");
					goto finish;
			}

			dbus_message_iter_get_basic(&arg_i, &iface);
			dbus_message_iter_next(&arg_i);

			path = dbus_message_get_path(m);

			if (strcmp(iface, HSPHFPD_ENDPOINT_INTERFACE) == 0) {
				struct hsphfpd_endpoint *endpoint = endpoint_find(backend, path);
				if (!endpoint) {
					spa_log_warn(backend->log, "Properties changed on unknown endpoint %s", path);
					goto finish;
				}
				spa_log_debug(backend->log, "Properties changed on endpoint %s", path);
				hsphfpd_parse_endpoint_properties(backend, endpoint, &arg_i);
			}
		}
	}

finish:
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void backend_hsphfpd_add_filters(struct spa_bt_backend *backend)
{
	DBusError err;

	if (backend->filters_added)
		return;

	dbus_error_init(&err);

	if (!dbus_connection_add_filter(backend->conn, hsphfpd_filter_cb, backend, NULL)) {
		spa_log_error(backend->log, "failed to add filter function");
		goto fail;
	}

	dbus_bus_add_match(backend->conn,
			"type='signal',sender='org.freedesktop.DBus',"
			"interface='org.freedesktop.DBus',member='NameOwnerChanged',"
			"arg0='" HSPHFPD_SERVICE "'", &err);
	dbus_bus_add_match(backend->conn,
			"type='signal',sender='" HSPHFPD_SERVICE "',"
			"interface='" DBUS_INTERFACE_OBJECTMANAGER "',member='InterfacesAdded'", &err);
	dbus_bus_add_match(backend->conn,
			"type='signal',sender='" HSPHFPD_SERVICE "',"
			"interface='" DBUS_INTERFACE_OBJECTMANAGER "',member='InterfacesRemoved'", &err);
	dbus_bus_add_match(backend->conn,
			"type='signal',sender='" HSPHFPD_SERVICE "',"
			"interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged',"
			"arg0='" HSPHFPD_ENDPOINT_INTERFACE "'", &err);

	backend->filters_added = true;

	return;

fail:
	dbus_error_free(&err);
}

void backend_hsphfpd_free(struct spa_bt_backend *backend)
{
	struct hsphfpd_endpoint *endpoint;

	dbus_connection_unregister_object_path(backend->conn, APPLICATION_OBJECT_MANAGER_PATH);

	spa_list_consume(endpoint, &backend->endpoint_list, link)
		endpoint_free(endpoint);

	free(backend);
}

struct spa_bt_backend *backend_hsphfpd_new(struct spa_bt_monitor *monitor,
		void *dbus_connection,
		const struct spa_support *support,
	  uint32_t n_support)
{
	struct spa_bt_backend *backend;
	static const DBusObjectPathVTable vtable_application_object_manager = {
		.message_function = application_object_manager_handler,
	};

	backend = calloc(1, sizeof(struct spa_bt_backend));
	if (backend == NULL)
		return NULL;

	backend->monitor = monitor;
	backend->log = spa_support_find(support, n_support, SPA_TYPE_INTERFACE_Log);
	backend->dbus = spa_support_find(support, n_support, SPA_TYPE_INTERFACE_DBus);
	backend->main_loop = spa_support_find(support, n_support, SPA_TYPE_INTERFACE_Loop);
	backend->conn = dbus_connection;

	spa_list_init(&backend->endpoint_list);

	if (!dbus_connection_register_object_path(backend->conn,
	            APPLICATION_OBJECT_MANAGER_PATH,
	            &vtable_application_object_manager, backend)) {
		free(backend);
		return NULL;
	}

	hsphfpd_register_application(backend);

	return backend;
}
