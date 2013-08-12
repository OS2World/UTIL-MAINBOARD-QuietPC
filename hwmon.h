/* $Id: hwmon.h,v 1.2 2005/12/18 20:42:11 root Exp $ */

#ifndef HWMON_INCLUDED
#define HWMON_INCLUDED

#include "quietpc.h"

int configure_hwmon();
int reconfigure_hwmon();
void tick_handler();
void shutdown_hwmon();
unsigned short hwmon_ioctl(unsigned short cmd, void *pparm, int parm_len,
                           void *pdata, int data_len);

#endif
