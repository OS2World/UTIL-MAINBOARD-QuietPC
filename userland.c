/* $Id: userland.c,v 1.8 2007-01-30 19:30:46 andrew_belov Exp $ */

/* User-mode control program */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__OS2__)
 #define INCL_BASE
 #include <os2.h>
 #ifdef USERMODE
  #include <strat2.h>
  #include <reqpkt.h>
  #include "hwmon.h"
 #endif
#elif defined (LINUX)
 #include <fcntl.h>
 #include <unistd.h>
 #ifdef LINUX
  #include <sys/ioctl.h>
  #include <linux/fs.h>
 #endif
 #define stricmp strcasecmp
 #define memicmp strncasecmp
#endif

#ifdef __OS2__
 static char ascend[]={25, 0};
 static char descend[]={24, 0};
 static char degree[]={248, 0};
#else
 static char ascend[]="^";
 static char descend[]="v";
 static char degree[]=" deg. ";
#endif

#include "quietpc.h"

/* Graph parameters */

#define MAX_COL                  128
#define MAX_ROW                   64

/* Local variables */

#ifdef __OS2__
static HFILE hf;
#else
static int hf;
#endif
static int show_fan_cfg=0;
#ifdef USE_SPINUP
static int use_spinup=0;
#endif
static char is_processed[512];

/* Perform IOCTL with necessary checks */

static void do_ioctl(void *pData, unsigned int cbDataLenMax,  
                     void *pParams, unsigned int cbParmLenMax,
                     unsigned int function, unsigned int category, unsigned int hDevice)
{
 /* OS/2 */
#if defined(__OS2__)
#ifdef USERMODE
 hwmon_ioctl((unsigned short)function, pParams, (int)cbParmLenMax, pData, (int)cbDataLenMax);
#else
 USHORT rc;
 ULONG pcbParmLen=cbParmLenMax, pcbDataLen=cbDataLenMax;

 if(rc=
#ifdef __32BIT__
 DosDevIOCtl2(hDevice, category, function, pParams, cbParmLenMax,
                    &pcbParmLen, pData, cbDataLenMax, &pcbDataLen)
#else
 DosDevIOCtl2(pData, (USHORT)cbDataLenMax, pParams, (USHORT)cbParmLenMax,
              (USHORT)function, (USHORT)category, hDevice)
#endif
   )
 {
  printf("IOCtl #%02lx/%02lx failed, rc=%u\n", category, function, rc);
  DosClose(hDevice);
  exit(1);
 }
#endif
#elif defined(LINUX)
 struct ioctl_encaps prm;
 unsigned int rc;

 prm.pparm=pParams;
 prm.parm_len=cbParmLenMax;
 prm.pdata=pData;
 prm.data_len=cbDataLenMax;
 if((rc=ioctl(hf, function, &prm)))
 {
  printf("IOCtl #%02x failed, rc=%u\n", function, rc);
  exit(1);
 }
#else
#error No IOCtl method defined.
#endif
}

/* Reinitialize the driver */

static void reset()
{
 unsigned short rc;

 do_ioctl(&rc, 2, NULL, 0, QPC_CMD_RESET, IOCTL_CAT_QPC, hf);
 if(!rc)
  printf("Reset OK\n");
 else
  printf("Reset failed, rc=%u\n", rc);
}

/* Queries fan parameters */

