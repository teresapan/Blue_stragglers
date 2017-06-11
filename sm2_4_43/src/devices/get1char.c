/************************************************************/
/*
 * This routine gets one character from the keyboard, without
 * waiting for a <CR> or echoing. The routine runs in RAW mode.
 *
 * It must be initialised with a ^A, and finished with an EOF.
 */
#include "options.h"
#include <stdio.h>
#include <errno.h>
#include "charset.h"
#include "devices.h"
#include "sm_declare.h"

extern int sm_interrupt;			/* handle ^C interrupts */
#if defined(WIN32) && defined(__GNUC__)
#define POSIX_TTY
#endif
#if defined(unix) || (defined(WIN32) && defined(__GNUC__))
#if defined(POSIX_TTY)
#  include <termios.h>
#else
#  ifdef SYS_V
#    include <termio.h>
#  else
#    include <sgtty.h>
#  endif
#endif

#define GET1CHAR			/* we have a get1char() */
int
get1char(c)
int c;					/* what shall I do ? */
{
   char i;
   int old_interrupt;			/* value of interrupt before read */
   static int fildes = -1,
              first = 1,		/* is this first entry? */
	      is_tty;			/* is stdin attached to a tty? */
#if defined(POSIX_TTY) || defined(SYS_V)
#  if defined(POSIX_TTY)
   struct termios arg;
   static struct termios save_arg;
#  else
   struct termio arg;
   static struct termio save_arg;
#  endif
#else
   struct sgttyb arg;			/* state of terminal */
   static struct sgttyb save_arg;	/* initial state of terminal */
   struct ltchars ltarg;		/* interrupt chars for terminal */
   static struct ltchars save_ltarg;	/* saved value of ltarg */
#endif

   if(c == CTRL_A) {                    /* set up for 1-character mode */
      if(first) {
	 first = 0;

	 is_tty = isatty(0);
	 if((fildes = open("/dev/tty",2)) < 0) {
	    msg("Can't open /dev/tty in get1char\n");
	    return(-1);
	 }
#if defined(POSIX_TTY) || defined(SYS_V)
#  if defined(POSIX_TTY)
         (void)tcgetattr(fildes,&save_arg);
#  else
         (void)ioctl(fildes,TCGETA,(char *)&save_arg);
#  endif
#else
	 (void)ioctl(fildes,TIOCGLTC,(char *)&save_ltarg);
         (void)ioctl(fildes,TIOCGETP,(char *)&save_arg);
#endif
	 (void)close(fildes);
      }
      if(!is_tty) {			/* stdin isn't attached to a tty */
	 fildes = 0;			/* read stdin */
	 return(CTRL_A);
      }

      if((fildes = open("/dev/tty",2)) < 0) {
	 msg("Can't open /dev/tty in get1char\n");
	 return(-1);
      }
      arg = save_arg;				/* get initial state */
#if defined(POSIX_TTY) || defined(SYS_V)
      arg.c_lflag &= ~(ICANON | ECHO);
      arg.c_iflag &= ~(INLCR | ICRNL);
      arg.c_cc[VMIN] = 1;
      arg.c_cc[VTIME] = 0;
#if defined(VDSUSP)			/* ^Y on solaris */
      arg.c_cc[VDSUSP] = -1;
#endif
#  if defined(POSIX_TTY)
      arg.c_cc[VSUSP] = -1;
      (void)tcsetattr(fildes,TCSADRAIN,&arg);
#  else
      (void)ioctl(fildes,TCSETA,(char *)&arg);
#  endif
#else
      ltarg = save_ltarg;
      ltarg.t_suspc = -1;		/* default: ^Z */
      ltarg.t_dsuspc = -1;		/*          ^Y */
      ltarg.t_flushc = -1;		/*          ^O */
      ltarg.t_lnextc = -1;		/*          ^V */
      (void)ioctl(fildes,TIOCSLTC,(char *)&ltarg);
      arg.sg_flags |= CBREAK;
      arg.sg_flags &= ~(ECHO | CRMOD);
      (void)ioctl(fildes,TIOCSETN,(char *)&arg); /* set new mode */
#endif
      return(CTRL_A);
   } else if(c == EOF) {			/* revert to initial state */
      if(fildes <= 0){
         return(EOF);
      }
#if defined(POSIX_TTY) || defined(SYS_V)
#  if defined(POSIX_TTY)
      (void)tcsetattr(fildes,TCSADRAIN,&save_arg);
#  else
      (void)ioctl(fildes,TCSETA,(char *)&save_arg);
#  endif
#else
      (void)ioctl(fildes,TIOCSETN,(char *)&save_arg);	/* reset terminal */
      (void)ioctl(fildes,TIOCSLTC,(char *)&save_ltarg);
#endif
      (void)close(fildes);
      fildes = -1;
      return(EOF);
   } else {				/* read one character */
      if(fildes < 0){
         return(EOF);
      }
/*
 * give the device a chance 
 */
#if defined(macOSX)
      REDRAW(0);			/* appears to have trouble  mixing
					   select() and /dev/tty */
#else
      REDRAW(fildes);
#endif

      old_interrupt = sm_interrupt;
      while(read(fildes,&i,1) < 0) {	/* EOF or ^C */
#if defined(EINTR)
	 if(errno == EINTR) continue;
#endif
	 if(sm_interrupt == old_interrupt) { /* not ^C; must be EOF */
	    (void)get1char(EOF);
	    CLOSE(); stg_close();
	    exit(EXIT_OK);
	 }
	 sm_interrupt = old_interrupt;
      }
      i &= 0177;	                        /* get a char */
      if(i == CTRL_C) hand_C(0);
      return(i);
   }
}
#endif

