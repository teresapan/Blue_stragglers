#include "copyright.h"
/*
 * This is a short stub that simply calls the SM main programme, after
 * playing games on DOS machines
 */
#include <stdio.h> 
#include "options.h"
#include "sm_declare.h"

#if defined(__BORLANDC__) && !defined(__OS2__)
#  include <fcntl.h>

#  if defined(_Windows)
#     define BUGSTK 0xe000U		/* 56k for our new stack */
      char far BugStack[BUGSTK];	/* bug ?? Windows always allocates a
					   stack in the data segment, so we
					   get an overflow if we specify
					   `STACKSIZE=alot` in sm.def */
      extern unsigned _stklen = 0x0200U; /* we won't need much */
#  else
      extern unsigned _stklen = 0xe000U; /* Allocate 56k stack */
      extern unsigned _ovrbuffer = 0x1800; /* 96k overlay buffer */

   void interrupt (*getvect( int ))(void);
   void setvect(int, void interrupt (*)(void));
   void interrupt (*old1B)();

   void
   interrupt handler1B(void)
   {
      hand_C(0);
   }

   void
   install1B(void)
   {
      old1B = getvect(0x1B);
      setvect(0x1B,handler1B);
   }
   #pragma startup install1B
   void
   restore1B(void)
   {
      setvect(0x1B,old1B);
   }
   #pragma exit restore1B
#endif
#endif

int
main(ac,av)
int ac;
char **av;
{
#if defined(__BORLANDC__)
   extern int _fmode;
#endif
#if defined(__OVERLAY__)
   extern int far	_OvrInitExt( unsigned long, unsigned long );
   _OvrInitExt( 0, 0 );
#endif

#if !defined(_Windows)
   return(sm_main(ac,av));
#else					/* we must perform a stack switch */
   static int argc;
   static char **argv;
   argc = ac, argv = av;
      
#  define SEG(p) (((unsigned long) p) >> 16)
#  define OFS(p) (((unsigned long) p) & 0xffff)
   _SS = SEG(BugStack);
   _SP = OFS(BugStack) + BUGSTK;
   exit(sm_main(argc,argv));
#endif
}
