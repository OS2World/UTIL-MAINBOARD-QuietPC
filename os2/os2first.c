/* $Id: os2first.c,v 1.2 2004/12/16 08:04:35 root Exp $ */

/* OS/2 device driver header */

#define INCL_BASE
#include <os2.h>

#include <devhdr.h>
#include <strat2.h>
#include <reqpkt.h>

extern void strategy();

/* OS/2 device header */

static DDHDR ddhdrs[2]=
{
 {
  (PVOID)0xFFFFFFFF,
  (USHORT)DEVLEV_3|DEV_30|DEV_CHAR_DEV,
  (USHORT)strategy,
  (USHORT)NULL,
  "QUIETPC$",
  0,
  0,
  0,
  0,
  DEV_SAVERESTORE|DEV_INITCOMPLETE
 } 
};
