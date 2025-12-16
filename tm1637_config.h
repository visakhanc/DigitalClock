/*
 * tm1637_config.h
 *
 * Pin definitions for TM1637 interfacing AVR
 *
 *  Created on: May 19, 2022
 *      Author: Visakhan
 */

#ifndef TM1637_CONFIG_H_
#define TM1637_CONFIG_H_


/* Clock pin of TM1637 */
#define TM1637_CLK_PORT		PORTD
#define TM1637_CLK_PIN		PIND
#define TM1637_CLK_DDR		DDRD
#define TM1637_CLK_BIT		PD4

/* DIO pin of TM1637 */
#define TM1637_DIO_PORT		PORTD
#define TM1637_DIO_PIN		PIND
#define TM1637_DIO_DDR		DDRD
#define TM1637_DIO_BIT		PD1



#endif /* TM1637_CONFIG_H_ */
