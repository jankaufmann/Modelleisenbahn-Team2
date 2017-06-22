/*
 * IncFile1.h
 *
 * Created: 21.06.2017 11:16:05
 *  Author: Admin
 */ 


#ifndef INCFILE1_H_
#define INCFILE1_H_


typedef struct keydata_t {
	byte adress;				//Abzufragende Adresse
	byte bit;					//Abzufragendes Bit der Adresse
	uint8_t counter;			//Counter zum Entprellen
} keydata;

typedef struct leddata_t {

	
} leddata;

int ProcessKeyInput8Streak (keydata *key);

#endif /* INCFILE1_H_ */