/*
 * transport_defs.h
 *
 *  Created on: Apr 20, 2022
 *      Author: serh
 */

#ifndef COMPONENTS_TRANSPORT_TRANSPORT_DEFS_H_
#define COMPONENTS_TRANSPORT_TRANSPORT_DEFS_H_

#include <stdio.h>
#include "../coder/flag_coder.h"
#include "string.h"

#define MAX_TRANSPORT_BUFFER		512
#define BUF_SIZE(x)			(sizeof(x)/sizeof(x[0]))


typedef void (*user_msg)(int msg_id, ...);
typedef size_t (*drv_send_fn) (uint32_t peer, uint8_t * const data, const size_t length);
typedef size_t (*txfn)(uint8_t * const data, const size_t length);
typedef size_t (*codefn)(uint8_t mode, uint8_t * data, size_t len);
typedef uint8_t * (*buffer)(void);
typedef size_t (*length)(void);

typedef enum {	DrvInited = 0,
				PeerConnected,
				PeerDisconnected,
				DrvDataReceived,
				DrvDataSent,
				DrvError,
				Undefined,
				}transport_msg_t;

typedef struct {
	int				id;
	txfn			send;
}driver_t;


struct TRANSPORT{
	int 			transport_id;		//id is assigned by mediator
	txfn			fn_send;				//transport send wrapper
	void			(*notify)(int id, ...);	//mediator notify callback
	buffer			out_buffer;
	length			to_send;
	length			out_length;
	struct TRANSPORT * 	direct_transport;							//pointer to merged transport to direct send
///	encoder_t		* coder;
	codefn			decode;
	codefn			encode;
    size_t   (*add_tx)(uint8_t data, size_t length);
};

typedef struct TRANSPORT transport_t;

#endif /* COMPONENTS_TRANSPORT_TRANSPORT_DEFS_H_ */
