/* $Id: winbond.c,v 1.4 2006/09/09 14:16:25 andrew_belov Exp $ */

/* Code for Winbond W83697xx and compatibles. */

#include "quietpc.h"
#include "quietpcp.h"
#include "regio.h"
#include "portio.h"

/* W83697[H]F configuration */

#define WINBOND_BASE           0x290    /* Standard I/O port for Winbond */
#define WINBOND_IDX (WINBOND_BASE+5)    /* Address register */
#define WINBOND_VAL (WINBOND_BASE+6)    /* Value register */
#define read_reg(r) read_based_reg(WINBOND_IDX, r)
#define write_reg(r, v) write_based_reg(WINBOND_IDX, r, v)

#define WINBOND_F1SCL_REG       0x00    /* Fan 1 prescale */
#define WINBOND_F1PWM_REG       0x01    /* Fan 1 PWM duty cycle */
#define WINBOND_F2SCL_REG       0x02    /* Fan 2 prescale */
#define WINBOND_F2PWM_REG       0x03    /* Fan 2 PWM duty cycle */
#define WINBOND_FAN_CFG_REG     0x04    /* Fan configuration */
 #define WINBOND_MANUAL_PWM     0x00    /* Manual PWM control mode */
/* W83697HF ONLY */
#define WINBOND_F1TGT_REG       0x05    /* Fan 1 target speed/temperature */
#define WINBOND_F2TGT_REG       0x06    /* Fan 2 target speed/temperature */
#define WINBOND_FAN_TOL_REG     0x07    /* Fan tolerance register */
#define WINBOND_F1STOP_REG      0x08    /* Fan 1 shutdown rate */
#define WINBOND_F2STOP_REG      0x09    /* Fan 2 shutdown rate */
#define WINBOND_F1START_REG     0x0A    /* Fan 1 startup rate */
#define WINBOND_F2START_REG     0x0B    /* Fan 2 startup rate */
#define WINBOND_F1TSTOP_REG     0x0C    /* Fan 1 stop time (x0.100 sec) */
#define WINBOND_F2TSTOP_REG     0x0D    /* Fan 2 stop time (x0.100 sec) */
#define WINBOND_FAN_DOWN_REG    0x0E    /* Fan step-down time (x0.100 sec) */
#define WINBOND_FAN_UP_REG      0x0F    /* Fan step-up time (x0.100 sec) */

#define WINBOND_TEMP2_REG       0x27    /* Secondary temperature register */
#define WINBOND_F1REV_REG       0x28    /* Time per revolution of fan 1 */
#define WINBOND_F2REV_REG       0x29    /* Time per revolution of fan 2 */
#define WINBOND_FAN_DIV_REG     0x47    /* LS bits of divisors */

#define WINBOND_BANK_SEL        0x4E    /* Bank selection register */
#define WINBOND_VENDOR_REG      0x4F    /* Vendor ID register */
#define WINBOND_CHIP_REG        0x58    /* Chip ID register */

#define WINBOND_VENDOR_ID     0x5CA3    /* Vendor ID */
#define WINBOND_CHIP_ID         0x60    /* Chip ID */

#define WINBOND_GET_MSB         0x80    /* 0x4E flag to retrieve MSB of 0x4F */
#define WINBOND_GET_LSB         0x00    /* 0x4E flag to retrieve LSB of 0x4F */
#define WINBOND_BANK            0x50    /* Banked RAM start */

/* Bank 0 */
#define WINBOND_VBAT_FAN_REG    0x0D    /* VBAT/fan divider register */
/* Bank 1 */
#define WINBOND_TEMP_HIGH_REG   0x00    /* Temperature MSB */
#define WINBOND_TEMP_LOW_REG    0x01    /* Temperature LSB */
 #define WINBOND_HALF_DEGREE    0x80    /* Half-degree bit */

/* A helper macro to get the "ticks" (period value for computing RPMs) */
#define get_ticks(fan) (read_reg(WINBOND_F1REV_REG+fan))

/* Fan spin-up acceleration parameters -- AAB 07/09/2003 +
   Fan spin-down (auto-shutdown) parameters -- AAB 26/09/2003 */

#ifdef USE_SPINUP
#define SPINUP_THRESHOLD         11     /* Anything more won't require a
                                           spin-up */
#define SPINUP_ACCEL            115     /* Anything less will
                                           accelerate to this value */
#define SPINUP_TICKS              4     /* # of successive returns to
                                           grant spinup */
/* The fan is considered OK if enough RPMs have been gained */
#define SPINUP_RPM_OK(rpm) ((rpm)>=3400L&&(rpm)<=10500L)
/* Duty cycle values for which spindown control will be active */
#define CHECK_DC_FOR_SPINDOWN(dc) (dc>=1&&dc<=5)
/* Duty cycle to which spindown will be performed (this will also be
   reflected in the fan control record, hence values actually meaning a
   "spin-up" are tolerable as well) */
