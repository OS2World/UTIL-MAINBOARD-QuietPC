/* $Id: ite.c,v 1.4 2006/09/09 14:16:25 andrew_belov Exp $ */

/* Code for ITE IT8712 and compatibles. */

#include "quietpc.h"
#include "quietpcp.h"
#include "regio.h"
#include "portio.h"

/* Configuration mode */

#define ITE_BASE_IDX          0x002E    /* Peephole port */
#define ITE_BASE_VAL (ITE_CONF_BASE+1)  /* Value register */
#define read_base_reg(r) read_based_reg(ITE_BASE_IDX, r)
#define write_base_reg(r, v) write_based_reg(ITE_BASE_IDX, r, v)

#define ITE_LDN_SEL             0x07    /* LDN selector */
#define ITE_HWMON_LDN           0x04    /* LDN that corresponds to
                                           hardware monitor */
 #define ITE_HWMON_ENABLE       0x30    /* Enablement flags */
 #define ITE_HWMON_ADDRESS_HI   0x60    /* Base addr #15...8 */
 #define ITE_HWMON_ADDRESS_LO   0x61    /* Base addr # 7...0 */

#define ITE_BASE_ID1            0x20    /* 0x87 */
#define ITE_BASE_ID2            0x21    /* 0x12 */

#define IDX_SHIFT                  5    /* Index subport */
#define ITE_HWMON_IDX (hwmon_gateway+IDX_SHIFT)
#define VAL_SHIFT                  6    /* Data subport */
#define ITE_HWMON_VAL (hwmon_gateway+VAL_SHIFT)
#define read_reg(r) read_based_reg(ITE_HWMON_IDX, r)
#define write_reg(r, v) write_based_reg(ITE_HWMON_IDX, r, v)

#define ITE_TAC_DIV_REG         0x0B    /* Tachometer divisors */
#define ITE_TAC_SWITCHBOARD     0x0C    /* Tachometer controls */
#define ITE_FAN_CTL_MAIN        0x13    /* Main control register */
#define ITE_FAN_SWITCHBOARD     0x14    /* Freq/ON-OFF control */
#define ITE_FAN1_PWM            0x15    /* Base for FAN1...FAN3 */
#define ITE_VENDOR_ID_REG       0x58    /* 0x90 */
#define ITE_CODE_ID_REG         0x5B    /* 0x12 for IT8712 */

static unsigned short hwmon_gateway=0x290;

/* Reads from an LDN-addressed register */

static unsigned char read_ldn_reg(int ldn, int reg)
{
 unsigned char rc;

 write_base_reg(ITE_LDN_SEL, (unsigned char)ldn);
 rc=read_base_reg(reg);
 return(rc);
}

/* Writes to a banked RAM register */

static void write_ldn_reg(int ldn, int reg, unsigned char val)
{
 write_base_reg(ITE_LDN_SEL, (unsigned char)ldn);
 write_base_reg(reg, val);
}

/* Unlocks the device for LDN base selector access */

static void unlock_ite_base()
{
 outp(0x2E, 0x87);
 outp(0x2E, 0x01);
 outp(0x2E, 0x55);
 outp(0x2E, 0x55);
}

/* Locks the device, preventing further access to LDNs */

static void lock_ite_base()
{
 write_base_reg(0x02, (unsigned char)(read_base_reg(0x02)|0x02));
}

/* Queries fan divisor value (the monitoring results should be shifted left
   by divisor) */

static int get_fan_divisor(int num)
{
 unsigned char f;

 if(num>2)
  return(-1);
 f=read_reg(ITE_TAC_DIV_REG);
 switch(num)
 {
  case 0:
   return(f&0x07);
  case 1:
   return((f>>3)&0x07);
  case 2:
   return((f&0x40)?8:2);
 }
 return(1);
}

/* Not implemented */

static void set_fan_divisor(int num, int v)
{
}

/* Retrieves the fan RPM, if any */

static long query_rpm(short fan)
{
 long c;

 if(fan>2)
  return(-1L);
 c=read_reg(0x0D+fan)+((long)read_reg(0x18+fan)<<8);
 if(c<=0||c>=65535)
  return(-1L);
 return(345521023L/(c<<(long)get_fan_divisor(fan)));
}

/* Reports the temperature index i - in the internal 0...125 [0...251] range.
   Negative values mean failure. */

static short query_temperature(short i)
{
 if(i>2)
  return(-1);
 return(read_reg(0x29+i)<<1);
}

/* Take the control of the given fan */

