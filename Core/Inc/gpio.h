/*
 * gpio.h
 *
 *  Created on: 28 abr. 2021
 *      Author: David
 */

#ifndef INC_GPIO_H_
#define INC_GPIO_H_

#include "main.h"


#define _CON2(a,b)      a##b
#define _PORT(p)        (_CON2(p,_GPIO_Port))
#define _PIN(p)         (_CON2(p,_Pin))


#define _IDR(p)         (_PORT(p)->IDR)
#define _ODR(p)         (_PORT(p)->ODR)
#define _BSRR(p)        (_PORT(p)->BSRR)
#define _MODER(p)       (_PORT(p)->MODER)
#define _OSPEEDR(p)     (_PORT(p)->OSPEEDR)
#define _OTYPER(p)      (_PORT(p)->OTYPER)
#define _PUPDR(p)       (_PORT(p)->PUPDR)

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
#define SetPinMode(pin,mode)          _MODER(pin) = (_MODER(pin) & ~(__expand_16to32(_PIN(pin)))) | mode<<(__get_GPIO_Pos(_PIN(pin))*2)

// GPIOx_OTYPER
// Otypes: OUTPUT_OD, OUTPUT_PP
#define SetPinOtype(pin,Otype)        _OTYPER(pin) = (_OTYPER(pin) & ~(_PIN(pin))) | (_PIN(pin)*(Otype&&1))

// GPIOx_OSPEEDR
// Speeds: GPIO_SPEED_FREQ_LOW, GPIO_SPEED_FREQ_MEDIUM, GPIO_SPEED_FREQ_HIGH, GPIO_SPEED_FREQ_VERY_HIGH
#define SetPinSpeed(pin,speed)        _OSPEEDR(pin) = _OSPEEDR(pin) & ~(__expand_16to32(_PIN(pin))) | speed<<(__get_GPIO_Pos(_PIN(pin))*2)

// GPIOx_PUPDR
// Pulls: GPIO_NOPULL, GPIO_PULLUP, GPIO_PULLDOWN
#define SetPinPull(pin,pull)          _PUPDR(pin) = _PUPDR(pin) & ~(__expand_16to32(_PIN(pin))) | pull<<(__get_GPIO_Pos(_PIN(pin))*2)

// GPIOx_IDR
#define ReadPin(pin)                  (_IDR(pin) & _PIN(pin) && 1)
#define ReadPort(port)                port->IDR

// GPIOx_ODR
#define WritePort(port,val)           port->ODR = val
#define TogglePin(pin)                _ODR(pin) ^= _PIN(pin)

// GPIOx_BSRR
#define WritePin(pin,val)             _BSRR(pin) = _PIN(pin) << (val ? 0 : 16)
#define SetPinHigh(pin)               _BSRR(pin) = _PIN(pin)
#define SetPinLow(pin)                _BSRR(pin) = _PIN(pin)<<16

#endif /* INC_GPIO_H_ */
