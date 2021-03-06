/****************************************************************************
    Copyright (C) 2006, 2011 Olaf Funke, Martin Pischky

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    $Id: fredi.c,v 1.13 2011/07/31 15:54:12 pischky Exp $
******************************************************************************/


/******************************************************************************/
// Fredi
// The new FRED for Fremo!
/******************************************************************************/


/******************************************************************************/
// State by LED
/******************************************************************************/
//  - red on, green left off, green right off
//    Fredi ready for dispatching
//
//  - red and green blinking alternately
//    Fredi is connecting
//
//  - red, and green running around slowly
//    Fredi is in selftest, press all buttons and move knob around
//
//  - red, and green running around fast
//    Fredi has finished selftest successfully, remove cable and plug in again
//
//  - red off, green left on, green right off
//    normal running, direction backward
//
//  - red off, green left off, green right on
//    normal running, direction forward
//    
//  - red off, green left blinking, green right off
//    emergency stop, so move knob to the left (0), direction backward
//    
//  - red off, green left blinking, green right on
//    emergency stop, so move knob to the left (0), direction forward
//    
//  - all on
//    Fredi has no ID, you need a programmer
//    
//  - all off 
//    Plug in cable, ;-)
/******************************************************************************/


/******************************************************************************/
// overview
/******************************************************************************/
//         
//            ----------
//          /    ---     \                                                     .
//  red    | 0 /     \    |
//         |  |       |   | knob speed (and stop)
//         |   \     /    |
//         |     ---      |
// green l | 0         0  | green r
//         |              |
// stop    | O   (o)o     | direction switch
//         |              |
//         |      O       | F0
//         |              |
//         |      O       | F1 / F5
//         |              |
//         |      O       | F2 / F6
//         |              |
//         |      O       | F3 / F7
//         |              |
//         |      O       | F4 / F8
//         |              |
//         |      O       | Shift
//          \            /
//            ----------
//
// - dispatching
//   press stop and F0 together
// - F5..F8 = F1..F4 + Shift
/******************************************************************************/

#include <stdlib.h>
#include <stdint.h>         // typedef int8_t, typedef uint8_t,
                            // typedef int16_t, typedef uint16_t
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>     // #define EEMEM


#include "sysdef.h"
#include <util/delay.h>
#include "common_defs.h"
#include "ln_sw_uart.h"
#include "ln_interface.h"
#include "ln_status.h"
//#include "throttle.h"
#include "systimer.h"

#include "keys.h"
#include "processor.h"
#include "potadc.h"         // potAdcSpeedValue, potAdcRawValue
                            // potAdcPowerOff(), potAdcInit(),
                            // potAdcTimerAction()

/******************************************************************************/
// functions
/******************************************************************************/
void vSetState( byte bState, rwSlotDataMsg *currentSlot );
void vProcessRxLoconetMessage(rwSlotDataMsg *currentSlot);
void vProcessRxMonitorMessage(void);
void vProcessKey(rwSlotDataMsg *currentSlot);
void vProcessEncoder(rwSlotDataMsg *currentSlot);
void vProcessPoti(rwSlotDataMsg *currentSlot);
void vCheckSelfTestEnd(rwSlotDataMsg *currentSlot);
void vCopySlotFromRxPacket(rwSlotDataMsg *currentSlot);
void vSetUnconnected(rwSlotDataMsg *currentSlot);

LN_STATUS sendLocoNetSpd(rwSlotDataMsg *currentSlot);
void sendLocoNetDirf(rwSlotDataMsg *currentSlot);
void sendLocoNetSnd(rwSlotDataMsg *currentSlot);
void sendLocoNetWriteSlotData(rwSlotDataMsg *currentSlot);
void sendLocoNetMove(byte bSrc, byte bDest, rwSlotDataMsg *currentSlot);
LN_STATUS sendLocoNetAdr(rwSlotDataMsg *currentSlot);
//void sendLocoNetSelfTest(byte bTestCase, byte bValue);
void sendLocoNetFredAdc( uint16_t raw );
void sendLocoNetFredCd( uint8_t cdTime );
void sendLocoNetFredButton( uint8_t button );

uint8_t adcCount = 0;

void processADC(void);
void GET_SPDCNT_BY_SLOTSPD (rwSlotDataMsg *currentSlot);
byte IncrementTimerAction( void *UserPointer);
byte LEDTimerAction( void *UserPointer);
byte KeyTimerAction( void *UserPointer);
byte MessageTimerAction( void *UserPointer);
byte ReleaseStopTimerAction( void *UserPointer);
void initKeys( void );
void initSlots(rwSlotDataMsg *slotArray, leddata *array);
//LED Funktionen
void setLEDasOutput(void);		//setzt Port A Bit 0,1 und Port C Bit 3,4 als Ausgang
void initLEDS (leddata *array); 
void enableLED1(void);			//Einschalten LED 1
void enableLED2(void);			//Einschalten LED 2
void enableLED3(void);			//Einschalten LED 3
void enableLED4(void);			//Einschalten LED 4
void disableLED1(void);			//Ausschalten LED 1
void disableLED2(void);			//Ausschalten LED 2
void disableLED3(void);			//Ausschalten LED 3
void disableLED4(void);			//Ausschalten LED 4
void enableLED(int8_t number);
void disableLED(int8_t number);
void toggleLED1(void);			//Toggle LED1
void toggleLED2(void);			//Toggle LED2
void toggleLED3(void);			//Toggle LED3
void toggleLED4(void);			//Toggle LED4
void toggleLED(int8_t number);
void ProcessKeyInput (byte *pin, byte *port);

//int ProcessKeyInput8Streak (keydata *key);
void ProcessDirKeyInput8Streak (byte pin, byte port, int8_t keyNumber, rwSlotDataMsg *currentSlot);
void ProcessShiftKeyInput8StreakFun (byte *pin, byte *port);
void ProcessShiftKeyInput8Streak (byte pin, int8_t port, int8_t keyNumber);
void ProcessFunKeyInput8StreakFun (byte *pin, byte *port);
void ProcessFunKeyInput8Streak (byte pin, int8_t port, int8_t keyNumber, rwSlotDataMsg *currentSlot);
void transmitInputLoco(rwSlotDataMsg *currentSlot, int8_t currentNumber);
LN_STATUS testLoco (byte opc, byte data);
void processValue(int8_t);
void setPullUps (void);
void entprellen_druecken(byte new_val, byte val);
void entprellen_loslassen(byte new_val, byte val);

//TMT Funktionen
void testLEDMethods (void);
void testFalseSignalStreakIsSet (byte pin, byte taste, long middleCompare, long distance, long tries);
void testFalseSignalStreakIsClear (byte pin, byte taste, long middleCompare, long distance);
void testSignalRatio (byte pin, byte taste, long tries);
void testKeySignal (byte pin, byte taste);
void _delay_1500ms (void);
/******************************************************************************/
// main defines & variables
/******************************************************************************/

/******************************************************************************/
// eeprom 
/******************************************************************************/

byte abEEPROM[10] EEMEM;

enum FREDI_VERSION
{
  FREDI_VERSION_UNDEF             = 0,
  FREDI_VERSION_INCREMENT         = 1,
  FREDI_VERSION_INCREMENT_SWITCH  = 2,
  FREDI_VERSION_ANALOG            = 3,
  FREDI_VERSION_LAST
};


byte bFrediVersion = FREDI_VERSION_UNDEF;





byte bNewState = THR_STATE_INIT;
byte bThrState = THR_STATE_SELFTEST;
volatile byte  bEvent     = 0;


/******************************************************************************/
// speed 
/******************************************************************************/

static volatile byte bSpdCnt = 0;

#define MAX_SPEED  33

const byte abSpd[MAX_SPEED+4] =
{
  0,  3,  5,  7, 11, 15, 19, 23, 27, 31,
  35, 39, 43, 47, 51, 55, 59, 63, 67, 71,
  75, 79, 83, 87, 91, 95, 99,103,107,111,
  115,119,123,127,127,127,127
};

void GET_SPDCNT_BY_SLOTSPD (rwSlotDataMsg *currentSlot) {									
	bSpdCnt = MAX_SPEED;                                        
	for (int i = 0; i < MAX_SPEED; i++)                             
	{                                                           
		if ((abSpd[i] <= currentSlot->spd) && (abSpd[i+1] > currentSlot->spd))
		{                                                       
			bSpdCnt = i;                                        
		}                                                       
	}
}




/******************************************************************************/
// loconet
/******************************************************************************/
LnBuf RxBuffer;

lnMsg *RxPacket;

//LN_STATUS RxStatus ;
//word      RxMsgCount ;
lnMsg TxPacket;
#define NUMBER_OF_SLOTS 4 //number of slots to be managed by the device
#define NUMBER_OF_LEDS 4  //number of LEDS to be managed
struct rwslotdata_t slotArray[NUMBER_OF_SLOTS];
leddata ledArray[NUMBER_OF_LEDS];
int8_t testoverflow;
volatile  unsigned char *led1Adr = &PORTA;
volatile uint8_t *led2Adr = &PORTA;
int8_t		slotnumber = 0;
int neuerADCwert[NUMBER_OF_SLOTS];
uint8_t value = 0;
byte old_value = 0; //war 1 bei Jan
byte entprell = 0;
long keyStatus = 0;

uint8_t shiftKeyCounter[NUMBER_OF_SLOTS];
uint8_t funKeyCounter[NUMBER_OF_SLOTS];
uint8_t dirKeyCounter[NUMBER_OF_SLOTS];
int8_t shiftedKeyStatus = 0;
int8_t shiftStatustwo[2]; // [0] = Nummer des Reglers f�r den shift Freigeschaltet ist, [1] = Funktionen 0-4 (1) oder Funktionen 5-8 Freigeschaltet
byte dirfSCopy[2];
int8_t shiftStatus = 0;
uint8_t funkeyStatus = 0;
unsigned long retryRxCounter = 0;
uint8_t dirKeyStatus = 0;
LN_STATUS dirfSendStatus;
LN_STATUS spdSendStatus;
LN_STATUS driveLockSendStatus;
LN_STATUS sndSendStatus;
uint8_t dirfKeyStatus[NUMBER_OF_SLOTS];
uint8_t potiStatus = 0;
LN_STATUS sendStatus;
unsigned long shiftPressed = 0;
int8_t shiftTimeOut = 0;



//TMT Variables
unsigned long int streak = 0;
unsigned long int highStreak = 1;
unsigned long int correctSignal = 0;
unsigned long int wrongSignal = 0;
uint16_t ratioCounter = 0;

//First Bit is status of first fun/dir/erwfun - key, second bit of..... 0 = key was not pressed, 1 = key was pressed
byte bERWFunKey1Status;
byte bERWFunKey2Status;
byte bERWFunKey3Status;
byte bERWFunKey4Status;


/******************************************************************************/
// timer
/******************************************************************************/
TimerAction  IncrementTimer;
TimerAction  KeyTimer;
TimerAction  LEDTimer;
TimerAction  MessageTimer;
TimerAction  ReleaseStopTimer;


byte bLEDReload = LED_BLINK_TIME;

/******************************************************************************/
// switches and decoder
/******************************************************************************/

static volatile int8_t    sEncDir       = 0;
static volatile byte    bCurrentKey   = 0;
static          uint16_t  wSelfTest     = 0;
static volatile byte    fSetSpeed     = TRUE;
static volatile byte    bStopPressed  = FALSE;

/******************************************************************************/

void setLEDasOutput() {
	DDRA |= _BV(LED1);
	DDRA |= _BV(LED2);
	DDRC |= _BV(LED3);
	DDRC |= _BV(LED4);
}

void enableLED1(){
	PORTA &= ~(_BV(LED1));
}

