/*
 * KEYS_H_.h
 *
 * Created: 21.06.2017 11:16:05
 *  Author: Phillip Wollschläger
 */ 


#ifndef KEYS_H_
#define KEYS_H_


typedef struct keydata_t {
	volatile unsigned char *adress;				//Abzufragende Adresse
	byte bit;					//Abzufragendes Bit der Adresse
	uint16_t counter;			//Counter zum Entprellen
} keydata;

typedef struct leddata_t {		//Struktur für 2-Farbige LED
	volatile unsigned char *ledAdress;				//Adresse des LED bits
	byte bitToSet;				//Bit das zur LED gehört
	byte polung;				//0 setzen
} leddata;

int ProcessKeyInputSgPress (keydata *key);
int ProcessKeyInputKeepPressed (keydata *key, uint16_t delay);
int ProcessKeyInputAtRelease (keydata *key);
void setLEDStatus(leddata *LED, int8_t status);
void toggleLEDStatus(leddata *LED);

#endif /* KEYS_H_ */