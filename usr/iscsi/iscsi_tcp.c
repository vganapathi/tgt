/*
 * Software iSCSI target over TCP/IP Data-Path
 *
 * Copyright (C) 2006-2007 FUJITA Tomonori <tomof@acm.org>
 * Copyright (C) 2006-2007 Mike Christie <michaelc@cs.wisc.edu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>

#include "iscsid.h"
#include "tgtd.h"
#include "util.h"

static void iscsi_tcp_event_handler(int fd, int events, void *data);

static int listen_fds[8];
static struct iscsi_transport iscsi_tcp;

struct iscsi_tcp_connection {
	int fd;

	struct iscsi_connection iscsi_conn;
};

static inline struct iscsi_tcp_connection *TCP_CONN(struct iscsi_connection *conn)
{
	return container_of(conn, struct iscsi_tcp_connection, iscsi_conn);
}

static int set_keepalive(int fd)
{
	int ret, opt;

	opt = 1;
	ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
	if (ret)
		return ret;

	opt = 1800;
	ret = setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &opt, sizeof(opt));
	if (ret)
		return ret;

	opt = 6;
	ret = setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &opt, sizeof(opt));
	if (ret)
		return ret;

	opt = 300;
	ret = setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &opt, sizeof(opt));
	if (ret)
		return ret;

	return 0;
}

static int set_nodelay(int fd)
{
	int ret, opt;

	opt = 1;
	ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
	return ret;
}

static void accept_connection(int afd, int events, void *data)
{
	struct sockaddr_storage from;
	socklen_t namesize;
	struct iscsi_connection *conn;
	struct iscsi_tcp_connection *tcp_conn;
	int fd, ret;

	dprintf("%d\n", afd);

	namesize = sizeof(from);
	fd = accept(afd, (struct sockaddr *) &from, &namesize);
	if (fd < 0) {
		eprintf("can't accept, %m\n");
		return;
	}

	if (!is_system_available())
		goto out;

	if (list_empty(&iscsi_targets_list))
		goto out;

	ret = set_keepalive(fd);
	if (ret)
		goto out;

	ret = set_nodelay(fd);
	if (ret)
		goto out;

	tcp_conn = zalloc(sizeof(*tcp_conn));
	if (!tcp_conn)
		goto out;

	conn = &tcp_conn->iscsi_conn;

	ret = conn_init(conn);
	if (ret) {
		free(tcp_conn);
		goto out;
	}

	tcp_conn->fd = fd;
	conn->tp = &iscsi_tcp;

	conn_read_pdu(conn);
	set_non_blocking(fd);

	ret = tgt_event_add(fd, EPOLLIN, iscsi_tcp_event_handler, conn);
	if (ret) {
		conn_exit(conn);
		free(tcp_conn);
		goto out;
	}

	return;
out:
	close(fd);
	return;
}

static void iscsi_tcp_event_handler(int fd, int events, void *data)
{
	struct iscsi_connection *conn = (struct iscsi_connection *) data;

	if (events & EPOLLIN)
		iscsi_rx_handler(conn);

	if (conn->state == STATE_CLOSE)
		dprintf("connection closed\n");

	if (conn->state != STATE_CLOSE && events & EPOLLOUT)
		iscsi_tx_handler(conn);

	if (conn->state == STATE_CLOSE) {
		dprintf("connection closed %p\n", conn);
		conn_close(conn);
	}
}

static int iscsi_tcp_init(void)
{
	struct addrinfo hints, *res, *res0;
	char servname[64];
	int ret, fd, opt, nr_sock = 0;

	memset(servname, 0, sizeof(servname));
	snprintf(servname, sizeof(servname), "%d", iscsi_listen_port);

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	ret = getaddrinfo(iscsi_portal_addr, servname, &hints, &res0);
	if (ret) {
		eprintf("unable to get address info, %m\n");
		return -errno;
	}

	for (res = res0; res; res = res->ai_next) {
		fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (fd < 0) {
			if (res->ai_family == AF_INET6)
				dprintf("IPv6 support is disabled.\n");
			else
				eprintf("unable to create fdet %d %d %d, %m\n",
					res->ai_family,	res->ai_socktype,
					res->ai_protocol);
			continue;
		}

		opt = 1;
		ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt,
				 sizeof(opt));
		if (ret)
			dprintf("unable to set SO_REUSEADDR, %m\n");

		opt = 1;
		if (res->ai_family == AF_INET6) {
			ret = setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &opt,
					 sizeof(opt));
			if (ret) {
				close(fd);
				continue;
			}
		}

		ret = bind(fd, res->ai_addr, res->ai_addrlen);
		if (ret) {
			close(fd);
			eprintf("unable to bind server socket, %m\n");
			continue;
		}

		ret = listen(fd, SOMAXCONN);
		if (ret) {
			eprintf("unable to listen to server socket, %m\n");
			close(fd);
			continue;
		}

		set_non_blocking(fd);
		ret = tgt_event_add(fd, EPOLLIN, accept_connection, NULL);
		if (ret)
			close(fd);
		else {
			listen_fds[nr_sock] = fd;
			nr_sock++;
		}
	}

	freeaddrinfo(res0);

	return !nr_sock;
}

static void iscsi_tcp_exit(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(listen_fds); i++) {
		if (!listen_fds[i])
			break;

		tgt_event_del(listen_fds[i]);
		close(listen_fds[i]);
	}
}

static int iscsi_tcp_conn_login_complete(struct iscsi_connection *conn)
{
	return 0;
}

static size_t iscsi_tcp_read(struct iscsi_connection *conn, void *buf,
			     size_t nbytes)
{
	struct iscsi_tcp_connection *tcp_conn = TCP_CONN(conn);
	return read(tcp_conn->fd, buf, nbytes);
}

static size_t iscsi_tcp_write_begin(struct iscsi_connection *conn, void *buf,
				    size_t nbytes)
{
	struct iscsi_tcp_connection *tcp_conn = TCP_CONN(conn);
	int opt = 1;

	setsockopt(tcp_conn->fd, SOL_TCP, TCP_CORK, &opt, sizeof(opt));
	return write(tcp_conn->fd, buf, nbytes);
}

static void iscsi_tcp_write_end(struct iscsi_connection *conn)
{
	struct iscsi_tcp_connection *tcp_conn = TCP_CONN(conn);
	int opt = 0;

	setsockopt(tcp_conn->fd, SOL_TCP, TCP_CORK, &opt, sizeof(opt));
}

static size_t iscsi_tcp_close(struct iscsi_connection *conn)
{
	struct iscsi_tcp_connection *tcp_conn = TCP_CONN(conn);

	tgt_event_del(tcp_conn->fd);
	return close(tcp_conn->fd);
}

static void iscsi_tcp_release(struct iscsi_connection *conn)
{
	struct iscsi_tcp_connection *tcp_conn = TCP_CONN(conn);

	conn_exit(conn);
	free(tcp_conn);
}

static int iscsi_tcp_show(struct iscsi_connection *conn, char *buf, int rest)
{
	struct iscsi_tcp_connection *tcp_conn = TCP_CONN(conn);
	int err, total = 0;
	socklen_t slen;
	char dst[INET6_ADDRSTRLEN];
	struct sockaddr_storage from;

	slen = sizeof(from);
	err = getpeername(tcp_conn->fd, (struct sockaddr *) &from, &slen);
	if (err < 0) {
		eprintf("%m\n");
		return 0;
	}

	err = getnameinfo((struct sockaddr *)&from, sizeof(from), dst,
			  sizeof(dst), NULL, 0, NI_NUMERICHOST);
	if (err < 0) {
		eprintf("%m\n");
		return 0;
	}

	total = snprintf(buf, rest, "IP Address: %s", dst);

	return total > 0 ? total : 0;
}

static void iscsi_event_modify(struct iscsi_connection *conn, int events)
{
	struct iscsi_tcp_connection *tcp_conn = TCP_CONN(conn);
	int ret;

	ret = tgt_event_modify(tcp_conn->fd, events);
	if (ret)
		eprintf("tgt_event_modify failed\n");
}

static struct iscsi_task *iscsi_tcp_alloc_task(struct iscsi_connection *conn,
					size_t ext_len)
{
	struct iscsi_task *task;

	task = malloc(sizeof(*task) + ext_len);
	if (task)
		memset(task, 0, sizeof(*task) + ext_len);
	return task;
}

static void iscsi_tcp_free_task(struct iscsi_task *task)
{
	free(task);
}

static void *iscsi_tcp_alloc_data_buf(struct iscsi_connection *conn, size_t sz)
{
	return valloc(sz);
}

static void iscsi_tcp_free_data_buf(struct iscsi_connection *conn, void *buf)
{
	if (buf)
		free(buf);
}

static int iscsi_tcp_getsockname(struct iscsi_connection *conn,
				 struct sockaddr *sa, socklen_t *len)
{
	struct iscsi_tcp_connection *tcp_conn = TCP_CONN(conn);

	return getsockname(tcp_conn->fd, sa, len);
}

static int iscsi_tcp_getpeername(struct iscsi_connection *conn,
				 struct sockaddr *sa, socklen_t *len)
{
	struct iscsi_tcp_connection *tcp_conn = TCP_CONN(conn);

	return getpeername(tcp_conn->fd, sa, len);
}

static struct iscsi_transport iscsi_tcp = {
	.name			= "iscsi",
	.rdma			= 0,
	.data_padding		= PAD_WORD_LEN,
	.ep_init		= iscsi_tcp_init,
	.ep_exit		= iscsi_tcp_exit,
	.ep_login_complete	= iscsi_tcp_conn_login_complete,
	.alloc_task		= iscsi_tcp_alloc_task,
	.free_task		= iscsi_tcp_free_task,
	.ep_read		= iscsi_tcp_read,
	.ep_write_begin		= iscsi_tcp_write_begin,
	.ep_write_end		= iscsi_tcp_write_end,
	.ep_close		= iscsi_tcp_close,
	.ep_release		= iscsi_tcp_release,
	.ep_show		= iscsi_tcp_show,
	.ep_event_modify	= iscsi_event_modify,
	.alloc_data_buf		= iscsi_tcp_alloc_data_buf,
	.free_data_buf		= iscsi_tcp_free_data_buf,
	.ep_getsockname		= iscsi_tcp_getsockname,
	.ep_getpeername		= iscsi_tcp_getpeername,
};

__attribute__((constructor)) static void iscsi_transport_init(void)
{
	iscsi_transport_register(&iscsi_tcp);
}