void enableLED2(){
	PORTA &= ~(_BV(LED2));
}

void enableLED3(){
	PORTC &= ~(_BV(LED3));
}

void enableLED4(){
	PORTC |= _BV(LED4);
}

void disableLED1(){
	PORTA |= _BV(LED1);
}

void disableLED2(){
	PORTA |= _BV(LED2);
}

void disableLED3(){
	PORTC |= _BV(LED3);
}

void disableLED4(){
	PORTC &= ~(_BV(LED4));
}

void disableLED (int8_t number) {
	switch (number)
	{
	case (1): 
		disableLED1();
		break;
	case (2): 
		disableLED2();
		break;
	case (3): 
		disableLED3();
		break;
	case (4): 
		disableLED4();
		break;
	}
}

void enableLED (int8_t number) {
	switch (number)
	{
		case (1): 
			enableLED1();
			break;
		case (2): 
			enableLED2();
			break;
		case (3): 
			enableLED3();
			break;
		case (4): 
			enableLED4();
			break;
	}
}


void toggleLED1() {
	if (bit_is_set(PORTA, LED1)) {
		PORTA &= ~(_BV(LED1));
	} else {
		PORTA |= _BV(LED1);
	}
}

void toggleLED2() {
	if (bit_is_set(PORTA, LED2)) {
		PORTA &= ~(_BV(LED2));
		} else {
		PORTA |= _BV(LED2);
	}
}

void toggleLED3() {
	if (bit_is_set(PORTC, LED3)) {
		PORTC &= ~(_BV(LED3));
		} else {
		PORTC |= _BV(LED3);
	}
}

void toggleLED4() {
	if (bit_is_set(PORTC, LED4)) {
		PORTC &= ~(_BV(LED4));
		} else {
		PORTC |= _BV(LED4);
	}
}

void toggleLED (int8_t number) {
	switch (number)
	{
		case (1): 
			toggleLED1();
			break;
		case (2): 
			toggleLED2();
			break;
		case (3): 
			toggleLED3();
			break;
		case (4): 
			toggleLED4();
			break;
	}
}


/******************************************************FunctionHeaderBegin******
 * FUNCTION    : IncrementTimerAction
 * CREATED     : 2005-01-21
 * AUTHOR      : Olaf Funke
 * DESCRIPTION : For debouncing of increment switch a timer is started. At 
 *               expiry of the timer the interrupt for the next increment is set
 *               in dependencie of the current level.
 *******************************************************************************
 * ARGUMENTS   : void *UserPointer
 * RETURN VALUE: byte, is set to 0 because this timer is has only 
 *               "monoflop"-functionality
 *******************************************************FunctionHeaderEnd******/
byte IncrementTimerAction( void *UserPointer)
{
  // detect actual state of pin to set next interrupt edge
  if ( bit_is_set(ENC_PIN, ENC_BIT_1))
  { // set falling edge
    ENC_ISC_REG &= ~_BV(ENC_ISC_BIT0);
  }
  else
  { // set rising edge
    ENC_ISC_REG |= _BV(ENC_ISC_BIT0);
  }

  // clear pending interrupt
  ENC_EIRF_REG |= _BV(ENC_EIRF_BIT);
  // set interrupt activ
  ENC_EIRE_REG |= _BV(ENC_EIRE_BIT);

  return 0;
}


/******************************************************FunctionHeaderBegin******
 * FUNCTION    : LEDTimerAction
 * CREATED     : 20051-01-08
 * AUTHOR      : Olaf Funke
 * DESCRIPTION : This function is responsible for a blinking of the LEDs. There 
 *               are different reasons for blinking:
 *               - An analog Fredi is in connected, but the position of the poti
 *                 mismatches to the current speed of loco. So the blinking green 
 *                 LED shows shows the direction you have to turn the speed 
 *                 button.
 *               - Selftest is active. A running light shows an active selftest.
 *                 If all keys were pressed and the speed button has been turned 
 *                 to the left and to the right, the frequency of blinking is 
 *                 changed. For leaving selftest you have to disconnect the 
 *                 Fredi from loconet and reconnect again.
 *               - While throttle is dispatching an alterning blinking of red 
 *                 and green LEDs shows this state
 *******************************************************************************
 * ARGUMENTS   : void *UserPointer
 * RETURN VALUE: byte, depence on blinkfrequency
 *******************************************************FunctionHeaderEnd******/
byte LEDTimerAction( void *UserPointer)
{
	/******************************************************************************/
	// mismatching speed
	/******************************************************************************/
	if (bThrState == THR_STATE_CONNECTED)
	{
		if (  (bFrediVersion == FREDI_VERSION_ANALOG)
		&& (!fSetSpeed))                         // if analog value does not correspond, show blinking
		{
			if (slotArray[slotnumber].spd > potAdcSpeedValue)         // speed is higher than position of poti
			{
				// -> speed up (turn right)
				if (bit_is_clear(LED_PORT, LED_GREEN_R))
				{
					LED_PORT |= _BV(LED_GREEN_R);
				}
				else
				{
					LED_PORT &= ~_BV(LED_GREEN_R);
				}
			}
			else                                      // speed is lower than position of poti
			{
				// -> speed down (turn left)
				if (bit_is_clear(LED_PORT, LED_GREEN_L))
				{
					LED_PORT |= _BV(LED_GREEN_L);
				}
				else
				{
					LED_PORT &= ~_BV(LED_GREEN_L);
				}
			}
		}
		else                                        // no use for blinking anymore, so stop timer
		{
			bLEDReload = LED_ON;
		}
	}
	/******************************************************************************/
	// Selftest
	/******************************************************************************/
	else if (bThrState >= THR_STATE_SELFTEST)     // while selftest is active show rotating LEDs
	{
		// -> fast rotation shows selftest active
		if (bit_is_set(LED_PORT, LED_GREEN_R))      // -> slow rotation shows selftest done
		{
			LED_PORT &= ~_BV(LED_GREEN_R);
			LED_PORT |=  _BV(LED_GREEN_L);
			LED_PORT &= ~_BV(LED_RED);
		}
		else if (bit_is_set(LED_PORT, LED_GREEN_L))
		{
			LED_PORT &= ~_BV(LED_GREEN_R);
			LED_PORT &= ~_BV(LED_GREEN_L);
			LED_PORT |=  _BV(LED_RED);
		}
		else
		{
			LED_PORT |=  _BV(LED_GREEN_R);
			LED_PORT &= ~_BV(LED_GREEN_L);
			LED_PORT &= ~_BV(LED_RED);
		}
	}
	/******************************************************************************/
	// dispatching
	/******************************************************************************/
	else  // show alternating blinking between red and green
	{
		if ( bit_is_set(LED_PORT, LED_GREEN_L))
		{
			LED_PORT |=  _BV(LED_RED);
			LED_PORT &= ~_BV(LED_GREEN_R) ;
			LED_PORT &= ~_BV(LED_GREEN_L) ;
		}
		else
		{
			LED_PORT &= ~_BV(LED_RED);
			LED_PORT |=  _BV(LED_GREEN_R) ;
			LED_PORT |=  _BV(LED_GREEN_L) ;
		}
	}
	return bLEDReload;
}



/******************************************************FunctionHeaderBegin******
 * FUNCTION    : KeyTimerAction
 * CREATED     : 2004-12-20
 * AUTHOR      : Olaf Funke
 * DESCRIPTION : polling of keys, the direction switch and the analog value
 *               with 2ms cycle
 *               This is called from processTimerActions()
 *******************************************************************************
 * ARGUMENTS   : void *UserPointer
 * RETURN VALUE: byte, allways KEY_POLL_TIME (2ms)
 *******************************************************FunctionHeaderEnd******/
byte KeyTimerAction( void *UserPointer)
{
  static  byte bLastEncSwitch   = 1;
  byte bActEncSwitch;
  static  byte bLastKey         = 1;
  byte bActKey;
  static  byte blastDirfSwitch   = 1; 
  byte bActDirSwitch;

/******************************************************************************/
//  keys
/******************************************************************************/
  bActKey = KEYPIN_PIN & KEYPIN_ALL ; // 0 means pressed, 1 means released

  if (bActKey != bLastKey)
  {
    bEvent      |= EVENT_KEY;
    bLastKey     = bActKey;

    bCurrentKey &= ~KEYPIN_ALL;               // clear all possible keys
    bCurrentKey |= (~bActKey) & KEYPIN_ALL;   // set relevant keys
  }

/******************************************************************************/
//  Stop button or increment button
/******************************************************************************/

  bActEncSwitch = ENC_PIN & _BV(ENC_SWITCH);

  if (bActEncSwitch != bLastEncSwitch)  // Change from 0->1 and 1->0
  {
    bEvent        |= EVENT_KEY;
    bLastEncSwitch = bActEncSwitch;

    if (bActEncSwitch)                          // 0 means switch is pressed, 1 key is releaseds
    {
      bCurrentKey &= ~Key_Stop;                 // released key stop
    }
    else
    {
      bCurrentKey |= Key_Stop;                  // pressed key stop
    }
  }

/******************************************************************************/
//  direction button
/******************************************************************************/

  if (  (bFrediVersion == FREDI_VERSION_INCREMENT_SWITCH)
     || (bFrediVersion == FREDI_VERSION_ANALOG          ))
  {
    bActDirSwitch = DIRSWITCH_PIN & _BV(DIRSWITCH);

    if (bActDirSwitch != blastDirfSwitch)         // change of direction
    {
      bEvent        |= EVENT_KEY;
      blastDirfSwitch = bActDirSwitch;

      if (bActDirSwitch)
      {
        bCurrentKey &= ~Key_Dir;
      }
      else
      {
        bCurrentKey |= Key_Dir;
      }
    }
  }

/******************************************************************************/
//  poti
/******************************************************************************/

  if (bFrediVersion == FREDI_VERSION_ANALOG)
  {
    potAdcTimerAction();
  }

/******************************************************************************/

  return KEY_POLL_TIME ;
}


/******************************************************FunctionHeaderBegin******
 * FUNCTION    : MessageTimerAction
 * CREATED     : 2005-04-29
 * AUTHOR      : Olaf Funke
 * DESCRIPTION : Timer for repeat of Messages. There are three possibilities
 *               - In state "Connected" there is a cyclic sending of speed 
 *                 message for announcing throttle is still alive
 *               - In case of dispatching there it could be that the message 
 *                 hasn't been sent out correctly. So send message again, if 
 *                 there was no reaction of further message
 *               - In state "init" or unconnected is nothing to do so stop timer
 *******************************************************************************
 * ARGUMENTS   : void *UserPointer
 * RETURN VALUE: byte
 *******************************************************FunctionHeaderEnd******/
