#include "Energia.h"
#include <Enrf24.h>
#include <nRF24L01.h>
#include <SPI.h>
#include <msp430.h>
#include <string.h>

String dump_radio_status_to_serialport(uint8_t);

#define debug 0 //sets debug to computer mode

//Variables
volatile int BPM;                   // used to hold the pulse rate
volatile int Signal;                // holds the incoming raw data
volatile int IBI = 600;             // holds the time between beats, must be seeded!
volatile bool Pulse = false;     // true when pulse wave is high, false when it's low
volatile bool QS = false;        // becomes true when Arduoino finds a beat.
volatile int QScnt = 0;
volatile int BPMtotal = 0;

//Global Variables
volatile int rate[10];                    			// array to hold last ten IBI values
volatile unsigned long sampleCounter = 0;          // used to determine pulse timing
volatile unsigned long lastBeatTime = 0;           // used to find IBI
volatile int P =512;                      // used to find peak in pulse wave, seeded
volatile int T = 512;                     // used to find trough in pulse wave, seeded
volatile int thresh = 525;                // used to find instant moment of heart beat, seeded
volatile int amp = 100;                   // used to hold amplitude of pulse waveform, seeded
volatile bool firstBeat = true;        // used to seed rate array so we startup with reasonable BPM
volatile bool secondBeat = false;      // used to seed rate array so we startup with reasonable BPM

void setup();
void loop();
void startRadio();
void stopRadio();

Enrf24 radio(P2_1, P2_0, P1_3);  // P2.0=CE, P2.1=CSN, P2.2=IRQ
const uint8_t txaddr[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01 };

void setup() {
	//	WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer
	if(debug){
		Serial.begin(9600);			//debug mode only
		Serial.println("Starting Up");
	}



	P1SEL |= BIT0;
	ADC10CTL1 = INCH_0 + ADC10DIV_3 ; // Channel 5, ADC10CLK/4
	ADC10CTL0 = SREF_0 + ADC10SHT_3 + ADC10ON + ADC10IE; //Vcc & Vss as reference
	ADC10AE0 |= BIT0;


	TACTL = TASSEL_1 + MC_1 + ID_0;
	TACCR0 =  65;	//set count to 1 sec
	TACCTL0 = CCIE;
	if(!debug)
		startRadio();

}

void startRadio(){
	SPI.begin();
	SPI.setDataMode(SPI_MODE0);
	SPI.setBitOrder(MSBFIRST);

	radio.begin();  // Defaults 1Mbps, channel 0, max TX power

	radio.setTXaddress((void*)txaddr);
	dump_radio_status_to_serialport(radio.radioState());
}

void stopRadio(){
	radio.end();
	SPI.end();
	TACTL = TASSEL_1 + MC_1 + ID_0;
	TACCR0 =  65;	//set count to 1 sec
	TACCTL0 = CCIE;
}

void loop() {
	if (QS){
		if (debug){
			Serial.print("BPM is ");
			Serial.println(BPM);
		}
		QS = false;
		String stat = dump_radio_status_to_serialport(radio.radioState());
		if(!debug){
			radio.print(BPM);
			radio.flush();
		}else{
			Serial.print("Reciver Mode: ");
			Serial.println(stat);
		}
		//		stopRadio();

	}
}


#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR (void){
	volatile int sig = ADC10MEM;
	sampleCounter += 2;                         // keep track of the time in mS with this variable
	int time = (int)(sampleCounter - lastBeatTime);       // monitor the time since the last beat to avoid noise

	//  find the peak and trough of the pulse wave
	if(sig < thresh && time > (IBI/5)*3){       // avoid dichrotic noise by waiting 3/5 of last IBI
		if (sig < T){                        // T is the trough
			T = sig;                         // keep track of lowest point in pulse wave
		}
	}

	if(sig > thresh && sig > P){          // thresh condition helps avoid noise
		P = sig;                             // P is the peak
	}                                        // keep track of highest point in pulse wave

	//  NOW IT'S TIME TO LOOK FOR THE HEART BEAT
	// signal surges up in value every time there is a pulse
	if (time > 250){                                   // avoid high frequency noise
		if ( (sig > thresh) && (Pulse == false) && (time > (IBI/5)*3) ){
			Pulse = true;                               // set the Pulse flag when we think there is a pulse

			IBI = sampleCounter - lastBeatTime;         // measure time between beats in mS
			lastBeatTime = sampleCounter;               // keep track of time for next pulse

			if(secondBeat){                        // if this is the second beat, if secondBeat == TRUE
				secondBeat = false;                  // clear secondBeat flag
				for(int i=0; i<=9; i++){             // seed the running total to get a realisitic BPM at startup
					rate[i] = IBI;
				}
			}

			if(firstBeat){                         // if it's the first time we found a beat, if firstBeat == TRUE
				firstBeat = false;                   // clear firstBeat flag
				secondBeat = true;                   // set the second beat flag
				return;                              // IBI value is unreliable so discard it
			}


			// keep a running total of the last 10 IBI values
			unsigned int runningTotal = 0;                  // clear the runningTotal variable

			for(int i=0; i<=8; i++){                // shift data in the rate array
				rate[i] = rate[i+1];                  // and drop the oldest IBI value
				runningTotal += rate[i];              // add up the 9 oldest IBI values
			}

			rate[9] = IBI;                          // add the latest IBI to the rate array
			runningTotal += rate[9];                // add the latest IBI to runningTotal
			runningTotal /= 10;                     // average the last 10 IBI values
			BPM = 60000/runningTotal;               // how many beats can fit into a minute? that's BPM!
			QS = true;                              // set Quantified Self flag
			// QS FLAG IS NOT CLEARED INSIDE THIS ISR
		}
	}

	if (sig < thresh && Pulse == true){   // when the values are going down, the beat is over
		Pulse = false;                         // reset the Pulse flag so we can do it again
		amp = P - T;                           // get amplitude of the pulse wave
		thresh = amp/2 + T;                    // set thresh at 50% of the amplitude
		P = thresh;                            // reset these for next time
		T = thresh;
	}

	if (time > 2500){                           // if 2.5 seconds go by without a beat
		thresh = 512;                          // set thresh default
		P = 512;                               // set P default
		T = 512;                               // set T default
		lastBeatTime = sampleCounter;          // bring the lastBeatTime up to date
		firstBeat = true;                      // set these to avoid noise
		secondBeat = false;                    // when we get the heartbeat back
	}
}

String dump_radio_status_to_serialport(uint8_t status)
{
	//	Serial.print("Enrf24 radio transceiver status: ");
	switch (status) {
	case ENRF24_STATE_NOTPRESENT:
		return "NO TRANSCEIVER PRESENT";
		break;

	case ENRF24_STATE_DEEPSLEEP:
		return "DEEP SLEEP <1uA power consumption";
		break;

	case ENRF24_STATE_IDLE:
		return "IDLE module powered up w/ oscillators running";
		break;

	case ENRF24_STATE_PTX:
		return "Actively Transmitting";
		break;

	case ENRF24_STATE_PRX:
		return "Receive Mode";
		break;

	default:
		return "UNKNOWN STATUS CODE";
	}
}

// Timer A0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void TA0_ISR(void)
{
	ADC10CTL0 |= ENC + ADC10SC;
}

