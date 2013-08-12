/* $Id: regio.h,v 1.2 2004/12/16 08:04:35 root Exp $ */

#ifndef REGIO_INCLUDED
#define REGIO_INCLUDED

unsigned char read_based_reg(unsigned short baseport, int reg);
void write_based_reg(unsigned short baseport, int reg, unsigned char val);
int is_region_used(unsigned int base, int numports);

#endif