byte MessageTimerAction( void *UserPointer)
{
	byte bRetVal = MESSAGE_TIME;

	switch (bThrState)
	{
		case THR_STATE_CONNECTED:
		sendLocoNet4BytePacket(OPC_LOCO_SPD,slotArray[slotnumber].slot,slotArray[slotnumber].spd);
		bRetVal = SPEED_TIME;
		break;
		case THR_STATE_ACQUIRE_LOCO_GET:
		case THR_STATE_ACQUIRE_LOCO_WRITE:
		case THR_STATE_RECONNECT_GET_SLOT:
		case THR_STATE_RECONNECT_WRITE:
		case THR_STATE_RECONNECT_NULL_MOVE:
		vSetState(THR_STATE_RECONNECT_GET_SLOT, &slotArray[slotnumber]);
		if (sendLocoNet4BytePacket(OPC_LOCO_ADR, slotArray[slotnumber].adr2, slotArray[slotnumber].adr) != LN_DONE)
		{
			bRetVal = MESSAGE_TIME;
		}
		else
		{
			bRetVal = RESPONSE_TIME;
		}
		break;
		case THR_STATE_UNCONNECTED_WRITE:
		{
			lnMsg SendPacket ;

			SendPacket.sd.command   = OPC_WR_SL_DATA  ; //opcode
			SendPacket.sd.mesg_size = 14              ; // length
			SendPacket.sd.slot      = slotArray[slotnumber].slot      ; // slot    2
			SendPacket.sd.stat      = slotArray[slotnumber].stat      ; // stat    3
			SendPacket.sd.adr       = slotArray[slotnumber].adr       ; // adr     4
			SendPacket.sd.spd       = slotArray[slotnumber].spd       ; // spd     5
			SendPacket.sd.dirf      = slotArray[slotnumber].dirf      ; // dirf    6
			SendPacket.sd.trk       = slotArray[slotnumber].trk       ; // trk     7
			SendPacket.sd.ss2       = slotArray[slotnumber].ss2       ; // ss2     8
			SendPacket.sd.adr2      = slotArray[slotnumber].adr2      ; // adr2    9
			SendPacket.sd.snd       = slotArray[slotnumber].snd       ; // snd    10
			SendPacket.sd.id1       = slotArray[slotnumber].id1       ; // id1    11
			SendPacket.sd.id2       = slotArray[slotnumber].id2       ; // id2    12

			LN_STATUS status = sendLocoNetPacket( &SendPacket );

			if (status != LN_DONE)
			{
				bRetVal = MESSAGE_TIME;
			}
			else
			{
				bRetVal = RESPONSE_TIME;
			}
		}
		break;
		case THR_STATE_INIT:          // stop timer, there is nothing to do
		case THR_STATE_UNCONNECTED:
		default:
		bRetVal = 0;
		break;
	}

	return bRetVal;
}


byte ReleaseStopTimerAction( void *UserPointer)
{
	bStopPressed = FALSE;
	return 0;
}


/******************************************************FunctionHeaderBegin******
 * CREATED     : 2004-12-20
 * AUTHOR      : Olaf Funke
 * DESCRIPTION : Set hardware for the keys and LEDs to defined state
 *******************************************************************************
 * ARGUMENTS   : none
 * RETURN VALUE: none
 * NOTES       :   -
 *******************************************************FunctionHeaderEnd******/
void initKeys( void )
{
  /***************************************/
  //  init encoder or poti
  /***************************************/

  if (bFrediVersion == FREDI_VERSION_ANALOG)
  {
    potAdcInit();
  }
  else // FREDI_VERSION_INCREMENT or FREDI_VERSION_INCREMENT_SWITCH
  {
    potAdcPowerOff();
		bStopPressed = FALSE;
		addTimerAction(&ReleaseStopTimer, 0, ReleaseStopTimerAction, 0, TIMER_SLOW ) ;
    // set data direction register for encoder
    ENC_DDR &= ~( _BV(ENC_BIT_0) | _BV(ENC_BIT_1) ) ;

    // Enable the pull-ups
    ENC_PORT |= ( _BV(ENC_BIT_0) | _BV(ENC_BIT_1) ) ;

    addTimerAction(&IncrementTimer, 0, IncrementTimerAction, 0, TIMER_FAST) ;

    // detect actual state of pin to set next interrupt edge
    ENC_ISC_REG |= _BV(ENC_ISC_BIT1);

    if (bit_is_set(ENC_PIN, ENC_BIT_1))
    { // set falling edge
      ENC_ISC_REG &= ~_BV(ENC_ISC_BIT0);
    }
    else
    { // set rising edge
      ENC_ISC_REG |= _BV(ENC_ISC_BIT0);
    }

    // clear pending interrupt
    ENC_EIRF_REG |= _BV(ENC_EIRF_BIT);
    // set interrupt activ
    ENC_EIRE_REG |= _BV(ENC_EIRE_BIT);
  }

  /***************************************/
  //  init keys
  /***************************************/

  // set data direction register for encoder
  ENC_DDR &= ~( _BV(ENC_SWITCH) ) ;

  // Enable the pull-ups
  ENC_PORT |= ( _BV(ENC_SWITCH) ) ;

  // set data direction register for encoder
  ENC_DDR &= ~( _BV(ENC_SWITCH) ) ;

  // Enable the pull-ups
  ENC_PORT |= ( _BV(ENC_SWITCH) ) ;

  // set data direction register for keys
 // KEYPIN_DDR  &= ~KEYPIN_ALL ;
  DDRC		&= ~(_BV(FUNKEY1));
  DDRB		&= ~(_BV(FUNKEY2));
  DDRB		&= ~(_BV(FUNKEY3));
  DDRA		&= ~(_BV(FUNKEY4));
  //-------------------erweiterte Funktionstasten
  DDRD		&= ~(_BV(ERW_FUNKEY1));
//  DDRD		&= ~(_BV(ERW_FUNKEY2));
  DDRD		&= ~(_BV(ERW_FUNKEY3));
  DDRD		&= ~(_BV(ERW_FUNKEY4));
  // Enable the pull-ups
  KEYPIN_PORT |=  KEYPIN_ALL ;

  if (  (bFrediVersion == FREDI_VERSION_INCREMENT_SWITCH)
        || (bFrediVersion == FREDI_VERSION_ANALOG          ))
  {
    // set data direction register for direction switch
   // DIRSWITCH_DDR   &= ~( _BV(DIRSWITCH) );
		DDRC		&= ~(_BV(DIRKEY1));		//setzen der Richtungstasten 1-4 19.05.2017
		DDRA		&= ~(_BV(DIRKEY2));
		DDRC		&= ~(_BV(DIRKEY3));
		DDRC		&= ~(_BV(DIRKEY4));
		
    // Enable the pull-up
    DIRSWITCH_PORT  |=  ( _BV(DIRSWITCH) );
	//????????????? Widerst�nde auch setzen?
  }

  addTimerAction(&KeyTimer, KEY_POLL_TIME, KeyTimerAction, 0, TIMER_FAST ) ;

  /***************************************/
  //  init LEDs
  /***************************************/

  DDRA	   |= (_BV(LED1));
  PORTA		&= ~(_BV(LED1));
//  LED_DDR  |=  _BV(LED_GREEN_L); 
//  LED_PORT &= ~_BV(LED_GREEN_L); 

  DDRA	   |= (_BV(LED2));
  PORTA	   &= ~(_BV(LED2));
  //LED_DDR  |=  _BV(LED_GREEN_R); 
  //LED_PORT &= ~_BV(LED_GREEN_R); 

  DDRC	   |= (_BV(LED3));
  PORTC	   &= ~(_BV(LED3));
  //LED_DDR  |=  _BV(LED_RED); 
  //LED_PORT |= _BV(LED_RED);       // set red LED at startup
  
  DDRC	   |= (_BV(LED4));
  PORTC	   &= ~(_BV(LED4));

 // addTimerAction(&LEDTimer, LED_BLINK_TIME, LEDTimerAction, 0, TIMER_SLOW ) ;
}






/******************************************************FunctionHeaderBegin******
 * CREATED     : 2004-12-20
 * AUTHOR      : Olaf Funke
 * DESCRIPTION : Every change of state is handed by this function. A change of 
 *               state is responsible for changing the LEDs and starting the
 *               blinking timer
 *******************************************************************************
 * ARGUMENTS   : byte bState, new state to set, could be the same as before
 * RETURN VALUE: none
 * NOTES       :   -
 *******************************************************FunctionHeaderEnd******/
void vSetState( byte bState, rwSlotDataMsg *currentSlot)
{
	bThrState = bState;
}


void initSlots (rwSlotDataMsg *slotArray,leddata *array) {
	
  slotArray[0].command   = OPC_WR_SL_DATA;
  slotArray[0].mesg_size = 14;
  slotArray[0].slot      = 0;                        /* slot number for this request                         */
  slotArray[0].stat      = 0;                        /* slot status                                          */
  slotArray[0].epromAdr = 2;
  slotArray[0].adr       = 0;                        /* loco address                                         */
  slotArray[0].spd       = 0;                        /* command speed                                        */
  slotArray[0].dirf      = 0;                        /* direction and F0-F4 bits                             */
  slotArray[0].trk       = 0;                        /* track status                                         */
  slotArray[0].ss2       = 0;                        /* slot status 2 (tells how to use ID1/ID2 & ADV Consist*/
  slotArray[0].epromAdr = 3;
  slotArray[0].adr2      = 0;                        /* loco address high                                    */
  slotArray[0].snd       = 0;                        /* Sound 1-4 / F5-F8                                    */
  slotArray[0].dirKey	  = DIRKEY1;
  slotArray[0].dirKeyStatus	  = 0;
  slotArray[0].funKey	  = FUNKEY1;
  slotArray[0].funKeyStatus	  = 0;
  slotArray[0].funkeyTimeOut = 0;
  slotArray[0].ledNumber = 1;
  slotArray[0].resetKey.adress = &PINC;
  slotArray[0].resetKey.bit = FUNKEY1;
  slotArray[0].resetKey.counter = 0;
 
 
  //-----------------------------------------------------------------RSLOTZWEI---------------------------------
  
  slotArray[1].command   = OPC_WR_SL_DATA;
  slotArray[1].mesg_size = 14;
  slotArray[1].slot      = 0;                        /* slot number for this request                         */
  slotArray[1].stat      = 0;                        /* slot status                                          */
  slotArray[1].epromAdr = 4;
  slotArray[1].adr       = 0;                        /* loco address                                         */
  slotArray[1].spd       = 0;                        /* command speed                                        */
  slotArray[1].dirf      = 0;                        /* direction and F0-F4 bits                             */
  slotArray[1].trk       = 0;                        /* track status                                         */
  slotArray[1].ss2       = 0;                        /* slot status 2 (tells how to use ID1/ID2 & ADV Consist*/
  slotArray[1].epromAdr = 5;
  slotArray[1].adr2      = 0;                        /* loco address high                                    */
  slotArray[1].snd       = 0;                        /* Sound 1-4 / F5-F8                                    */
  slotArray[1].dirKey	  = DIRKEY2;
  slotArray[1].dirKeyStatus	  = 0;
  slotArray[1].funKey	  = FUNKEY2;
  slotArray[1].funKeyStatus	  = 0;
  slotArray[1].funkeyTimeOut = 0;
  slotArray[1].ledNumber = 2;
  slotArray[1].resetKey.adress = &PINB;
  slotArray[1].resetKey.bit = FUNKEY2;
  slotArray[1].resetKey.counter = 0;
 
 
  //----------------------------------------------------------RSLOTDREI-----------------------------------------------
  slotArray[2].command   = OPC_WR_SL_DATA;
  slotArray[2].mesg_size = 14;
  slotArray[2].slot      = 0;                        /* slot number for this request                         */
  slotArray[2].stat      = 0;                        /* slot status                                          */
  slotArray[2].epromAdr = 6;
  slotArray[2].adr       = 0;                        /* loco address                                         */
  slotArray[2].spd       = 0;                        /* command speed                                        */
  slotArray[2].dirf      = 0;                        /* direction and F0-F4 bits                             */
  slotArray[2].trk       = 0;                        /* track status                                         */
  slotArray[2].ss2       = 0;                        /* slot status 2 (tells how to use ID1/ID2 & ADV Consist*/
  slotArray[2].epromAdr = 7;
  slotArray[2].adr2      = 0;                        /* loco address high                                    */
  slotArray[2].snd       = 0;                        /* Sound 1-4 / F5-F8                                    */
  slotArray[2].dirKey	  = DIRKEY3;
  slotArray[2].dirKeyStatus	  = 0;
  slotArray[2].funKey	  = FUNKEY3;
  slotArray[2].funKeyStatus	  = 0;
  slotArray[2].funkeyTimeOut = 0;
  slotArray[2].ledNumber = 3;
  slotArray[2].resetKey.adress = &PINC;
  slotArray[2].resetKey.bit = FUNKEY3;
  slotArray[2].resetKey.counter = 0;


  //-----------------------------------------------------------------RSLOTVIER---------------------------------
  
  slotArray[3].command   = OPC_WR_SL_DATA;
  slotArray[3].mesg_size = 14;
  slotArray[3].slot      = 0;                        /* slot number for this request                         */
  slotArray[3].stat      = 0;                        /* slot status                                          */
  slotArray[3].epromAdr = 8;
  slotArray[3].adr       = 0;                        /* loco address                                         */
  slotArray[3].spd       = 0;                        /* command speed                                        */
  slotArray[3].dirf      = 0;                        /* direction and F0-F4 bits                             */
  slotArray[3].trk       = 0;                        /* track status                                         */
  slotArray[3].ss2       = 0;                        /* slot status 2 (tells how to use ID1/ID2 & ADV Consist*/
  slotArray[3].epromAdr = 9;
  slotArray[3].adr2      = 0;                        /* loco address high                                    */
  slotArray[3].snd       = 0;                        /* Sound 1-4 / F5-F8                                    */
  slotArray[3].dirKey	  = DIRKEY4;
  slotArray[3].dirKeyStatus	  = 0;
  slotArray[3].funKey	  = FUNKEY4;
  slotArray[3].funKeyStatus	  = 0;
  slotArray[3].funkeyTimeOut = 0;
  slotArray[3].ledNumber = 4;
  slotArray[3].resetKey.adress = &PINA;
  slotArray[3].resetKey.bit = FUNKEY4;
  slotArray[3].resetKey.counter = 0;
}

