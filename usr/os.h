#ifndef __TGT_OS_H__
#define __TGT_OS_H__

#include <sys/types.h>
#include <linux/fs.h>

int os_sync_file_range(int fd, __off64_t offset, __off64_t bytes);

#endif /* ndef __TGT_OS_H__*/
