/* $Id: hwmon.c,v 1.5 2006/09/09 14:39:57 andrew_belov Exp $ */

/* Hardware monitor */

#include <stdarg.h>
/* memcpy */
#ifdef LINUX
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "hwmon.h"
#include "portio.h"
#include "quietpcp.h"

/* These are called from the respective modules */

int detect_w83697(struct device_properties *pdp);
int detect_ite(struct device_properties *pdp);

/* POST display settings */

#define POST_PORT            0x0080     /* POST base address */
#define POST_DIVISOR             10     /* Ticks between screen update
                                           (to prevent flicker) */

/* Local data */

static struct fan_ctrl fctrl[MAX_FANS]; /* Fan control structures */
static unsigned short sensor_to_post;   /* Temperature sensor to be POSTed */
static struct device_properties dp;

/* Interrupt-time readjustment of fan settings with respect to CPU
   temperature */

static void readjust_fan(int fan, struct fan_ctrl *c)
{
 int cur_t;
 unsigned char d;

 cur_t=dp.query_temperature(c->ref_sensor);
 /* Sensor failed - this was not expected! Set full PWM until next time */
 if(cur_t<0)
 {
  dp.set_dc(fan, (unsigned char)dp.max_dc);
  c->last_t=0;
  c->error_rate++;
  c->spinup_underway=0;
  c->spindown_events=0;
  c->spindown_delay=NO_SPINDOWN;
  c->spindown_countdown=0;
  return;
 }
 if(cur_t==c->last_t)                   /* Nothing changed? */
 {
  dp.spinup_magic(SPINUP_IDLE, fan, c);
  return;
 }
 if(cur_t>c->last_t)
 {
  d=c->dc_rise[cur_t];
  /* Special case: leave the fan spinning (choose fall course) if the rise
     course has zero value at this point AND it's our first pass. */
  if(d==0&&c->last_t==0)
  {
   d=c->dc_fall[cur_t];
   dp.set_dc(fan, c->prev_d=c->last_d=d);  /* This is required since last_d isn't
                                           yet established and d may be == 0 */
   dp.spinup_magic(SPINUP_FALL, fan, c);
  }
  /* Indeed the temperature has grown. But we need to change the PWM only if
     it has been programmed to a lower value. */
  else if(d>c->last_d)
  {
   /* Check if a special spinup condition is needed. If there is no spinup
      support, go ahead with programming the duty cycle directly */
   if(dp.spinup_magic(SPINUP_RISE, fan, c)==SPINUP_NOT_SUPPORTED)
    dp.set_dc(fan, d);
   c->prev_d=c->last_d;
   c->last_d=d;
  }
 }
 else
 {
  d=c->dc_fall[cur_t];
  /* Same here. The only way into the upper part of hysteresis loop is from
     the right. */
  if(d<c->last_d)
  {
   dp.set_dc(fan, d);
   dp.spinup_magic(SPINUP_RESTART, fan, c);
   c->prev_d=c->last_d;
   c->last_d=d;
  }
 }
 /* Use the current values if we're at the beginning */
 if(c->prev_d==0)
  c->prev_d=c->last_d;
 if((c->prev_t=c->last_t)==0)
  c->prev_t=cur_t;
 c->last_t=cur_t;
 /* Update min/max values */
 if(cur_t>c->max_t)
  c->max_t=cur_t;
 if(cur_t<c->min_t)
  c->min_t=cur_t;
}

/* Display value at the POST card */

static void post_display(unsigned char v)
{
 unsigned char x;
 static unsigned int post_ticks=POST_DIVISOR-1;

 if(++post_ticks>=POST_DIVISOR)
  post_ticks=0;
 else
  return;
 if(v>=100)
  x=0xAA;
 else
  x=((v/10)<<4)+(v%10);
 outp(POST_PORT, x);
}

/* Disables any fan control, reverting to 100% duty cycle */

static void shutdown(int fan)
{
 dp.set_pwm(fan, 1, 1);
 dp.set_dc(fan, 255);
}

/* Sets various operating parameters for a fan */

static void set_fan_params(int fan, struct fan_ctrl *c)
{
 if(fan<0||fan>=MAX_FANS)
  return;
 switch(c->mode)
 {
  case FANCTRL_NONE:
   shutdown(fan);
   break;
  case FANCTRL_MANUAL_PWM:
   dp.set_pwm(fan, c->pwmi.prescale, c->pwmi.hifreq);
   dp.set_dc(fan, c->pwmi.duty_cycle);
   break;
  case FANCTRL_SW_MODE:
   c->prev_d=c->prev_t=
   c->last_d=c->last_t=c->error_rate=0;
   c->min_t=0xFF;
   c->max_t=0;
   readjust_fan(fan, &fctrl[fan]);
   break;
  case FANCTRL_SET_DIVISOR:
   dp.set_fan_divisor(fan, c->div);
   break;
 }
}

/* Fills the global configuration with default parameters */

static void fill_dp()
{
 dp.hwmon_chip=HWMON_CHIP_NONE;
 dp.num_sensors=0;
 dp.num_fans=0;
 dp.max_dc=0;
}

/* Timer tick handler */

void tick_handler()
{
 int i;

 for(i=0; i<MAX_FANS; i++)
 {
  switch(fctrl[i].mode)
  {
   case FANCTRL_SW_MODE:
    readjust_fan(i, &fctrl[i]);
    break;
  }
 }
 if(sensor_to_post!=NO_SENSOR)
  post_display((unsigned char)(dp.query_temperature(sensor_to_post)>>1));
}

/* Sets the fan control parameters */

