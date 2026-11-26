#ifndef NL_OWNER_H
#define NL_OWNER_H
#include <PalmOS.h>
extern UInt32 nl_owner_token, nl_owner_db;
extern UInt16 nl_owner_card, nl_busy, nl_uncertain_operation;
Boolean nl_physical_owned(void);
#endif