void initLEDS (leddata *array) {
	array[0].ledAdress = &PORTA;
	array[0].bitToSet = LED1;
	array[0].polung = 1;
	
	array[1].ledAdress = &PORTA;
	array[1].bitToSet = LED2;
	array[1].polung = 1;
	
	array[2].ledAdress = &PORTC;
	array[2].bitToSet = LED3;
	array[2].polung = 1;
	
	array[3].ledAdress = &PORTC;
	array[3].bitToSet = LED4;
	array[3].polung = 0;
	
}




/******************************************************FunctionHeaderBegin******
 * CREATED     : 2004-12-20
 * AUTHOR      : Olaf Funke
 * DESCRIPTION :   -
 *******************************************************************************
 * ARGUMENTS   : none
 * RETURN VALUE: int, never reached, cause of endless loop
 * NOTES       :   -
 *******************************************************FunctionHeaderEnd******/
int main(void)
{
  RESET_RESET_SOURCE(); // Clear Reset Status Register (WDRF,BORF,EXTRF,PORF)

  byte bCount = 0;
  bFrediVersion = FREDI_VERSION_ANALOG;
  setPullUps();
  /***************************************/
  //  init analog input for getting 
  //  FrediVersion
  /***************************************/

/*	 DDRC  &= ~_BV(DDC5); // set version detector to tristate to get kind of fredi
	 PORTC |=  _BV(PC5);

  if (bit_is_set(PINC, PINC5))
  {
    bFrediVersion = FREDI_VERSION_ANALOG;
  }
  else
  {
    bFrediVersion = FREDI_VERSION_INCREMENT;
  } */
  
  /***************************************/
  //  init throttle slot
  /***************************************/

  bSpdCnt = 0;
  
	
		
initLEDS(ledArray);
setLEDasOutput();
initSlots(slotArray, ledArray);			// initialisierung der vier rSlots.


	
  
  

slotArray[0].id1   = eeprom_read_byte(&abEEPROM[0]); // get ID from EEPROM
slotArray[0].id2   = eeprom_read_byte(&abEEPROM[1]);

/*  if ((slotArray[0].id1 & 0x80) || (slotArray[0].id2 & 0x80))
  { // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // if no slot ID was programmed, you get the ID "0xff 0xff"
		// or if an unguilty ID was programmed
    // stop program at this point and switch all leds on
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    LED_DDR  |= _BV(LED_GREEN_L); 
    LED_PORT |= _BV(LED_GREEN_L); 
    LED_DDR  |= _BV(LED_GREEN_R); 
    LED_PORT |= _BV(LED_GREEN_R); 
    LED_DDR  |= _BV(LED_RED); 
    LED_PORT |= _BV(LED_RED);
    while (1);
  } */
  
  #ifdef LOCONET_LEVEL_TEST
    LED_DDR  |= _BV(LED_GREEN_L);
    LED_PORT |= _BV(LED_GREEN_L);
    LED_DDR  |= _BV(LED_GREEN_R);
    LED_PORT |= _BV(LED_GREEN_R);
    LED_DDR  |= _BV(LED_RED);
    LED_PORT |= _BV(LED_RED);
    ACSR  = (0<<ACD)  | (0<<ACBG) | (0<<ACO)   | (0<<ACI)
          | (0<<ACIE) | (0<<ACIC) | (0<<ACIS1) | (0<<ACIS0);
    #if defined(__AVR_ATmega48__)  | defined(__AVR_ATmega48A__)  \
      | defined(__AVR_ATmega48P__) | defined(__AVR_ATmega88__)   \
      | defined(__AVR_ATmega88A__) | defined(__AVR_ATmega88P__)  \
      | defined(__AVR_ATmega168__) | defined(__AVR_ATmega168A__) \
      | defined(__AVR_ATmega168P__)| defined(__AVR_ATmega328__)  \
      | defined(__AVR_ATmega328P__)
      //AIN1, AIN0 digital input disable:
      // disable digital input buffer on the pins to reduce power consumption
      DIDR1 |= (1<<AIN1D) | (1<<AIN0D);
    #endif
    while (1) {
      if( bit_is_set(ACSR,ACO) ) {
        LED_PORT |= _BV(LED_GREEN_L);
        LED_PORT |= _BV(LED_GREEN_R);
        LED_PORT |= _BV(LED_RED);
      } else {
        LED_PORT &= ~_BV(LED_GREEN_L);
        LED_PORT &= ~_BV(LED_GREEN_R);
        LED_PORT &= ~_BV(LED_RED);
      }
    }
  #endif

  /***************************************/
  //  init loconet
  /***************************************/

  initLocoNet(&RxBuffer) ;

  /***************************************/
  //  init keys and timer
  /***************************************/

 
  initTimer();
 
  

  /***************************************/
  //  set state and start interrupts
  /***************************************/

  sei();

  //if (bThrState < THR_STATE_SELFTEST)
  //{
     while (bit_is_clear(ACSR, ACO))     // wait for start of loconet
    {
      processTimerActions();
    }
    // loconet is available, now
    for (bCount =0;bCount < 50;bCount++)
    {
      delayTimer( 10 );                 // wait a little bit longer
      processTimerActions();
    }

    if (slotArray[0].adr)                      // wait for a pseudo random time
    {
      delayTimer(slotArray[0].adr);
    }

    if (slotArray[0].adr2)
    {
      delayTimer(slotArray[0].adr2);
    }

    
  //}
  //else
  //{
    //sendLocoNetFredCd( bCount );
  //}  

/******************************************************************************/
// main endless loop 
/******************************************************************************/
//rectry
	for (int i = 0; i < NUMBER_OF_SLOTS; i++) {
		setLEDStatus(&ledArray[i], 1);
	}
	_delay_1500ms();
	
	for (int i = 0; i < NUMBER_OF_SLOTS; i++) {
		if (eeprom_read_byte(&abEEPROM[slotArray[i].epromAdr]) || eeprom_read_byte(&abEEPROM[slotArray[i].epromAdr2])) {
			slotArray[i].adr = eeprom_read_byte(&abEEPROM[slotArray[i].epromAdr]);
			slotArray[i].adr2 = eeprom_read_byte(&abEEPROM[slotArray[i].epromAdr2]);
			vSetState(THR_STATE_RECONNECT_GET_SLOT, &slotArray[i]);
			while (slotArray[i].dirfSendStatus != LN_DONE) {
				slotArray[i].dirfSendStatus = sendLocoNetAdr(&slotArray[i]);
				toggleLEDStatus(&ledArray[2]);
			}

			while (!(slotArray[i].slot) && retryRxCounter < 500000L ) {
				
				vProcessRxLoconetMessage(&slotArray[i]);
				++retryRxCounter;
			}
			retryRxCounter = 0;
			if (slotArray[i].slot == 0) {
				eeprom_write_byte(&abEEPROM[slotArray[i].epromAdr], (uint8_t) (0x00));
				eeprom_write_byte(&abEEPROM[slotArray[i].epromAdr2], (uint8_t) (0x00));
				slotArray[i].adr = 0x00;
				slotArray[i].adr2 = 0x00;
			}
		}
	}			
		  //alle LEDS ausschalten
		/*	PORTA &= ~(1<<LED1);
			PORTA &= ~(1<<LED2);
			PORTC &= ~(1<<LED3);
			PORTC &= ~(1<<LED4); */
		// alle LED anschalten
			PORTA |= (1<<LED1);
			PORTA |= (1<<LED2);
			PORTC |= (1<<LED3);
			disableLED4();
			
		  ADCSRA = 0<< ADEN;		//ADC ausschalten
		  
	
	
	
	disableLED1();
	_delay_1500ms();
	enableLED1();
	_delay_1500ms();
	shiftStatustwo[1] = 0;
//-----------------------------------------------------ADC vorbereiten------------------------------------
neuerADCwert[0] = 15;
ADCSRA |= (1<<ADEN | 1<< REFS0 | 0 << REFS1 |1<<ADPS0|1<<ADPS1|1<<ADPS2);


cbi(ADMUX,MUX0);								//setze ADMUX Channel 4
cbi(ADMUX,MUX1);
sbi(ADMUX,MUX2);
cbi(ADMUX,MUX3);
cbi(ADMUX,MUX4);
sbi(ADCSRA,ADSC);								//starte Messung
_delay_ms(500);
setLEDStatus(&ledArray[0], 0);

	//slotArray[0].slot = 1;
	//slotArray[1].slot = 2;
	//slotArray[2].slot = 3;
	//slotArray[3].slot = 4;

//------------------------------------------------ADC vorbereiten ENDE-----------------------------------------
  while (1)
  {
	
	
	for (int i = 0; i < NUMBER_OF_SLOTS; i++) {
		 
		//Main loop final
		ProcessDirKeyInput8Streak(PINC, DIRKEY1,  1, &slotArray[0]);
		ProcessDirKeyInput8Streak(PINA, DIRKEY2,  2, &slotArray[1]);
		ProcessDirKeyInput8Streak(PINC, DIRKEY3,  3, &slotArray[2]);
		ProcessDirKeyInput8Streak(PINC, DIRKEY4,  4, &slotArray[3]);
		
		
		
		ProcessShiftKeyInput8Streak(PINC, FUNKEY1, 1);
		ProcessShiftKeyInput8Streak(PINB, FUNKEY2, 2);
		ProcessShiftKeyInput8Streak(PINC, FUNKEY3, 3);
		ProcessShiftKeyInput8Streak(PINA, FUNKEY4, 4);
		if (shiftStatustwo[0] - 1 == i) {
			if (ProcessKeyInputKeepPressed(&slotArray[i].resetKey, 5000)) {
				vSetUnconnected(&slotArray[i]);
				toggleLEDStatus(&ledArray[i]);
				shiftStatustwo[0] = 0;
			}
			ProcessFunKeyInput8Streak(PIND, ERW_FUNKEY1, 1, &slotArray[i]);
			ProcessFunKeyInput8Streak(PIND, ERW_FUNKEY2, 2, &slotArray[i]);
			ProcessFunKeyInput8Streak(PIND, ERW_FUNKEY3, 3, &slotArray[i]);
			ProcessFunKeyInput8Streak(PIND, ERW_FUNKEY4, 4, &slotArray[i]);
		}
		processADC();
		vProcessRxLoconetMessage(&slotArray[i]);
		
		
	    //Wenn kein Zug zugewiesen1 ist
		if (shiftStatustwo[0] - 1 == i && slotArray[i].adr == 0 && slotArray[i].adr2 == 0) {
			slotArray[i].spd ^= slotArray[i].spd;
			slotArray[i].dirf ^= slotArray[i].dirf;
			slotArray[i].snd ^= slotArray[i].snd;
			vSetState(THR_STATE_ACQUIRE_LOCO_GET,&slotArray[i]);
			sendLocoNetMove(0, 0, &slotArray[i]);
			while (slotArray[i].adr == 0 && retryRxCounter < 20000L) {
				vProcessRxLoconetMessage(&slotArray[i]);
				++retryRxCounter;
				//toggleLED(i + 1);
			}
			retryRxCounter = 0;
			shiftStatustwo[0] = 0;
		} else if (/* slotArray[i].adr || slotArray[i].adr2 */ 1){
			transmitInputLoco(&slotArray[i], i);
		}
		if (slotArray[i].dirf & 0x20 || (slotArray[i].adr == 0 && slotArray[i].adr2 == 0)) {
			setLEDStatus(&ledArray[i], 0);
			
			} else {
			
			setLEDStatus(&ledArray[i], 1);
		}
		
		
		
	}  //end of for
  } // end of while(1)
} // end of main