static void query_fan_params(char num_fan, struct hwmon_info *hinfo)
{
 struct hwmon_fan_op g;
 unsigned short t=QPC_HWMON_QUERY_FAN;
 char *sv="";

 g.num_fan=num_fan;
 do_ioctl(&g, sizeof(g), &t, sizeof(t), QPC_CMD_QUERY_HWMON_PARAMS, IOCTL_CAT_QPC, hf);
 switch(g.fctrl.mode)
 {
  case FANCTRL_NONE:
   printf("The fan is running in standard mode, no fan control active\n");
   break;
  case FANCTRL_MANUAL_PWM:
   printf("Manual override mode:\n"
          "PWM frequency:       %s\n"
          "Divisor              %u\n"
          "Duty cycle           %u (%u%%)\n",
          g.fctrl.pwmi.hifreq?"High":"Low", g.fctrl.pwmi.prescale, g.fctrl.pwmi.duty_cycle,
          ((unsigned int)g.fctrl.pwmi.duty_cycle)*100/hinfo->max_dc);
   break;
  case FANCTRL_SW_MODE:
#ifdef USERMODE
  tick_handler();
  while(1)
  {
   tick_handler();
   do_ioctl(&g, sizeof(g), &t, sizeof(t), QPC_CMD_QUERY_HWMON_PARAMS, IOCTL_CAT_QPC, hf);
#endif
#ifdef USE_SPINUP
   if(g.fctrl.f_spinup)
    sv=g.fctrl.spinup_underway?", SPIN-UP":", steady";
#endif
   printf("Software control mode:\n"
          "Temperature:     %s%u%s%sC (min=%u%s%sC, max=%u%s%sC)\n"
          "Fan duty cycle:  %s%u%s\n"
          "Sensor errors:   %lu\n"
#ifdef USE_SPINUP
          "Spindown events: %lu\n"
#endif
          ,
          g.fctrl.prev_t<g.fctrl.last_t?ascend:descend, (unsigned int)g.fctrl.last_t>>1,
          (g.fctrl.last_t&1)?".5":"", degree,
          (unsigned int)g.fctrl.min_t>>1, (g.fctrl.min_t&1)?".5":"", degree,
          (unsigned int)g.fctrl.max_t>>1, (g.fctrl.max_t&1)?".5":"", degree,
          g.fctrl.prev_d<g.fctrl.last_d?ascend:descend, g.fctrl.last_d, sv,
          g.fctrl.error_rate
#ifdef USE_SPINUP
          , g.fctrl.spindown_events
#endif
          );
#if defined(__OS2__)&&defined(USERMODE)
    DosSleep(1000);
   }
#endif
   break;
  default:
   printf("The fan operation mode is unknown [%u]\n", g.fctrl.mode);
 }
}

/* General-purpose linear interpolation function. Don't ask for a higher
   order, it is only confusing when the fan control is concerned. */

unsigned char interp(unsigned char x, unsigned char xy[][2], int xy_total)
{
 int i;
 long rc;

 for(i=0; i<xy_total; i++)
 {
  if(x>=xy[i][0])
   break;
 }
 i--;
 if(i<0)
  i=0;
 if(i>=xy_total)
  i=xy_total-1;
 if(xy[i][0]==xy[i+1][0])
  return(xy[i][1]);
 rc=(long)xy[i][1]+((long)xy[i+1][1]-(long)xy[i][1])*((long)x-(long)xy[i][0])/((long)xy[i+1][0]-(long)xy[i][0]);
 if(rc<0)
  rc=0;
 if(rc>255)
  rc=255;
 return((unsigned char)rc);
}

/* Sorts the lists of reference coordinates */

static void sort_list(unsigned char l[][2], int num_entries)
{
 int i, j;
 unsigned char t;

 for(i=0; i<num_entries; i++)
 {
  for(j=0; j<i; j++)
  {
   if(l[i][0]>l[j][0])
   {
    t=l[j][0]; l[j][0]=l[i][0]; l[i][0]=t;
    t=l[j][1]; l[j][1]=l[i][1]; l[i][1]=t;
   }
  }
 }
}

/* Software fan control startup */

