/* $Id: quietpc.h,v 1.6 2007-01-30 11:48:33 andrew_belov Exp $ */

/* Public declarations */

#ifndef QUIETPC_INCLUDED
#define QUIETPC_INCLUDED

#ifdef __OS2__
 #define DEVICE_NAME      "QUIETPC$"
 #define DEVICE_FILENAME "\\DEV\\QUIETPC$"
#else
 #define DEVICE_NAME       "quietpc"
 #define DEVICE_FILENAME "/dev/quietpc"
 #define MAJOR_NUM               241
#endif

/* Linux hacking */

#ifdef LINUX
 #include <linux/autoconf.h>
 /* This will be overriden in the main part */
 #define __NO_VERSION__
 /* Hacking for MODVERSIONS */
 #if defined(CONFIG_MODVERSIONS)&&CONFIG_MODVERSIONS==1
  #ifndef MODVERSIONS
   #define MODVERSIONS
  #endif
 #endif
 #include <linux/ioctl.h>
#endif

#define HWMON_REVISION             1
#define MAX_FANS                   8    /* Max. # of fan ctls */
#define MAX_SENSORS                8    /* Max. # of sensors */

#define IOCTL_CAT_QPC           0xA0
#define QPC_CMD_RESET           0xC0    /* Reset */
#define QPC_CMD_SET_HWMON_PARAMS 0xC4   /* Set hardware monitor
                                           parameters */
 #define QPC_HWMON_SET_FAN      0x01    /* Configure a fan */
#define QPC_CMD_QUERY_HWMON_PARAMS 0xE4 /* Query hardware monitor
                                           parameters */
 #define QPC_HWMON_QUERY_FAN    0x01    /* Query fan_ctrl structure
                                           associated with a fan */
 #define QPC_HWMON_QUERY_INFO   0x02    /* Information structure */
#define QPC_CMD_SET_POST        0xC5    /* Choose a sensor for POST */
#define QPC_CMD_QUERY_POST      0xE5    /* Query the current POST
                                           sensor */

/* Hardware monitoring chip types */

#define HWMON_CHIP_NONE            0    /* None/unknown chip */
#define HWMON_CHIP_W83697F         1    /* Winbond W83697F */
#define HWMON_CHIP_W83697HF        2    /* Winbond W83697HF */
#define HWMON_CHIP_IT8712          3    /* ITE IT8712 */

/* Fan control operation */

#define FANCTRL_TRANSIENT       0x80    /* Flag indicating that configuration
                                           structure is not to be stored
                                           permanently */

#define FANCTRL_NONE            0x00    /* None: set 100% duty cycle, divisor 0 */
#define FANCTRL_MANUAL_PWM      0x01    /* Specify duty cycle/divisor manually */
#define FANCTRL_THERM_CRUISE    0x02    /* Thermal cruise mode */
#define FANCTRL_SPEED_CRUISE    0x03    /* Fan speed cruise mode */
#define FANCTRL_SW_MODE         0x04    /* Software control mode */
#define FANCTRL_SET_DIVISOR     (FANCTRL_TRANSIENT|0x05)
                                        /* Specify divisor for RPM monitoring */

/* Special indices */

#define NO_SENSOR             0xFFFF    /* Undefined sensor */

#pragma pack(1)

/* PWM information structure */

struct pwm_info
{
 unsigned char hifreq;                  /* High frequency flag */
 unsigned char prescale;                /* Divisor (0=no PWM) */
 unsigned char duty_cycle;              /* 0...255 */
};

/* Fan control exported structure */

struct fan_ctrl
{
 unsigned char mode;                    /* One of FANCTRL_ */
 /* Individual settings for each mode */
 union
 {
  /* Manual PWM */
  struct pwm_info pwmi;
  /* Set divisor */
  unsigned int div;
  /* Software control mode */
  struct
  {
   char ref_sensor;                     /* Reference sensor number. This needs
                                           to be resolved by the driver. */
   unsigned char dc_rise[251];          /* T -> D during rise of T */
   unsigned char dc_fall[251];          /* T -> D during fall of T (normally
                                           dc_fall[j] >= dc_rise[j]) */
   /* System use only - zeroed by driver */
   unsigned char last_t;                /* Temperature during last execution */
   unsigned char last_d;                /* Duty cycle during last execution */
   unsigned char prev_t;                /* Previous temperature */
   unsigned char prev_d;                /* Previous duty cycle */
   unsigned char min_t;                 /* Minimum temperature */
   unsigned char max_t;                 /* Maximum temperature */
   unsigned long error_rate;            /* Malfunction counter */
   /* Spinup/spindown-related */
   unsigned char f_spinup;              /* Spin-up enablement flag */
   unsigned char spinup_underway;       /* 1 if warming up */
   unsigned long spindown_events;       /* # of software-controlled spindowns */
   unsigned long spindown_delay;        /* Wait until a stable state is attained
                                           before the spindown check begins */
   unsigned char spindown_countdown;    /* Spindown ticker */
   unsigned char spindown_countup;      /* Spindown anti-ticker */
  };
 };
};

/* IOCTL set command */

struct hwmon_fan_op
{
 unsigned short cmd;
 struct
 {
  char num_fan;
  struct fan_ctrl fctrl;
 };
};

/* Information structure */

struct hwmon_info
{
 unsigned short revision;               /* +0x00 - protocol revision */
 short hwmon_chip;                      /* +0x02 - chip type index */
 unsigned short num_fans;               /* +0x04 - # of fans */
 unsigned short num_sensors;            /* +0x06 - # of t/sensors */
 unsigned short max_dc;                 /* +0x08 - maximum fan duty cycle */
 unsigned short temp_granularity;       /* +0x0A - temperature granularity */
 long fan_rpm[MAX_FANS];                /* +0x0C - fan RPMs */
 short sensor_temp[MAX_SENSORS];        /* +0x2C - sensor readings */
 /* Next revision boundary */
#define HWMON_INFO_R1           0x3A
};

/* IOCtl encapsulation structure for platforms with a single ioctl parameter */

struct ioctl_encaps
{
 void *pparm;
 unsigned int parm_len;
 void *pdata;
 unsigned int data_len;
};

#pragma pack()

#endif
