/* Simple Plugin API
 * Copyright (C) 2018 Wim Taymans <wim.taymans@gmail.com>
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

#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <spa/param/props.h>
#include <spa/pod/iter.h>
#include <spa/pod/builder.h>

static inline int spa_pod_compare_value(enum spa_pod_type type, const void *r1, const void *r2)
{
	switch (type) {
	case SPA_POD_TYPE_INVALID:
		return 0;
	case SPA_POD_TYPE_BOOL:
	case SPA_POD_TYPE_ID:
		return *(int32_t *) r1 == *(uint32_t *) r2 ? 0 : 1;
	case SPA_POD_TYPE_INT:
		return *(int32_t *) r1 - *(int32_t *) r2;
	case SPA_POD_TYPE_LONG:
		return *(int64_t *) r1 - *(int64_t *) r2;
	case SPA_POD_TYPE_FLOAT:
		return *(float *) r1 - *(float *) r2;
	case SPA_POD_TYPE_DOUBLE:
		return *(double *) r1 - *(double *) r2;
	case SPA_POD_TYPE_STRING:
		return strcmp(r1, r2);
	case SPA_POD_TYPE_RECTANGLE:
	{
		const struct spa_rectangle *rec1 = (struct spa_rectangle *) r1,
		    *rec2 = (struct spa_rectangle *) r2;
		if (rec1->width == rec2->width && rec1->height == rec2->height)
			return 0;
		else if (rec1->width < rec2->width || rec1->height < rec2->height)
			return -1;
		else
			return 1;
	}
	case SPA_POD_TYPE_FRACTION:
	{
		const struct spa_fraction *f1 = (struct spa_fraction *) r1,
		    *f2 = (struct spa_fraction *) r2;
		int64_t n1, n2;
		n1 = ((int64_t) f1->num) * f2->denom;
		n2 = ((int64_t) f2->num) * f1->denom;
		if (n1 < n2)
			return -1;
		else if (n1 > n2)
			return 1;
		else
			return 0;
	}
	default:
		break;
	}
	return 0;
}

static inline int spa_pod_compare_part(const struct spa_pod *pod1, uint32_t pod1_size,
				       const struct spa_pod *pod2, uint32_t pod2_size)
{
	const struct spa_pod *p1, *p2;
	int res;

	p2 = pod2;

	SPA_POD_FOREACH(pod1, pod1_size, p1) {
		bool do_advance = true;
		uint32_t recurse_offset = 0;

		if (p2 == NULL)
			return -EINVAL;

		switch (SPA_POD_TYPE(p1)) {
		case SPA_POD_TYPE_STRUCT:
		case SPA_POD_TYPE_OBJECT:
			if (SPA_POD_TYPE(p2) != SPA_POD_TYPE(p1))
				return -EINVAL;

			if (SPA_POD_TYPE(p1) == SPA_POD_TYPE_STRUCT)
				recurse_offset = sizeof(struct spa_pod_struct);
			else
				recurse_offset = sizeof(struct spa_pod_object);

			do_advance = true;
			break;
		case SPA_POD_TYPE_PROP:
		{
			struct spa_pod_prop *pr1, *pr2;
			void *a1, *a2;

			pr1 = (struct spa_pod_prop *) p1;
			pr2 = spa_pod_contents_find_prop(pod2, pod2_size, pr1->body.key);

			if (pr2 == NULL)
				return -EINVAL;

			/* incompatible property types */
			if (pr1->body.value.type != pr2->body.value.type)
				return -EINVAL;

			if (pr1->body.flags & SPA_POD_PROP_FLAG_UNSET ||
			    pr2->body.flags & SPA_POD_PROP_FLAG_UNSET)
				return -EINVAL;

			a1 = SPA_MEMBER(pr1, sizeof(struct spa_pod_prop), void);
			a2 = SPA_MEMBER(pr2, sizeof(struct spa_pod_prop), void);

			res = spa_pod_compare_value(pr1->body.value.type, a1, a2);
			break;
		}
		default:
			if (SPA_POD_TYPE(p1) != SPA_POD_TYPE(p2))
				return -EINVAL;

			res = spa_pod_compare_value(SPA_POD_TYPE(p1), SPA_POD_BODY(p1), SPA_POD_BODY(p2));
			do_advance = true;
			break;
		}
		if (recurse_offset) {
			res = spa_pod_compare_part(SPA_MEMBER(p1,recurse_offset,void),
						   SPA_POD_SIZE(p1) - recurse_offset,
						   SPA_MEMBER(p2,recurse_offset,void),
						   SPA_POD_SIZE(p2) - recurse_offset);
		}
		if (do_advance) {
			p2 = spa_pod_next(p2);
			if (!spa_pod_is_inside(pod2, pod2_size, p2))
				p2 = NULL;
		}
		if (res != 0)
			return res;
	}
	if (p2 != NULL)
		return -EINVAL;

	return 0;
}

static inline int spa_pod_compare(const struct spa_pod *pod1,
				  const struct spa_pod *pod2)
{
        spa_return_val_if_fail(pod1 != NULL, -EINVAL);
        spa_return_val_if_fail(pod2 != NULL, -EINVAL);

	return spa_pod_compare_part(pod1, SPA_POD_SIZE(pod1), pod2, SPA_POD_SIZE(pod2));
}
