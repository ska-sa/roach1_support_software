#ifndef SELECTMAP_H_
#define SELECTMAP_H_

#define CPLD_SM_STATUS  0x8
#define CPLD_SM_OREGS   0x9
#define CPLD_SM_DATA    0xa
#define CPLD_SM_CTRL    0xb
#define BITFILE_SIZE    3889856
#define BITFILE_PACKETS (BITFILE_SIZE + 127)/128

/* This bit re-ordering is needed for ROACH v0 due to bad selectMAP bit ordering */
#define SM_BITHACK(x) ( \
         (((x) & 0x0001) << 3) | \
         (((x) & 0x0002) << 0) | \
         (((x) & 0x0004) << 5) | \
         (((x) & 0x0008) << 2) | \
         (((x) & 0x0010) << 2) | \
         (((x) & 0x0020) << 6) | \
         (((x) & 0x0040) >> 6) | \
         (((x) & 0x0080) >> 5) | \
         (((x) & 0x0100) << 6) | \
         (((x) & 0x0200) << 3) | \
         (((x) & 0x0400) >> 6) | \
         (((x) & 0x0800) >> 2) | \
         (((x) & 0x1000) >> 2) | \
         (((x) & 0x2000) >> 5) | \
         (((x) & 0x4000) >> 1) | \
         (((x) & 0x8000) << 0))

#endif

