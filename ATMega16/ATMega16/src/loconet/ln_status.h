/*
 * LN_STATUS.h
 *
 * Created: 23.06.2017 14:20:41
 *  Author: Admin
 */ 


#ifndef LN_STATUS_H_
#define LN_STATUS_H_


typedef enum
{
	LN_CD_BACKOFF = 0,
	LN_PRIO_BACKOFF,
	LN_NETWORK_BUSY,
	LN_DONE,
	LN_NEW_DATA,
	LN_COLLISION,
	LN_UNKNOWN_ERROR,
	LN_RETRY_ERROR
} LN_STATUS ;


#endif /* LN_STATUS */