static int parse_sw_control(struct hwmon_fan_op *g, FILE *stream, struct hwmon_info *hinfo)
{
 /* (1) read control points, then
    (2) estimate the Y coordinate, then
    (3) add missing X coordinates and pack into g->dc_rise/dc_fall */
 /* The left boundary should be chosen as minimum value from both graphs */
 char buf[MAX_COL], c, tmp[MAX_COL], desc[MAX_COL];
 char *t=NULL, *p;
 short rise[MAX_COL], fall[MAX_COL];
#ifndef __32BIT__
 static                         /* Hack */
#endif
 unsigned char marks[MAX_ROW][2], v_rise[MAX_ROW][2], v_fall[MAX_ROW][2];
 int i, j, temp, zero_found=0;
 unsigned int l=0, key, d, err=0, total_marks=0, total_rise, total_fall;
 int min_rise=32767, max_rise=-1, min_fall=32767, max_fall=-1;
 unsigned char min_i, max_i, min_v, max_v;
 int min_rise_c=0, max_rise_c=0, min_fall_c=0, max_fall_c=0;
 unsigned char f;

 memset(rise, 0xFF, sizeof(rise));
 memset(fall, 0xFF, sizeof(fall));
 while(fgets(buf, MAX_COL, stream)!=NULL)
 {
  l++;
  key=MAX_COL-l;
  if(l>=MAX_ROW)
  {
   fprintf(stderr, "Too many rows (%u)\n", l);
   return(++err);
  }
  if((t=strchr(buf, '\r'))!=NULL)
   *t='\0';
  if((t=strchr(buf, '\n'))!=NULL)
   *t='\0';
  if((t=strchr(buf, ';'))!=NULL)
   *t='\0';
  /* Find the Y axis */
  if((t=strchr(buf, '|'))==NULL&&(t=strchr(buf, '+'))==NULL)
   continue;
  memcpy(tmp, buf, t-buf);
  tmp[t-buf]='\0';
  d=atoi(tmp);
  if(d>0||strchr(tmp, '0')!=NULL)
  {
   marks[total_marks][0]=key;
   marks[total_marks++][1]=d;
  }
  /* Presence of at least two '+' characters will make us think we've
     reached the end. */
  if((p=strchr(t+1, '+'))!=NULL&&strchr(p+1, '+')!=NULL)
  {
   zero_found=1;
   break;
  }
  for(i=1; t[i]!='\0'; i++)
  {
   c=t[i];
   if(c=='*')
    rise[i]=fall[i]=key;
   else if(c=='^')
    rise[i]=key;
   else if(c=='v')
    fall[i]=key;
   /* Don't complain about any other characters */
  }
 }
 if(!zero_found||fgets(desc, MAX_COL, stream)==NULL)
 {
  fprintf(stderr, "Can't locate the X axis!\n");
  return(++err);
 }
 /* Convert the Y values into duty cycle */
 for(i=0; i<MAX_COL; i++)
 {
  if(rise[i]>=0)
  {
   rise[i]=interp((unsigned char)rise[i], marks, total_marks);
   if(rise[i]<min_rise)
   {
    min_rise=rise[i];
    min_rise_c=i;
   }
   if(rise[i]>max_rise)
   {
    max_rise=rise[i];
    max_rise_c=i;
   }
  }
  if(fall[i]>=0)
  {
   fall[i]=interp((unsigned char)fall[i], marks, total_marks);
   if(fall[i]<min_fall)
   {
    min_fall=fall[i];
    min_fall_c=i;
   }
   if(fall[i]>max_fall)
   {
    max_fall=fall[i];
    max_fall_c=i;
   }
  }
 }
 /* Parse the labels */
 total_marks=0;
 for(i=(t-buf); buf[i]!='\0'; i++)
 {
  if(buf[i]=='+')
  {
   /* Descend! */
   if(!isdigit(desc[i])&&desc[i]!='.')
   {
    /* AAB 27/05/2002: Hack to allow omitting labels for leftmost corner */
    if(i==(t-buf))
    {
     f=0;
     goto store_mark;
    }
    fprintf(stderr, "Line %u, column %u: malformed tick-mark\n", l, i+1);
    err++;
    continue;
   }
   /* Go left to find the beginning */
   for(j=i; j>=0&&(isdigit(desc[j])||desc[j]=='.'); j--)
   {
    if(j<i&&buf[j]=='+')
    {
     fprintf(stderr, "Line %u, column %u/%u: ambiguous label!\n", l, i+1, j+1);
     err++;
    }
   }
   j++;
   strcpy(tmp, desc+j);
   f=0;
   for(j=0; isdigit(tmp[j])||tmp[j]=='.'; j++)
   {
    if(tmp[j]=='.')
     f=1;                               /* Fraction => assume 0.5 degrees */
   }
   tmp[j]='\0';
   temp=atoi(tmp);
   if(temp<0||temp>125)
   {
    printf("Line %u: scale exceeds the temperature limits!\n", l);
    err++;
   }
   f+=(temp<<1);
store_mark:;
   marks[total_marks][0]=i-(t-buf);
   marks[total_marks++][1]=f;
  }
 }
 sort_list(marks, total_marks);
 /* Establish the left/right limits */
 if(min_rise<min_fall)
 {
  min_v=min_rise_c;
  fall[min_v]=rise[min_v];
 }
 else
 {
  min_v=min_fall_c;
  rise[min_v]=fall[min_v];
 }
 if(max_rise>max_fall)
 {
  max_v=max_rise_c;
  fall[max_v]=rise[max_v];
 }
 else
 {
  max_v=max_fall_c;
  rise[max_v]=fall[max_v];
 }
 /* The errors should drop off by now */
 if(err>0)
  return(err);
 /* Determine end points upon the freshly established X axis */
 min_i=interp(min_v, marks, total_marks);
 max_i=interp(max_v, marks, total_marks);
 if(min_i>250)
  min_i=250;
 if(max_i>250)
  max_i=250;
 /* Fill up the rise/fall curves. Pass 1: collect data. */
 total_rise=total_fall=0;
 for(i=min_v; i<=max_v; i++)
 {
  f=interp((unsigned char)i, marks, total_marks);
  if(rise[i]>=0)
  {
   v_rise[total_rise][0]=f;
   v_rise[total_rise++][1]=rise[i];
  }
  if(fall[i]>=0)
  {
   v_fall[total_fall][0]=f;
   v_fall[total_fall++][1]=fall[i];
  }
 }
 /* Sort 'em */
 sort_list(v_rise, total_rise);
 sort_list(v_fall, total_fall);
 /* OK, now pass 2: map the data to output array */
 for(i=min_i; i<=max_i; i++)
 {
  g->fctrl.dc_rise[i]=interp((unsigned char)i, v_rise, total_rise);
  g->fctrl.dc_fall[i]=interp((unsigned char)i, v_fall, total_fall);
 }
 for(i=0; i<min_i; i++)
  g->fctrl.dc_rise[i]=g->fctrl.dc_fall[i]=g->fctrl.dc_rise[min_i];
 for(i=max_i+1; i<251; i++)
  g->fctrl.dc_rise[i]=g->fctrl.dc_fall[i]=g->fctrl.dc_rise[max_i];
 /* Postprocessing: remove possible side-effects of interpolation
    (duty cycle values outside the min/max values on graph */
 for(i=0; i<251; i++)
 {
  if(g->fctrl.dc_rise[i]<g->fctrl.dc_rise[min_i])
   g->fctrl.dc_rise[i]=g->fctrl.dc_rise[min_i];
  else if(g->fctrl.dc_rise[i]>g->fctrl.dc_rise[max_i])
   g->fctrl.dc_rise[i]=g->fctrl.dc_rise[max_i];
  if(g->fctrl.dc_fall[i]<g->fctrl.dc_fall[min_i])
   g->fctrl.dc_fall[i]=g->fctrl.dc_fall[min_i];
  else if(g->fctrl.dc_fall[i]>g->fctrl.dc_fall[max_i])
   g->fctrl.dc_fall[i]=g->fctrl.dc_fall[max_i];
 }
 /* Let the user see it! */
 if(show_fan_cfg)
 {
  j=0;
  for(i=min_i; i<=max_i; i+=hinfo->temp_granularity)
  {
   if(j++%4==0)
    printf("\n");
   printf("%3u%s%sC: %3d\x18 \x19%-3d  ", i>>1, (i&1)?".5":"", degree, g->fctrl.dc_rise[i], g->fctrl.dc_fall[i]);
  }
  printf("\n");
#ifdef USE_SPINUP
  if(use_spinup)
   printf("Spinup enabled\n");
#endif
 }
 /* Continue the processing */
 return(err);
}