void processADC(void){
	if(bit_is_clear(ADCSRA,ADSC)) {
		
		//neuerADCwert[0] = ADCW / 8;
		//sbi(ADCSRA,ADSC);
		//if(neuerADCwert[0] != slotArray[0].spd || 1) {
		//slotArray[0].spd = neuerADCwert[0];
		//sendLocoNetSpd(&slotArray[0]);
		//}
		
		
		
		if(adcCount != 0){
			
			neuerADCwert[adcCount-1] = ADCW / 8;
			
			if(abs(neuerADCwert[adcCount-1] - slotArray[adcCount-1].spd) > 2 || neuerADCwert[adcCount-1] < 2){
				if (neuerADCwert[adcCount-1] != slotArray[adcCount-1].spd) {
					slotArray[adcCount - 1].spdSendStatus = LN_NEW_DATA;
				}
				slotArray[adcCount-1].spd = neuerADCwert[adcCount-1];
				//sendLocoNetSpd(&slotArray[adcCount-1]);
			}
			
		}
		else {
			neuerADCwert[3] = ADCW / 8;
			
			if(abs(neuerADCwert[3] - slotArray[3].spd) > 2 || neuerADCwert[3] < 2) {
				if (neuerADCwert[3] != slotArray[3].spd) {
					slotArray[3].spdSendStatus = LN_NEW_DATA;
				}
				slotArray[3].spd = neuerADCwert[3];
				//sendLocoNetSpd(&slotArray[3]);

			}
		}
		
		if (adcCount == 0) {
			cbi(ADMUX,MUX0);								// setze ADMUX Channel 6
			sbi(ADMUX,MUX1);
			sbi(ADMUX,MUX2);
			cbi(ADMUX,MUX3);
			cbi(ADMUX,MUX4);
			
			sbi(ADCSRA,ADSC);								//starte Messung
			adcCount = 1;
		}
		else if (adcCount == 1) {
			sbi(ADMUX,MUX0);								// setze ADMUX Channel 7
			sbi(ADMUX,MUX1);
			sbi(ADMUX,MUX2);
			cbi(ADMUX,MUX3);
			cbi(ADMUX,MUX4);
			
			sbi(ADCSRA,ADSC);								//starte Messung
			adcCount = 2;
		}
		else if (adcCount == 2) {
			sbi(ADMUX,MUX0);								// setze ADMUX Channel 5
			cbi(ADMUX,MUX1);
			sbi(ADMUX,MUX2);
			cbi(ADMUX,MUX3);
			cbi(ADMUX,MUX4);
			
			sbi(ADCSRA,ADSC);								//starte Messung
			adcCount = 3;
		}
		else if (adcCount == 3) {
			cbi(ADMUX,MUX0);								// setze ADMUX Channel 4
			cbi(ADMUX,MUX1);
			sbi(ADMUX,MUX2);
			cbi(ADMUX,MUX3);
			cbi(ADMUX,MUX4);
			
			sbi(ADCSRA,ADSC);								//starte Messung
			adcCount = 0;
		}
	}
}

void entprellen_druecken (byte new_value, byte val) {
	if ((new_value != val) && (entprell < 60)) { // 60x gleiches signal hinterneinander
		entprell++;
	}
	else {
		entprell = 0;
		value = new_value;
		if (value == 1) {
			old_value = 1;
		}
	}
}


/******************************************************FunctionHeaderBegin******
 * CREATED     : 2005-01-29
 * AUTHOR      : Olaf Funke
 * DESCRIPTION :   -
 *******************************************************************************
 * ARGUMENTS   : none
 * RETURN VALUE: none
 * NOTES       :   -
 *******************************************************FunctionHeaderEnd******/
void vProcessRxLoconetMessage(rwSlotDataMsg *currentSlot)
{
  RxPacket = recvLocoNetPacket();

  if (RxPacket)
  {
    switch (RxPacket->data[0]) // OP_CODE
    {
/***************************************/
//  Slot Read Data
/***************************************/
    case OPC_SL_RD_DATA:
      switch (bThrState)
      {
      case THR_STATE_ACQUIRE_LOCO_GET: // response of Dispatch Get
        {
          if ((RxPacket->data[3] & LOCO_IDLE) == LOCO_IDLE)
          {
            vCopySlotFromRxPacket(currentSlot);
            vSetState(THR_STATE_ACQUIRE_LOCO_WRITE, currentSlot);
            sendLocoNetWriteSlotData(currentSlot);
          }
        }
        break;
      case THR_STATE_RECONNECT_GET_SLOT: // response of Get Slot By Adress
        {
          if (  (currentSlot->adr  == RxPacket->data[4])
             && (currentSlot->adr2 == RxPacket->data[9]))
          { // slot not changed and in use , so we can use this slot further on
            if ((RxPacket->data[3] & LOCO_IN_USE) == LOCO_IN_USE)
            {
              if (  (  (currentSlot->id1 == RxPacket->data[11]) && (currentSlot->id2 == RxPacket->data[12]))
                 || (  (0         == RxPacket->data[11]) && (0         == RxPacket->data[12])))
              {
                vCopySlotFromRxPacket(currentSlot);
                vSetState(THR_STATE_RECONNECT_WRITE, currentSlot);
                sendLocoNetWriteSlotData(currentSlot);
              }
              else
              {
                vSetUnconnected(currentSlot);
              }
            }
            else
            {
              vSetState(THR_STATE_RECONNECT_NULL_MOVE, currentSlot);
              sendLocoNetMove(RxPacket->data[2], RxPacket->data[2], currentSlot);
            }
          }
        }
        break;
      case THR_STATE_RECONNECT_NULL_MOVE:
        {
          if (  (currentSlot->adr  == RxPacket->data[4]) 
             && (currentSlot->adr2 == RxPacket->data[9]))
          { // slot not changed and in use , so we can use this slot further on
            if ((RxPacket->data[3] & LOCO_IN_USE) == LOCO_IN_USE)
            {
              vCopySlotFromRxPacket(currentSlot);
              vSetState(THR_STATE_RECONNECT_WRITE, currentSlot);
              sendLocoNetWriteSlotData(currentSlot);
            }
            else
            {
              vSetUnconnected(currentSlot);
            }
          }
        }
        break;
      }
      break;
/***************************************/
//  Long Acknowledge
/***************************************/
    case OPC_LONG_ACK:
      switch (bThrState)
      {
      case THR_STATE_UNCONNECTED_WRITE:
        if (RxPacket->data[1] == (OPC_WR_SL_DATA & 0x7f))
        {
          vSetUnconnected(currentSlot);
        }
        break;
      case THR_STATE_RECONNECT_WRITE: // response of Get Slot By Adress
      case THR_STATE_ACQUIRE_LOCO_WRITE:
        if (RxPacket->data[1] == (OPC_WR_SL_DATA & 0x7f))
        {
          //eeprom_write_byte(&abEEPROM[EEPROM_ADR_LOCO_LB_1],  (uint8_t) (currentSlot->adr));
          //eeprom_write_byte(&abEEPROM[EEPROM_ADR_LOCO_HB_1],  (uint8_t) (currentSlot->adr2));
          

          vSetState(THR_STATE_CONNECTED, currentSlot);
        }
        break;
      case THR_STATE_ACQUIRE_LOCO_GET: // response of Dispatch Get
        vSetUnconnected(currentSlot);
        break;
      }
      break;
/***************************************/
//  Set Slot Speed
/***************************************/
    case OPC_LOCO_SPD:
      if (  (bThrState         == THR_STATE_CONNECTED)
         && (RxPacket->data[1] == currentSlot->slot         ))
      {
        currentSlot->spd = RxPacket->data[2];
        
        GET_SPDCNT_BY_SLOTSPD(currentSlot);
      }
      break;
/***************************************/
//  Set Slot Direction and F0 to F4
/***************************************/
    case OPC_LOCO_DIRF:
      if (  (bThrState         == THR_STATE_CONNECTED) && (RxPacket->data[1] == currentSlot->slot         )) {

        if (  (bFrediVersion == FREDI_VERSION_INCREMENT_SWITCH)
              || (bFrediVersion == FREDI_VERSION_ANALOG          ))
        {
          // if switch is not in direction of direction flag, so change 
          // directionflag to synchronize the direction
          if ( (currentSlot->dirf & 0x20) != (RxPacket->data[2] & 0x20))
          {
            currentSlot->dirf &= 0x20;                       // get direction of fredi
            currentSlot->dirf |= (RxPacket->data[2] & ~0x20); // and add F0..F4
            sendLocoNetDirf(currentSlot);
		  }
          else
          {
            currentSlot->dirf = RxPacket->data[2];          // direction is equal, so take F0..F4
          }
        }
        else
        { // take direction as new one
          currentSlot->dirf = RxPacket->data[2];
        }

        if (currentSlot->dirf & 0x20)
        {
			
         // LED_PORT &= ~_BV(LED_GREEN_R);			//Anpassen an LED.
         // LED_PORT |=  _BV(LED_GREEN_L);			//R�ckw�rts
		  //currentSlot->ledPort &= ~_BV(currentSlot->ledAdr);
		   
        }
        else
        {
          //LED_PORT &= ~_BV(LED_GREEN_L);			//Vorw�rts   15.05.2017
          //LED_PORT |=  _BV(LED_GREEN_R);
		  //currentSlot->ledPort |= _BV(currentSlot->ledAdr);
        }
      }
      break;
/***************************************/
//  Set Slot Sound Functions, 
/***************************************/
    case OPC_LOCO_SND:
      if (  (bThrState         == THR_STATE_CONNECTED)
         && (RxPacket->data[1] == currentSlot->slot         ))
      {
        currentSlot->snd = RxPacket->data[2];
      }
      break;
/***************************************/
//  All other, not used at the moment
/***************************************/
    default:
      break;
    }

    RxPacket = 0;  
  }
}


