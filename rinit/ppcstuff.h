#ifndef PPCSTUFF_H_
#define PPCSTUFF_H_

#define r0 0
#define r1 1
#define r2 2
#define r3 3
#define r4 4
#define r5 5
#define r6 6
#define r7 7
#define r8 8
#define r9 9
#define r10 10
#define r11 11
#define r12 12
#define r13 13
#define r14 14
#define r15 15
#define r16 16
#define r17 17
#define r18 18
#define r19 19
#define r20 20
#define r21 21
#define r22 22
#define r23 23
#define r24 24
#define r25 25
#define r26 26
#define r27 27
#define r28 28
#define r29 29
#define r30 30
#define r31 31

#define r_ccr0   0x3b3

#define r_xer    0x1
#define r_lr     0x8
#define r_mmucr  0x3b2
#define r_dbsr   0x130
#define r_dear   0x3d
#define r_esr    0x3e

#define r_ivpr   0x3f
#define r_ivor0  0x190
#define r_ivor1  0x191
#define r_ivor2  0x192
#define r_ivor3  0x193
#define r_ivor4  0x194
#define r_ivor5  0x195
#define r_ivor6  0x196
#define r_ivor7  0x197
#define r_ivor8  0x198
#define r_ivor9  0x199
#define r_ivor10 0x19a
#define r_ivor11 0x19b
#define r_ivor12 0x19c
#define r_ivor13 0x19d
#define r_ivor14 0x19e
#define r_ivor15 0x19f

#define r_tcr    0x154
#define r_dec    0x16

#define r_csrr0  0x3a
#define r_csrr1  0x3b
#define r_srr0   0x1a
#define r_srr1   0x1b
#define r_mcsrr0 0x23a
#define r_mcsrr1 0x23b

#define CCR0_DAPUIB   0x00100000

/******************************************/

#define TLB_VALID             0x0200
#define TLB_SIZE_4KB          0x0010
#define TLB_SIZE_16KB         0x0020
#define TLB_SIZE_1MB          0x0050
#define TLB_SIZE_16MB         0x0070
#define TLB_SIZE_256MB        0x0090

#define TLB_FLAG_SUPREAD      0x001
#define TLB_FLAG_SUPWRITE     0x002
#define TLB_FLAG_SUPEXECUTE   0x004

#define TLB_FLAG_USERREAD     0x008
#define TLB_FLAG_USERWRITE    0x010
#define TLB_FLAG_USEREXECUTE  0x020

#define TLB_FLAG_BIGENDIAN    0x080
#define TLB_FLAG_GUARDED      0x100
#define TLB_FLAG_COHERENT     0x200
#define TLB_FLAG_UNCACHED     0x400
#define TLB_FLAG_WRITETHROUGH 0x800

/******************************************/

#define d_sdraddr      0xe
#define d_sdrdata      0xf

#define SDR0_CUST0     0x4000

/******************************************/

#define d_uic0_er      0xc2
#define d_uic0_vr      0xc7
#define d_uic0_vcr     0xc8

/******************************************/

#define d_ebcaddr      0x12
#define d_ebcdata      0x13

#define EBC_BANK_1M    0x00000000
#define EBC_BANK_2M    0x00020000
#define EBC_BANK_4M    0x00040000
#define EBC_BANK_8M    0x00060000
#define EBC_BANK_16M   0x00080000
#define EBC_BANK_32M   0x000a0000
#define EBC_BANK_64M   0x000c0000
#define EBC_BANK_128M  0x000e0000

#define EBC_BANK_READ  0x00008000
#define EBC_BANK_WRITE 0x00010000

#define EBC_BANK_8B    0x00000000
#define EBC_BANK_16B   0x00002000
#define EBC_BANK_32B   0x00004000

#define EBC_B0CR       0x0
#define EBC_B1CR       0x1
#define EBC_B2CR       0x2
#define EBC_B4CR       0x4
#define EBC_B0AP       0x10
#define EBC_B1AP       0x11
#define EBC_B2AP       0x12
#define EBC_B4AP       0x14
#define EBC_CFG        0x23

#define EBC_AP_BME     0x80000000
#define EBC_AP_TWT(p)  (((p) << 23) & 0x7f800000)
#define EBC_AP_CSN(p)  (((p) << 18) & 0x000c0000)
#define EBC_AP_OEN(p)  (((p) << 16) & 0x00030000)
#define EBC_AP_WBN(p)  (((p) << 14) & 0x0000c000)
#define EBC_AP_WBF(p)  (((p) << 12) & 0x00003000)
#define EBC_AP_TH(p)   (((p) <<  9) & 0x00000e00)

#define EBC_AP_RE      0x00000100
#define EBC_AP_SOR     0x00000080
#define EBC_AP_BEM     0x00000040
#define EBC_AP_PEN     0x00000020

/* core config options */
#define EBC_CFG_EBTC    0x80000000
#define EBC_CFG_PTD     0x40000000
#define EBC_CFG_RTC(p)  (((p) << 27) & (0x38000000))
#define EBC_CFG_EMPH(p) (((p) << 25) & (0x06000000))
#define EBC_CFG_EMPL(p) (((p) << 23) & (0x01800000))
#define EBC_CFG_CSTC    0x00400000
#define EBC_CFG_BPF(p)  (((p) << 20) & (0x00300000))
#define EBC_CFG_EMS(p)  (((p) << 18) & (0x000c0000))
#define EBC_CFG_PME     0x00020000
#define EBC_CFG_PMT(p)  (((p) << 14) & (0x0001d000))


#define GPIO0_OSRL      0x0b08
#define GPIO0_OSRH      0x0b0c
#define GPIO0_TSRL      0x0b10
#define GPIO0_TSRH      0x0b14

#endif