/* Set parameters for the specified fan */

static void set_fan_params(char fan, char *arg, struct hwmon_info *hinfo)
{
 struct hwmon_fan_op g;
 char *p, *a;
 FILE *f;
 int rc, as;

 g.num_fan=fan;
 g.cmd=QPC_HWMON_SET_FAN;
 if(!stricmp(arg, "RESET"))
  g.fctrl.mode=FANCTRL_NONE;
 else if(!memicmp(arg, "DIV=", 4))
 {
  a=arg+4;
  if(!isdigit(a[0]))
  {
   fprintf(stderr, "Fan divisor not specified in <%s>", arg);
   return;
  }
  g.fctrl.mode=FANCTRL_SET_DIVISOR;
  g.fctrl.div=atoi(a);
 }
 else if(!memicmp(arg, "DUTY=", 5))
 {
  a=arg+5;
  if(!isdigit(a[0]))
  {
   fprintf(stderr, "Fan duty cycle not specified in <%s>", arg);
   return;
  }
  g.fctrl.mode=FANCTRL_MANUAL_PWM;
  g.fctrl.pwmi.duty_cycle=atoi(a);
  g.fctrl.pwmi.prescale=((p=strchr(a, ':'))!=NULL&&isdigit(*++p))?atoi(p):1;
  g.fctrl.pwmi.hifreq=(strchr(a, 'l')!=NULL||strchr(a, 'L')!=NULL)?0:1;
 }
 else if(!memicmp(arg, "CTRL=", 5))
 {
  a=arg+5;
  as=atoi(a);
  if(as==0||(a=strchr(a, ','))==NULL)
  {
   fprintf(stderr, "Invalid syntax for /CTRL: use comma to separate filename from the\nsensor number (1-based)\n");
   return;
  }
  g.fctrl.ref_sensor=as-1;
  a++;
  if((f=fopen(a, "r"))==NULL)
  {
   fprintf(stderr, "Can't open <%s>\n", a);
   return;
  }
  rc=parse_sw_control(&g, f, hinfo);
  fclose(f);
  if(rc)
  {
   fprintf(stderr, "Can't read the software control settings\n");
   return;
  }
#ifdef USE_SPINUP
  g.fctrl.f_spinup=(char)use_spinup;
#endif
  g.fctrl.mode=FANCTRL_SW_MODE;
 }
 else
 {
  fprintf(stderr, "Bad or missing fan control option: <%s>", arg);
  return;
 }
 do_ioctl(NULL, 0, &g, sizeof(g), QPC_CMD_SET_HWMON_PARAMS, IOCTL_CAT_QPC, hf);
}