/******************************************************FunctionHeaderBegin******
 * CREATED     : 2005-01-29
 * AUTHOR      : Olaf Funke
 * DESCRIPTION :   -
 *******************************************************************************
 * ARGUMENTS   : none
 * RETURN VALUE: none
 * NOTES       :   -
 *
 * Bug: if you set shift and another key and you release the shift key first
 *      the remaining key is executed.
 *      So press shift, then press Fx key and release key Fx first before 
 *      releasing the shift key
 *******************************************************FunctionHeaderEnd******/
void vProcessKey(rwSlotDataMsg *currentSlot)
{
  if (bEvent & EVENT_KEY) //event_key = 0x01
  {
    static byte bLastCurrentkey = 0;
    byte bSet;
    bEvent &= ~EVENT_KEY;

    if (bThrState < THR_STATE_SELFTEST)
    {
      if ((bLastCurrentkey & Key_SHIFT) && ( !(bCurrentKey & Key_SHIFT)))
      { // the changing key was a release of shift -> no action
      }
			else if ((bLastCurrentkey & Key_Stop) && ( !(bCurrentKey & Key_Stop)))
			{
				resetTimerAction(&ReleaseStopTimer, RELEASE_STOP_TIME); 
			}
      //else if ((bLastCurrentkey & Key_Dir) != (bCurrentKey & Key_Dir))			ge�ndert 16.05.2017
      else if ((bLastCurrentkey & currentSlot->dirKey) != (bCurrentKey & currentSlot->dirKey))
      { // dir switch changed
        if (  (bThrState == THR_STATE_CONNECTED)
           && (  (bFrediVersion == FREDI_VERSION_ANALOG)
              || (bFrediVersion == FREDI_VERSION_INCREMENT_SWITCH)))
        {
          sEncDir = 0;
          bSpdCnt = 0;

          if (  (bFrediVersion == FREDI_VERSION_ANALOG)
                && (currentSlot->spd > 1))
          {
            fSetSpeed = FALSE;
            vSetState(THR_STATE_CONNECTED, currentSlot);
          }

          currentSlot->spd = 1; // Not stop

          sendLocoNetSpd(currentSlot);

          //if (bCurrentKey & Key_Dir)
          if (bCurrentKey & currentSlot->dirKey)
          { // dir switch was pressed
            currentSlot->dirf |= 0x20;
			//currentSlot->dirKey |= 0x20;
          //  LED_PORT   &= ~_BV(LED_GREEN_L);					ge�ndert 15.05.2017
          //  LED_PORT   |=  _BV(LED_GREEN_R);
			//currentSlot->ledPort |= _BV(currentSlot->ledAdr);
          }
          else
          { // dir switch was released
            currentSlot->dirf &= ~0x20;
		   //currentSlot->dirKey &= ~0x20;
          //  LED_PORT &= ~_BV(LED_GREEN_R);
          //  LED_PORT |=  _BV(LED_GREEN_L); 
			//currentSlot->ledPort &= ~_BV(currentSlot->ledAdr);
          }
          sendLocoNetDirf(currentSlot);

          // Fredi is connected, so this causes an sendLocoNetSpd after 100ms
          // it seems to be the last sendLocoNetSpd is ignored by intellibox in some cases
          resetTimerAction(&MessageTimer, 1); 
        }
      }
      else
      {
        if (bThrState == THR_STATE_CONNECTED)
        {
          if (bCurrentKey & Key_SHIFT)
          {
            bSet = currentSlot->snd; 

            //switch (bCurrentKey & ~Key_Dir)
            switch (bCurrentKey & ~currentSlot->dirKey)   // ge�ndert 15.05.2017
            {
            case (Key_Stop | Key_SHIFT): // undispatch				?????????????
              currentSlot->stat = 0x20;

              vSetState(THR_STATE_UNCONNECTED_WRITE, currentSlot);

              sendLocoNetWriteSlotData(currentSlot);
              break;
            case Key_F5:  bSet ^= 0x01; break;
            case Key_F6:  bSet ^= 0x02; break;
            case Key_F7:  bSet ^= 0x04; break;
            case Key_F8:  bSet ^= 0x08; break;
            default:                    break;
            }

            if (bSet != currentSlot->snd)
            {
              currentSlot->snd = bSet; 
              sendLocoNetSnd(currentSlot);
            }
          }
          else
          {
            bSet = currentSlot->dirf; 

            //switch (bCurrentKey & ~Key_Dir)
            switch (bCurrentKey & ~currentSlot->dirKey) //  ge�ndert 15.05.2017
            {
            case Key_Stop: // increment pushbutton or extra button on analog fredi
              sEncDir = 0;
              bSpdCnt = 0;

              if (  (bFrediVersion == FREDI_VERSION_ANALOG)
                 && (currentSlot->spd > 1))
              {
                fSetSpeed = FALSE;                // show blinking LED
                vSetState(THR_STATE_CONNECTED, currentSlot);
              }
							else
							{
								bStopPressed = TRUE;
							}

              if (currentSlot->spd > 1)
							{
								currentSlot->spd = 1;                      // Emergency stop
							}
							else
							{
								currentSlot->spd = 0;                      // Normal stop
							}

              sendLocoNetSpd(currentSlot);

              if (bFrediVersion == FREDI_VERSION_INCREMENT) // invert direction
              {
								if (bSet & 0x20)
								{
									bSet &= ~0x20;

									LED_PORT &= ~_BV(LED_GREEN_R);
									LED_PORT |=  _BV(LED_GREEN_L);     // show left direction
								}
								else
								{
									bSet |= 0x20;

									LED_PORT &= ~_BV(LED_GREEN_L);     // show right direction
									LED_PORT |=  _BV(LED_GREEN_R);
								}
              }
              break;
            case Key_F0:  bSet ^= 0x10; break;
            case Key_F1:  bSet ^= 0x01; break;
            case Key_F2:  bSet ^= 0x02; break;
            case Key_F3:  bSet ^= 0x04; break;
            case Key_F4:  bSet ^= 0x08; break;
            default:                    break;
            }

            if (bSet != currentSlot->dirf)
            {
              currentSlot->dirf = bSet; 
              sendLocoNetDirf(currentSlot);
            }
          } // end of else if(bCurrentKey & Key_SHIFT)
        } // end of if(bThrState == THR_STATE_CONNECTED)
        else if (bThrState == THR_STATE_UNCONNECTED)
        {
          //if ((bCurrentKey & ~Key_Dir) == (Key_Stop | Key_SHIFT))   // try to dispatch
          if ((bCurrentKey & ~currentSlot->dirKey) == (Key_Stop | Key_SHIFT))   // try to dispatch  ge�ndert 16.05.2017
          {
            vSetState(THR_STATE_ACQUIRE_LOCO_GET, currentSlot);
            sendLocoNetMove(0, 0, currentSlot);
          }
        }
        else
        {
          if ((bCurrentKey & (Key_Stop | Key_SHIFT)) == (Key_Stop | Key_SHIFT))
          {
            currentSlot->stat = 0x20;

            vSetState(THR_STATE_UNCONNECTED_WRITE, currentSlot);

            sendLocoNetWriteSlotData(currentSlot);
          }
        }
      }
    }
    else
    {
      wSelfTest |= (uint16_t) (bCurrentKey);
      sendLocoNetFredButton(bCurrentKey);
      vCheckSelfTestEnd(currentSlot);
    }

    bLastCurrentkey = bCurrentKey;
  } // end of if (bEvent & EVENT_KEY)
} // end of void vProcessKey(void)

/*
shiftTimeOut wird auf 1 gesetzt wenn die funktionstatse als shift taste(zum benutzen der erw funktionstaste) benutzt wurde
die abfrage ist daf�r, dass die fuktionstaste nichts ausl�st wenn sie nur als shift taste gedr�ckt wurde und nach dem dr�cken der geshifteten taste wieder
losgelassen wird
*/
void ProcessKeyInput (byte *pin, byte *port) {
	for (int i = 0; i < 8; i++) {
		processValue(bit_is_set(PINC, FUNKEY1));
	}
	if (keyStatus == 0 && value == 0) {
		keyStatus = 1;
	} else if (keyStatus >=10 && value == 255) {
		keyStatus++;
		if (keyStatus >= 20 && shiftTimeOut == 0) { //siehe kommentar oben
		//hier info in locoinfo schreiben/tastenevent
			disableLED1();
			//PORTA &= ~(1<<LED1);
			keyStatus = 0;
		} else if (keyStatus >= 20) {
			shiftTimeOut = 0;
			keyStatus = 0;
		}
	} else if (value == 0 && keyStatus > 0) {
		keyStatus++;
	}
}



void ProcessDirKeyInput8Streak (byte pin, byte port, int8_t keyNumber, rwSlotDataMsg *currentSlot) {
	for (int i = 0; i < 8; i++) {
		processValue(bit_is_set(pin, port));
	}
	if (dirKeyCounter[keyNumber - 1] < 6 && value == 255) {
		dirKeyCounter[keyNumber - 1]++;
	} else if (value == 0 && dirKeyCounter[keyNumber - 1] >= 5) {
			dirKeyCounter[keyNumber - 1] = 0;
			//Tastenevent hier einf�gen
			currentSlot->dirf ^= 0b00100000; //Toggle dirKeyStatus
			currentSlot->driveLock = 1;
			currentSlot->driveLockSendStatus[0] = LN_NEW_DATA;
			currentSlot->driveLockSendStatus[1] = LN_NEW_DATA;
	}
}


void ProcessShiftKeyInput8StreakFun (byte *pin, byte *port) {
	for (int i = 0; i < 8; i++) {
		processValue(bit_is_set(PINB, FUNKEY2));
	}
	if (value == 0 && keyStatus >= 0) {
		keyStatus++;
		if (keyStatus > 30000L) {
			keyStatus = -1;
			if (shiftStatus) {
				shiftStatus = 0;
				//disableLED4();
			} else {
				shiftStatus = 1;
				//enableLED4();
			}
		}
	} else if (value == 255 && (keyStatus >= 5 || keyStatus == -1)) {
		//siehe kommentar oben
		//hier info in locoinfo schreiben/tastenevent
		
		if (keyStatus >= 5 && shiftStatus == 0) {
			disableLED1();
			keyStatus = 0;
		} else {
			keyStatus = 0;
		}
	} else if (value == 255 && shiftPressed) {
		shiftStatus = 0;
		shiftPressed = 0;
	}
}


void ProcessShiftKeyInput8Streak (byte pin, int8_t port, int8_t keyNumber) {
	for (int i = 0; i < 8; i++) {
		processValue(bit_is_set(pin, port));
	}
	if (shiftKeyCounter[keyNumber - 1] < 5 && value == 255) {
		shiftKeyCounter[keyNumber - 1]++;
	} else if (value == 0 && shiftKeyCounter[keyNumber - 1] >= 5) {
		shiftKeyCounter[keyNumber - 1] = 0;
		//Tastenevent hier einf�gen
		if (shiftStatustwo[0] == keyNumber) {
			shiftStatustwo[1] ^= 0x01;
		} else {
			shiftStatustwo[1] = 0;
		}
		shiftStatustwo[0] = keyNumber;
		
	} 
}


void ProcessFunKeyInput8StreakFun (byte *pin, byte *port) {
	if (shiftStatus) {
		for (int i = 0; i < 8; i++) {
			processValue(bit_is_set(PIND, ERW_FUNKEY2));
		}
		if (value == 0) {
			shiftedKeyStatus = 1;
		} else if (shiftedKeyStatus == 1 && value == 255) {
			// *keyStatus++;
			
			
			shiftedKeyStatus = 0;
			shiftPressed++;
			//hier info in locoinfo schreiben/tastenevent
			  disableLED2();
			  //disableLED4();
			//PORTA |= 1<<LED1;
		}
	}
	
}


