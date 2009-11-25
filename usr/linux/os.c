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
#include <unistd.h>
#include <sys/syscall.h>

#include "os.h"

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
