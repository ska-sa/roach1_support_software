#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netdb.h>

#include <sys/time.h>

#include <katcp.h>
#include <katpriv.h> /* for timeval arith */

#include "core.h"
#include "modes.h"
#include "poco.h"
#include "misc.h"

#define BUFFER_SIZE 16 

int poco_gain_cmd(struct katcp_dispatch *d, int argc)
{
	struct state_poco *sp;
	unsigned int count, i,m,n;
	char *label, *value, *end;
	char ram_buf[16];
	int inp, pol, chn;
	int gain;
	int rr;
	uint32_t rd_val;
	uint32_t wr_val;

	sp = need_current_mode_katcp(d, POCO_POCO_MODE);
	if(sp == NULL){
		return KATCP_RESULT_FAIL;
	}

	count = 0;

	for(i = 1; i < argc; i += 2){

		if(argc == 2){
			log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "ALL CASE:Gain value entered");
			value = arg_string_katcp(d, i);
			gain = strtoul(value, &end, 0);
			if(end[0] != '\0'){
				log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to parse gain value %s", value);
				extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
				return KATCP_RESULT_OWN;
			}
			for(m = 0; m < 4; m++){
				/*word-write:so advance position by 4*/
				for(n = 0; n < (sp->p_fft_window / 2); n += 1){
					snprintf(ram_buf, BUFFER_SIZE, "quant%d_ram", m);

					wr_val =  (gain << 16)| (gain & 0x0000FFFF);
					if(write_name_pce(d, ram_buf, &wr_val, n*4, 4) != 4){
						return -1;
					}
				}
			}
			log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "after write gain 0x%x to  all input, pol and  channels", gain);
			/*Hack:To match no of args when out of the loop*/
			i = i - 1;

		}
		else{

			label = arg_string_katcp(d, i);
			value = arg_string_katcp(d, i + 1);

			if((label == NULL) || (value == NULL)){
				log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to acquire parameters after %u entries", i);
				extra_response_katcp(d, KATCP_RESULT_FAIL, "internal");
				return KATCP_RESULT_OWN;
			}

			inp = extract_input_poco(label);
			pol = extract_polarisation_poco(label);
			chn = extract_channel_poco(label);

			gain = strtoul(value, &end, 0);

			if(end[0] != '\0'){
				log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to parse gain value %s", value);
				extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
				return KATCP_RESULT_OWN;
			}

			log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "should write gain 0x%x to input[%d], pol[%d] channel[%d]", gain, inp, pol, chn);

			/*********************CHECKS*******************/	

			/*Range checking*/
			if(gain <= (~(1 << 16 ) + 1) || gain >= (1 << 16)){
				log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "Out of 16-bit range");
			}

			/*********************MAIN LOGIC*******************/	

			if(pol == -1 && chn == -1){
				log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "No pol and chn values entered");

				/*Writing the equalisation and quantisation registers via reading the ram register, appending the gain value and writing it out*/
				/*For input,pol the values that needs to be appended to the addr and value register*/
				/*0x:0,0y:1,1x:2,1y:3 */
				if(inp)
					inp = 2;

				for(m = inp; m < (inp + 2); m++){
					/*word-write:so advance position by 4*/
					for(n = 0; n < (sp->p_fft_window / 2); n += 1){
						snprintf(ram_buf, BUFFER_SIZE, "quant%d_ram", m);

						wr_val =  (gain << 16)| (gain & 0x0000FFFF);
						if(write_name_pce(d, ram_buf, &wr_val, n*4, 4) != 4){
							return -1;
						}
					}
				}
				log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "after write gain 0x%x to input[%d], pol[%d] channel[%d]", gain, inp, pol, chn);


			}
			else if(pol >= 0 && chn == -1){
				log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "No channel value present only polarisation entered,chn = %d",chn);

				/*Logic determining the register to write based on input and polarisation*/
				if(!inp && !pol){
					inp = 0;
					log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "pol=0x ");
				}
				else if(!inp && pol){
					inp = 1;
					log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "pol=0y");
				}
				else if(inp && !pol){
					inp = 2;
					log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "pol=1x");
				}
				else{
					inp = 3;
					log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "pol=1y");
				}

				for(n = 0; n < (sp->p_fft_window / 2); n += 1){
					snprintf(ram_buf, BUFFER_SIZE, "quant%d_ram", inp);

					wr_val =  (gain << 16)| (gain & 0x0000FFFF);
					if(write_name_pce(d, ram_buf, &wr_val, n*4, 4) != 4){
						return -1;
					}
				}
			}
			else{
				log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "Pol and Chn values entered,chn = %d",chn);
				/*Channel sanity check*/
				if(chn < 0 || chn > 511){
					log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "Out of channel range");
				}	

				/*Logic determining the register to write based on input and polarisation*/
				if(!inp && !pol){
					inp = 0;
					log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "pol=0x ");
				}
				else if(!inp && pol){
					inp = 1;
					log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "pol=0y");
				}
				else if(inp && !pol){
					inp = 2;
					log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "pol=1x");
				}
				else{
					inp = 3;
					log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "pol=1y");
				}

				snprintf(ram_buf, BUFFER_SIZE, "quant%d_ram", inp);

				n = chn/2;
#if 0
				while(n  % 4 != 0){
					n = n - 1;	
					log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "chn:  %d", n);
				}
				log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "chn adjusted to %d", n);
#endif
				rr = read_name_pce(d, ram_buf, &rd_val, n*4 , 4);
				if(rr != 4){
					return -1;
				}

				if(chn % 2 == 0){
					wr_val = (rd_val & 0x0000FFFF) | (gain << 16);
				}
				else{
					wr_val = (rd_val & 0xFFFF0000) | (gain & 0x0000FFFF);
				}

				if(write_name_pce(d, ram_buf, &wr_val, n*4 , 4) != 4){
					return -1;
				}

			}
		}

		count++;
	}

	if((i != argc) || (i < 2)){
		log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "invalid number of parameters (need pairs)");
		extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
		return KATCP_RESULT_OWN;
	}

	log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "wrote %d gain entries", count);

	return KATCP_RESULT_OK;
}

