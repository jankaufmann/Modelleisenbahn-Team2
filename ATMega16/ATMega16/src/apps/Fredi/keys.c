/*
 * CFile1.c
 *
 * Created: 21.06.2017 10:57:21
 *  Author: Admin
 */ 

#include <stdint.h>         // typedef int8_t, typedef uint8_t, typedef int16_t, typedef uint16_t
#include "common_defs.h"
#include <avr/io.h>


typedef struct keydata_t {
	byte adress;				//Abzufragende Adresse
	byte bit;					//Abzufragendes Bit der Adresse
	uint8_t counter;			//Counter zum Entprellen
} keydata;

typedef struct leddata_t {		//Struktur für 2-Farbige LED
	byte ledAdress;				//Adresse des LED bits
	byte bitToSet;				//Bit das zur LED gehört
	byte polung;				//0 setzen 
} leddata;

int ProcessKeyInput8Streak (keydata *key);
void setLEDStatus(leddata *LED, int8_t status);
void toggleLEDStatus(leddata *LED);



uint8_t value;              //Wert der letzten Zustände (zum Entprellen)

/*
Überprüft den Status einer Taste. Wenn 8 mal hintereinander das gleiche Signal kam wird das Signal akzeptiert. Ein akzeptiertes High-Signal (Taste nicht gedrückt) erhöht den Zähler,
wenn der Zähler 5 erreicht hat wird auf ein Low-Signal (Taste gedrückt) reagiert.
*/
int ProcessKeyInput8Streak (keydata *key) {
	for (int i = 0; i < 8; i++) {
		value = value << 1;
		value |= (bit_is_set(key->adress, key->bit) >> key->bit); 
	}
	if (key->counter < 6 && value == 255) {
		key->counter++;
		} else if (value == 0 && key->counter >= 5) {
		key->counter = 0;
		return 1;
	}
	return 0;
}

void setLEDStatus (leddata *LED, int8_t status) {
	if (status - LED->polung) {
		LED->ledAdress |= _BV(LED->bitToSet); //LED an
	} else {
		LED->ledAdress &= ~(_BV(LED->bitToSet)); //LED aus
	}
}

void toggleLEDStatus (leddata *LED) {
	LED->ledAdress ^= 1 << LED->bitToSet;
}

