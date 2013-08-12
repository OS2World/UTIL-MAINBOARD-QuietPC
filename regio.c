/* $Id: regio.c,v 1.3 2005/12/18 20:42:11 root Exp $ */

/* General routines to access subport registers */

#include "quietpc.h"
#include "portio.h"

/* Reads a register */

unsigned char read_based_reg(unsigned int baseport, int reg)
{
 outp(baseport, (unsigned char)reg);
 return(inp(baseport+1));
}

/* Writes to a Winbond register */

void write_based_reg(unsigned int baseport, int reg, unsigned char val)
{
 outp(baseport, (unsigned char)reg);
 outp(baseport+1, val);
}

/* Checks if the I/O address region is occupied */

int is_region_used(unsigned int base, int numports)
{
 unsigned int i;
 unsigned char c=0xFF;

 for(i=0; i<numports; i++)
  c&=inp(base+i);
 if(c!=0xFF)
  return(1);
 outp(base, 0xF7);
 if(inp(base)!=0xFF)
 {
  outp(base, 0xFF);                     /* Oops! :*) */
  return(1);
 }
 return(0);
}
