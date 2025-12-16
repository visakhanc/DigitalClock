/*
 * adc.h
 *
 *  ADC header definitions for ATmega8
 *
 *  Created on: Apr 18, 2022
 *  Author: Visakhan
 */

#ifndef ADC_H_
#define ADC_H_

#include <avr/io.h>


/*************** MACROS ******************/
/* ADC Prescaler selections available. ADCclk = F_CPU/Prescaler */
#define ADC_PRESCALER_2		(1 << ADPS0)
#define ADC_PRESCALER_4		(1 << ADPS1)
#define ADC_PRESCALER_8		((1 << ADPS1)|(1 << ADPS0))
#define ADC_PRESCALER_16	(1 << ADPS2)
#define ADC_PRESCALER_32	((1 << ADPS2)|(1 << ADPS0))
#define ADC_PRESCALER_64	((1 << ADPS2)|(1 << ADPS1))
#define ADC_PRESCALER_128	((1 << ADPS2)|(1 << ADPS1)|(1 << ADPS0))



/* ADC Voltage Reference selections available */
#define ADC_VREF_AREF		0	/* External voltage reference */
#define ADC_VREF_AVCC		(1 << REFS0)	/* Reference voltage = AVCC */
#define ADC_VREF_INTERNAL	((1 << REFS1)|(1 << REFS0))  /* Reference voltage = 2.56V (ATmega8) */


/* ADC input channel selection */
#define ADC_CHANNEL_0 		0
#define ADC_CHANNEL_1 		1
#define ADC_CHANNEL_2 		2
#define ADC_CHANNEL_3 		3
#define ADC_CHANNEL_4 		4
#define ADC_CHANNEL_5 		5
#define ADC_CHANNEL_6 		6
#define ADC_CHANNEL_7 		7
#define ADC_CHANNEL_VBG		(7 << 1)  /* 1.30V (VBG) */
#define ADC_CHANNEL_0V		0xF		  /* 0V (GND) */



/***************** Macros ******************/

#define ADC_ENABLE()			(ADCSRA |= (1 << ADEN))
#define ADC_DISABLE()			(ADCSRA &= ~(1 << ADEN))
#define ADC_LEFT_ADJUST()		(ADMUX |= (1 << ADLAR))		/* Set the ADC result to be left adjusted. ADCH will have the MSB 8-bit of the result */
#define ADC_RIGHT_ADJUST()		(ADMUX &= ~(1 << ADLAR))	/* Set the ADC result to be right adjusted (default) */
#define ADC_INTERRUPT_ENABLE()	(ADCSRA |= (1 << ADIE))
#define ADC_INTERRUPT_DISABLE()	(ADCSRA &= ~(1 << ADIE))
#define ADC_START_CONV()		(ADCSRA |= (1 << ADSC))





/************ Function declarations *************/

void adc_init(uint8_t adc_ps, uint8_t adc_ref);
void adc_select_channel(uint8_t adc_ch);
void adc_select_vref(uint8_t vref);
uint16_t adc_samp(void);
uint8_t adc_samp_8(void);


#endif /* ADC_H_ */
