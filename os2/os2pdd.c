/* $Id: os2pdd.c,v 1.3 2005/12/18 20:42:11 root Exp $ */

/* OS/2 physical driver framework */

#define INCL_BASE
#include <os2.h>

#include <devhdr.h>
#include <devcmd.h>
#include <strat2.h>
#include <reqpkt.h>
#include <dhcalls.h>
#include "apmcalls.h"

#include <quietpc.h>
#include <quietpcp.h>
#include <hwmon.h>
#define RC_BUG  (STATUS_DONE|STERR|ERROR_I24_INVALID_PARAMETER)

#ifndef USERMODE

PFN Device_Help=0;
extern char EndOfCode, EndOfData;

#endif

void exp_tick_handler();

/* Installs the handler */

int install_tick_handler()
{
 #ifndef USERMODE
  return(DevHelp_SetTimer((NPFN)exp_tick_handler));
 #else
  return(0);
 #endif
}

/* IOCtl router */

unsigned short hwmon_os2_ioctl(PRP_GENIOCTL p)
{
 USHORT rc, v;
 PVOID pparm, pdata;
 USHORT parm_len, data_len;
 
 pparm=p->ParmPacket;
 pdata=p->DataPacket;
 parm_len=p->ParmLen;
 data_len=p->DataLen;
 /* Verify the access to parameter/result data areas */
#ifndef USERMODE
 if(p->Function&0x20||p->Function==QPC_CMD_RESET)
 {
  if(DevHelp_VerifyAccess(SELECTOROF(pdata), data_len, OFFSETOF(pdata),
                          VERIFY_READWRITE))
   return(RC_BUG);
 }
 if(((p->Function&0x20)==0||p->Function==QPC_CMD_QUERY_HWMON_PARAMS)&&
    p->Function!=QPC_CMD_RESET)
 {
  if(DevHelp_VerifyAccess(SELECTOROF(pparm), parm_len, OFFSETOF(pparm),
                          VERIFY_READONLY))
   return(RC_BUG);
 }
#endif
 return(hwmon_ioctl(p->Function, pparm, parm_len, pdata, data_len)?RC_BUG:STATUS_DONE);
}

#ifndef USERMODE

/* Responds to APM BIOS events */

static USHORT FAR apm_hdl(PAPMEVENT pev)
{
 USHORT pstate;

 switch((USHORT)pev->ulParm1)
 {
  case APM_SETPWRSTATE:
   if((USHORT)(pev->ulParm2>>16)!=APM_PWRSTATEREADY)
   {
    /* Nothing to be done for suspend */
   }
   break;
  case APM_NORMRESUMEEVENT:;            /* fallthru */
  case APM_CRITRESUMEEVENT:
   reconfigure_hwmon();
   break;
 }
 return(0);
}

/* Strategy routine */

void far strategy()
{
 RPH FAR *p;

 /* Save the request packet address */
 _asm
 {
  mov word ptr p[0], bx
  mov word ptr p[2], es
 }
 /* What kind of command was it? */
 switch(p->Cmd)
 {
  case CMDInit:
   Device_Help=((PRPINITIN)p)->DevHlpEP;
   if(configure_hwmon())
   {
    p->Status=RC_BUG;
    break;
   }
   p->Status=STATUS_DONE;
   ((PRPINITOUT)p)->CodeEnd=(USHORT)&EndOfCode;
   ((PRPINITOUT)p)->DataEnd=(USHORT)&EndOfData;
   break;
  case CMDInitComplete:
   if(!APMAttach())
    APMRegister(apm_hdl, APM_NOTIFYSETPWR|APM_NOTIFYNORMRESUME|APM_NOTIFYCRITRESUME, 0);
   p->Status=STATUS_DONE;
   break;
  case CMDGenIOCTL:
   if(((PRP_GENIOCTL)p)->Category==IOCTL_CAT_QPC)
    p->Status=hwmon_os2_ioctl((PRP_GENIOCTL)p);
   else
    p->Status=RC_BUG;                   /* Bogus request */
   break;
  case CMDSaveRestore:
   if(((PRPSAVERESTORE)p)->FuncCode==0)
    ;                                   /* Save => do nothing */
   else if(((PRPSAVERESTORE)p)->FuncCode==1)
    reconfigure_hwmon();
   p->Status=STATUS_DONE;
   break;
  case CMDOpen:
  case CMDClose:
  case CMDDeInstall:
  case CMDShutdown:
   p->Status=STATUS_DONE;
   break;
  default:
   p->Status=STATUS_DONE|STERR|STATUS_ERR_UNKCMD;
 }
}

#endif