void ProcessFunKeyInput8Streak (byte pin, int8_t port, int8_t keyNumber, rwSlotDataMsg *currentSlot) {
	
		for (int i = 0; i < 8; i++) {
			processValue(bit_is_set(pin, port));
		}
		if (funKeyCounter[keyNumber -1] < 5 && value == 255) {		//Bis 6 hochz�hlen falls der Tastenstatus 8 mal 1 hintereinander war (Taste nicht gedr�ckt)
			funKeyCounter[keyNumber - 1]++;
		} else if (funKeyCounter[keyNumber - 1] >= 5 && value == 0) {		//Falls der Z�hler >= 5 ist und der Tastenstatus 8 mal hintereinander 0 (Taste gedr�ckt) ist, f�hre Tastenevent aus
			funKeyCounter[keyNumber - 1] = 0;								//Setze Z�hler auf -20, damit erst wieder etwas Zeit vergehen muss damit der Z�hler >= 5 ist und somit bereit auf ein Dr�cken zu reagieren
			//Tastenevent hier einf�gen
			
			if (shiftStatustwo[1] == 0) { //Wenn Funtkionen F0-F5 aktiv sind
				currentSlot->dirfSendStatus = LN_NEW_DATA;
				if (keyNumber == 1) {	  //Wenn die 1. Taste gedr�ckt ist
					currentSlot->dirf ^= 1 << (4); //Setze das 5. Bit (F0) Funktion)
				} else if (keyNumber != 4) {
					currentSlot->dirf |= 1 << (keyNumber - 2);
				} else {
					currentSlot->dirf ^= 1 << (keyNumber - 2);
				}
			} else if (shiftStatustwo[1] == 1) {
				currentSlot->sndSendStatus = LN_NEW_DATA;
				if (!(keyNumber == 1)) {
					currentSlot->snd |= 1 << (keyNumber - 2);
				} else {
					currentSlot->dirf |= 1 << (3); //Setze das 4. bit (F4) Funktion)
				}
			}
			//if (shiftStatustwo[0] == 1) {
				////currentSlot[shiftStatus].noch_einf�gen
				//toggleLED1();
			//} else if (shiftStatustwo[0] == 2) {
				//toggleLED2();
			//} else if (shiftStatustwo[0] == 3) {
				//toggleLED3();
			//} else if (shiftStatustwo[0] == 4) {
				//toggleLED4();
			//}
		}
}

void transmitInputLoco (rwSlotDataMsg *currentSlot, int8_t currentNumber) {
	if (!(currentSlot->driveLock) && currentSlot->spdSendStatus != LN_DONE) {
		currentSlot->spdSendStatus = sendLocoNet4BytePacketTry(0xA0, currentSlot->slot, currentSlot->spd, 0x1A);
	}
	
	if (currentSlot->driveLock) {
		if (currentSlot->driveLockSendStatus[0] != LN_DONE) {
			sendLocoNet4BytePacketTry(0xA0, eeprom_read_byte(&abEEPROM[currentSlot->epromAdr2]), eeprom_read_byte(&abEEPROM[currentSlot->epromAdr2]), 0x1A);
			_delay_ms(25);
			sendLocoNet4BytePacketTry(0xA0, 0x50, 0x50, 0x1A);
			_delay_ms(25);
			sendLocoNet4BytePacketTry(0xA0, currentSlot->adr, currentSlot->adr2, 0x1A);
			_delay_ms(25);
			currentSlot->driveLockSendStatus[0] = sendLocoNet4BytePacketTry(0xA1, currentSlot->slot, currentSlot->dirf, 0x1A);
			_delay_ms(25);
		} else if (currentSlot->driveLockSendStatus[1] != LN_DONE) {
			//Hier led toggle einf�gen
			currentSlot->driveLockSendStatus[1] = sendLocoNet4BytePacketTry(0xA0, currentSlot->slot, 0x00, 0x1A);
		} else if (currentSlot->driveLockSendStatus[0] == LN_DONE && currentSlot->driveLockSendStatus[1] == LN_DONE && currentSlot->spd == 0){
			currentSlot->driveLock = 0;
		}
	}  
	
	if(currentSlot->dirfSendStatus != LN_DONE) { //Ausf�hren falls Funktionstasten 0-4 || Richtungstaste gedr�ckt wurde
		//loconet senden (dirf, also direction + f0-f5 bits)
		//sendLocoNetDirf(&currentSlot);
		currentSlot->dirfSendStatus = sendLocoNet4BytePacketTry(0xA1,currentSlot->slot, currentSlot->dirf, 0x1A);
		if (currentSlot->dirfSendStatus == LN_DONE) {
			currentSlot->dirf &= 0b00110100; //Alle bits l�schen au�er direction, f0, f3 bit
		}
	}
	
	if (currentSlot->sndSendStatus != LN_DONE) { //Ausf�hren falls Funktionstasten 5-7 gedr�ckt wurden
		//Funktionen 6-8 senden
		//sendStatus = funktionnen6-8 senden
		currentSlot->sndSendStatus = sendLocoNet4BytePacketTry(0xA2,currentSlot->slot, currentSlot->snd, 0x1A);
		if (currentSlot->sndSendStatus == LN_DONE) {
			currentSlot->snd &= 0x00;
		}
	}
}

LN_STATUS testLoco (byte opc, byte data) {
	return sendLocoNet4BytePacketTry(opc, 0x06, data, 0x1A);
}

void processValue (int8_t new_value) {
	if (new_value) {
		value = (value / 2) + 128;
	} else {
		value = value / 2;
	}
}

void setPullUps (void) {
	PORTC |= (1<<FUNKEY1);
	PORTB |= (1<<FUNKEY2);
	PORTC |= (1<<FUNKEY3);
	PORTA |= (1<<FUNKEY4);
	PORTC |= (1<<DIRKEY1);
	PORTA |= (1<<DIRKEY2);
	PORTC |= (1<<DIRKEY3);
	PORTC |= (1<<DIRKEY4);
	PORTD |= (1<<ERW_FUNKEY1);
	PORTD |= (1<<ERW_FUNKEY2);
	PORTD |= (1<<ERW_FUNKEY3);
	PORTD |= (1<<ERW_FUNKEY4);
} 



/*******************************
Test-Methoden (TMT)
*******************************/


void testFalseSignalStreakIsSet (byte pin, byte taste, long lowerRange, long distance, long tries) {
	for (long i = 0; i < tries; i++) {
		processValue(bit_is_set(PINC, FUNKEY1));
		if (value == 3) {
			streak++;
		} else {
			if (streak > highStreak) {
				highStreak = streak;
			}
			streak = 0;
		}
	}
	if (highStreak > (lowerRange)) {
		disableLED1();
	}
	if (highStreak > (lowerRange + distance)) {
		disableLED2();
	}
	if (highStreak > (lowerRange + (2 * distance))) {
		disableLED3();
	}
	if (highStreak > (lowerRange + (3 * distance))) {
		enableLED4();
	} 
}

void testFalseSignalStreakIsClear (byte pin, byte taste, long lowerRange, long distance) {
	for (long i = 0; i < 500000; i++) {
		processValue(bit_is_set(PIND, ERW_FUNKEY4));
		if (value == 3) {
			streak++;
			
			} else {
			if (streak > highStreak) {
				highStreak = streak;
				
			}
			streak = 0;
			
		}
	}
	
	
	if (highStreak > 2 && highStreak < 35000) {
		disableLED1();
	}
	if (highStreak > 3) {
		disableLED2();
	}
	if (highStreak > 4) {
		disableLED3();
	}
	if (highStreak > 6) {
		enableLED4();
	}
	
	if (highStreak > 1000) {
		enableLED1();
	}
}

void testSignalRatio (byte pin, byte taste, long tries) {
	for (long i = 0; i < tries; i++) {
		if (bit_is_set(pin, taste)) {
			correctSignal++;
			} else {
			wrongSignal++;
		}
	}
	if (correctSignal > wrongSignal) {
		while (wrongSignal < correctSignal) {
			for (long i = 0; i < tries || wrongSignal >= correctSignal; i++) {
				if (bit_is_clear(pin, taste)) {
					wrongSignal++;
				}
			}
			ratioCounter++;
		}
	} else {
		while (correctSignal < wrongSignal) {
			for (long i = 0; i < tries || wrongSignal >= correctSignal; i++) {
				if (bit_is_set(pin, taste)) {
					correctSignal++;
				}
			}
			ratioCounter++;
		}
	}
	for (int i = 0; i <  ratioCounter; i++) {
		_delay_1500ms();
		disableLED1();
		_delay_1500ms();
		enableLED1();
	}
}

void testLEDMethods () {
	setLEDStatus(&ledArray[0], 0);
	setLEDStatus(&ledArray[1], 1);
	setLEDStatus(&ledArray[2], 0);
	setLEDStatus(&ledArray[3], 1);
}

void testKeySignal (byte pin, byte taste) {
	if (bit_is_set(pin, taste)) {
		disableLED1();
	} else {
		enableLED1();
	}
}

void _delay_1500ms (void) {
	for (int t = 0; t < 100; t++) {
		_delay_ms(15);
	}
}	 



/******************************************************FunctionHeaderBegin******
 * CREATED     : 2005-01-29
 * AUTHOR      : Olaf Funke
 * DESCRIPTION :   -
 *******************************************************************************
 * ARGUMENTS   : none
 * RETURN VALUE: none
 * NOTES       :   -
 *******************************************************FunctionHeaderEnd******/
void vProcessEncoder(rwSlotDataMsg *currentSlot)
{
  if (sEncDir != 0)
  {
    resetTimerAction(&IncrementTimer, INCREMENT_TIME); // start timer for debouncing

    if (bThrState == THR_STATE_CONNECTED)
    {
			if (!(bStopPressed))                // get encoder steps only if stop is not pressed
			{
				if (sEncDir < 0)                  // left rotation
				{
					sEncDir *= -1;                  // get absolut value

					if (sEncDir > bSpdCnt)
					{
						bSpdCnt = 0;
					}
					else
					{
						bSpdCnt -= (byte) sEncDir;
					}
				}
				else                              // right rotation
				{
					bSpdCnt += sEncDir;
				}

      if (bSpdCnt > MAX_SPEED)          // limit the value for speed
      {
        bSpdCnt = MAX_SPEED;
      }

      if (currentSlot->spd != abSpd[bSpdCnt])       // get speedvalue for incrementvalue
      {
        currentSlot->spd = abSpd[bSpdCnt];
					sendLocoNetSpd(currentSlot);           // anounce new speed value
				}
			}
    }
    else if (bThrState >= THR_STATE_SELFTEST)
    {
      if (sEncDir < 0)                  // left rotation decoded
      {
        wSelfTest |= Key_Enc_L;
      }
      else                              // right ratation decoded
      {
        wSelfTest |= Key_Enc_R;
      }
      sendLocoNetFredButton( sEncDir );

      vCheckSelfTestEnd(currentSlot);
    }

    sEncDir = 0;
  }
}

/******************************************************FunctionHeaderBegin******
 * CREATED     : 2005-01-29
 * AUTHOR      : Olaf Funke
 * DESCRIPTION :   -
 *******************************************************************************
 * ARGUMENTS   : none
 * RETURN VALUE: none
 * NOTES       :   -
 *******************************************************FunctionHeaderEnd******/
