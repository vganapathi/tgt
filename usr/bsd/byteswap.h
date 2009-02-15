#ifndef __TGTBSD_BYTESWAP_H__
#define __TGTBSD_BYTESWAP_H__

#include <sys/endian.h>

#define bswap_16(x) __bswap16(x)
#define bswap_32(x) __bswap32(x)
#define bswap_64(x) __bswap64(x)

#endif /* __TGTBSD_BYTESWAP_H__ */