#define SPINDOWN_DC                 0
/* Ensures a "stable" state */
#define SPINDOWN_STABLE(f) (f==3||f==4||f>=60&&f<=254)
/* Delays */
#define STABLE_DELAY              250   /* Delay AFTER switching to a "dangerous"
                                           value */
#define CHECK_DELAY                45   /* # of successive "wrong" results to force a
                                           spin-down */
#define RESTORE_DELAY              15   /* # of successive "correct" results to
                                           cancel the countdown */
#endif

/* Reads from a banked RAM register */

static unsigned char read_bank_reg(int bank, int reg)
{
 unsigned char rc;

 write_reg(WINBOND_BANK_SEL, (unsigned char)bank);
 rc=read_reg(WINBOND_BANK+reg);
 write_reg(WINBOND_BANK_SEL, (unsigned char)0);
 return(rc);
}

/* Writes to a banked RAM register */

static void write_bank_reg(int bank, int reg, unsigned char val)
{
 write_reg(WINBOND_BANK_SEL, (unsigned char)bank);
 write_reg(WINBOND_BANK+reg, val);
}

/* Queries fan divisor value (the monitoring results should be shifted left
   by divisor) */

static int get_fan_divisor(int num)
{
 unsigned char f;
 int rc;

 f=read_reg(WINBOND_FAN_DIV_REG);
 rc=(f>>(4+num*2))&0x03;
 rc|=(read_bank_reg(0, WINBOND_VBAT_FAN_REG)&(32<<num))?4:0;
 return(rc);
}

/* Sets the fan divisor value */

static void set_fan_divisor(int num, int v)
{
 unsigned char f;

 f=read_reg(WINBOND_FAN_DIV_REG);
 if(num==0)
 {
  f&=0xCF;
  f|=(v&0x03)<<4;
 }
 else
 {
  f&=0x3F;
  f|=(v&0x03)<<6;
 }
 write_reg(WINBOND_FAN_DIV_REG, f);
 f=read_bank_reg(0, WINBOND_VBAT_FAN_REG);
 if(num==0)
 {
  f&=0xDF;
  f|=(v&0x04)<<3;
 }
 else
 {
  f&=0x7F;
  f|=(v&0x04)<<4;
 }
 write_bank_reg(0, WINBOND_VBAT_FAN_REG, f);
}

/* Queries the PWM divisor */

static int query_pwm_div(int fan)
{
 if(fan<0||fan>=2)
  return(-1);
 return(read_reg(WINBOND_F1SCL_REG+fan*2)&0x7F);
}

/* Retrieves the fan RPM, if any */

static long query_rpm(short fan)
{
 long c;
 
 c=(long)get_ticks(fan);
 if(c<=0||c>=255)
  return(-1L);
 return(1346000L/(c<<(long)get_fan_divisor(fan)));
}

/* Reports the temperature index i - in the internal 0...125 [0...251] range.
   Negative values mean failure. */

static short query_temperature(short i)
{
 int t;

 if(i==0)
 {
  t=(int)read_bank_reg(1, WINBOND_TEMP_HIGH_REG);
  if(t>=128)                            /* Negative values */
   return(0);
  if(t>=125)
   return(251);
  t<<=1;
  if(read_bank_reg(1, WINBOND_TEMP_LOW_REG)&WINBOND_HALF_DEGREE)
   t++;
  return(t);
 }
 else if(i==1)
 {
  t=read_reg(WINBOND_TEMP2_REG);
  if(t>=128)
   return(0);
  if(t>=125)
   return(251);
  return(t<<1);
 }
 else
  return(-1);
}

/* Sets the fan control mode */

static void set_fan_ctrl_mode(int fan, int mode)
{
 unsigned char f;

 if(fan<0||fan>=2)
  return;
 /* Enable PWM output */
 f=(read_reg(WINBOND_FAN_CFG_REG)&0xFC);
 if(fan==0)
 {
  f&=0xF3;
  f|=(mode<<2);
 }
 else if(fan==1)
 {
  f&=0xCF;
  f|=(mode<<4);
 }
 write_reg(WINBOND_FAN_CFG_REG, f);
}

/* Sets the PWM parameters */

static void set_pwm(int fan, int div, int hifreq)
{
 unsigned char f;

 if(fan<0||fan>=2)
  return;
 set_fan_ctrl_mode(fan, WINBOND_MANUAL_PWM);
 f=(hifreq?0:0x80)+(div&0x7F);
 write_reg(WINBOND_F1SCL_REG+fan*2, f);
}

