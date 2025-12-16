/*
 * adc.c
 *
 *  ADC relation functions for ATmega8
 *
 *  Created on: Apr 18, 2022
 *  Author: Visakhan
 */

#include "adc.h"


/*
 * 	Initialize ADC with provided Prescaler and Vref.
 * 	Also Enables ADC
 *
 * 	adc_ps : Prescaler value selection
 * 	adc_vref: Voltage reference selection
 */
void adc_init(uint8_t adc_ps, uint8_t adc_vref)
{
    /* Set Voltage Reference */
    ADMUX = adc_vref;

    /* Enable ADC with given prescaler */
    ADCSRA = (1 << ADEN)| adc_ps;
}


/* Set the ADC channel for the next convertions
 *
 *	ch : ADC channel selection
 */
void adc_select_channel(uint8_t ch)
{
	ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);
}



/* Set the ADC voltage reference
 *
 *  adc_ref: Reference voltage
 */
void adc_select_vref(uint8_t vref)
{
	ADMUX = (ADMUX & 0x3F) | (vref & 0xC0);
}




/* Do one ADC sample and return 10-bit result (Polled mode) */
uint16_t adc_samp(void)
{
    /* Start a conversion */
    ADCSRA |= (1 << ADSC);
    while( !(ADCSRA & (1 << ADIF)) )
        ;
    ADCSRA |= (1 << ADIF); /* Clear flag */

    /* Read results */
    return ADC;
}

/* Do one ADC sample and return 8-bit result ADCH (Polled mode)
 * Note: It is necessary to call ADC_LEFT_ADJUST() to before
 * using this function for meaningful ADC output value */
uint8_t adc_samp_8(void)
{
	/* Start a conversion */
	ADCSRA |= (1 << ADSC);
	while( !(ADCSRA & (1 << ADIF)) )
		;
	ADCSRA |= (1 << ADIF); /* Clear flag */

	/* Read results */
	return ADCH;

}

