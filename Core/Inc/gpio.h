/*
 * gpio.h
 *
 *  Created on: 28 abr. 2021
 *      Author: David
 */

#ifndef INC_GPIO_H_
#define INC_GPIO_H_

#include "main.h"


// Returns bit position. Ex. 100000 ->5
#define __get_GPIO_Pos(x)             (15*(1&&(x&(1<<15)))  + \
                                      14*(1&&(x&(1<<14)))   + \
                                      13*(1&&(x&(1<<13)))   + \
                                      12*(1&&(x&(1<<12)))   + \
                                      11*(1&&(x&(1<<11)))   + \
                                      10*(1&&(x&(1<<10)))   + \
                                      9*(1&&(x&(1<<9)))     + \
                                      8*(1&&(x&(1<<8)))     + \
                                      7*(1&&(x&(1<<7)))     + \
                                      6*(1&&(x&(1<<6)))     + \
                                      5*(1&&(x&(1<<5)))     + \
                                      4*(1&&(x&(1<<4)))     + \
                                      3*(1&&(x&(1<<3)))     + \
                                      2*(1&&(x&(1<<2)))     + \
                                      1*(1&&(x&(1<<1))))

// Source: https://stackoverflow.com/questions/38881877/bit-hack-expanding-bits
// Aux macros for __expand_16to32
#define __expand_1(x)                 ((x | (x << 8)) & 0x00FF00FF)
#define __expand_2(x)                 ((x | (x << 4)) & 0x0F0F0F0F)
#define __expand_3(x)                 ((x | (x << 2)) & 0x33333333)
#define __expand_4(x)                 ((x | (x << 1)) & 0x55555555)
#define __expand_5(x)                 ( x | (x << 1))

// Expands 16 bit to 32. Ex. 0001->00000011, 1000->11000000
#define __expand_16to32(x)            __expand_5(__expand_4(__expand_3(__expand_2(__expand_1(x)))))

// GPIOx_MODER
// Modes: MODE_INPUT, MODE_OUTPUT, MODE_AF, MODE_ANALOG
#define setPinMode(port,pin,mode)     port->MODER = (port->MODER & ~(__expand_16to32(pin))) | mode<<(__get_GPIO_Pos(pin)*2)

// GPIOx_OTYPER
// Otypes: MODE_OD, MODE_PP
#define setPinOtype(port,pin,Otype)   port->OTYPER = (port->OTYPER & ~(pin)) | (pin*Otype)

// GPIOx_OSPEEDR
// Speeds: GPIO_SPEED_FREQ_LOW, GPIO_SPEED_FREQ_MEDIUM, GPIO_SPEED_FREQ_HIGH, GPIO_SPEED_FREQ_VERY_HIGH
#define setPinSpeed(port,pin,speed)   port->OSPEEDR = (port->OSPEEDR & ~(__expand_16to32(pin))) | speed<<(__get_GPIO_Pos(pin)*2)

// GPIOx_PUPDR
// Pulls: GPIO_NOPULL, GPIO_PULLUP, GPIO_PULLDOWN
#define setPinPull(port,pin,pull)     port->PUPDR = (port->PUPDR & ~(__expand_16to32(pin))) | pull<<(__get_GPIO_Pos(pin)*2)

// GPIOx_IDR
#define readPin(port,pin)             ((port->IDR & pin)&&1)
#define readPort(port)                port->IDR

// GPIOx_ODR
#define writePort(port,val)           port->ODR = val
//#define togglePin(port,pin)          port->ODR = port->ODR^pin
#define togglePin(port,pin)           port->BSRR = (readPin(port,pin) ? pin<<16 : pin)

// GPIOx_BSRR
#define writePin(port,pin,val)        port->BSRR = pin<<(16*(!val))
#define setPinHigh(port,pin)          port->BSRR = pin
#define setPinLow(port,pin)           port->BSRR = pin<<16

#endif /* INC_GPIO_H_ */
