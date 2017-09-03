/* Simple Plugin API
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

#ifndef __SPA_POD_UTILS_H__
#define __SPA_POD_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <spa/pod.h>

#define SPA_POD_BODY_SIZE(pod)			(((struct spa_pod*)(pod))->size)
#define SPA_POD_TYPE(pod)			(((struct spa_pod*)(pod))->type)
#define SPA_POD_SIZE(pod)			(sizeof(struct spa_pod) + SPA_POD_BODY_SIZE(pod))
#define SPA_POD_CONTENTS_SIZE(type,pod)		(SPA_POD_SIZE(pod)-sizeof(type))

#define SPA_POD_CONTENTS(type,pod)		SPA_MEMBER((pod),sizeof(type),void)
#define SPA_POD_CONTENTS_CONST(type,pod)	SPA_MEMBER((pod),sizeof(type),const void)
#define SPA_POD_BODY(pod)			SPA_MEMBER((pod),sizeof(struct spa_pod),void)
#define SPA_POD_BODY_CONST(pod)			SPA_MEMBER((pod),sizeof(struct spa_pod),const void)

#define SPA_POD_VALUE(type,pod)			(((type*)pod)->value)

#define SPA_POD_PROP_N_VALUES(prop)		(((prop)->pod.size - sizeof(struct spa_pod_prop_body)) / (prop)->body.value.size)

static inline bool spa_pod_is_object_type(struct spa_pod *pod, uint32_t type)
{
	return (pod->type == SPA_POD_TYPE_OBJECT
		&& ((struct spa_pod_object *) pod)->body.type == type);
}

#define SPA_POD_ARRAY_BODY_FOREACH(body, _size, iter)							\
	for ((iter) = SPA_MEMBER((body), sizeof(struct spa_pod_array_body), __typeof__(*(iter)));	\
	     (iter) < SPA_MEMBER((body), (_size), __typeof__(*(iter)));					\
	     (iter) = SPA_MEMBER((iter), (body)->child.size, __typeof__(*(iter))))

#define SPA_POD_FOREACH(pod, size, iter)								\
	for ((iter) = (pod);										\
	     (iter) < SPA_MEMBER((pod), (size), struct spa_pod);					\
	     (iter) = SPA_MEMBER((iter), SPA_ROUND_UP_N (SPA_POD_SIZE (iter), 8), struct spa_pod))

#define SPA_POD_CONTENTS_FOREACH(pod, offset, iter)						\
	SPA_POD_FOREACH(SPA_MEMBER((pod), (offset), struct spa_pod),SPA_POD_SIZE (pod)-(offset),iter)

#define SPA_POD_OBJECT_BODY_FOREACH(body, size, iter)						\
	for ((iter) = SPA_MEMBER((body), sizeof(struct spa_pod_object_body), struct spa_pod);	\
	     (iter) < SPA_MEMBER((body), (size), struct spa_pod);				\
	     (iter) = SPA_MEMBER((iter), SPA_ROUND_UP_N(SPA_POD_SIZE (iter), 8), struct spa_pod))

#define SPA_POD_OBJECT_FOREACH(obj, iter)					\
	SPA_POD_OBJECT_BODY_FOREACH(&obj->body, SPA_POD_BODY_SIZE(obj), iter)

#define SPA_POD_PROP_ALTERNATIVE_FOREACH(body, _size, iter)					\
	for ((iter) = SPA_MEMBER((body), (body)->value.size +					\
				sizeof(struct spa_pod_prop_body), __typeof__(*iter));		\
	     (iter) <= SPA_MEMBER((body), (_size)-(body)->value.size, __typeof__(*iter));	\
	     (iter) = SPA_MEMBER((iter), (body)->value.size, __typeof__(*iter)))

static inline struct spa_pod_prop *spa_pod_contents_find_prop(const struct spa_pod *pod,
							      uint32_t offset, uint32_t key)
{
	struct spa_pod *res;
	SPA_POD_CONTENTS_FOREACH(pod, offset, res) {
		if (res->type == SPA_POD_TYPE_PROP
		    && ((struct spa_pod_prop *) res)->body.key == key)
			return (struct spa_pod_prop *) res;
	}
	return NULL;
}

static inline struct spa_pod_prop *spa_pod_object_find_prop(const struct spa_pod_object *obj,
							    uint32_t key)
{
	return spa_pod_contents_find_prop(&obj->pod, sizeof(struct spa_pod_object), key);
}

#define SPA_POD_COLLECT(pod,type,args,error)								\
do {													\
	if (type == SPA_POD_TYPE_POD) {									\
		*(va_arg(args, struct spa_pod **)) = pod;						\
	} else if ((pod)->type == SPA_POD_TYPE_NONE) {							\
		switch (type) {										\
		case -SPA_POD_TYPE_ARRAY:								\
		case -SPA_POD_TYPE_STRUCT:								\
		case -SPA_POD_TYPE_OBJECT:								\
		case -SPA_POD_TYPE_PROP:								\
			*(va_arg(args, struct spa_pod **)) = NULL;					\
			break;										\
		default:										\
			goto error;									\
		}											\
	} else if ((pod)->type == type || (pod)->type == -type) {					\
		switch (type) {										\
		case SPA_POD_TYPE_BOOL:									\
		case SPA_POD_TYPE_ID:									\
		case SPA_POD_TYPE_INT:									\
			*(va_arg(args, int32_t*)) = SPA_POD_VALUE(struct spa_pod_int, pod);		\
			break;										\
		case SPA_POD_TYPE_LONG:									\
			*(va_arg(args, int64_t*)) = SPA_POD_VALUE(struct spa_pod_long, pod);		\
			break;										\
		case SPA_POD_TYPE_FLOAT:								\
			*(va_arg(args, float*)) = SPA_POD_VALUE(struct spa_pod_float, pod);		\
			break;										\
		case SPA_POD_TYPE_DOUBLE:								\
			*(va_arg(args, double*)) = SPA_POD_VALUE(struct spa_pod_double, pod);		\
			break;										\
		case SPA_POD_TYPE_STRING:								\
			*(va_arg(args, char **)) = SPA_POD_CONTENTS(struct spa_pod_string, pod);	\
			break;										\
		case -SPA_POD_TYPE_STRING:								\
		{											\
			char *dest = va_arg(args, char *);						\
			uint32_t maxlen = va_arg(args, uint32_t);					\
			strncpy(dest, SPA_POD_CONTENTS(struct spa_pod_string, pod), maxlen-1);		\
			break;										\
		}											\
		case SPA_POD_TYPE_BYTES:								\
			*(va_arg(args, void **)) = SPA_POD_CONTENTS(struct spa_pod_bytes, pod);		\
			*(va_arg(args, uint32_t *)) = SPA_POD_BODY_SIZE(pod);				\
			break;										\
		case SPA_POD_TYPE_POINTER:								\
		{											\
			struct spa_pod_pointer_body *b = SPA_POD_BODY(pod);				\
			*(va_arg(args, void **)) = b->value;						\
			break;										\
		}											\
		case SPA_POD_TYPE_RECTANGLE:								\
			*(va_arg(args, struct spa_rectangle *)) =					\
				SPA_POD_VALUE(struct spa_pod_rectangle, pod);				\
			break;										\
		case SPA_POD_TYPE_FRACTION:								\
			*(va_arg(args, struct spa_fraction *)) =					\
				SPA_POD_VALUE(struct spa_pod_fraction, pod);				\
			break;										\
		case SPA_POD_TYPE_BITMASK:								\
			*(va_arg(args, uint32_t **)) = SPA_POD_CONTENTS(struct spa_pod, pod);		\
			break;										\
		case SPA_POD_TYPE_ARRAY:								\
		case SPA_POD_TYPE_STRUCT:								\
		case SPA_POD_TYPE_OBJECT:								\
		case SPA_POD_TYPE_PROP:									\
		case -SPA_POD_TYPE_ARRAY:								\
		case -SPA_POD_TYPE_STRUCT:								\
		case -SPA_POD_TYPE_OBJECT:								\
		case -SPA_POD_TYPE_PROP:								\
			*(va_arg(args, struct spa_pod **)) = pod;					\
			break;										\
		default:										\
			goto error;									\
		}											\
	} else												\
		goto error;										\
} while (false);

#define SPA_POD_COLLECT_SKIP(type,args)		\
	switch (type) {				\
	case SPA_POD_TYPE_BYTES:		\
	case SPA_POD_TYPE_POINTER:		\
		va_arg (args, void*);		\
		/* fallthrough */		\
	case SPA_POD_TYPE_BOOL:			\
	case SPA_POD_TYPE_ID:			\
	case SPA_POD_TYPE_INT:			\
	case SPA_POD_TYPE_LONG:			\
	case SPA_POD_TYPE_FLOAT:		\
	case SPA_POD_TYPE_DOUBLE:		\
	case SPA_POD_TYPE_STRING:		\
	case -SPA_POD_TYPE_STRING:		\
	case SPA_POD_TYPE_RECTANGLE:		\
	case SPA_POD_TYPE_FRACTION:		\
	case SPA_POD_TYPE_BITMASK:		\
	case SPA_POD_TYPE_ARRAY:		\
	case SPA_POD_TYPE_STRUCT:		\
	case SPA_POD_TYPE_OBJECT:		\
	case SPA_POD_TYPE_PROP:			\
	case SPA_POD_TYPE_POD:			\
	case -SPA_POD_TYPE_ARRAY:		\
	case -SPA_POD_TYPE_STRUCT:		\
	case -SPA_POD_TYPE_OBJECT:		\
	case -SPA_POD_TYPE_PROP:		\
		va_arg(args, void*);		\
	default:				\
		break;				\
	}

static inline uint32_t
spa_pod_contents_queryv(const struct spa_pod *pod, uint32_t offset, uint32_t key, va_list args)
{
	uint32_t count = 0;

	while (key) {
		uint32_t type;
		struct spa_pod_prop *prop = spa_pod_contents_find_prop(pod, offset, key);

		type = va_arg(args, uint32_t);

		if (prop && prop->body.key == key && !(prop->body.flags & SPA_POD_PROP_FLAG_UNSET)) {
			SPA_POD_COLLECT(&prop->body.value, type, args, next);
			count++;
		} else {
			SPA_POD_COLLECT_SKIP(type, args);
		}
	      next:
		key = va_arg(args, uint32_t);
	}
	return count;
}

static inline uint32_t
spa_pod_contents_query(const struct spa_pod *pod, uint32_t offset, uint32_t key, ...)
{
	va_list args;
	uint32_t count;

	va_start(args, key);
	count = spa_pod_contents_queryv(pod, offset, key, args);
	va_end(args);

	return count;
}

#define spa_pod_object_query(object,key, ...) \
	spa_pod_contents_query(&(object)->pod, sizeof(struct spa_pod_object), key, __VA_ARGS__)

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* __SPA_POD_UTILS_H__ */
