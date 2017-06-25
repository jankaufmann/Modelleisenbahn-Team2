/*
 * KEYS.c
 *
 * Created: 21.06.2017 10:57:21
 *  Author: Phillip Wollschläger
 */ 

#include <stdint.h>         // typedef int8_t, typedef uint8_t, typedef int16_t, typedef uint16_t
#include "common_defs.h"
#include <avr/io.h>
#include "sysdef.h"
#include "keys.h"



uint8_t value;              //Wert der letzten Zustände (zum Entprellen)
char sbiString[9] = "sbi 27, 0";
char cbiString[9] = "cbi 27, 0";
char String[5] = "27, 0";


/*
Überprüft den Status einer Taste. Wenn 8 mal hintereinander das gleiche Signal kam wird das Signal akzeptiert. Ein akzeptiertes High-Signal (Taste nicht gedrückt) erhöht den Zähler,
wenn der Zähler 5 erreicht hat wird auf ein Low-Signal (Taste gedrückt) reagiert.
*/
int ProcessKeyInputSgPress (keydata *key) {
	for (int i = 0; i < 8; i++) {
		value = value << 1;
		if(bit_is_set(*key->adress, key->bit)) {
			++value;
		}
	}
	if (key->counter < 6 && value == 255) {
		key->counter++;
	} else if (value == 0 && key->counter >= 5) {
		key->counter = 0;
		return 1;
	}
	return 0;
}


int ProcessKeyInputKeepPressed (keydata *key, uint16_t delay) {
	for (int i = 0; i < 8; i++) {
		value = value << 1;
		if(bit_is_set(*key->adress, key->bit)) {
			++value;
		}
	}
	if (key->counter < delay && value == 0) {
		++key->counter;
		} else if (value == 0 && key->counter == delay) {
			++key->counter;
		return 1;
		} else if (value == 255) {
			key->counter = 0;
	}
	return 0;
}


int ProcessKeyInputAtRelease (keydata *key) {
	for (int i = 0; i < 8; i++) {
		value = value << 1;
		if(bit_is_set(*key->adress, key->bit)) {
			++value;
		}
	}
	if (key->counter < 6 && value == 0) {
		key->counter++;
	} else if (value == 255 && key->counter >= 5) {
		key->counter = 0;
		return 1;
	}
	return 0;
}


void setLEDStatus (leddata *LED, int8_t status) {
	if (status - LED->polung) {
		*LED->ledAdress |= _BV(LED->bitToSet); //LED an
		//sbi(LED->ledAdress, LED->bitToSet);
		//_asm__(sbiString);
	} else {
		//__asm__("a");
		//cbi(LED->ledAdress, LED->bitToSet);
		*LED->ledAdress &= ~(_BV(LED->bitToSet)); //LED aus
	}
}

void toggleLEDStatus (leddata *LED) {
	*LED->ledAdress ^= _BV(LED->bitToSet);
}

