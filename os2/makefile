# $Id: makefile,v 1.4 2006/09/09 14:16:25 andrew_belov Exp $

!ifdef EXPERIMENTAL
EXP_COPT = /DUSE_SPINUP=1
!endif
COPT = -Fo$@ -c -Lp -D__OS2__ -G2 -Gs -I.. -Oxarz -B2C2L $(EXP_COPT)
DRIVER_COPT = $(COPT) -Asfu -Zlp
USER_COPT = $(COPT) -AC
TESTCASE_COPT = $(USER_COPT) -DUSERMODE
ASMOPT = -Fo$@ -c
LINKOPT = /MAP:FULL

OBJS = regio.obj hwmon.obj winbond.obj ite.obj helpers.obj os2pdd.obj
KOBJS = os2first.kbj $(OBJS:.obj=.kbj)
UOBJS = $(OBJS:.obj=.ubj) userland.ubj contrib\varstck.obj
XOBJS = userland.obj contrib\varstck.obj

all: quietpc.sys qctrl.exe testcase.exe

quietpc.sys: $(KOBJS)
 link $(LINKOPT) $(KOBJS),$@,$(@B).map,os2286+dhcalls+apmcalls+clibcep,quietpc.def;
 mapsym $(@B).map
!ifndef NO_PACK
 nelite $@ $@ /P:255
!endif

qctrl.exe: $(XOBJS)
 link $(LINKOPT) /STACK:30720 $(XOBJS),$@,$(@B).map,clibcep+os2286,qctrl.def;
 mapsym $(@B).map
!ifndef NO_PACK
 nelite $@ $@ /P:255
!endif

testcase.exe: $(UOBJS)
 link $(LINKOPT) $(UOBJS) /STACK:30720,$@,$(@B).map,clibcep+os2286,testcase.def;
 mapsym $(@B).map

clean: distclean
 -del *.sym *.err *.exe *.sys

distclean:
 -del *.?bj *.map testcase.exe testcase.sym

.c.kbj:
 cl $(DRIVER_COPT) $<
{..}.c.kbj:
 cl $(DRIVER_COPT) $<

.c.ubj:
 cl $(TESTCASE_COPT) $<
{..}.c.ubj:
 cl $(TESTCASE_COPT) $<

.c.obj:
 cl $(USER_COPT) $<
{..}.c.obj:
 cl $(USER_COPT) $<

.asm.kbj:
 ml $(ASMOPT) $<

.asm.ubj:
 ml $(ASMOPT) -DUSERMODE $<
