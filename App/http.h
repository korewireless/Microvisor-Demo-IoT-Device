/**
 *
 * Microvisor IoT Device Demo
 *
 * Copyright © 2023, KORE Wireless
 * Licence: MIT
 *
 */
#ifndef _HTTP_H_
#define _HTTP_H_


#ifdef __cplusplus
extern "C" {
#endif


/*
 * PROTOTYPES
 */
void            http_notification_center_setup(void);
bool            http_open_channel(void);
void            http_close_channel(void);
enum MvStatus   http_send_request(double temp);
enum MvStatus   http_send_warning(void);


#ifdef __cplusplus
}
#endif


#endif      // _HTTP_H_
