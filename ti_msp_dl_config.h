/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0G350X
#define CONFIG_MSPM0G3507

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)


#define CPUCLK_FREQ                                                     32000000



/* Defines for WHEEL */
#define WHEEL_INST                                                         TIMG7
#define WHEEL_INST_IRQHandler                                   TIMG7_IRQHandler
#define WHEEL_INST_INT_IRQN                                     (TIMG7_INT_IRQn)
#define WHEEL_INST_CLK_FREQ                                              4000000
/* GPIO defines for channel 0 */
#define GPIO_WHEEL_C0_PORT                                                 GPIOA
#define GPIO_WHEEL_C0_PIN                                         DL_GPIO_PIN_26
#define GPIO_WHEEL_C0_IOMUX                                      (IOMUX_PINCM59)
#define GPIO_WHEEL_C0_IOMUX_FUNC                     IOMUX_PINCM59_PF_TIMG7_CCP0
#define GPIO_WHEEL_C0_IDX                                    DL_TIMER_CC_0_INDEX
/* GPIO defines for channel 1 */
#define GPIO_WHEEL_C1_PORT                                                 GPIOA
#define GPIO_WHEEL_C1_PIN                                         DL_GPIO_PIN_24
#define GPIO_WHEEL_C1_IOMUX                                      (IOMUX_PINCM54)
#define GPIO_WHEEL_C1_IOMUX_FUNC                     IOMUX_PINCM54_PF_TIMG7_CCP1
#define GPIO_WHEEL_C1_IDX                                    DL_TIMER_CC_1_INDEX



/* Defines for UART0 */
#define UART0_INST                                                         UART0
#define UART0_INST_FREQUENCY                                             4000000
#define UART0_INST_IRQHandler                                   UART0_IRQHandler
#define UART0_INST_INT_IRQN                                       UART0_INT_IRQn
#define GPIO_UART0_RX_PORT                                                 GPIOA
#define GPIO_UART0_TX_PORT                                                 GPIOA
#define GPIO_UART0_RX_PIN                                         DL_GPIO_PIN_11
#define GPIO_UART0_TX_PIN                                         DL_GPIO_PIN_10
#define GPIO_UART0_IOMUX_RX                                      (IOMUX_PINCM22)
#define GPIO_UART0_IOMUX_TX                                      (IOMUX_PINCM21)
#define GPIO_UART0_IOMUX_RX_FUNC                       IOMUX_PINCM22_PF_UART0_RX
#define GPIO_UART0_IOMUX_TX_FUNC                       IOMUX_PINCM21_PF_UART0_TX
#define UART0_BAUD_RATE                                                 (115200)
#define UART0_IBRD_4_MHZ_115200_BAUD                                         (2)
#define UART0_FBRD_4_MHZ_115200_BAUD                                        (11)





/* Port definition for Pin Group LED */
#define LED_PORT                                                         (GPIOB)

/* Defines for LIGHT: GPIOB.22 with pinCMx 50 on package pin 21 */
#define LED_LIGHT_PIN                                           (DL_GPIO_PIN_22)
#define LED_LIGHT_IOMUX                                          (IOMUX_PINCM50)
/* Port definition for Pin Group OLED */
#define OLED_PORT                                                        (GPIOB)

/* Defines for SCL: GPIOB.2 with pinCMx 15 on package pin 50 */
#define OLED_SCL_PIN                                             (DL_GPIO_PIN_2)
#define OLED_SCL_IOMUX                                           (IOMUX_PINCM15)
/* Defines for SDA: GPIOB.3 with pinCMx 16 on package pin 51 */
#define OLED_SDA_PIN                                             (DL_GPIO_PIN_3)
#define OLED_SDA_IOMUX                                           (IOMUX_PINCM16)
/* Port definition for Pin Group encode */
#define encode_PORT                                                      (GPIOA)

