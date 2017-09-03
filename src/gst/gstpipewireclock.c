/* GStreamer
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

#include "gstpipewireclock.h"

GST_DEBUG_CATEGORY_STATIC (gst_pipewire_clock_debug_category);
#define GST_CAT_DEFAULT gst_pipewire_clock_debug_category

G_DEFINE_TYPE (GstPipeWireClock, gst_pipewire_clock, GST_TYPE_SYSTEM_CLOCK);

GstClock *
gst_pipewire_clock_new (struct pw_stream *stream)
{
  GstPipeWireClock *clock;

  clock = g_object_new (GST_TYPE_PIPEWIRE_CLOCK, NULL);
  clock->stream = stream;

  return GST_CLOCK_CAST (clock);
}

static GstClockTime
gst_pipewire_clock_get_internal_time (GstClock * clock)
{
  GstPipeWireClock *pclock = (GstPipeWireClock *) clock;
  GstClockTime result;
  struct pw_time t;

  pw_stream_get_time (pclock->stream, &t);

  if (t.rate)
    result = gst_util_uint64_scale_int (t.ticks, GST_SECOND, t.rate);
  else
    result = GST_CLOCK_TIME_NONE;

  GST_DEBUG ("%"PRId64", %d %"PRId64, t.ticks, t.rate, result);

  return result;
}


static void
gst_pipewire_clock_finalize (GObject * object)
{
  GstPipeWireClock *clock = GST_PIPEWIRE_CLOCK (object);

  GST_DEBUG_OBJECT (clock, "finalize");

  G_OBJECT_CLASS (gst_pipewire_clock_parent_class)->finalize (object);
}

static void
gst_pipewire_clock_class_init (GstPipeWireClockClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstClockClass *gstclock_class = GST_CLOCK_CLASS (klass);

  gobject_class->finalize = gst_pipewire_clock_finalize;

  gstclock_class->get_internal_time = gst_pipewire_clock_get_internal_time;

  GST_DEBUG_CATEGORY_INIT (gst_pipewire_clock_debug_category, "pipewireclock", 0,
      "debug category for pipewireclock object");
}

static void
gst_pipewire_clock_init (GstPipeWireClock * clock)
{
  GST_OBJECT_FLAG_SET (clock, GST_CLOCK_FLAG_CAN_SET_MASTER);
}
