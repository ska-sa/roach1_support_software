#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netdb.h>

#include <sys/time.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <katcp.h>
#include <katpriv.h> /* for timeval arith */

#include "core.h"
#include "modes.h"
#include "misc.h"
#include "holo.h"
#include "holo-registers.h"
#include "holo-config.h"
#include "katadc.h"

int kat_adc_iic_status(struct katcp_dispatch *d, uint8_t adc){
	uint32_t data;
	char name[9];
	snprintf(name, 9, "iic_adc%d",adc); 
	if(read_name_pce(d, name, &data, IIC_STATUS_REG, 4) != 4){
		return -1;
	}
	return data;
}

/* katadc reset write */
int kat_adc_iic_reset(struct katcp_dispatch *d, uint8_t slot)
{
	uint32_t adc_value;
	char adc_buf[9];

	adc_value = 0x1;

	snprintf(adc_buf, BUFFER_SIZE, "iic_adc%d", slot);
	if(write_name_pce(d, adc_buf, &adc_value, IIC_STATUS_REG, 4) != 4){
		return -1;
	}
return 0;
}

int kat_adc_set_iic_reg(struct katcp_dispatch *d, uint8_t adc, unsigned char dev_addr, unsigned char reg_addr, unsigned char val)
{
	uint8_t data[4];
	uint32_t data_value;
	char name[9];
	snprintf(name, 9, "iic_adc%d", adc); 
	/* block operation fifo */
	data[0] = 0x00;
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = 0x1;

	data_value = (data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]);
	if(write_name_pce(d, name, &data_value, IIC_CTRL_REG, 4) != 4){
		return -1;
	}

	data[0] = 0x00;
	data[1] = 0x00;
	data[2] = IIC_START | IIC_WRITE;
	data[3] = IIC_CMD(dev_addr, IIC_CMD_WR);

	data_value = (data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]);
	if(write_name_pce(d, name, &data_value, IIC_OPFIFO_REG, 4) != 4){
		return -1;
	}

	data[0] = 0x00;
	data[1] = 0x00;
	data[2] = IIC_WRITE;
	data[3] = reg_addr;

	data_value = (data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]);
	if(write_name_pce(d, name, &data_value, IIC_OPFIFO_REG, 4) != 4){
		return -1;
	}

	data[0] = 0x00;
	data[1] = 0x00;
	data[2] = IIC_STOP | IIC_WRITE;
	data[3] = val;

	data_value = (data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]);
	if(write_name_pce(d, name, &data_value, IIC_OPFIFO_REG, 4) != 4){
		return -1;
	}

	/* unblock operation fifo */
	data[0] = 0x00;
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = 0x0;
	data_value = (data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]);
	if(write_name_pce(d, name, &data_value, IIC_CTRL_REG, 4) != 4){
		return -1;
	}

        usleep(20000); /* wait for the iic transaction to complete */

        if (kat_adc_iic_status(d, adc) & 0x100){
          fprintf(stderr, "error: no ack on iic read reg\n");
          kat_adc_iic_reset(d, adc);
          return 1;
        }

	return 0;
}
int kat_adc_set_reg(struct katcp_dispatch *d, unsigned char slot, unsigned short data, unsigned char addr)
{
	uint8_t buf[4];
	uint32_t buf_value;
	buf[0] = (data & 0xff00) >> 8;
	buf[1] = (data & 0x00ff) >> 0;
	buf[2] = addr;
	buf[3] = 0x1;
	buf_value = (buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3]);
	if(write_name_pce(d, "kat_adc_controller", &buf_value, 4 + (slot ? 4 : 0), 4) != 4){
		return -1;
	}
	return 0;
}

struct adc_reg{
	unsigned char addr;
	unsigned short data;
};

#define NUM_REG 9
struct adc_reg adc_reg_init [NUM_REG] = {
	{.addr =  0x0, .data = 0x7FFF},
	{.addr =  0x1, .data = 0xB2FF},
	{.addr =  0x2, .data = 0x007F},
	{.addr =  0x3, .data = 0x807F},
	{.addr =  0x9, .data = 0x03FF},
	{.addr =  0xa, .data = 0x007F},
	{.addr =  0xb, .data = 0x807F},
	{.addr =  0xe, .data = 0x00FF},
	{.addr =  0xf, .data = 0x007F}
};

