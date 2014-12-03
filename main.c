#include <msp430.h> 
#include "printstuff.h"

#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR (void){
	int i = ADC10MEM;
	printf("Next Val %i\r", i);

}

// Timer A0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void)
{
  P1OUT ^= BIT6;                            // Toggle LED
  ADC10CTL0 |= ENC + ADC10SC;
}

/*
 * main.c
 */
int main(void) {
	WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer
	P1SEL |= BIT0;
	P1DIR |= BIT6;
	ADC10CTL1 = INCH_0 + ADC10DIV_3 ; // Channel 5, ADC10CLK/4
	ADC10CTL0 = SREF_0 + ADC10SHT_3 + ADC10ON + ADC10IE; //Vcc & Vss as reference
	ADC10AE0 |= BIT0;

	initUART();
	TACCTL0 = CCIE;
	TACTL = TASSEL_1 + MC_1 + ID_0;
	TACCR0 =  5000;	//set count to 1 sec
	_enable_interrupt();


	while (1){
		//do nothing
	}
}