/* Checks the presence of hardware monitor */

static int check_hwmon(struct hwmon_info *hinfo)
{
 if(hinfo->hwmon_chip<0)
 {
  fprintf(stderr, "No hardware monitoring chip detected\n");
  return(1);
 }
 return(0);
}

/* Queries the current state of hardware monitor */

static void query(struct hwmon_info *hinfo)
{
 unsigned int i, m;
 static char hfmt[]="%-27s", vfmt[]="%8s", v[32], t[32];

 printf("Hardware monitor: ");
 switch(hinfo->hwmon_chip)
 {
  case HWMON_CHIP_NONE:
   printf("None");
   return;
  case HWMON_CHIP_W83697F:
   printf("Winbond W83697F");
   break;
  case HWMON_CHIP_W83697HF:
   printf("Winbond W83697HF");
   break;
  case HWMON_CHIP_IT8712:
   printf("ITE IT8712");
   break;
  default:
   printf("unknown chip #%u", hinfo->hwmon_chip);
   break;
 }
 m=(hinfo->num_fans>hinfo->num_sensors)?hinfo->num_fans:hinfo->num_sensors;
 printf(", max. duty cycle: %u, granularity %u%s%sC\n", hinfo->max_dc,
        hinfo->temp_granularity>>1, (hinfo->temp_granularity&1)?".5":"", degree);
 printf(hfmt, "");
 for(i=0; i<m; i++)
  printf("      #%u", i+1);
 printf("\n");
 printf(hfmt, "Fans (rpm)");
 for(i=0; i<m; i++)
 {
  if(i>=hinfo->num_fans)
   strcpy(v, "-");
  else if(hinfo->fan_rpm[i]<0)
   strcpy(v, "0");
  else if(hinfo->fan_rpm[i]>20000)
   strcpy(v, "ERR");
  else
   sprintf(v, "%ld", hinfo->fan_rpm[i]);
  printf(vfmt, v);
 }
 printf("\n");
 sprintf(t, "Temp.Sensors (%sC)", degree);
 printf(hfmt, t);
 for(i=0; i<m; i++)
 {
  if(i>=hinfo->num_sensors||hinfo->sensor_temp[i]<0||hinfo->sensor_temp[i]>=254)
   strcpy(v, "-");
  else
   sprintf(v, "%d%s", hinfo->sensor_temp[i]>>1, (hinfo->sensor_temp[i]&1)?".5":"");
  printf(vfmt, v);
 }
 printf("\n");
}

/* Handles POST code display configuration */

static void proc_post(struct hwmon_info *hinfo, char *params)
{
 short post_sensor;

 if(*params==':')
 {
  post_sensor=atoi(params+1);
  if(post_sensor<=0)
   post_sensor=-1;
  else
   post_sensor--;
  do_ioctl(NULL, 0, &post_sensor, sizeof(post_sensor), QPC_CMD_SET_POST, IOCTL_CAT_QPC, hf);
 }
 else
 {
  post_sensor=0x7FFF;                   /* Checkpoint */
  do_ioctl(&post_sensor, sizeof(post_sensor), NULL, 0, QPC_CMD_QUERY_POST, IOCTL_CAT_QPC, hf);
  if(post_sensor<0)
   printf("POST display is turned off\n");
  else if(post_sensor>=hinfo->num_sensors)
   fprintf(stderr, "Internal error\n");
  else
  {
   printf("POST monitors sensor #%u, current reading is %u%s%sC\n", post_sensor+1,
          hinfo->sensor_temp[post_sensor]>>1, (hinfo->sensor_temp[post_sensor]&1)?".5":"", degree);
  }
 }
}

