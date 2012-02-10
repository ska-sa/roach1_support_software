#ifndef PPC440_EPX_DMA3_H_
#define PPC440_EPX_DMA3_H_

#define DMA2P30_CR0 0x0100 /* DMA to PLB 3 Channel Control Register 0 */
#define DMA2P30_CT0 0x0101 /* DMA to PLB 3 Count Register 0 */
#define DMA2P30_DA0 0x0102 /* DMA to PLB 3 Destination Address Register 0 */
#define DMA2P30_SA0 0x0103 /* DMA to PLB 3 Source Address Register 0 */
#define DMA2P30_SG0 0x0104 /* DMA to PLB 3 Scatter/Gather Descriptor Address Register 0 */
#define DMA2P30_SC0 0x0107 /* DMA to PLB3 Subchannel ID for Register 0 */
#define DMA2P30_CR1 0x0108 /* DMA to PLB 3 Channel Control Register 1 */
#define DMA2P30_CT1 0x0109 /* DMA to PLB 3 Count Register 1 */
#define DMA2P30_DA1 0x010A /* DMA to PLB 3 Destination Address Register 1 */
#define DMA2P30_SA1 0x010B /* DMA to PLB 3 Source Address Register 1 */
#define DMA2P30_SG1 0x010C /* DMA to PLB 3 Scatter/Gather Descriptor Address Register 1 */
#define DMA2P30_SC1 0x010F /* DMA to PLB3 Subchannel ID for Register 1 */
#define DMA2P30_CR2 0x0110 /* DMA to PLB 3 Channel Control Register 2 */
#define DMA2P30_CT2 0x0111 /* DMA to PLB 3 Count Register 2 */
#define DMA2P30_DA2 0x0112 /* DMA to PLB 3 Destination Address Register 2 */
#define DMA2P30_SA2 0x0113 /* DMA to PLB 3 Source Address Register 2 */
#define DMA2P30_SG2 0x0114 /* DMA to PLB 3 Scatter/Gather Descriptor Address Register 2 */
#define DMA2P30_SC2 0x0117 /* DMA to PLB3 Subchannel ID for Register 2 */
#define DMA2P30_CR3 0x0118 /* DMA to PLB 3 Channel Control Register 3 */
#define DMA2P30_CT3 0x0119 /* DMA to PLB 3 Count Register 3 */
#define DMA2P30_DA3 0x011A /* DMA to PLB 3 Destination Address Register 3 */
#define DMA2P30_SA3 0x011B /* DMA to PLB 3 Source Address Register 3 */
#define DMA2P30_SG3 0x011C /* DMA to PLB 3 Scatter/Gather Descriptor Address Register 3 */
#define DMA2P30_SC3 0x011F /* DMA to PLB3 Subchannel ID for Register 3 */
#define DMA2P30_SR  0x0120 /* DMA to PLB 3 Status Register */
#define DMA2P30_SGC 0x0123 /* DMA to PLB 3 Scatter/Gather Command Register */
#define DMA2P30_ADR 0x0124 /* DMA Address Decode Register */
#define DMA2P30_SLP 0x0125 /* DMA to PLB 3 Sleep Mode Register */
#define DMA2P30_POL 0x0126 /* DMA to PLB 3 Polarity Configuration Register */

#define DMA2P3_CE     (1 << 31)
#define DMA2P3_CIE    (1 << 30)
#define DMA2P3_PW_W   (2 << 26)
#define DMA2P3_DIA    (1 << 25)
#define DMA2P3_SIA    (1 << 24)
#define DMA2P3_BEN    (1 << 23)
#define DMA2P3_TM_SMM (2 << 21)
#define DMA2P3_ETD    (1 << 9)
#define DMA2P3_TCE    (1 << 8)

#define DMA_READ(a)  ({mfdcr((a));})
#define DMA_WRITE(a, d) ({mtdcr((a), (d));})

#endif