#ifdef vms
#   include <descrip.h>
#   include <ttdef.h>
#   include <iodef.h>

#define GET1CHAR			/* we have a get1char() */
int
get1char(i)
int i;                          /* what shall I do ? */
{
   static int setbuff[2];               /* buffer to set terminal attributes */
   static short chan = -1;              /* channel to terminal */
   struct dsc$descriptor_s descrip;     /* VMS descriptor */

   if(i == CTRL_A) {                    /* set up for 1-character mode */
      descrip.dsc$w_length  = strlen("tt:");	/* What VMS requires */
      descrip.dsc$b_dtype   = DSC$K_DTYPE_T;
      descrip.dsc$b_class   = DSC$K_CLASS_S;
      descrip.dsc$a_pointer = "tt:";
      (void)sys$assign(&descrip,&chan,0,0);

      (void)sys$qiow(0,chan,IO$_SENSEMODE,0,0,0,setbuff,8,0,0,0,0);
      setbuff[1] |= TT$M_NOECHO;                        /* set noecho */
      (void)sys$qiow(0,chan,IO$_SETMODE,0,0,0,setbuff,8,0,0,0,0);
      return(CTRL_A);
   } else if(i == EOF) {
      if(chan < 0) {
         return(EOF);
      }
      setbuff[1] &= ~TT$M_NOECHO;
      (void)sys$qiow(0,chan,IO$_SETMODE,0,0,0,setbuff,8,0,0,0,0);
      sys$dassgn(chan);
      chan = -1;
      return(EOF);
   } else {
      if(chan < 0) {
         return(EOF);
      }
      (void)sys$qiow(0,chan,IO$_TTYREADALL,0,0,0,&i,1,0,0,0,0);
      i &= 0177;                        /* get a char */
      if(i == CTRL_C) hand_C(0);
      return(i);
   }
}
#endif

#if defined(__MSDOS__) || defined(__OS2__)
#include <conio.h>
#if defined(_Windows) || defined(__OS2__)
#define GET1CHAR			/* we have a get1char() */

int
get1char(i)
int i;
{
   if(i == EOF || i == CTRL_A) return(i);
   i = getch();
   return(i == '\0' ? '\033' : i);	/* replace \0 by escape */
}
#else
#define	HOTKEY	0x6800

#define GET1CHAR			/* we have a get1char() */
int
get1char(i)
int i;
{
   static c = -1;
   
   if(i == EOF || i == CTRL_A ) return(i);

   if(c == -1) {
      do {
	 asm {
	    mov	ax, 0000h
	    int	16h
	    mov	i, ax
	 }
	 if(i == HOTKEY) {
	    bgi_enable();
	    asm {
	       mov ax, 0000h
	       int 16h
	    }
	    bgi_idle();
	 }
      } while(i == HOTKEY);
      
      if((i & 0xff) == CTRL_C) hand_C(0);

      if((i & 0xff) == 0) {		/* it's an extended key */
	 c = i >> 8;
	 return	'\033';			/* simulate escape */
      }
      return(i & 0xff);
   } else {
      i = c;
      c = -1;
      return(i);
   }
}
#endif
#endif

#if !defined(GET1CHAR)
int
get1char(c)
int c;					/* what shall I do ? */
{
   extern int no_editor;

   if(c == CTRL_A) {
      msg("You do not have a functioning get1char();\n");
      msg("I am disabling the history editor while you fix things");
      no_editor = 1;
   } else if(c == 0) {
      return('\n');
   }
   
   return(c);
}
#endif

