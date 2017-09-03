/* Simple Plugin API
 * Copyright (C) 2016 Wim Taymans <wim.taymans@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/eventfd.h>

#include <spa/format-utils.h>
#include <spa/loop.h>

#include "debug.h"

static const struct spa_type_map *map;

void spa_debug_set_type_map(const struct spa_type_map *m)
{
	map = m;
}

int spa_debug_port_info(const struct spa_port_info *info)
{
	if (info == NULL)
		return SPA_RESULT_INVALID_ARGUMENTS;

	fprintf(stderr, "struct spa_port_info %p:\n", info);
	fprintf(stderr, " flags: \t%08x\n", info->flags);

	return SPA_RESULT_OK;
}

int spa_debug_buffer(const struct spa_buffer *buffer)
{
	int i;

	if (buffer == NULL)
		return SPA_RESULT_INVALID_ARGUMENTS;

	fprintf(stderr, "spa_buffer %p:\n", buffer);
	fprintf(stderr, " id:      %08X\n", buffer->id);
	fprintf(stderr, " n_metas: %u (at %p)\n", buffer->n_metas, buffer->metas);
	for (i = 0; i < buffer->n_metas; i++) {
		struct spa_meta *m = &buffer->metas[i];
		const char *type_name;

		type_name = spa_type_map_get_type(map, m->type);
		fprintf(stderr, "  meta %d: type %d (%s), data %p, size %d:\n", i, m->type,
			type_name, m->data, m->size);

		if (!strcmp(type_name, SPA_TYPE_META__Header)) {
			struct spa_meta_header *h = m->data;
			fprintf(stderr, "    struct spa_meta_header:\n");
			fprintf(stderr, "      flags:      %08x\n", h->flags);
			fprintf(stderr, "      seq:        %u\n", h->seq);
			fprintf(stderr, "      pts:        %" PRIi64 "\n", h->pts);
			fprintf(stderr, "      dts_offset: %" PRIi64 "\n", h->dts_offset);
		} else if (!strcmp(type_name, SPA_TYPE_META__Pointer)) {
			struct spa_meta_pointer *h = m->data;
			fprintf(stderr, "    struct spa_meta_pointer:\n");
			fprintf(stderr, "      type:       %s\n",
				spa_type_map_get_type(map, h->type));
			fprintf(stderr, "      ptr:        %p\n", h->ptr);
		} else if (!strcmp(type_name, SPA_TYPE_META__VideoCrop)) {
			struct spa_meta_video_crop *h = m->data;
			fprintf(stderr, "    struct spa_meta_video_crop:\n");
			fprintf(stderr, "      x:      %d\n", h->x);
			fprintf(stderr, "      y:      %d\n", h->y);
			fprintf(stderr, "      width:  %d\n", h->width);
			fprintf(stderr, "      height: %d\n", h->height);
		} else if (!strcmp(type_name, SPA_TYPE_META__Ringbuffer)) {
			struct spa_meta_ringbuffer *h = m->data;
			fprintf(stderr, "    struct spa_meta_ringbuffer:\n");
			fprintf(stderr, "      readindex:   %d\n", h->ringbuffer.readindex);
			fprintf(stderr, "      writeindex:  %d\n", h->ringbuffer.writeindex);
			fprintf(stderr, "      size:        %d\n", h->ringbuffer.size);
			fprintf(stderr, "      mask:        %d\n", h->ringbuffer.mask);
		} else if (!strcmp(type_name, SPA_TYPE_META__Shared)) {
			struct spa_meta_shared *h = m->data;
			fprintf(stderr, "    struct spa_meta_shared:\n");
			fprintf(stderr, "      flags:  %d\n", h->flags);
			fprintf(stderr, "      fd:     %d\n", h->fd);
			fprintf(stderr, "      offset: %d\n", h->offset);
			fprintf(stderr, "      size:   %d\n", h->size);
		} else {
			fprintf(stderr, "    Unknown:\n");
			spa_debug_dump_mem(m->data, m->size);
		}
	}
	fprintf(stderr, " n_datas: \t%u (at %p)\n", buffer->n_datas, buffer->datas);
	for (i = 0; i < buffer->n_datas; i++) {
		struct spa_data *d = &buffer->datas[i];
		fprintf(stderr, "   type:    %d (%s)\n", d->type,
			spa_type_map_get_type(map, d->type));
		fprintf(stderr, "   flags:   %d\n", d->flags);
		fprintf(stderr, "   data:    %p\n", d->data);
		fprintf(stderr, "   fd:      %d\n", d->fd);
		fprintf(stderr, "   offset:  %d\n", d->mapoffset);
		fprintf(stderr, "   maxsize: %u\n", d->maxsize);
		fprintf(stderr, "   chunk:   %p\n", d->chunk);
		fprintf(stderr, "    offset: %d\n", d->chunk->offset);
		fprintf(stderr, "    size:   %u\n", d->chunk->size);
		fprintf(stderr, "    stride: %d\n", d->chunk->stride);
	}
	return SPA_RESULT_OK;
}

int spa_debug_dump_mem(const void *mem, size_t size)
{
	const uint8_t *t = mem;
	int i;

	if (mem == NULL)
		return SPA_RESULT_INVALID_ARGUMENTS;

	for (i = 0; i < size; i++) {
		if (i % 16 == 0)
			printf("%p: ", &t[i]);
		printf("%02x ", t[i]);
		if (i % 16 == 15 || i == size - 1)
			printf("\n");
	}
	return SPA_RESULT_OK;
}

int spa_debug_props(const struct spa_props *props)
{
	spa_debug_pod(&props->object.pod);
	return SPA_RESULT_OK;
}

int spa_debug_param(const struct spa_param *param)
{
	spa_debug_pod(&param->object.pod);
	return SPA_RESULT_OK;
}

static const char *pod_type_names[] = {
	"invalid",
	"none",
	"bool",
	"id",
	"int",
	"long",
	"float",
	"double",
	"string",
	"bytes",
	"pointer",
	"rectangle",
	"fraction",
	"bitmask",
	"array",
	"struct",
	"object",
	"prop",
	"pod"
};

static void
print_pod_value(uint32_t size, uint32_t type, void *body, int prefix)
{
	switch (type) {
	case SPA_POD_TYPE_BOOL:
		printf("%-*sBool %d\n", prefix, "", *(int32_t *) body);
		break;
	case SPA_POD_TYPE_ID:
		printf("%-*sId %d %s\n", prefix, "", *(int32_t *) body,
		       spa_type_map_get_type(map, *(int32_t *) body));
		break;
	case SPA_POD_TYPE_INT:
		printf("%-*sInt %d\n", prefix, "", *(int32_t *) body);
		break;
	case SPA_POD_TYPE_LONG:
		printf("%-*sLong %" PRIi64 "\n", prefix, "", *(int64_t *) body);
		break;
	case SPA_POD_TYPE_FLOAT:
		printf("%-*sFloat %f\n", prefix, "", *(float *) body);
		break;
	case SPA_POD_TYPE_DOUBLE:
		printf("%-*sDouble %f\n", prefix, "", *(double *) body);
		break;
	case SPA_POD_TYPE_STRING:
		printf("%-*sString \"%s\"\n", prefix, "", (char *) body);
		break;
	case SPA_POD_TYPE_POINTER:
	{
		struct spa_pod_pointer_body *b = body;
		printf("%-*sPointer %s %p\n", prefix, "",
		       spa_type_map_get_type(map, b->type), b->value);
		break;
	}
	case SPA_POD_TYPE_RECTANGLE:
	{
		struct spa_rectangle *r = body;
		printf("%-*sRectangle %dx%d\n", prefix, "", r->width, r->height);
		break;
	}
	case SPA_POD_TYPE_FRACTION:
	{
		struct spa_fraction *f = body;
		printf("%-*sFraction %d/%d\n", prefix, "", f->num, f->denom);
		break;
	}
	case SPA_POD_TYPE_BITMASK:
		printf("%-*sBitmask\n", prefix, "");
		break;
	case SPA_POD_TYPE_ARRAY:
	{
		struct spa_pod_array_body *b = body;
		void *p;
		printf("%-*sArray: child.size %d, child.type %d\n", prefix, "",
		       b->child.size, b->child.type);

		SPA_POD_ARRAY_BODY_FOREACH(b, size, p)
			print_pod_value(b->child.size, b->child.type, p, prefix + 2);
		break;
	}
	case SPA_POD_TYPE_STRUCT:
	{
		struct spa_pod *b = body, *p;
		printf("%-*sStruct: size %d\n", prefix, "", size);
		SPA_POD_FOREACH(b, size, p)
			print_pod_value(p->size, p->type, SPA_POD_BODY(p), prefix + 2);
		break;
	}
	case SPA_POD_TYPE_OBJECT:
	{
		struct spa_pod_object_body *b = body;
		struct spa_pod *p;

		printf("%-*sObject: size %d, id %d, type %s\n", prefix, "", size, b->id,
		       spa_type_map_get_type(map, b->type));
		SPA_POD_OBJECT_BODY_FOREACH(b, size, p)
			print_pod_value(p->size, p->type, SPA_POD_BODY(p), prefix + 2);
		break;
	}
	case SPA_POD_TYPE_PROP:
	{
		struct spa_pod_prop_body *b = body;
		void *alt;
		int i;

		printf("%-*sProp: key %s, flags %d\n", prefix, "",
		       spa_type_map_get_type(map, b->key), b->flags);
		if (b->flags & SPA_POD_PROP_FLAG_UNSET)
			printf("%-*sUnset (Default):\n", prefix + 2, "");
		else
			printf("%-*sValue: size %u\n", prefix + 2, "", b->value.size);
		print_pod_value(b->value.size, b->value.type, SPA_POD_BODY(&b->value),
				prefix + 4);

		i = 0;
		SPA_POD_PROP_ALTERNATIVE_FOREACH(b, size, alt) {
			if (i == 0)
				printf("%-*sAlternatives:\n", prefix + 2, "");
			print_pod_value(b->value.size, b->value.type, alt, prefix + 4);
			i++;
		}
		break;
	}
	case SPA_POD_TYPE_BYTES:
		printf("%-*sBytes\n", prefix, "");
		spa_debug_dump_mem(body, size);
		break;
	case SPA_POD_TYPE_NONE:
		printf("%-*sNone\n", prefix, "");
		spa_debug_dump_mem(body, size);
		break;
	default:
		printf("unhandled POD type %d\n", type);
		break;
	}
}

int spa_debug_pod(const struct spa_pod *pod)
{
	print_pod_value(pod->size, pod->type, SPA_POD_BODY(pod), 0);
	return SPA_RESULT_OK;
}

static void
print_format_value(uint32_t size, uint32_t type, void *body)
{
	switch (type) {
	case SPA_POD_TYPE_BOOL:
		fprintf(stderr, "%s", *(int32_t *) body ? "true" : "false");
		break;
	case SPA_POD_TYPE_ID:
	{
		const char *str = spa_type_map_get_type(map, *(int32_t *) body);
		if (str) {
			const char *h = rindex(str, ':');
			if (h)
				str = h + 1;
		} else {
			str = "unknown";
		}
		fprintf(stderr, "%s", str);
		break;
	}
	case SPA_POD_TYPE_INT:
		fprintf(stderr, "%" PRIi32, *(int32_t *) body);
		break;
	case SPA_POD_TYPE_LONG:
		fprintf(stderr, "%" PRIi64, *(int64_t *) body);
		break;
	case SPA_POD_TYPE_FLOAT:
		fprintf(stderr, "%f", *(float *) body);
		break;
	case SPA_POD_TYPE_DOUBLE:
		fprintf(stderr, "%g", *(double *) body);
		break;
	case SPA_POD_TYPE_STRING:
		fprintf(stderr, "%s", (char *) body);
		break;
	case SPA_POD_TYPE_RECTANGLE:
	{
		struct spa_rectangle *r = body;
		fprintf(stderr, "%" PRIu32 "x%" PRIu32, r->width, r->height);
		break;
	}
	case SPA_POD_TYPE_FRACTION:
	{
		struct spa_fraction *f = body;
		fprintf(stderr, "%" PRIu32 "/%" PRIu32, f->num, f->denom);
		break;
	}
	case SPA_POD_TYPE_BITMASK:
		fprintf(stderr, "Bitmask");
		break;
	case SPA_POD_TYPE_BYTES:
		fprintf(stderr, "Bytes");
		break;
	default:
		break;
	}
}

int spa_debug_format(const struct spa_format *format)
{
	int i;
	const char *media_type;
	const char *media_subtype;
	struct spa_pod_prop *prop;
	uint32_t mtype, mstype;

	if (format == NULL)
		return SPA_RESULT_INVALID_ARGUMENTS;

	mtype = format->body.media_type.value;
	mstype = format->body.media_subtype.value;

	media_type = spa_type_map_get_type(map, mtype);
	media_subtype = spa_type_map_get_type(map, mstype);

	fprintf(stderr, "%-6s %s/%s\n", "", rindex(media_type, ':') + 1,
		rindex(media_subtype, ':') + 1);

	SPA_FORMAT_FOREACH(format, prop) {
		const char *key;

		if ((prop->body.flags & SPA_POD_PROP_FLAG_UNSET) &&
		    (prop->body.flags & SPA_POD_PROP_FLAG_OPTIONAL))
			continue;

		key = spa_type_map_get_type(map, prop->body.key);

		fprintf(stderr, "  %20s : (%s) ", rindex(key, ':') + 1,
			pod_type_names[prop->body.value.type]);

		if (!(prop->body.flags & SPA_POD_PROP_FLAG_UNSET)) {
			print_format_value(prop->body.value.size,
					   prop->body.value.type, SPA_POD_BODY(&prop->body.value));
		} else {
			const char *ssep, *esep, *sep;
			void *alt;

			switch (prop->body.flags & SPA_POD_PROP_RANGE_MASK) {
			case SPA_POD_PROP_RANGE_MIN_MAX:
			case SPA_POD_PROP_RANGE_STEP:
				ssep = "[ ";
				sep = ", ";
				esep = " ]";
				break;
			default:
			case SPA_POD_PROP_RANGE_ENUM:
			case SPA_POD_PROP_RANGE_FLAGS:
				ssep = "{ ";
				sep = ", ";
				esep = " }";
				break;
			}

			fprintf(stderr, "%s", ssep);

			i = 0;
			SPA_POD_PROP_ALTERNATIVE_FOREACH(&prop->body, prop->pod.size, alt) {
				if (i > 0)
					fprintf(stderr, "%s", sep);
				print_format_value(prop->body.value.size,
						   prop->body.value.type, alt);
				i++;
			}
			fprintf(stderr, "%s", esep);
		}
		fprintf(stderr, "\n");
	}
	return SPA_RESULT_OK;
}

int spa_debug_dict(const struct spa_dict *dict)
{
	unsigned int i;

	if (dict == NULL)
		return SPA_RESULT_INVALID_ARGUMENTS;

	for (i = 0; i < dict->n_items; i++)
		fprintf(stderr, "          %s = \"%s\"\n", dict->items[i].key,
			dict->items[i].value);

	return SPA_RESULT_OK;
}
