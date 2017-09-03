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

#ifndef __SPA_GRAPH_SCHEDULER_H__
#define __SPA_GRAPH_SCHEDULER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <spa/graph.h>

#define SPA_GRAPH_STATE_IN		0
#define SPA_GRAPH_STATE_OUT		1
#define SPA_GRAPH_STATE_CHECK_IN	2
#define SPA_GRAPH_STATE_CHECK_OUT	3

struct spa_graph_data {
	struct spa_graph *graph;
        struct spa_list ready;
        struct spa_graph_node *node;
};

static inline void spa_graph_data_init(struct spa_graph_data *data,
				       struct spa_graph *graph)
{
	data->graph = graph;
	spa_list_init(&data->ready);
	data->node = NULL;
}

static inline int spa_graph_node_impl_input(void *data)
{
	struct spa_node *n = data;
	return spa_node_process_input(n);
}

static inline int spa_graph_node_impl_output(void *data)
{
	struct spa_node *n = data;
	return spa_node_process_output(n);
}

static const struct spa_graph_node_callbacks spa_graph_node_impl_default = {
	SPA_VERSION_GRAPH_NODE_CALLBACKS,
	spa_graph_node_impl_input,
	spa_graph_node_impl_output,
};

static inline void spa_graph_data_port_check(struct spa_graph_data *data, struct spa_graph_port *port)
{
	struct spa_graph_node *node = port->node;

	if (port->io->status == SPA_RESULT_HAVE_BUFFER)
		node->ready_in++;

	debug("port %p node %p check %d %d %d\n", port, node, port->io->status, node->ready_in, node->required_in);

	if (node->required_in > 0 && node->ready_in == node->required_in) {
		node->state = SPA_GRAPH_STATE_IN;
		if (node->ready_link.next == NULL)
			spa_list_insert(data->ready.prev, &node->ready_link);
	} else if (node->ready_link.next) {
		spa_list_remove(&node->ready_link);
		node->ready_link.next = NULL;
	}
}

static inline bool spa_graph_data_iterate(struct spa_graph_data *data)
{
	bool res;
	int state;
	struct spa_graph_port *p;
	struct spa_graph_node *n;

	res = !spa_list_is_empty(&data->ready);
	if (res) {
		n = spa_list_first(&data->ready, struct spa_graph_node, ready_link);

		spa_list_remove(&n->ready_link);
		n->ready_link.next = NULL;

		debug("node %p state %d\n", n, n->state);

		switch (n->state) {
		case SPA_GRAPH_STATE_IN:
			state = n->callbacks->process_input(n->callbacks_data);
			if (state == SPA_RESULT_NEED_BUFFER)
				n->state = SPA_GRAPH_STATE_CHECK_IN;
			else if (state == SPA_RESULT_HAVE_BUFFER)
				n->state = SPA_GRAPH_STATE_CHECK_OUT;
			debug("node %p processed input state %d\n", n, n->state);
			if (n == data->node)
				break;
			spa_list_append(&data->ready, &n->ready_link);
			break;

		case SPA_GRAPH_STATE_OUT:
			state = n->callbacks->process_output(n->callbacks_data);
			if (state == SPA_RESULT_NEED_BUFFER)
				n->state = SPA_GRAPH_STATE_CHECK_IN;
			else if (state == SPA_RESULT_HAVE_BUFFER)
				n->state = SPA_GRAPH_STATE_CHECK_OUT;
			debug("node %p processed output state %d\n", n, n->state);
			spa_list_append(&data->ready, &n->ready_link);
			break;

		case SPA_GRAPH_STATE_CHECK_IN:
			n->ready_in = 0;
			spa_list_for_each(p, &n->ports[SPA_DIRECTION_INPUT], link) {
				struct spa_graph_node *pn = p->peer->node;
				if (p->io->status == SPA_RESULT_NEED_BUFFER) {
					if (pn != data->node
					    || pn->flags & SPA_GRAPH_NODE_FLAG_ASYNC) {
						pn->state = SPA_GRAPH_STATE_OUT;
						spa_list_append(&data->ready,
								&pn->ready_link);
					}
				} else if (p->io->status == SPA_RESULT_OK)
					n->ready_in++;
			}
		case SPA_GRAPH_STATE_CHECK_OUT:
			spa_list_for_each(p, &n->ports[SPA_DIRECTION_OUTPUT], link)
				spa_graph_data_port_check(data, p->peer);
			break;

		default:
			break;
		}
		res = !spa_list_is_empty(&data->ready);
	}
	return res;
}

static inline int spa_graph_impl_need_input(void *data, struct spa_graph_node *node)
{
	struct spa_graph_data *d = data;
	debug("node %p start pull\n", node);
	node->state = SPA_GRAPH_STATE_CHECK_IN;
	d->node = node;
	if (node->ready_link.next == NULL)
		spa_list_append(&d->ready, &node->ready_link);

	while(spa_graph_data_iterate(data));

	return SPA_RESULT_OK;
}

static inline int spa_graph_impl_have_output(void *data, struct spa_graph_node *node)
{
	struct spa_graph_data *d = data;
	debug("node %p start push\n", node);
	node->state = SPA_GRAPH_STATE_OUT;
	d->node = node;
	if (node->ready_link.next == NULL)
		spa_list_append(&d->ready, &node->ready_link);

	while(spa_graph_data_iterate(data));

	return SPA_RESULT_OK;
}

static const struct spa_graph_callbacks spa_graph_impl_default = {
	SPA_VERSION_GRAPH_CALLBACKS,
	.need_input = spa_graph_impl_need_input,
	.have_output = spa_graph_impl_have_output,
};


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* __SPA_GRAPH_SCHEDULER_H__ */
