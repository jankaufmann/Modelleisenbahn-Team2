/*
 * SLOT_STATE.h
 *
 * Created: 25.06.2017 14:47:52
 *  Author: Admin
 */ 


#ifndef SLOT_STATE_H_
#define SLOT_SATE_H_

typedef enum
{
	THR_STATE_INIT                    = 0,
	THR_STATE_CONNECTED               = 1,

	THR_STATE_ACQUIRE_LOCO_GET        = 10,
	THR_STATE_ACQUIRE_LOCO_WRITE      = 11,

	THR_STATE_RECONNECT_GET_SLOT      = 20,
	THR_STATE_RECONNECT_NULL_MOVE     = 22,
	THR_STATE_RECONNECT_WRITE         = 24,

	THR_STATE_UNCONNECTED_WRITE       = 30,
	THR_STATE_UNCONNECTED             = 32,

	THR_STATE_SELFTEST                = 100, // must be higher than other values
	THR_STATE_SELFTEST_DONE           = 101,
	THR_STATE_LAST
} THR_STATE;



#endif /* SLOT_STATE_H_ */