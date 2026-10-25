// Test-only stand-in for the ST peripheral library header, used ONLY to compile the unmodified firmware
// files for the differential test (docs/architecture.md section 14, V1). It is never part of src/core or the plugin.
// It declares exactly the names that the five firmware files (and the headers they include) use. Peripheral
// functions are empty inline functions: the oracle never starts a timer or touches a pin.
#ifndef ORACLE_SHIM_STM32F4XX_H
#define ORACLE_SHIM_STM32F4XX_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define __IO volatile
#define __I  volatile const

typedef enum { RESET = 0, SET = 1 } FlagStatus, ITStatus;
typedef enum { DISABLE = 0, ENABLE = 1 } FunctionalState;

typedef struct
{
    __IO uint32_t MODER, OTYPER, OSPEEDR, PUPDR;
    __IO uint32_t IDR, ODR;
    __IO uint16_t BSRRL, BSRRH;
    __IO uint32_t LCKR, AFR[2];
} GPIO_TypeDef;

typedef struct
{
    __IO uint32_t CR1, CR2, SMCR, DIER, SR, EGR, CCMR1, CCMR2, CCER, CNT, PSC, ARR, RCR;
    __IO uint32_t CCR1, CCR2, CCR3, CCR4;
    __IO uint32_t BDTR, DCR, DMAR, OR;
} TIM_TypeDef;

extern GPIO_TypeDef oracle_gpio[7];
extern TIM_TypeDef  oracle_tim[4];
#define GPIOA (&oracle_gpio[0])
#define GPIOB (&oracle_gpio[1])
#define GPIOC (&oracle_gpio[2])
#define GPIOD (&oracle_gpio[3])
#define GPIOE (&oracle_gpio[4])
#define GPIOF (&oracle_gpio[5])
#define GPIOG (&oracle_gpio[6])
#define TIM1  (&oracle_tim[0])
#define TIM3  (&oracle_tim[1])
#define TIM9  (&oracle_tim[2])
#define TIM10 (&oracle_tim[3])

#define GPIO_Pin_0  ((uint16_t) 0x0001)
#define GPIO_Pin_1  ((uint16_t) 0x0002)
#define GPIO_Pin_2  ((uint16_t) 0x0004)
#define GPIO_Pin_3  ((uint16_t) 0x0008)
#define GPIO_Pin_4  ((uint16_t) 0x0010)
#define GPIO_Pin_5  ((uint16_t) 0x0020)
#define GPIO_Pin_6  ((uint16_t) 0x0040)
#define GPIO_Pin_7  ((uint16_t) 0x0080)
#define GPIO_Pin_8  ((uint16_t) 0x0100)
#define GPIO_Pin_9  ((uint16_t) 0x0200)
#define GPIO_Pin_10 ((uint16_t) 0x0400)
#define GPIO_Pin_11 ((uint16_t) 0x0800)
#define GPIO_Pin_12 ((uint16_t) 0x1000)
#define GPIO_Pin_13 ((uint16_t) 0x2000)
#define GPIO_Pin_14 ((uint16_t) 0x4000)
#define GPIO_Pin_15 ((uint16_t) 0x8000)

#define GPIO_PinSource4  ((uint8_t) 0x04)
#define GPIO_PinSource5  ((uint8_t) 0x05)
#define GPIO_PinSource8  ((uint8_t) 0x08)
#define GPIO_PinSource9  ((uint8_t) 0x09)
#define GPIO_PinSource10 ((uint8_t) 0x0A)

#define GPIO_AF_TIM1 ((uint8_t) 0x01)
#define GPIO_AF_TIM3 ((uint8_t) 0x02)

#define RCC_AHB1Periph_GPIOA ((uint32_t) 0x00000001)
#define RCC_AHB1Periph_GPIOB ((uint32_t) 0x00000002)
#define RCC_AHB1Periph_GPIOC ((uint32_t) 0x00000004)
#define RCC_AHB1Periph_GPIOD ((uint32_t) 0x00000008)
#define RCC_AHB1Periph_GPIOE ((uint32_t) 0x00000010)
#define RCC_AHB1Periph_GPIOF ((uint32_t) 0x00000020)
#define RCC_AHB1Periph_GPIOG ((uint32_t) 0x00000040)
#define RCC_APB1Periph_TIM3  ((uint32_t) 0x00000002)
#define RCC_APB2Periph_TIM1  ((uint32_t) 0x00000001)
#define RCC_APB2Periph_TIM9  ((uint32_t) 0x00010000)
#define RCC_APB2Periph_TIM10 ((uint32_t) 0x00020000)

#define TIM1_BRK_TIM9_IRQn 24
#define TIM1_UP_TIM10_IRQn 25

typedef struct
{
    uint32_t GPIO_Pin;
    uint32_t GPIO_Mode;
    uint32_t GPIO_Speed;
    uint32_t GPIO_OType;
    uint32_t GPIO_PuPd;
} GPIO_InitTypeDef;
#define GPIO_Mode_AF     2
#define GPIO_Speed_2MHz  0
#define GPIO_OType_PP    0
#define GPIO_PuPd_UP     1

