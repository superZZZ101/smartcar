#include <stdint.h>
#include "ti_msp_dl_config.h" // 确保包含了系统时钟定义


void delay(uint16_t x)  {   
	do { delay_cycles(32000); 
	} while(x--);
}

