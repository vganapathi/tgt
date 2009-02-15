#ifndef __TGTBSD_NETINET_IN_H__
#define __TGTBSD_NETINET_IN_H__

/*FIXME: define _KERNEL is needed in BSD for some of the ipv6 stuff in
 *       usr/iscsi/target.c
 */

#define _KERNEL
#include <../include/netinet/in.h>

#endif /* __TGTBSD_NETINET_IN_H__ */
