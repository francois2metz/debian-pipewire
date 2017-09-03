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

#ifndef __SPA_VIDEO_FORMAT_UTILS_H__
#define __SPA_VIDEO_FORMAT_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <spa/format-utils.h>
#include <spa/video/format.h>
#include <spa/video/raw-utils.h>

struct spa_type_format_video {
	uint32_t format;
	uint32_t size;
	uint32_t framerate;
	uint32_t max_framerate;
	uint32_t views;
	uint32_t interlace_mode;
	uint32_t pixel_aspect_ratio;
	uint32_t multiview_mode;
	uint32_t multiview_flags;
	uint32_t chroma_site;
	uint32_t color_range;
	uint32_t color_matrix;
	uint32_t transfer_function;
	uint32_t color_primaries;
	uint32_t profile;
	uint32_t level;
	uint32_t stream_format;
	uint32_t alignment;
};

static inline void
spa_type_format_video_map(struct spa_type_map *map, struct spa_type_format_video *type)
{
	if (type->format == 0) {
		type->format = spa_type_map_get_id(map, SPA_TYPE_FORMAT_VIDEO__format);
		type->size = spa_type_map_get_id(map, SPA_TYPE_FORMAT_VIDEO__size);
		type->framerate = spa_type_map_get_id(map, SPA_TYPE_FORMAT_VIDEO__framerate);
		type->max_framerate = spa_type_map_get_id(map, SPA_TYPE_FORMAT_VIDEO__maxFramerate);
		type->views = spa_type_map_get_id(map, SPA_TYPE_FORMAT_VIDEO__views);
		type->interlace_mode = spa_type_map_get_id(map, SPA_TYPE_FORMAT_VIDEO__interlaceMode);
		type->pixel_aspect_ratio = spa_type_map_get_id(map, SPA_TYPE_FORMAT_VIDEO__pixelAspectRatio);
		type->multiview_mode = spa_type_map_get_id(map, SPA_TYPE_FORMAT_VIDEO__multiviewMode);
		type->multiview_flags = spa_type_map_get_id(map, SPA_TYPE_FORMAT_VIDEO__multiviewFlags);
		type->chroma_site = spa_type_map_get_id(map, SPA_TYPE_FORMAT_VIDEO__chromaSite);
		type->color_range = spa_type_map_get_id(map, SPA_TYPE_FORMAT_VIDEO__colorRange);
		type->color_matrix = spa_type_map_get_id(map, SPA_TYPE_FORMAT_VIDEO__colorMatrix);
		type->transfer_function = spa_type_map_get_id(map, SPA_TYPE_FORMAT_VIDEO__transferFunction);
		type->color_primaries = spa_type_map_get_id(map, SPA_TYPE_FORMAT_VIDEO__colorPrimaries);
		type->profile = spa_type_map_get_id(map, SPA_TYPE_FORMAT_VIDEO__profile);
		type->level = spa_type_map_get_id(map, SPA_TYPE_FORMAT_VIDEO__level);
		type->stream_format = spa_type_map_get_id(map, SPA_TYPE_FORMAT_VIDEO__streamFormat);
		type->alignment = spa_type_map_get_id(map, SPA_TYPE_FORMAT_VIDEO__alignment);
	}
}

static inline bool
spa_format_video_raw_parse(const struct spa_format *format,
			   struct spa_video_info_raw *info, struct spa_type_format_video *type)
{
	spa_format_query(format,
			 type->format, SPA_POD_TYPE_ID, &info->format,
			 type->size, SPA_POD_TYPE_RECTANGLE, &info->size,
			 type->framerate, SPA_POD_TYPE_FRACTION, &info->framerate,
			 type->max_framerate, SPA_POD_TYPE_FRACTION, &info->max_framerate,
			 type->views, SPA_POD_TYPE_INT, &info->views,
			 type->interlace_mode, SPA_POD_TYPE_INT, &info->interlace_mode,
			 type->pixel_aspect_ratio, SPA_POD_TYPE_FRACTION, &info->pixel_aspect_ratio,
			 type->multiview_mode, SPA_POD_TYPE_INT, &info->multiview_mode,
			 type->multiview_flags, SPA_POD_TYPE_INT, &info->multiview_flags,
			 type->chroma_site, SPA_POD_TYPE_INT, &info->chroma_site,
			 type->color_range, SPA_POD_TYPE_INT, &info->color_range,
			 type->color_matrix, SPA_POD_TYPE_INT, &info->color_matrix,
			 type->transfer_function, SPA_POD_TYPE_INT, &info->transfer_function,
			 type->color_primaries, SPA_POD_TYPE_INT, &info->color_primaries, 0);
	return true;
}

static inline bool
spa_format_video_h264_parse(const struct spa_format *format,
			    struct spa_video_info_h264 *info, struct spa_type_format_video *type)
{
	spa_format_query(format,
			 type->size, SPA_POD_TYPE_RECTANGLE, &info->size,
			 type->framerate, SPA_POD_TYPE_FRACTION, &info->framerate,
			 type->max_framerate, SPA_POD_TYPE_FRACTION, &info->max_framerate,
			 type->stream_format, SPA_POD_TYPE_INT, &info->stream_format,
			 type->alignment, SPA_POD_TYPE_INT, &info->alignment, 0);
	return true;
}

static inline bool
spa_format_video_mjpg_parse(const struct spa_format *format,
			    struct spa_video_info_mjpg *info, struct spa_type_format_video *type)
{
	spa_format_query(format,
			 type->size, SPA_POD_TYPE_RECTANGLE, &info->size,
			 type->framerate, SPA_POD_TYPE_FRACTION, &info->framerate,
			 type->max_framerate, SPA_POD_TYPE_FRACTION, &info->max_framerate, 0);
	return true;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __SPA_VIDEO_FORMAT_UTILS */
