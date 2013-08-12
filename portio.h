/* $Id: portio.h,v 1.3 2005/12/18 20:42:11 root Exp $ */

#ifndef PORTIO_INCLUDED
#define PORTIO_INCLUDED

#ifdef USERMODE
 #define EXPORTED _far
 void EXPORTED priv_outp(unsigned short p, unsigned char v);
 unsigned char EXPORTED priv_inp(unsigned short p);
 #define inp priv_inp
 #define outp priv_outp
#endif

#ifdef LINUX
 #include <asm/io.h>
 #define inp(p) inb(p)
 #define outp(p, v) outb(v, p)
#endif

#endif