void vProcessPoti(rwSlotDataMsg *currentSlot)
{
  if (bThrState == THR_STATE_CONNECTED)
  {
    byte fOldSetSpeed = fSetSpeed;

    if (!fSetSpeed)
    {
      if (potAdcSpeedValue == 0)     // potivalue is in right range for  set speed again
      {
        fSetSpeed = TRUE;
      }
    }

    if (fSetSpeed)
    {
      byte bSpd = potAdcSpeedValue;
      if (currentSlot->spd != bSpd)
      {
        currentSlot->spd = bSpd;
        sendLocoNetSpd(currentSlot);
      }
    }

    if (fOldSetSpeed != fSetSpeed)
    {
      vSetState(THR_STATE_CONNECTED, currentSlot); // reset of blinking LEDs
    }
  }
  else if (bThrState >= THR_STATE_SELFTEST)
  {
    static uint16_t bOldValue = 0xffff; // init of potAdcSpeedValue is different,
                                        // so set first value anyway

    if (potAdcSpeedValue == 0)          // Poti on left side
    {
      wSelfTest |= Key_Poti_L;
    }
    else if (potAdcSpeedValue >= 126)   // Poti on right side
    {
      wSelfTest |= Key_Poti_R;
    }

    if ( bOldValue != potAdcRawValue )
    {
      bOldValue = potAdcRawValue;
      sendLocoNetFredAdc( potAdcRawValue );
    }

    vCheckSelfTestEnd(currentSlot);
  }
}


/******************************************************FunctionHeaderBegin******
 * CREATED     : 2005-01-29
 * AUTHOR      : Olaf Funke
 * DESCRIPTION :   -
 *******************************************************************************
 * ARGUMENTS   : none
 * RETURN VALUE: none
 * NOTES       :   -
 *******************************************************FunctionHeaderEnd******/
LN_STATUS sendLocoNetSpd(rwSlotDataMsg *currentSlot)
{
  return sendLocoNet4BytePacket(OPC_LOCO_SPD,currentSlot->slot,currentSlot->spd); // sendstatus = hinzugef�gt 10.6.2017
  
}

/******************************************************FunctionHeaderBegin******
 * CREATED     : 2005-01-29
 * AUTHOR      : Olaf Funke
 * DESCRIPTION :   -
 *******************************************************************************
 * ARGUMENTS   : none
 * RETURN VALUE: none
 * NOTES       :   -
 *******************************************************FunctionHeaderEnd******/
void sendLocoNetDirf(rwSlotDataMsg *currentSlot)
{
  sendStatus = sendLocoNet4BytePacket(OPC_LOCO_DIRF,currentSlot->slot,currentSlot->dirf); // sendstatus = hinzugef�gt 10.6.2017
}

/******************************************************FunctionHeaderBegin******
 * CREATED     : 2005-01-29
 * AUTHOR      : Olaf Funke
 * DESCRIPTION :   -
 *******************************************************************************
 * ARGUMENTS   : none
 * RETURN VALUE: none
 * NOTES       :   -
 *******************************************************FunctionHeaderEnd******/
void sendLocoNetSnd(rwSlotDataMsg *currentSlot)
{
  sendStatus = sendLocoNet4BytePacket(OPC_LOCO_SND,currentSlot->slot,currentSlot->snd); // sendstatus = hinzugef�gt 10.6.2017
}

/******************************************************FunctionHeaderBegin******
 * CREATED     : 2005-01-29
 * AUTHOR      : Olaf Funke
 * DESCRIPTION :   -
 *******************************************************************************
 * ARGUMENTS   : none
 * RETURN VALUE: none
 * NOTES       :   -
 *******************************************************FunctionHeaderEnd******/
void sendLocoNetWriteSlotData(rwSlotDataMsg *currentSlot)
{
  lnMsg SendPacket ;

  SendPacket.sd.command   = OPC_WR_SL_DATA  ; //opcode
  SendPacket.sd.mesg_size = 14              ; // length
  SendPacket.sd.slot      = currentSlot->slot   ; // slot    2    
  SendPacket.sd.stat      = currentSlot->stat   ; // stat    3    
  SendPacket.sd.adr       = currentSlot->adr    ; // adr     4    
  SendPacket.sd.spd       = currentSlot->spd    ; // spd     5    
  SendPacket.sd.dirf      = currentSlot->dirf   ; // dirf    6    
  SendPacket.sd.trk       = currentSlot->trk    ; // trk     7    
  SendPacket.sd.ss2       = currentSlot->ss2    ; // ss2     8    
  SendPacket.sd.adr2      = currentSlot->adr2   ; // adr2    9    
  SendPacket.sd.snd       = currentSlot->snd    ; // snd    10    
  SendPacket.sd.id1       = currentSlot->id1    ; // id1    11   
  SendPacket.sd.id2       = currentSlot->id2    ; // id2    12   

  if (sendLocoNetPacket( &SendPacket ) != LN_DONE)
  { // send message failed, so set new state
    

    switch (bThrState)
    {
    case THR_STATE_ACQUIRE_LOCO_WRITE:
    case THR_STATE_RECONNECT_WRITE:
      vSetState(THR_STATE_RECONNECT_GET_SLOT, currentSlot);
      break;
    case THR_STATE_UNCONNECTED_WRITE:
      vSetState(THR_STATE_CONNECTED, currentSlot);
      break;
    }
  }
}

/******************************************************FunctionHeaderBegin******
 * CREATED     : 2005-01-29
 * AUTHOR      : Olaf Funke
 * DESCRIPTION :   -
 *******************************************************************************
 * ARGUMENTS   : none
 * RETURN VALUE: none
 * NOTES       :   -
 *******************************************************FunctionHeaderEnd******/
void sendLocoNetMove(byte bSrc, byte bDest, rwSlotDataMsg *currentSlot)
{
  if (sendLocoNet4BytePacket(OPC_MOVE_SLOTS, bSrc, bDest) != LN_DONE)
  {
    
    // send message failed, so set new state
    if (bThrState == THR_STATE_RECONNECT_NULL_MOVE)
    {
      vSetState(THR_STATE_RECONNECT_GET_SLOT, currentSlot);
    }
    else
    {
      vSetState(THR_STATE_UNCONNECTED, currentSlot);
    }
  } 
}

/******************************************************FunctionHeaderBegin******
 * CREATED     : 2005-01-29
 * AUTHOR      : Olaf Funke
 * DESCRIPTION :   -
 *******************************************************************************
 * ARGUMENTS   : none
 * RETURN VALUE: none
 * NOTES       :   -
 *******************************************************FunctionHeaderEnd******/
LN_STATUS sendLocoNetAdr(rwSlotDataMsg *currentSlot)
{
  return sendLocoNet4BytePacket(OPC_LOCO_ADR, currentSlot->adr2, currentSlot->adr);
  
}

/******************************************************FunctionHeaderBegin******
 * CREATED     : 2005-01-29
 * AUTHOR      : Olaf Funke
 * DESCRIPTION :   -
 *******************************************************************************
 * ARGUMENTS   : none
 * RETURN VALUE: none
 * NOTES       :   -
 *******************************************************FunctionHeaderEnd******/
/*void sendLocoNetSelfTest(byte bTestCase, byte bValue)
{
  if (bThrState >= THR_STATE_SELFTEST)
  {
    if (bValue & 0x80)
    {
      bTestCase |=  0x01;
      bValue    &= ~0x80;
    }
    sendLocoNet4BytePacket(OPC_SELFTEST, bTestCase, bValue);
  }
}*/

/**
 * Send ADC raw value according to FRED.
 * @param raw the unfiltered value (0..1023)
 */
void sendLocoNetFredAdc( uint16_t raw )
{
  sendLocoNet4BytePacket( OPC_FRED_ADC, 0x7f & raw, 0x7f & raw>>7 );
}

/**
 * Send button press/release code according to FRED.
 * @param button a code from 1 to 127
 */
void sendLocoNetFredCd( uint8_t cdTime )
{
  sendLocoNet4BytePacket( OPC_FRED_BUTTON, 42, 0x7f & cdTime );
}

/**
 * Send button press/release code according to FRED.
 * @param button a code from 1 to 127
 */
void sendLocoNetFredButton( uint8_t button )
{
  sendLocoNet4BytePacket( OPC_FRED_BUTTON, 43, 0x7f & button );
}
/******************************************************FunctionHeaderBegin******
 * CREATED     : 2005-01-29
 * AUTHOR      : Olaf Funke
 * DESCRIPTION :   -
 *******************************************************************************
 * ARGUMENTS   : none
 * RETURN VALUE: none
 * NOTES       :   -
 *******************************************************FunctionHeaderEnd******/
void vCheckSelfTestEnd(rwSlotDataMsg *currentSlot)
{
  byte fSelfTestEnd = FALSE;

  switch (bFrediVersion)
  {
  case FREDI_VERSION_INCREMENT:
    if (wSelfTest == Key_Fredi_Inkrement)
    {
      fSelfTestEnd = TRUE;        
    }
    break;
  case FREDI_VERSION_INCREMENT_SWITCH:
    if (wSelfTest == Key_Fredi_Inkrement_Switch)
    {
      fSelfTestEnd = TRUE;        
    }
    break;
  case FREDI_VERSION_ANALOG:
    if (wSelfTest == Key_Fredi_Poti)
    {
      fSelfTestEnd = TRUE;        
    }
    break;
  default:
    break;
  }

  if (fSelfTestEnd == TRUE)
  {
    vSetState(THR_STATE_SELFTEST_DONE, currentSlot);
    delayTimer( 100 ); // wait a little bit longer
    sendLocoNetFredButton( 0x7F );

   
  }
}

/******************************************************FunctionHeaderBegin******
 * CREATED     : 2006-04-21
 * AUTHOR      : Olaf Funke
 * DESCRIPTION :   -
 *******************************************************************************
 * ARGUMENTS   : none
 * RETURN VALUE: none
 * NOTES       :   -
 *******************************************************FunctionHeaderEnd******/
void vCopySlotFromRxPacket(rwSlotDataMsg *currentSlot)
{

	//if (bThrState == THR_STATE_ACQUIRE_LOCO_GET)
	//{
		currentSlot->stat    = RxPacket->data[ 3];                 // slot status
		currentSlot->adr     = RxPacket->data[ 4];					// loco address
		
		currentSlot->adr2    = RxPacket->data[ 9];                 // loco address high
		
	//}
	//else
	//{
		currentSlot->stat    |= RxPacket->data[ 3] & ~DEC_MODE_MASK; // slot status
	//}

	currentSlot->slot = RxPacket->data[ 2];                 // slot number for this request

	
	currentSlot->spd   = RxPacket->data[ 5];                   // command speed
	currentSlot->dirf  = RxPacket->data[ 6];                   // direction and function keys
	

	currentSlot->trk       = RxPacket->data[ 7];                 // track status
	currentSlot->ss2       = RxPacket->data[ 8];                 // slot status 2 (tells how to use ID1/ID2 & ADV Consist
	currentSlot->snd       = RxPacket->data[10];
	eeprom_write_byte(&abEEPROM[currentSlot->epromAdr], (uint8_t) (currentSlot->adr));
	eeprom_write_byte(&abEEPROM[currentSlot->epromAdr2], (uint8_t) (currentSlot->adr2));
	                 // Sound 1-4 / F5-F8
}

/******************************************************FunctionHeaderBegin******
 * CREATED     : 2006-04-21
 * AUTHOR      : Olaf Funke
 * DESCRIPTION :   -
 *******************************************************************************
 * ARGUMENTS   : none
 * RETURN VALUE: none
 * NOTES       :   -
 *******************************************************FunctionHeaderEnd******/
void vSetUnconnected(rwSlotDataMsg *currentSlot)
{
  currentSlot->adr   = 0x00;
  currentSlot->adr2  = 0x00;

  eeprom_write_byte(&abEEPROM[currentSlot->epromAdr],  (uint8_t) (0x00));
  eeprom_write_byte(&abEEPROM[currentSlot->epromAdr2], (uint8_t) (0x00));
  

  vSetState(THR_STATE_UNCONNECTED, currentSlot);
  sendLocoNetMove(currentSlot->slot, 0, currentSlot);
}