int kat_config_adc(struct katcp_dispatch *d, uint8_t slot, int interleaved)
{
	uint8_t buf[4];
	uint32_t buf_value;
	int i;

	if (kat_adc_set_iic_reg(d, slot, GPIOI_A, GPIO_REG_OEN, 0x0)){
		fprintf(stderr, "error: GPIOI output-enabde configuration failed\n");
	}

	if (kat_adc_set_iic_reg(d, slot, GPIOQ_A, GPIO_REG_OEN, 0x0)){
		fprintf(stderr, "error: GPIOQ output-enabde configuration failed\n");
	}

	if (kat_adc_set_iic_reg(d, slot, GPIOI_A, GPIO_REG_OUT, GPIO_SW_DISABLE | GPIO_LATCH | GPIO_GAIN_31_5DB)){
		fprintf(stderr, "error: GPIOI output configuration failed\n");
	}

	if (kat_adc_set_iic_reg(d, slot, GPIOQ_A, GPIO_REG_OUT, GPIO_SW_DISABLE | GPIO_LATCH | GPIO_GAIN_31_5DB)){
		fprintf(stderr, "error: GPIOQ output configuration failed\n");
	}

	for (i = 0; i < NUM_REG; i++){
		kat_adc_set_reg(d, slot, adc_reg_init[i].data, adc_reg_init[i].addr);
		usleep(1000);
	}

	/* TODO: interleaved */

	/* Initiate ADC Resets */
	buf[0] = 0x0;
	buf[1] = 0x0;
	buf[2] = 0x0;
	buf[3] = 0x3;
	buf_value = (buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3]);
	if(write_name_pce(d, "kat_adc_controller", &buf_value, 0x0, 4) != 4){
		return -1;
	}
	return 0;
}
int kat_adc_get_iic_reg(struct katcp_dispatch *d, uint8_t adc, unsigned char dev_addr, unsigned char reg_addr, unsigned char* val)
{
  uint8_t data[4];
  uint32_t data_value;
  uint32_t read_value;
  char name[9];
  snprintf(&name[0], 9, "iic_adc%d", adc);
  /* careful - intel integers little endian */
  /* Block the operation fifo to compensate for extra latency. */
  data[0] = 0x0;
  data[1] = 0x0;
  data[2] = 0x0;
  data[3] = 0x1;

  data_value = (data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]);
  if(write_name_pce(d, name, &data_value, IIC_CTRL_REG, 4) != 4){
    return -1;
  }

  /* Issue a IIC write to load the register address */
  data[0] = 0x00;
  data[1] = 0x00;
  data[2] = IIC_START | IIC_WRITE;
  data[3] = IIC_CMD(dev_addr, IIC_CMD_WR);
  data_value = (data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]);
  if(write_name_pce(d, name, &data_value, IIC_OPFIFO_REG, 4) != 4){
    return -1;
  }

  data[0] = 0x00;
  data[1] = 0x00;
  data[2] = IIC_WRITE;
  data[3] = reg_addr;
  data_value = (data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]);
  if(write_name_pce(d, name, &data_value, IIC_OPFIFO_REG, 4) != 4){
    return -1;
  }

  /* Issue a repeated start to read the data IIC operation */
  data[0] = 0x00;
  data[1] = 0x00;
  data[2] = IIC_START | IIC_WRITE;
  data[3] = IIC_CMD(dev_addr, IIC_CMD_RD);
  data_value = (data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]);
  if(write_name_pce(d, name, &data_value, IIC_OPFIFO_REG, 4) != 4){
    return -1;
  }

  data[0] = 0x00;
  data[1] = 0x00;
  data[2] = IIC_STOP | IIC_READ;
  data[3] = 0x0;
  data_value = (data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]);
  if(write_name_pce(d, name, &data_value, IIC_OPFIFO_REG, 4) != 4){
    return -1;
  }

  /* unblock operation fifo */
  data[0] = 0x00;
  data[1] = 0x00;
  data[2] = 0x00;
  data[3] = 0x0;
  data_value = (data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]);
  if(write_name_pce(d, name, &data_value, IIC_CTRL_REG, 4) != 4){
    return -1;
  }

  usleep(20000); /* wait for the iic transaction to complete */

  if(read_name_pce(d, name, &read_value, IIC_RDFIFO_REG, 4) != 4){
    fprintf(stderr, "kat_adc_get_iic_reg: unable to read %s\n", name);
    return -1;
  }
  
  (*val) = (read_value & 0x000000FF);

  if (kat_adc_iic_status(d, adc) & 0x100){
    fprintf(stderr, "error: no ack on iic read reg\n");
    kat_adc_iic_reset(d, adc);
    return 1;
  }

  return 0;
}

#ifdef KATADC
int katadc_get_core_temp(struct katp_dispatch *d, uint8_t slot, uint8_t *temp_c){
  if( kat_adc_iic_reset(d, slot) < 0){
    fprintf(stderr,"katadc_init: error reseting adc in slot %d\n",slot);
    return -1;
  }

  if (kat_adc_get_iic_reg(d, slot, TMP421_A, TMP421_REG_ADC, temp_c)){
    fprintf(stderr, "katadc_get_core_temp: temperature read failed\n");
    return -1;
  }
  return 0;
}

int katadc_get_amb_temp(struct katcp_dispatch *d, uint8_t slot, uint8_t *temp_c){
  if( kat_adc_iic_reset(d, slot) < 0){
    fprintf(stderr,"katadc_init: error reseting adc in slot %d\n",slot);
    return -1;
  }

  if (kat_adc_get_iic_reg(d, slot, TMP421_A, TMP421_REG_AMB, temp_c)){
    fprintf(stderr, "katadc_get_amb_temp: temperature read failed\n");
    return -1;
  }
  return 0;
}
#endif

int katadc_init(struct katcp_dispatch *d, uint8_t slot)
{
#ifdef KATADC
  uint8_t temp;
  /* TODO: Add as sensor LATER*/
  katadc_get_amb_temp(d, slot, &temp);
  fprintf(stderr, "katadc_get_amb_temp: %d\n", temp);
  temp = 0;
  katadc_get_core_temp(d, slot, &temp);
  fprintf(stderr, "katadc_core_temp: %d\n", temp);
#endif
  kat_adc_iic_reset(d, slot);
  return kat_config_adc(d, slot, 0);
}