static void grab_fan(int fan)
{
 if(fan<0||fan>=2)
  return;
 /* Enable tachometer and SmartGuardian<tm> mode */
 write_reg(ITE_FAN_CTL_MAIN, (unsigned char)(read_reg(ITE_FAN_CTL_MAIN)|(0x11<<fan)));
}

/* Sets the PWM parameters */

static void set_pwm(int fan, int div, int hifreq)
{
 unsigned char f;

 if(fan<0||fan>2)
  return;
 grab_fan(fan);
 /* ITE has nothing close to a "divisor", but it provides us with 8 frequency
    options. Consider 23.43 kHz (101) as "low" and 375 kHz (000) as "high".
    Note they have global effect... */
 f=(read_reg(ITE_FAN_SWITCHBOARD)&0x07)|0x80; /* ...as do the polarity and
                                          minimum duty cycle options which
                                          we have flipped surreptitiously */
 f|=(hifreq?0x00:0x50);
 write_reg(ITE_FAN_SWITCHBOARD, f);
}

/* Sets the duty cycle */

static void set_dc(int fan, unsigned char dc)
{
 if(fan<0||fan>2)
  return;
 write_reg(ITE_FAN1_PWM+fan, (unsigned char)((dc>0x7F)?0x7F:dc));
}

/* No spinup magic here */

static short spinup_magic(int event, int fan, struct fan_ctrl *c)
{
 return(SPINUP_NOT_SUPPORTED);
}

/* ITE hardware monitor probe routine */

static int probe_hwmon(unsigned int base)
{
 unsigned char c;
 outp(base+IDX_SHIFT, ITE_VENDOR_ID_REG);
 c=inp(base+VAL_SHIFT);
 outp(base+IDX_SHIFT, ITE_CODE_ID_REG);
 return((inp(base+VAL_SHIFT)+(c<<8))==0x9012);
}

/* Returns the base address of ITE hardware monitor */

static unsigned int query_base_addr()
{
 return(read_ldn_reg(ITE_HWMON_LDN, ITE_HWMON_ADDRESS_LO)|(read_ldn_reg(ITE_HWMON_LDN, ITE_HWMON_ADDRESS_HI)<<8));
}

/* Locates a new region and configures the ITE card for it */

static unsigned int locate_and_configure()
{
 unsigned int p;

 p=query_base_addr();
 if((p>0x20||p<=0xFFF9)&&probe_hwmon(p))
  return(p);
 for(p=0x280; p<0x340; p+=8)
 {  
  if(!is_region_used(p, 8))
  {
   write_ldn_reg(ITE_HWMON_LDN, ITE_HWMON_ADDRESS_LO, (unsigned char)(p&0xFF));
   write_ldn_reg(ITE_HWMON_LDN, ITE_HWMON_ADDRESS_HI, (unsigned char)(p>>8));
   return((query_base_addr()==p&&probe_hwmon(p))?p:0x0000);
  }
 }
 return(0x0000);
}

/* Detection routine */

int detect_ite(struct device_properties *pdp)
{
 /* First aim for a simple approach - look for IDs at the well-known
    base address 0x290. */
 if(!(read_reg(ITE_VENDOR_ID_REG)==0x90&&read_reg(ITE_CODE_ID_REG)==0x12))
 {
  /* Perform the usual lookup if not matched */
  unlock_ite_base();
  if(read_base_reg(ITE_BASE_ID1)!=0x87||read_base_reg(ITE_BASE_ID2)!=0x12)
  {
   lock_ite_base();
   return(-1);
  }
  /* Here we are certain that ITE is up. Now find an empty location for its
     hardware monitor */
  if((hwmon_gateway=locate_and_configure())==0x0000)
   return(-1);
  lock_ite_base();
 }
 /* Enable tachometers */
 write_reg(ITE_TAC_SWITCHBOARD, (unsigned char)(read_reg(ITE_TAC_SWITCHBOARD)|0x07));
 write_reg(ITE_FAN_CTL_MAIN, (unsigned char)(read_reg(ITE_FAN_CTL_MAIN)|0x70));
 write_reg(0x0B, 0x6d);         /* 32:32:8 divisor layout */
 pdp->hwmon_chip=HWMON_CHIP_IT8712;
 pdp->num_sensors=3;
 pdp->num_fans=3;               /* Actually more */
 pdp->max_dc=127;
 pdp->temp_granularity=2;
 pdp->spinup_magic=spinup_magic;
 pdp->set_fan_divisor=set_fan_divisor;
 pdp->query_rpm=query_rpm;
 pdp->query_temperature=query_temperature;
 pdp->set_pwm=set_pwm;
 pdp->set_dc=set_dc;
 return(0);
}