typedef struct
{
    uint8_t NVIC_IRQChannel;
    uint8_t NVIC_IRQChannelPreemptionPriority;
    uint8_t NVIC_IRQChannelSubPriority;
    FunctionalState NVIC_IRQChannelCmd;
} NVIC_InitTypeDef;

typedef struct
{
    uint16_t TIM_Prescaler;
    uint16_t TIM_CounterMode;
    uint32_t TIM_Period;
    uint16_t TIM_ClockDivision;
    uint8_t  TIM_RepetitionCounter;
} TIM_TimeBaseInitTypeDef;
#define TIM_CounterMode_Up 0

typedef struct
{
    uint16_t TIM_OCMode;
    uint16_t TIM_OutputState;
    uint16_t TIM_OutputNState;
    uint32_t TIM_Pulse;
    uint16_t TIM_OCPolarity;
    uint16_t TIM_OCNPolarity;
    uint16_t TIM_OCIdleState;
    uint16_t TIM_OCNIdleState;
} TIM_OCInitTypeDef;
#define TIM_OCMode_PWM1          0x60
#define TIM_OutputState_Enable   1
#define TIM_OutputNState_Disable 0
#define TIM_OCPolarity_High      0
#define TIM_OCNPolarity_High     0
#define TIM_OCIdleState_Set      0x100
#define TIM_OCNIdleState_Reset   0
#define TIM_OCPreload_Enable     8
#define TIM_TRGOSource_Update    0x20
#define TIM_IT_Update            1

#define ORACLE_NOP_FN(ret, name, args) static inline ret name args { }

static inline void NVIC_Init (NVIC_InitTypeDef* x) { (void) x; }
static inline void NVIC_SetVectorTable (uint32_t a, uint32_t b) { (void) a; (void) b; }
static inline void RCC_APB1PeriphClockCmd (uint32_t p, FunctionalState s) { (void) p; (void) s; }
static inline void RCC_APB2PeriphClockCmd (uint32_t p, FunctionalState s) { (void) p; (void) s; }
static inline void RCC_AHB1PeriphClockCmd (uint32_t p, FunctionalState s) { (void) p; (void) s; }
static inline void GPIO_Init (GPIO_TypeDef* g, GPIO_InitTypeDef* i) { (void) g; (void) i; }
static inline void GPIO_PinAFConfig (GPIO_TypeDef* g, uint16_t s, uint8_t a) { (void) g; (void) s; (void) a; }
static inline void TIM_TimeBaseStructInit (TIM_TimeBaseInitTypeDef* t) { (void) t; }
static inline void TIM_TimeBaseInit (TIM_TypeDef* t, TIM_TimeBaseInitTypeDef* i) { (void) t; (void) i; }
static inline void TIM_ITConfig (TIM_TypeDef* t, uint16_t i, FunctionalState s) { (void) t; (void) i; (void) s; }
static inline void TIM_Cmd (TIM_TypeDef* t, FunctionalState s) { (void) t; (void) s; }
static inline ITStatus TIM_GetITStatus (TIM_TypeDef* t, uint16_t i) { (void) t; (void) i; return RESET; }
static inline void TIM_ClearITPendingBit (TIM_TypeDef* t, uint16_t i) { (void) t; (void) i; }
static inline void TIM_OCStructInit (TIM_OCInitTypeDef* o) { (void) o; }
static inline void TIM_OC1Init (TIM_TypeDef* t, TIM_OCInitTypeDef* o) { (void) t; (void) o; }
static inline void TIM_OC2Init (TIM_TypeDef* t, TIM_OCInitTypeDef* o) { (void) t; (void) o; }
static inline void TIM_OC3Init (TIM_TypeDef* t, TIM_OCInitTypeDef* o) { (void) t; (void) o; }
static inline void TIM_OC4Init (TIM_TypeDef* t, TIM_OCInitTypeDef* o) { (void) t; (void) o; }
static inline void TIM_OC1PreloadConfig (TIM_TypeDef* t, uint16_t p) { (void) t; (void) p; }
static inline void TIM_OC2PreloadConfig (TIM_TypeDef* t, uint16_t p) { (void) t; (void) p; }
static inline void TIM_OC3PreloadConfig (TIM_TypeDef* t, uint16_t p) { (void) t; (void) p; }
static inline void TIM_OC4PreloadConfig (TIM_TypeDef* t, uint16_t p) { (void) t; (void) p; }
static inline void TIM_CtrlPWMOutputs (TIM_TypeDef* t, FunctionalState s) { (void) t; (void) s; }
static inline void TIM_SelectOutputTrigger (TIM_TypeDef* t, uint16_t s) { (void) t; (void) s; }
static inline void TIM_SelectCOM (TIM_TypeDef* t, FunctionalState s) { (void) t; (void) s; }
static inline void TIM_CCPreloadControl (TIM_TypeDef* t, FunctionalState s) { (void) t; (void) s; }
static inline void TIM_SelectCCDMA (TIM_TypeDef* t, FunctionalState s) { (void) t; (void) s; }
static inline void TIM_SelectHallSensor (TIM_TypeDef* t, FunctionalState s) { (void) t; (void) s; }

#ifdef __cplusplus
}
#endif

#endif