static unsigned short hwmon_fan_set(struct hwmon_fan_op *p)
{
 int num_fan;

 if(dp.hwmon_chip<0)
  return(1);
 switch(p->cmd)
 {
  case QPC_HWMON_SET_FAN:
   num_fan=p->num_fan;
   if(num_fan<0||num_fan>=dp.num_fans)
    return(1);
   if(p->fctrl.mode&FANCTRL_TRANSIENT)
   {
    set_fan_params(num_fan, &p->fctrl);
    return(0);
   }
   fctrl[num_fan]=p->fctrl;
   /* Init the temperature records with meaningless values to pin up the
      course change at the next iteration */
   if(fctrl[num_fan].mode==FANCTRL_SW_MODE)
   {
    if(fctrl[num_fan].ref_sensor>=dp.num_sensors)
     return(1);
    fctrl[num_fan].prev_d=fctrl[num_fan].prev_t=
    fctrl[num_fan].last_d=fctrl[num_fan].last_t=0;
#ifdef USE_SPINUP
    fctrl[num_fan].spinup_underway=0;
    fctrl[num_fan].spindown_delay=NO_SPINDOWN;
    fctrl[num_fan].spindown_events=fctrl[num_fan].spindown_countdown=0;
#endif
   }
   set_fan_params(num_fan, &fctrl[num_fan]);
   return(0);
 }
 return(1);
}

/* Queries fan control parameters */

static unsigned short hwmon_fan_query(unsigned short cmd, int data_len, void *data)
{
 struct hwmon_fan_op *s;
 struct hwmon_info *q;
 int num_fan, i;

 switch(cmd)
 {
  case QPC_HWMON_QUERY_FAN:
   if(data_len<((char *)&s->fctrl-(char *)s))
    return(1);
   if(data_len>sizeof(struct hwmon_fan_op))
    data_len=sizeof(struct hwmon_fan_op);
   data_len-=((char *)&s->fctrl-(char *)s);
   s=(struct hwmon_fan_op *)data;
   num_fan=s->num_fan;
   if(num_fan==-1)                      /* Compatibility trick */
    num_fan=0;
   if(num_fan<0||num_fan>=dp.num_fans)
    return(1);
   memcpy(&s->fctrl, &fctrl[num_fan], data_len);
   return(0);
  case QPC_HWMON_QUERY_INFO:
   if(data_len<HWMON_INFO_R1)           /* Check for min. size */
    return(1);
   q=(struct hwmon_info *)data;
   q->revision=HWMON_REVISION;
   q->hwmon_chip=dp.hwmon_chip;
   for(i=0; i<dp.num_fans; i++)
    q->fan_rpm[i]=dp.query_rpm(i);
   for(i=0; i<dp.num_sensors; i++)
    q->sensor_temp[i]=dp.query_temperature(i);
   q->num_fans=dp.num_fans;
   q->num_sensors=dp.num_sensors;
   q->max_dc=dp.max_dc;
   q->temp_granularity=dp.temp_granularity;
   return(0);
 }
 return(1);
}

/* IOCtl router */

unsigned short hwmon_ioctl(unsigned short cmd, void *pparm, int parm_len,
                           void *pdata, int data_len)
{
 unsigned short rc;

 /* Process the functions */
 switch(cmd)
 {
  case QPC_CMD_RESET:
   rc=reconfigure_hwmon();
   if(data_len>=2)
    *((unsigned short *)pdata)=rc;
   return(0);
  case QPC_CMD_SET_HWMON_PARAMS:
   if(parm_len<sizeof(struct hwmon_fan_op))
    return(1);
   return(hwmon_fan_set((struct hwmon_fan_op *)pparm));
  case QPC_CMD_QUERY_HWMON_PARAMS:
   if(parm_len<sizeof(unsigned short))
    return(1);
   return(hwmon_fan_query(*(unsigned short *)pparm, (int)data_len, (void *)pdata)?1:0);
  case QPC_CMD_SET_POST:
   if(parm_len<sizeof(short))
    return(1);
   if(*(short *)pparm>=(short)dp.num_sensors)
    return(1);
   if(*(short *)pparm<0&&sensor_to_post!=NO_SENSOR)
    outp(POST_PORT, 0xFF);
   sensor_to_post=*(short *)pparm;
   return(0);
  case QPC_CMD_QUERY_POST:
   if(data_len<sizeof(short))
    return(1);
   *(short *)pdata=sensor_to_post;
   return(0);
  default:
   return(1);
 }
 return(1);
}

/* Hardware monitoring initialization. Detects the chip and performs
   startup. */

static int init_hwmon()
{
 int rc;

 fill_dp();
 if((rc=(detect_w83697(&dp)&&
         detect_ite(&dp) )))
  return(rc);
 return(0);
}

/* Configures the hardware monitor for the first time */

int configure_hwmon()
{
 int i, rc;

 sensor_to_post=NO_SENSOR;
 if((rc=init_hwmon()))
  return(rc);
 /* Reset the fan controls */
 for(i=0; i<MAX_FANS; i++)
  fctrl[i].mode=FANCTRL_NONE;
 return(install_tick_handler());
}

/* Reconfigures the hardware monitor */

int reconfigure_hwmon()
{
 int i, rc;

 if((rc=init_hwmon()))
  return(rc);
 for(i=0; i<dp.num_fans; i++)
  set_fan_params(i, &fctrl[i]);
 return(0);
}

/* Terminates the control loop, returning all fans to their default mode */

void shutdown_hwmon()
{
 int i;

 for(i=0; i<dp.num_fans; i++)
 {
  fctrl[i].mode=FANCTRL_NONE;
  set_fan_params(i, &fctrl[i]);
 }
}