/* Defines for LEFT_A: GPIOA.0 with pinCMx 1 on package pin 33 */
// pins affected by this interrupt request:["LEFT_A","RIGHT_A"]
#define encode_INT_IRQN                                         (GPIOA_INT_IRQn)
#define encode_INT_IIDX                         (DL_INTERRUPT_GROUP1_IIDX_GPIOA)
#define encode_LEFT_A_IIDX                                   (DL_GPIO_IIDX_DIO0)
#define encode_LEFT_A_PIN                                        (DL_GPIO_PIN_0)
#define encode_LEFT_A_IOMUX                                       (IOMUX_PINCM1)
/* Defines for LEFT_B: GPIOA.1 with pinCMx 2 on package pin 34 */
#define encode_LEFT_B_PIN                                        (DL_GPIO_PIN_1)
#define encode_LEFT_B_IOMUX                                       (IOMUX_PINCM2)
/* Defines for RIGHT_A: GPIOA.8 with pinCMx 19 on package pin 54 */
#define encode_RIGHT_A_IIDX                                  (DL_GPIO_IIDX_DIO8)
#define encode_RIGHT_A_PIN                                       (DL_GPIO_PIN_8)
#define encode_RIGHT_A_IOMUX                                     (IOMUX_PINCM19)
/* Defines for RIGHT_B: GPIOA.9 with pinCMx 20 on package pin 55 */
#define encode_RIGHT_B_PIN                                       (DL_GPIO_PIN_9)
#define encode_RIGHT_B_IOMUX                                     (IOMUX_PINCM20)
/* Port definition for Pin Group WHEEL_CTRL */
#define WHEEL_CTRL_PORT                                                  (GPIOA)

/* Defines for IN1_1: GPIOA.14 with pinCMx 36 on package pin 7 */
#define WHEEL_CTRL_IN1_1_PIN                                    (DL_GPIO_PIN_14)
#define WHEEL_CTRL_IN1_1_IOMUX                                   (IOMUX_PINCM36)
/* Defines for IN1_2: GPIOA.15 with pinCMx 37 on package pin 8 */
#define WHEEL_CTRL_IN1_2_PIN                                    (DL_GPIO_PIN_15)
#define WHEEL_CTRL_IN1_2_IOMUX                                   (IOMUX_PINCM37)
/* Defines for IN2_1: GPIOA.16 with pinCMx 38 on package pin 9 */
#define WHEEL_CTRL_IN2_1_PIN                                    (DL_GPIO_PIN_16)
#define WHEEL_CTRL_IN2_1_IOMUX                                   (IOMUX_PINCM38)
/* Defines for IN2_2: GPIOA.17 with pinCMx 39 on package pin 10 */
#define WHEEL_CTRL_IN2_2_PIN                                    (DL_GPIO_PIN_17)
#define WHEEL_CTRL_IN2_2_IOMUX                                   (IOMUX_PINCM39)
/* Defines for IR1: GPIOA.28 with pinCMx 3 on package pin 35 */
#define IR_SENSOR_IR1_PORT                                               (GPIOA)
#define IR_SENSOR_IR1_PIN                                       (DL_GPIO_PIN_28)
#define IR_SENSOR_IR1_IOMUX                                       (IOMUX_PINCM3)
/* Defines for IR2: GPIOA.31 with pinCMx 6 on package pin 39 */
#define IR_SENSOR_IR2_PORT                                               (GPIOA)
#define IR_SENSOR_IR2_PIN                                       (DL_GPIO_PIN_31)
#define IR_SENSOR_IR2_IOMUX                                       (IOMUX_PINCM6)
/* Defines for IR3: GPIOB.4 with pinCMx 17 on package pin 52 */
#define IR_SENSOR_IR3_PORT                                               (GPIOB)
#define IR_SENSOR_IR3_PIN                                        (DL_GPIO_PIN_4)
#define IR_SENSOR_IR3_IOMUX                                      (IOMUX_PINCM17)
/* Defines for IR4: GPIOB.5 with pinCMx 18 on package pin 53 */
#define IR_SENSOR_IR4_PORT                                               (GPIOB)
#define IR_SENSOR_IR4_PIN                                        (DL_GPIO_PIN_5)
#define IR_SENSOR_IR4_IOMUX                                      (IOMUX_PINCM18)
/* Defines for IR5: GPIOB.0 with pinCMx 12 on package pin 47 */
#define IR_SENSOR_IR5_PORT                                               (GPIOB)
#define IR_SENSOR_IR5_PIN                                        (DL_GPIO_PIN_0)
#define IR_SENSOR_IR5_IOMUX                                      (IOMUX_PINCM12)
/* Defines for IR6: GPIOB.11 with pinCMx 28 on package pin 63 */
#define IR_SENSOR_IR6_PORT                                               (GPIOB)
#define IR_SENSOR_IR6_PIN                                       (DL_GPIO_PIN_11)
#define IR_SENSOR_IR6_IOMUX                                      (IOMUX_PINCM28)
/* Defines for IR7: GPIOB.12 with pinCMx 29 on package pin 64 */
#define IR_SENSOR_IR7_PORT                                               (GPIOB)
#define IR_SENSOR_IR7_PIN                                       (DL_GPIO_PIN_12)
#define IR_SENSOR_IR7_IOMUX                                      (IOMUX_PINCM29)
/* Defines for IR8: GPIOB.13 with pinCMx 30 on package pin 1 */
#define IR_SENSOR_IR8_PORT                                               (GPIOB)
#define IR_SENSOR_IR8_PIN                                       (DL_GPIO_PIN_13)
#define IR_SENSOR_IR8_IOMUX                                      (IOMUX_PINCM30)
/* Port definition for Pin Group KEY */
#define KEY_PORT                                                         (GPIOB)