/* Sets the duty cycle */

static void set_dc(int fan, unsigned char dc)
{
 if(fan<0||fan>=2)
  return;
 write_reg(WINBOND_F1SCL_REG+fan*2+1, dc);
}

/* Spinup magic */

static short spinup_magic(int event, int fan, struct fan_ctrl *c)
{
#ifdef USE_SPINUP
 unsigned long rpm;
 unsigned char ticks;
 unsigned char d;

 d=c->last_d;
 if(event==SPINUP_IDLE)
 {
  /* Spinup */
  if(c->spinup_underway>0)
  {
   rpm=query_rpm(fan);
   if(SPINUP_RPM_OK(rpm))
    c->spinup_underway--;
   else
    c->spinup_underway=SPINUP_TICKS;    /* Penalty! Keep retrying the spinup */
   if(c->spinup_underway==0)
    set_dc(fan, c->last_d);
  }
  /* Steady spindown control */
  else if(c->spindown_delay==0)
  {
   ticks=get_ticks(fan);
   if(!SPINDOWN_STABLE(ticks))
   {
    c->spindown_countdown--;
    c->spindown_countup=RESTORE_DELAY;
    if(c->spindown_countdown==0)
    {
     /* We are at spindown. */
     set_dc(fan, SPINDOWN_DC);
     c->prev_d=c->last_d;
     c->last_d=SPINDOWN_DC;
     c->spindown_delay=NO_SPINDOWN;
     c->spindown_events++;
    }
   }
   else
   {
    if(c->spindown_countdown!=CHECK_DELAY)
    {
     c->spindown_countup--;
     if(c->spindown_countup==0)
      c->spindown_countdown=CHECK_DELAY;
    }
   }
  }
  /* Spindown check */
  else if(c->spindown_delay<NO_SPINDOWN)
  {
   c->spindown_delay--;
   /* Arm the spindown check */
   if(c->spindown_delay=0)
    c->spindown_countdown=CHECK_DELAY;
  }
 }
 else if(event==SPINUP_RESTART)
 {
  c->spinup_underway=0;
  c->spindown_delay=(CHECK_DC_FOR_SPINDOWN(d))?STABLE_DELAY:NO_SPINDOWN;
 }
 else if(event==SPINUP_RISE)
 {
  if(c->f_spinup&&d<SPINUP_THRESHOLD)
  {
   set_dc(fan, SPINUP_ACCEL);
   c->spinup_underway=SPINUP_TICKS;
  }
  else
  {
   set_dc(fan, d);
   c->spindown_delay=(CHECK_DC_FOR_SPINDOWN(d))?STABLE_DELAY:NO_SPINDOWN;
   c->spinup_underway=0;
  }
 }
 else if(event==SPINUP_FALL)
 {
  c->spinup_underway=0;
  c->spindown_delay=(CHECK_DC_FOR_SPINDOWN(c->last_d))?STABLE_DELAY:NO_SPINDOWN;
 }
 return(SPINUP_OK);
#else
 return(SPINUP_NOT_SUPPORTED);
#endif
}

/* Detection routine */

int detect_w83697(struct device_properties *pdp)
{
 unsigned char l1, l2;
 unsigned short v;

 /* Retrieve the vendor id */
 write_reg(WINBOND_BANK_SEL, WINBOND_GET_MSB);
 v=(unsigned short)read_reg(WINBOND_VENDOR_REG);
 v<<=8;
 write_reg(WINBOND_BANK_SEL, WINBOND_GET_LSB);
 v|=read_reg(WINBOND_VENDOR_REG);
 if(v!=WINBOND_VENDOR_ID)
  return(-1);
 if(read_reg(WINBOND_CHIP_REG)!=WINBOND_CHIP_ID)
  return(-1);
 /* We are certain it's Winbond. Let's check if it is W83697HF
    (one that provides automatic control - not implemented here) */
 l1=read_reg(WINBOND_F2START_REG);
 write_reg(WINBOND_F2START_REG, (unsigned char)(l1^1)); /* Mess up with it */
 l2=read_reg(WINBOND_F2START_REG)^1;
 write_reg(WINBOND_F2START_REG, l1);
 pdp->hwmon_chip=(l1!=l2)?HWMON_CHIP_W83697F:HWMON_CHIP_W83697HF;
 pdp->num_sensors=2;
 pdp->num_fans=2;
 pdp->max_dc=255;
 pdp->temp_granularity=1;
 pdp->spinup_magic=spinup_magic;
 pdp->set_fan_divisor=set_fan_divisor;
 pdp->query_rpm=query_rpm;
 pdp->query_temperature=query_temperature;
 pdp->set_pwm=set_pwm;
 pdp->set_dc=set_dc;
 return(0);
}
