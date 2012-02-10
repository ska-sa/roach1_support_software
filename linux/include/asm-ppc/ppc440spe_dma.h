/*
 * include/asm/ppc440spe_dma.h
 *
 * 440SPe's DMA engines support header file
 *
 * 2006 (c) DENX Software Engineering
 *
 * Author: Yuri Tikhonov <yur@emcraft.com>
 *
 * This file is licensed under the term of  the GNU General Public License
 * version 2. The program licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#ifndef	PPC440SPE_DMA_H
#define PPC440SPE_DMA_H

#include <asm/types.h>

/* Number of elements in the array with statical CDBs */
#define	MAX_STAT_DMA_CDBS	16
/* Number of DMA engines available on the contoller */
#define DMA_ENGINES_NUM		2

/* FIFO's params */
#define DMA0_FIFO_SIZE		0x1000
#define DMA1_FIFO_SIZE		0x1000

/* DMA Opcodes */
#define	DMA_NOP_OPC		(u8)(0x00)
#define DMA_MOVE_SG1_SF2_OPC	(u8)(0x01)
#define DMA_MULTICAST_OPC	(u8)(0x05)

/* I2O Memory Mapped Registers base address */
#define I2O_MMAP_BASE		0x400100000ULL
#define I2O_MMAP_SIZE		0xF4ULL

/* DMA Memory Mapped Registers base address */
#define DMA0_MMAP_BASE		0x400100100ULL
#define DMA1_MMAP_BASE		0x400100200ULL
#define DMA_MMAP_SIZE		0x80

/* DMA Interrupt Sources, UIC0[20],[22] */
#define DMA0_CP_FIFO_NEED_SERVICE	19
#define DMA0_CS_FIFO_NEED_SERVICE	20
#define DMA1_CP_FIFO_NEED_SERVICE	21
#define DMA1_CS_FIFO_NEED_SERVICE	22

/*UIC0:*/
#define D0CPF_INT		(1<<12)
#define D0CSF_INT		(1<<11)
#define D1CPF_INT		(1<<10)
#define D1CSF_INT		(1<<9)
/*UIC1:*/
#define DMAE_INT		(1<<9)


/*
 * DMAx engines Command Descriptor Block Type
 */
typedef struct dma_cdb {
	/*
	 * Basic CDB structure (Table 20-17, p.499, 440spe_um_1_22.pdf) 
	 */
	u32	opc;		/* opcode */
#if 0
	u8	pad0[2];        /* reserved */
	u8	attr;		/* attributes */
	u8	opc;		/* opcode */
#endif
	u32	sg1u;		/* upper SG1 address */
	u32	sg1l;		/* lower SG1 address */
	u32	cnt;		/* SG count, 3B used */
	u32	sg2u;		/* upper SG2 address */
	u32	sg2l;		/* lower SG2 address */
	u32	sg3u;		/* upper SG3 address */
	u32	sg3l;		/* lower SG3 address */
} dma_cdb_t;

/*
 * Descriptor of allocated CDB
 */
typedef struct {
	dma_cdb_t		*vaddr;	/* virtual address of CDB */
	dma_addr_t		paddr;	/* physical address of CDB */
	/*
	 * Additional fields
	 */
	struct list_head 	link;	/* link in processing list */
	u32			status;	/* status of the CDB */
	/* status bits:  */
	#define	DMA_CDB_DONE	(1<<0)	/* CDB processing competed */
	#define DMA_CDB_CANCEL	(1<<1)	/* waiting thread was interrupted */
#if 0
	#define DMA_CDB_STALLOC (1<<2)  /* CDB allocated dynamically */

	/*
	 *  Each CDB must be 16B-alligned, if we use static array we should
	 * take care of aligment for each array's element.
	 */
	u8	pad1[1];
#endif
} dma_cdbd_t;

/*
 * DMAx hardware registers (p.515 in 440SPe UM 1.22)
 */
typedef struct {
	u32	cpfpl;
	u32	cpfph;
	u32	csfpl;
	u32	csfph;
	u32	dsts;
	u32	cfg;
	u8	pad0[0x8];
	u16	cpfhp;
	u16	cpftp;
	u16	csfhp;
	u16	csftp;
	u8	pad1[0x8];
	u32	acpl;
	u32	acph;
	u32	s1bpl;
	u32	s1bph;
	u32	s2bpl;
	u32	s2bph;
	u32	s3bpl;
	u32	s3bph;
	u8	pad2[0x10];
	u32	earl;
	u32	earh;
	u8	pad3[0x8];
	u32	seat;
	u32	sead;
	u32	op;
	u32	fsiz;
} dma_regs_t;

/*
 * I2O hardware registers (p.528 in 440SPe UM 1.22)
 */
typedef struct {
	u32	ists;
	u32	iseat;
	u32	isead;
	u8	pad0[0x14];
	u32	idbel;
	u8	pad1[0xc];
	u32	ihis;
	u32	ihim;
	u8	pad2[0x8];
	u32	ihiq;
	u32	ihoq;
	u8	pad3[0x8];
	u32	iopis;
	u32	iopim;
	u32	iopiq;
	u8	iopoq;
	u8	pad4[3];
	u16	iiflh;
	u16	iiflt;
	u16	iiplh;
	u16	iiplt;
	u16	ioflh;
	u16	ioflt;
	u16	ioplh;
	u16	ioplt;
	u32	iidc;
	u32	ictl;
	u32	ifcpp;
	u8	pad5[0x4];
	u16	mfac0;
	u16	mfac1;
	u16	mfac2;
	u16	mfac3;
	u16	mfac4;
	u16	mfac5;
	u16	mfac6;
	u16	mfac7;
	u16	ifcfh;
	u16	ifcht;
	u8	pad6[0x4];
	u32	iifmc;
	u32	iodb;
	u32	iodbc;
	u32	ifbal;
	u32	ifbah;
	u32	ifsiz;
	u32	ispd0;
	u32	ispd1;
	u32	ispd2;
	u32	ispd3;
	u32	ihipl;
	u32	ihiph;
	u32	ihopl;
	u32	ihoph;
	u32	iiipl;
	u32	iiiph;
	u32	iiopl;
	u32	iioph;
	u32	ifcpl;
	u32	ifcph;
	u8	pad7[0x8];
	u32	iopt;
} i2o_regs_t;

/*
 *  Prototypes
 */
int dma_copy (char *dst,char *src, unsigned int data_sz);


#endif /* PPC440SPE_DMA_H */

