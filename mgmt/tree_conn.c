/*
 *   Copyright (C) 2018 Samsung Electronics Co., Ltd.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/list.h>
#include <linux/slab.h>

#include "tree_conn.h"
#include "user_config.h"
#include "share_config.h"
#include "../buffer_pool.h"
#include "../transport_ipc.h"
#include "../transport_tcp.h"

#include "../export.h" /* FIXME */
#include "../cifsd_server.h" /* FIXME */

enum CIFSD_TREE_CONN_STATUS cifsd_tree_conn_connect(struct cifsd_sess *sess,
						    char *share_name,
						    int protocol)
{
	struct cifsd_tree_connect_response *resp = NULL;
	struct cifsd_share_config *sc = NULL;
	struct cifsd_tree_connect *tree_conn = NULL;
	struct sockaddr *peer_addr;
	int ret;

	sc = cifsd_share_config_get(share_name);
	if (!sc)
		return -EINVAL;

	tree_conn = cifsd_alloc(sizeof(struct cifsd_tree_connect));
	if (!tree_conn) {
		ret = -ENOMEM;
		goto out_error;
	}

	peer_addr = CIFSD_TCP_PEER_SOCKADDR(sess->conn);
	resp = cifsd_ipc_tree_connect_request(protocol,
					      sess->user,
					      sc,
					      peer_addr);
	if (!resp) {
		ret = -EINVAL;
		goto out_error;
	}

	ret = resp->status;
	if (ret != CIFSD_TREE_CONN_STATUS_OK)
		goto out_error;

	tree_conn->flags = resp->connection_flags;
	tree_conn->id = resp->connection_id;
	tree_conn->user = sess->user;
	tree_conn->share_conf = sc;

	list_add(&tree_conn->list, &sess->tcon_list);

	cifsd_free(resp);
	return ret;

out_error:
	cifsd_share_config_put(sc);
	cifsd_free(tree_conn);
	cifsd_free(resp);
	return ret;
}

int cifsd_tree_conn_disconnect(struct cifsd_tree_connect *tree_conn)
{
	int ret;
	
	ret = cifsd_ipc_tree_disconnect_request(tree_conn->id);
	list_del(&tree_conn->list);
	cifsd_share_config_put(tree_conn->share_conf);
	cifsd_free(tree_conn);
	return ret;
}

struct cifsd_tree_connect *cifsd_tree_conn_lookup(struct cifsd_sess *sess,
						  unsigned int id)
{
	struct cifsd_tree_connect *tree_conn;
	struct list_head *tmp;

	list_for_each(tmp, &sess->tcon_list) {
		tree_conn = list_entry(tmp, struct cifsd_tree_connect, list);
		if (tree_conn->id == id)
			return tree_conn;
	}
	return NULL;
}