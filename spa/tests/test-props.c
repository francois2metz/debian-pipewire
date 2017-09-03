/* Spa
 * Copyright (C) 2017 Wim Taymans <wim.taymans@gmail.com>
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <spa/type-map.h>
#include <spa/type-map-impl.h>
#include <spa/log.h>
#include <spa/node.h>
#include <spa/loop.h>
#include <spa/video/format-utils.h>
#include <spa/format-builder.h>

#include <lib/debug.h>

#if 0
/* { video/raw,
 *   format: (int 1) { 1  2 },
 *   size:   (rect (320 240)) { (1 1) (MAX MAX) },
 *   framerate: (frac (25 1)) { (0 1) (MAX 1) },
 *
 * "(object $format
 * 	(id $video) (id $raw)
 *      (prop $format    (int $I420) { $I420 $YUY2 } )
 *      (prop $size      (rect (320 240)) [ (1 1) (MAX MAX) ] )
 *      (prop $framerate (frac (25 1))))
 * "
 *
 */

bool:      true | false
int:       <num>
float:     <num.frac>
string:    "<string>"
bytes:     "$<base64>"
pointer:   "@<pointer>"
rectangle: "<width>x<height>"
fraction:  "<num>/<denom>"
bitmask:   "|<bits>"
array:     "[<values>,...]"
object:    "{<key>: <value>,...}"
key:       "<name>"


  "format": "S16LE",
  "format": [ "S16LE", "S24LE" ],
  "size": "320x240",
  "+size": [ "320x240", { "min": "320x240", "max": "640x480", "step": "8x8" ],
  "-size": [ "320x240", ["320x240", "640x480", "512x400"] ],

  "schema.size": { "default": "320x240",
		   "flags": ["range", "optional"],
		   "values": ["320x240", "640x480", "8x8"] }


  "size-schema":
	{ "default": "320x240",
	  "values": ["320x240", "640x480"]
	},
  "size-schema":
	{ "default": "320x240",
	  "values": { "min": "320x240", "max": "640x480", "step": "8x8" }
	},


  "size-alt": { "def": "320x240", "type": "range", "vals": ["320x240", "640x480", "8x8"] },
  "size-alt": { "def": "320x240", "type": "enum", "vals": ["320x240", "640x480"],

{
  "type": "audio/raw",
  "format": ["S16LE", "enum", "S16LE", "S24LE" ],
  "size": [ "320x240", "range", "320x240", "640x480" ],
  "framerate": [ "25/1", "range", "25/1", "30/1" ]
}

{ "type": "audio", "subtype": "raw",
  "format": "S16LE",
  "size": { "min": "320x240", "max": "640x480", "step": "8x8" },

  "framerate": [ "25/1", "30/1" ] }

spa_build(SPA_MEDIA_TYPE_VIDEO, SPA_MEDIA_SUBTYPE_RAW,
	  type.format_video.format, SPA_PROP_TYPE_ID,
	  video_format.I420
	  SPA_POD_PROP_FLAG_UNSET |
	  SPA_PROP_RANGE_ENUM, 2,
	  video_format.I420,
	  video_format.YUY2,
	  type.format_video.size, SPA_PROP_TYPE_RECTANGLE,
	  320, 240,
	  SPA_POD_PROP_FLAG_UNSET |
	  SPA_PROP_RANGE_MIN_MAX,
	  1, 1,
	  INT32_MAX, INT32_MAX,
	  type.format_video.framerate, SPA_PROP_TYPE_FRACTION, 25, 1,
	  SPA_POD_PROP_FLAG_UNSET | SPA_PROP_RANGE_MIN_MAX, 0, 1, INT32_MAX, 1, 0);
#endif

static SPA_TYPE_MAP_IMPL(default_map, 4096);

static struct {
	uint32_t format;
	struct spa_type_media_type media_type;
	struct spa_type_media_subtype media_subtype;
	struct spa_type_format_video format_video;
	struct spa_type_video_format video_format;
} type = { 0,};

static inline void type_init(struct spa_type_map *map)
{
	type.format = spa_type_map_get_id(map, SPA_TYPE__Format);
	spa_type_media_type_map(map, &type.media_type);
	spa_type_media_subtype_map(map, &type.media_subtype);
	spa_type_format_video_map(map, &type.format_video);
	spa_type_video_format_map(map, &type.video_format);
}

static void do_static_struct(struct spa_type_map *map)
{
	struct _test_format {
		struct spa_format fmt;

		struct {
			struct spa_pod_prop prop_format		SPA_ALIGNED(8);
			struct {
				uint32_t def_format;
				uint32_t enum_format[2];
			} format_vals;

			struct spa_pod_prop prop_size		SPA_ALIGNED(8);
			struct {
				struct spa_rectangle def_size;
				struct spa_rectangle min_size;
				struct spa_rectangle max_size;
			} size_vals;

			struct spa_pod_prop prop_framerate	SPA_ALIGNED(8);
			struct {
				struct spa_fraction def_framerate;
				struct spa_fraction min_framerate;
				struct spa_fraction max_framerate;
			} framerate_vals;
		} props;
	} test_format = {
		SPA_FORMAT_INIT(sizeof(test_format.props) + sizeof(struct spa_format_body),
				type.format, type.media_type.video, type.media_subtype.raw),
		{
			SPA_POD_PROP_INIT(sizeof(test_format.props.format_vals) +
						sizeof(struct spa_pod_prop_body),
					  type.format_video.format,
					  SPA_POD_PROP_RANGE_ENUM | SPA_POD_PROP_FLAG_UNSET,
					  sizeof(uint32_t), SPA_POD_TYPE_ID),
			{
				type.video_format.I420,
				{ type.video_format.I420, type.video_format.YUY2 }
			},
			SPA_POD_PROP_INIT(sizeof(test_format.props.size_vals) +
						sizeof(struct spa_pod_prop_body),
					  type.format_video.size,
					  SPA_POD_PROP_RANGE_MIN_MAX | SPA_POD_PROP_FLAG_UNSET,
					  sizeof(struct spa_rectangle), SPA_POD_TYPE_RECTANGLE),

			{
				{ 320, 243},
				{ 1, 1}, { INT32_MAX, INT32_MAX}
			},
			SPA_POD_PROP_INIT(sizeof(test_format.props.framerate_vals) +
						sizeof(struct spa_pod_prop_body),
					  type.format_video.framerate,
					  SPA_POD_PROP_RANGE_MIN_MAX | SPA_POD_PROP_FLAG_UNSET,
					  sizeof(struct spa_fraction), SPA_POD_TYPE_FRACTION),
			{
				{ 25, 1},
				{ 0, 1}, { INT32_MAX, 1}
			}
		}
	};

	spa_debug_pod(&test_format.fmt.pod);
	spa_debug_format(&test_format.fmt);

	{
		uint32_t format = 0, match;
		struct spa_fraction frac = { 0, 0 };

		match = spa_pod_contents_query(&test_format.fmt.pod, sizeof(struct spa_format),
					       type.format_video.format, SPA_POD_TYPE_ID, &format,
					       type.format_video.framerate, SPA_POD_TYPE_FRACTION,
					       &frac, 0);

		printf("%d %d %d %d\n", match, format, frac.num, frac.denom);

		spa_format_fixate(&test_format.fmt);

		match = spa_pod_contents_query(&test_format.fmt.pod, sizeof(struct spa_format),
					       type.format_video.format, SPA_POD_TYPE_ID, &format,
					       type.format_video.framerate, SPA_POD_TYPE_FRACTION,
					       &frac, 0);

		printf("%d %d %d %d\n", match, format, frac.num, frac.denom);
	}

}

int main(int argc, char *argv[])
{
	struct spa_pod_builder b = { NULL, };
	struct spa_pod_frame frame[4];
	uint8_t buffer[1024];
	struct spa_format *fmt;
	struct spa_type_map *map = &default_map.map;

	type_init(map);
	spa_debug_set_type_map(map);

	spa_pod_builder_init(&b, buffer, sizeof(buffer));

	fmt = SPA_MEMBER(buffer, spa_pod_builder_push_format(&b, &frame[0], type.format,
							     type.media_type.video,
							     type.media_subtype.raw),
			 struct spa_format);
	spa_pod_builder_push_prop(&b, &frame[1], type.format_video.format,
				  SPA_POD_PROP_RANGE_ENUM | SPA_POD_PROP_FLAG_UNSET);
	spa_pod_builder_id(&b, type.video_format.I420);
	spa_pod_builder_id(&b, type.video_format.I420);
	spa_pod_builder_id(&b, type.video_format.YUY2);
	spa_pod_builder_pop(&b, &frame[1]);

	struct spa_rectangle size_min_max[] = { {1, 1}, {INT32_MAX, INT32_MAX} };
	spa_pod_builder_push_prop(&b, &frame[1],
				  type.format_video.size,
				  SPA_POD_PROP_RANGE_MIN_MAX | SPA_POD_PROP_FLAG_UNSET);
	spa_pod_builder_rectangle(&b, 320, 240);
	spa_pod_builder_raw(&b, size_min_max, sizeof(size_min_max));
	spa_pod_builder_pop(&b, &frame[1]);

	struct spa_fraction rate_min_max[] = { {0, 1}, {INT32_MAX, 1} };
	spa_pod_builder_push_prop(&b, &frame[1],
				  type.format_video.framerate,
				  SPA_POD_PROP_RANGE_MIN_MAX | SPA_POD_PROP_FLAG_UNSET);
	spa_pod_builder_fraction(&b, 25, 1);
	spa_pod_builder_raw(&b, rate_min_max, sizeof(rate_min_max));
	spa_pod_builder_pop(&b, &frame[1]);

	spa_pod_builder_pop(&b, &frame[0]);

	spa_debug_pod(&fmt->pod);

	spa_pod_builder_init(&b, buffer, sizeof(buffer));

	spa_pod_builder_format(&b, &frame[0], type.format,
		type.media_type.video,
		type.media_subtype.raw,
		SPA_POD_TYPE_PROP, &frame[1],
			type.format_video.format,
			SPA_POD_PROP_FLAG_UNSET | SPA_POD_PROP_RANGE_ENUM,
			SPA_POD_TYPE_ID, 3,
				type.video_format.I420,
				type.video_format.I420,
				type.video_format.YUY2,
		-SPA_POD_TYPE_PROP, &frame[1],
		SPA_POD_TYPE_PROP, &frame[1],
			type.format_video.size,
			SPA_POD_PROP_FLAG_UNSET | SPA_POD_PROP_RANGE_MIN_MAX,
			SPA_POD_TYPE_RECTANGLE, 3,
				320, 241,
				1, 1,
				INT32_MAX, INT32_MAX,
		-SPA_POD_TYPE_PROP, &frame[1],
		SPA_POD_TYPE_PROP, &frame[1],
			type.format_video.framerate,
			SPA_POD_PROP_FLAG_UNSET | SPA_POD_PROP_RANGE_MIN_MAX,
			SPA_POD_TYPE_FRACTION, 3,
				25, 1,
				0, 1,
				INT32_MAX, 1,
		-SPA_POD_TYPE_PROP, &frame[1]);

	fmt = SPA_MEMBER(buffer, frame[0].ref, struct spa_format);
	spa_debug_pod(&fmt->pod);
	spa_debug_format(fmt);

	spa_pod_builder_init(&b, buffer, sizeof(buffer));

	spa_pod_builder_add(&b,
		SPA_POD_TYPE_OBJECT, &frame[0], 0, type.format,
			SPA_POD_TYPE_ID, type.media_type.video,
			SPA_POD_TYPE_ID, type.media_subtype.raw,
			SPA_POD_TYPE_PROP, &frame[1],
				type.format_video.format,
				SPA_POD_PROP_FLAG_UNSET | SPA_POD_PROP_RANGE_ENUM,
				SPA_POD_TYPE_ID, 3,
					type.video_format.I420,
					type.video_format.I420,
					type.video_format.YUY2,
			-SPA_POD_TYPE_PROP, &frame[1],
			SPA_POD_TYPE_PROP, &frame[1],
				type.format_video.size,
				SPA_POD_PROP_FLAG_UNSET | SPA_POD_PROP_RANGE_MIN_MAX,
				SPA_POD_TYPE_RECTANGLE, 3,
				320, 242,
				1, 1,
				INT32_MAX, INT32_MAX,
			-SPA_POD_TYPE_PROP, &frame[1],
			SPA_POD_TYPE_PROP, &frame[1],
				type.format_video.framerate,
				SPA_POD_PROP_FLAG_UNSET | SPA_POD_PROP_RANGE_MIN_MAX,
				SPA_POD_TYPE_FRACTION, 3,
				25, 1,
				0, 1,
				INT32_MAX, 1,
			-SPA_POD_TYPE_PROP, &frame[1],
		-SPA_POD_TYPE_OBJECT, &frame[0],
		0);

	fmt = SPA_MEMBER(buffer, frame[0].ref, struct spa_format);
	spa_debug_pod(&fmt->pod);
	spa_debug_format(fmt);

	do_static_struct(map);

	printf("media type is enum %d\n", spa_type_is_a(SPA_TYPE__MediaType, SPA_TYPE_ENUM_BASE));
	printf("media sybtype is enum %d\n",
	       spa_type_is_a(SPA_TYPE__MediaSubtype, SPA_TYPE_ENUM_BASE));
	printf("format is enum %d\n", spa_type_is_a(SPA_TYPE__Format, SPA_TYPE_ENUM_BASE));
	printf("format is pod %d\n", spa_type_is_a(SPA_TYPE__Format, SPA_TYPE_POD_BASE));
	printf("format is object %d\n", spa_type_is_a(SPA_TYPE__Format, SPA_TYPE_POD_OBJECT_BASE));

	return 0;
}
