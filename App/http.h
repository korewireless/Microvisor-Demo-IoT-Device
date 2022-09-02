/**
 *
 * Microvisor IoT Device Demo
 * Version 2.1.0
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */
#ifndef _HTTP_H_
#define _HTTP_H_


/*
 * PROTOTYPES
 */
void        http_channel_center_setup(void);
bool        http_open_channel(void);
void        http_close_channel(void);
bool        http_send_request(double temp);
bool        http_send_warning();
void        http_process_response(void);



#endif      // _HTTP_H_