/* Option parser */

static void parse_opts(int argc, char **argv)
{
 int i;

 for(i=1; i<argc; i++)
 {
  is_processed[i-1]=1;
  if(!stricmp(argv[i], "/SHOWFAN"))
   show_fan_cfg=1;
#ifdef USE_SPINUP  
  else if(!stricmp(argv[i], "/SPINUP"))
   use_spinup=1;
#endif
  else
   is_processed[i-1]=0;
 }
}

/* Main routine */

int main(int argc, char **argv)
{
 unsigned int i;
 unsigned short t;
#ifdef __OS2__
 int rc;
#ifdef __32BIT__
 ULONG action;
#else
 USHORT action;
#endif
#endif
 struct hwmon_info hinfo;

 if(argc<2||!stricmp(argv[1], "/?"))
 {
  fprintf(stderr, "Quiet PC control program v 1.01 on " __DATE__ ", " __TIME__ "\n"
                  "\n"
                  "Usage: QCTRL <option> [<option> ...]\n"
                  "\n"
                  "General options:\n"
                  "              /Q     Display the hardware type and readings\n"
                  "          /RESET     Reset the hardware monitor after suspend\n"
                  "\n"
                  "Fan control setup (READ THE MANUAL FIRST):\n"
                  "   /FAN[n]:<cmd>     Query or configure the particular fan\n"
                  "\n"
                  );
  return(1);
 }
 memset(is_processed, 0, sizeof(is_processed));
#if defined(__OS2__)
#ifdef USERMODE
 hf=0;
 if(rc=configure_hwmon())
 {
  fprintf(stderr, "Testcase initialization error %u\n", rc);
  return(1);
 }
#else
 if(DosOpen(DEVICE_FILENAME, &hf, &action, 0L, FILE_NORMAL,
            OPEN_ACTION_FAIL_IF_NEW|OPEN_ACTION_OPEN_IF_EXISTS,
            OPEN_ACCESS_READONLY|OPEN_SHARE_DENYNONE, 0L))
 {
  fprintf(stderr, "Driver not installed\n");
  return(2);
 }
#endif
#else
 if((hf=open(DEVICE_FILENAME, O_RDONLY))==-1)
 {
  fprintf(stderr, "Cannot access driver: %s\n", strerror(errno));
  return(2);
 }
#endif
 t=QPC_HWMON_QUERY_INFO;
 do_ioctl(&hinfo, sizeof(hinfo), &t, sizeof(t), QPC_CMD_QUERY_HWMON_PARAMS, IOCTL_CAT_QPC, hf);
 if(hinfo.revision!=HWMON_REVISION)
 {
  fprintf(stderr, "Driver interface rev. %u does not match the revision %u of control program.\n"
                  "The program will continue but it is suggested that you update both the driver\n"
                  "and control program to the same version as soon as possible.\n\n",
                  hinfo.revision, HWMON_REVISION);
 }
 parse_opts(argc, argv);
 check_hwmon(&hinfo);
 for(i=1; i<argc; i++)
 {
  if(is_processed[i-1])
   ;
  else if(!stricmp(argv[i], "/RESET"))
   reset();
  else if(!stricmp(argv[i], "/Q"))
   query(&hinfo);
  else if(!memicmp(argv[i], "/POST", 5))
   proc_post(&hinfo, argv[i]+5);
  else if(!memicmp(argv[i], "/FAN", 4))
  {
   if(isdigit(argv[i][4]))
   {
    if(argv[i][5]==':')
     set_fan_params((char)(argv[i][4]-49), argv[i]+6, &hinfo);
    else
     query_fan_params((char)(argv[i][4]-49), &hinfo);
   }
   else
    fprintf(stderr, "The default syntax for /FAN is no longer supported,\n"
                    "be sure to specify a fan number (1...%u), e.g. /FAN1\n", hinfo.num_fans);
  }
  else
   fprintf(stderr, "Invalid parameter <%s> ignored\n", argv[i]);
 }
#ifdef __OS2__
#ifndef USERMODE
 DosClose(hf);
#endif
#else
 close(hf);
#endif
 return(0);
}
