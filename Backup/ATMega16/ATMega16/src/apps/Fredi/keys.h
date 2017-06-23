/*
 * IncFile1.h
 *
 * Created: 21.06.2017 11:16:05
 *  Author: Admin
 */ 


#ifndef KEYS_H_
#define KEYS_H_


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
void setLEDStatus(int8_t status, leddata *LED);
void toggleLEDStatus(leddata *LED);

#endif /* INCFILE1_H_ */