/* Defines for UP: GPIOB.8 with pinCMx 25 on package pin 60 */
#define KEY_UP_PIN                                               (DL_GPIO_PIN_8)
#define KEY_UP_IOMUX                                             (IOMUX_PINCM25)
/* Defines for DOWN: GPIOB.7 with pinCMx 24 on package pin 59 */
#define KEY_DOWN_PIN                                             (DL_GPIO_PIN_7)
#define KEY_DOWN_IOMUX                                           (IOMUX_PINCM24)
/* Defines for ENTER: GPIOB.6 with pinCMx 23 on package pin 58 */
#define KEY_ENTER_PIN                                            (DL_GPIO_PIN_6)
#define KEY_ENTER_IOMUX                                          (IOMUX_PINCM23)
/* Defines for cs: GPIOA.27 with pinCMx 60 on package pin 31 */
#define W25Q64_cs_PORT                                                   (GPIOA)
#define W25Q64_cs_PIN                                           (DL_GPIO_PIN_27)
#define W25Q64_cs_IOMUX                                          (IOMUX_PINCM60)
/* Defines for do: GPIOA.25 with pinCMx 55 on package pin 26 */
#define W25Q64_do_PORT                                                   (GPIOA)
#define W25Q64_do_PIN                                           (DL_GPIO_PIN_25)
#define W25Q64_do_IOMUX                                          (IOMUX_PINCM55)
/* Defines for clk: GPIOB.25 with pinCMx 56 on package pin 27 */
#define W25Q64_clk_PORT                                                  (GPIOB)
#define W25Q64_clk_PIN                                          (DL_GPIO_PIN_25)
#define W25Q64_clk_IOMUX                                         (IOMUX_PINCM56)
/* Defines for di: GPIOB.20 with pinCMx 48 on package pin 19 */
#define W25Q64_di_PORT                                                   (GPIOB)
#define W25Q64_di_PIN                                           (DL_GPIO_PIN_20)
#define W25Q64_di_IOMUX                                          (IOMUX_PINCM48)


/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_WHEEL_init(void);
void SYSCFG_DL_UART0_init(void);


bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
