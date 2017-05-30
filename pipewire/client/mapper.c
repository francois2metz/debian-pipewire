/* PipeWire
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

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <spa/type-map.h>
#include <spa/lib/mapper.h>

#include <pipewire/client/map.h>

/** \cond */
struct impl {
	struct spa_type_map map;
	struct pw_map types;
	struct pw_array strings;
};
/** \endcond */

static uint32_t type_map_get_id(struct spa_type_map *map, const char *type)
{
	struct impl *this = SPA_CONTAINER_OF(map, struct impl, map);
	uint32_t i = 0, len;
	void *p;
	off_t o;

	if (type != NULL) {
		for (i = 0; i < pw_map_get_size(&this->types); i++) {
			o = (off_t) pw_map_lookup_unchecked(&this->types, i);
			if (strcmp(SPA_MEMBER(this->strings.data, o, char), type) == 0)
				return i;
		}
		len = strlen(type);
		p = pw_array_add(&this->strings, SPA_ROUND_UP_N(len + 1, 2));
		memcpy(p, type, len + 1);
		o = (p - this->strings.data);
		i = pw_map_insert_new(&this->types, (void *) o);
	}
	return i;
}

static const char *type_map_get_type(const struct spa_type_map *map, uint32_t id)
{
	struct impl *this = SPA_CONTAINER_OF(map, struct impl, map);

	if (id == SPA_ID_INVALID)
		return NULL;

	if (SPA_LIKELY(pw_map_check_id(&this->types, id))) {
		off_t o = (off_t) pw_map_lookup_unchecked(&this->types, id);
		return SPA_MEMBER(this->strings.data, o, char);
	}
	return NULL;
}

static size_t type_map_get_size(const struct spa_type_map *map)
{
	struct impl *this = SPA_CONTAINER_OF(map, struct impl, map);
	return pw_map_get_size(&this->types);
}

static struct impl default_type_map = {
	{sizeof(struct spa_type_map),
	 NULL,
	 type_map_get_id,
	 type_map_get_type,
	 type_map_get_size,
	 },
	PW_MAP_INIT(128),
	PW_ARRAY_INIT(4096)
};

/** Get the default type map
 * \return the default type map
 * \memberof pw_pipewire
 */
struct spa_type_map *pw_type_map_get_default(void)
{
	spa_type_map_set_default(&default_type_map.map);
	return &default_type_map.map;
}
