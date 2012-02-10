#ifndef SELECTMAP_H_
#define SELECTMAP_H_

#define V5LX110_BITFILE_SIZE    3889856

/* This bit re-ordering is needed for ROACH v0 due to bad selectMAP bit ordering */
#define ROACH_V1_SMHACK(x) ( \
                (((x) & 0x0100) >> 5)  | \
                (((x) & 0x0200) >> 8)  | \
                (((x) & 0x0400) >> 3)  | \
                (((x) & 0x0800) >> 6)  | \
                (((x) & 0x1000) >> 6)  | \
                (((x) & 0x2000) >> 2)  | \
                (((x) & 0x4000) >> 14) | \
                (((x) & 0x8000) >> 13) | \
                (((x) & 0x0001) << 14) | \
                (((x) & 0x0002) << 11) | \
                (((x) & 0x0004) << 2)  | \
                (((x) & 0x0008) << 6)  | \
                (((x) & 0x0010) << 6)  | \
                (((x) & 0x0020) << 3)  | \
                (((x) & 0x0040) << 7)  | \
                (((x) & 0x0080) << 8))

#endif

