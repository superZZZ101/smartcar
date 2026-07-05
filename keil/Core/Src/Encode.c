
#include "Encode.h"
volatile int16_t encode_L = 0;
volatile int16_t encode_R = 0;
void encode_getcount(void) {
    // 左编码器处理
    if (DL_GPIO_getEnabledInterruptStatus(encode_PORT, encode_LEFT_A_PIN)) {
        if (DL_GPIO_readPins(encode_PORT, encode_LEFT_B_PIN))
            encode_L++;
        else
            encode_L--;
        DL_GPIO_clearInterruptStatus(encode_PORT, encode_LEFT_A_PIN);
    }
    
    // 右编码器处理
    if (DL_GPIO_getEnabledInterruptStatus(encode_PORT, encode_RIGHT_A_PIN)) {
        if (DL_GPIO_readPins(encode_PORT, encode_RIGHT_B_PIN))
            encode_R--;
        else
            encode_R++;
        DL_GPIO_clearInterruptStatus(encode_PORT, encode_RIGHT_A_PIN);
    }
}
//int16_t Encode_GetspeedL(){
//    return encode_L;
//}
//int16_t Encode_GetspeedR(){
//    return encode_R;
//}

int16_t Encode_GetspeedL(){
	int16_t temp=encode_L;
  temp = temp * 3.769f;  
	encode_L=0;
	return temp;
}
int16_t Encode_GetspeedR(){
  int16_t temp=encode_R;
  temp = temp * 3.769f;  
	encode_R=0;
	return temp;
}