/* $Id: apmcalls.h,v 1.2 2004/12/16 08:04:35 root Exp $ */

/* APM IDC Routines (from ThinkPad Dock II driver) */

typedef struct _APMEVENT
{
 USHORT Function;
 ULONG ulParm1;
 ULONG ulParm2;
} APMEVENT;
typedef APMEVENT FAR *PAPMEVENT;

typedef USHORT (FAR *APMHANDLER)(PAPMEVENT p);

/* Prototypes */

USHORT FAR APMAttach(void);
USHORT FAR APMRegister(APMHANDLER Handler, ULONG NotifyMask, USHORT DeviceID);
USHORT FAR APMDeregister( void );

#define APM_SETPWRSTATE          0x6
#define APM_NORMRESUMEEVENT      0x8
#define APM_CRITRESUMEEVENT      0x9

#define APM_NOTIFYSETPWR     (1<<APM_SETPWRSTATE)
#define APM_NOTIFYNORMRESUME (1<<APM_NORMRESUMEEVENT)
#define APM_NOTIFYCRITRESUME (1<<APM_CRITRESUMEEVENT)

#define APM_PWRSTATEREADY        0x0
