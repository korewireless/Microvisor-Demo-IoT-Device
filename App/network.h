/**
 *
 * Microvisor IoT Device Demo
 * Version 3.0.0
 * Copyright © 2023, Twilio
 * Licence: Apache 2.0
 *
 */
#ifndef _NETWORK_H_
#define _NETWORK_H_


#ifdef __cplusplus
extern "C" {
#endif


/*
 * PROTOTYPES
 */
void            net_open_network(void);
MvNetworkHandle net_get_handle(void);


#ifdef __cplusplus
}
#endif


#endif      // _NETWORK_H_