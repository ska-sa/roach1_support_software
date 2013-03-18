#define BUFFER_SIZE 9

#define IIC_START  0x2
#define IIC_STOP   0x4
#define IIC_READ   0x1
#define IIC_WRITE  0x0

#define IIC_CMD(a,rnw) ((((a) & 0x7f) << 1) | ((rnw) & 0x1))
#define IIC_CMD_RD 0x1
#define IIC_CMD_WR 0x0

#define TMP421_A       0x4c
#define TMP421_REG_AMB 0x0
#define TMP421_REG_ADC 0x1

#define GPIOI_A 0x20
#define GPIOQ_A 0x21
#define GPIO_REG_OEN 0x6
#define GPIO_REG_OUT 0x2

#define GPIO_SW_DISABLE 0x80
#define GPIO_SW_ENABLE  0x00
#define GPIO_LATCH      0x40
#define GPIO_GAIN_0DB   0x3f
#define GPIO_GAIN_31_5DB 0x00

#define IIC_OPFIFO_REG 0x0
#define IIC_RDFIFO_REG 0x4
#define IIC_STATUS_REG 0x8
#define IIC_CTRL_REG   0x12

#define MAX_ATTENUATION 31
#define MIN_ATTENUATION 0
#define ATTENUATION_STEP 1

int kat_adc_iic_status(struct katcp_dispatch *d, uint8_t adc);
int kat_adc_iic_reset(struct katcp_dispatch *d, uint8_t slot);
int kat_adc_set_iic_reg(struct katcp_dispatch *d, uint8_t adc, unsigned char dev_addr, unsigned char reg_addr, unsigned char val);
int kat_config_adc(struct katcp_dispatch *d, uint8_t slot, int interleaved);
int kat_adc_get_iic_reg(struct katcp_dispatch *d, uint8_t adc, unsigned char dev_addr, unsigned char reg_addr, unsigned char* val);
