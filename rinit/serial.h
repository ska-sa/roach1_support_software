#ifndef SERIAL_H_
#define SERIAL_H_

#define SERIAL_TIMEOUT      0xfff

#define SERIAL_CTRL    0x20800001
#define SDR_SERIAL          0x120
#define SDR_ADDR              0xe
#define SDR_DATA              0xf

/*******************************************************************/

#define SERIAL_OUT     0x70000300
#define SERIAL_RBR              0
#define SERIAL_THR              0
#define SERIAL_IER              1
#define SERIAL_IIR              2
#define SERIAL_FCR              2
#define SERIAL_LCR              3
#define SERIAL_MCR              4
#define SERIAL_LSR              5
#define SERIAL_MSR              6
#define SERIAL_SCR              7
#define SERIAL_DLL              0
#define SERIAL_DLM              1

#endif
