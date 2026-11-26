#ifndef NL_CLIENT_PRIVATE_H
#define NL_CLIENT_PRIVATE_H
#include "NioLibClient.h"
extern UInt16 nl_client_ref, nl_client_active, nl_client_result_code;
extern UInt32 nl_client_token;
uint8_t nl_client_translate(Err err);
uint8_t nl_client_extended_ready(void);
extern UInt16 nl_client_key_creator, nl_client_key_app, nl_client_key_size;
uint8_t nl_client_appkey_ready(void);
#endif
