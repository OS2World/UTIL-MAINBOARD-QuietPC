/* $Id: quietpcp.h,v 1.5 2007-04-19 06:48:15 andrew_belov Exp $ */

/* Private (internal) declarations */

#ifndef QUIETPCP_INCLUDED
#define QUIETPCP_INCLUDED

/* Spinup/spindown magic codes. In systems where the duty cycle control is too
   rough and the range of "quiet" duty cycles is too narrow, spinup hooks come
   in to serve two objectives:
   1. Boost the duty cycle when bringing the fan up from a stop condition
   2. Monitor the fan RPM at "risky" duty cycles and shutdown it completely
      if it spins down. */

/* Entry codes */
#define SPINUP_IDLE                0    /* Idletime hook
                                           (spindown support) */
#define SPINUP_RESTART             1    /* Spinup after spindown */
#define SPINUP_RISE                2    /* Temperature rises */
#define SPINUP_FALL                3    /* Temperature declines */
/* Return codes */
#define SPINUP_OK                  0    /* Handled normally */
#define SPINUP_NOT_SUPPORTED     127    /* Feature not supported */

#define NO_SPINDOWN       0xFFFFFFFF    /* A delay value meaning "don't bother" */

/* Global device descriptor. Filled in by the corresponding detection
   routine */

struct device_properties
{
 short hwmon_chip;                      /* One of predefined types */
 unsigned short num_sensors;            /* # of thermal sensors */
 unsigned short num_fans;               /* # of fans */
 unsigned short max_dc;                 /* Maximum duty cycle */
 unsigned short temp_granularity;       /* Temperature granularity (x0.5øC) */
 /* Hardware-specific interfaces */
 short (*spinup_magic)(int event, int fan, struct fan_ctrl *c); /* Spinup hook */
 void (*set_fan_divisor)(int num, int v); /* Adjusts RPM divisors */
 long (*query_rpm)(short fan);          /* Returns the fan RPMs */
 short (*query_temperature)(short sensor); /* Returns sensor readings */
 void (*set_pwm)(int fan, int div, int hifreq);
 void (*set_dc)(int fan, unsigned char dc);
};

/* OS-specific */

int install_tick_handler();

#endif
