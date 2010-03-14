/*
 *  OS dependent services implementation for Linux platform
 *
 * Copyright (C) 2009 Boaz Harrosh <bharrosh@panasas.com>
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
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>

#include <linux/fs.h>

#include "os.h"
#include "log.h"

#if defined (SYNC_FILE_RANGE_WAIT_BEFORE) && \
	(defined(__NR_sync_file_range) || defined(__NR_sync_file_range2))
static inline int __sync_file_range(int fd, __off64_t offset, __off64_t bytes)
{
	int ret;
#if defined(__NR_sync_file_range)
	long int n = __NR_sync_file_range;
#else
	long int n = __NR_sync_file_range2;
#endif
	unsigned int flags = SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WRITE
		| SYNC_FILE_RANGE_WAIT_AFTER;
	ret = syscall(n, fd, offset, bytes, flags);
	if (ret)
		ret = fsync(fd);
	return ret;
}
#else
#define __sync_file_range(fd, offset, bytes) fsync(fd)
#endif

int os_sync_file_range(int fd, __off64_t offset, __off64_t bytes)
{
	return __sync_file_range(fd, offset, bytes);
}

int os_ipc_perm(int fd)
{
	struct ucred cred;
	socklen_t len;
	int err;

	len = sizeof(cred);
	err = getsockopt(fd, SOL_SOCKET, SO_PEERCRED, (void *) &cred, &len);
	if (err) {
		eprintf("can't get sockopt, %m\n");
		return -1;
	}

	if (cred.uid || cred.gid)
		return -EPERM;

	return 0;
}

int os_oom_adjust(void)
{
	int fd, err;
	char path[64];

	/* Avoid oom-killer */
	sprintf(path, "/proc/%d/oom_adj", getpid());
	fd = open(path, O_WRONLY);
	if (fd < 0) {
		fprintf(stderr, "can't adjust oom-killer's pardon %s, %m\n", path);
		return errno;
	}
	err = write(fd, "-17\n", 4);
	if (err < 0) {
		fprintf(stderr, "can't adjust oom-killer's pardon %s, %m\n", path);
		close(fd);
		return errno;
	}
	close(fd);
	return 0;
}

int os_blockdev_size(int fd, uint64_t *size)
{
	return ioctl(fd, BLKGETSIZE64, size);
}
