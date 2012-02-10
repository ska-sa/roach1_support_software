#ifndef ROACHBSP_H_
#define ROACHBSP_H_

#define ROACHBSP_ID        0xdead

/* Module Offsets */
#define SYS_OFFSET         0x0

#define DRAMC_OFFSET       0x00050000
#define DRAM_OFFSET        0x04000000

#define QDRC1_OFFSET       0x00040000
#define QDR1_OFFSET        0x03000000

#define QDRC0_OFFSET       0x00030000
#define QDR0_OFFSET        0x02000000

#define IADC0_OFFSET       0x00010000
#define IADC1_OFFSET       0x00020000

#define TGE0_OFFSET        0x00380000
#define TGE1_OFFSET        0x00300000
#define TGE2_OFFSET        0x00280000
#define TGE3_OFFSET        0x00200000

/* System Block Registers */

#define RBSP_SYS_ID        0x0
#define RBSP_SYS_MAJOR     0x2
#define RBSP_SYS_MINOR     0x4
#define RBSP_SYS_RCS       0x6
#define RBSP_SYS_UPTODATE  0x8
#define RBSP_SYS_SCRATCH   0xa

/* DRAM Config Registers */

#define RBSP_DRAM_PHYSTATUS  0x0
#define RBSP_DRAM_RESET      0x2
#define RBSP_DRAM_FREQ       0x4

/* DRAM Indirect Memory Access Registers */

#define RBSP_DRAM_WREN      0x0
#define RBSP_DRAM_RDEN      0x2
#define RBSP_DRAM_ADDR      0x4
#define RBSP_DRAM_MASK1     0x8
#define RBSP_DRAM_MASK0     0xc
#define RBSP_DRAM_WRDATA(x) (0x10 + 2*(x))
#define RBSP_DRAM_RDDATA(x) (0x34 + 2*(x))

#define RBSP_DRAM_PHYRDY     0x0001
#define RBSP_DRAM_CALFAIL    0x0010

/* QDR Config Registers */

#define RBSP_QDR_PHYSTATUS  0x0
#define RBSP_QDR_RESET      0x2
#define RBSP_QDR_FREQ       0x4

/* QDR Indirect Memory Access Registers */
#define RBSP_QDR_WREN       0x0
#define RBSP_QDR_RDEN       0x2
#define RBSP_QDR_ADDR       0x4
#define RBSP_QDR_MASK       0x8
#define RBSP_QDR_WRDATA(x)  (0xa  + 4*(x) + 2)
#define RBSP_QDR_RDDATA(x)  (0x1a + 4*(x) + 2)

#define RBSP_QDR_PHYRDY     0x0001
#define RBSP_QDR_CALFAIL    0x0010

/* IADC Interface Registers */

#define RBSP_IADC_RESET       0x0 
#define RBSP_IADC_MODE        0x2 
#define RBSP_IADC_TWI_ADDR    0x4 
#define RBSP_IADC_TWI_DATA    0x6 
#define RBSP_IADC_TWI_TX      0x8
#define RBSP_IADC_FIFODATA(x) (0xa + 2*(x))
#define RBSP_IADC_FIFO_ADV    0x14
#define RBSP_IADC_FIFO_STATUS 0x16 
#define RBSP_IADC_FIFO_CTRL   0x18 
#define RBSP_IADC_CLKTEST1    0x1a 
#define RBSP_IADC_CLKTEST0    0x1c 

/* TGE Registers */
#define RBSP_XAUI_LINKSTATUS  0x18

#endif

