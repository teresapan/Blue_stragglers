#include "copyright.h"
/*
 * This routine provides the unix system() call. If NO_SYSTEM is
 * defined, you'll have to provide your own, or try the one provided
 */
#if defined(NO_SYSTEM)
#ifdef VMS
/*
 * This version is for VMS. It was written by
 * Jay Travisano for the IRAF group at STScI.
 */
#include <stdio.h>

system(string)
char *string;
{
   int ret;
   static int dcl();
   
   if((ret = dcl(string)) == -1) {
      msg("Error in DCL call\n");
   } else if(ret%2 == 0) {		/* error return */
     msg_1d("DCL returns %d\n",ret);
   }
   return(ret);
}
/*
 *   dcl  --   set up DCL co-process 
 *                   start_dcl () -- start the DCL subprocess 
 *                   stop_dcl ()  -- kill the DCL subprocess
 *                   dcl (command) -- execute DCL command (does a start_dcl()
 *                                    if not already done)
 *  NOTE: only need to do a dcl() to start and finish.
 */
#include  <descrip.h>
#include  <dvidef.h>
#include  <iodef.h>
#include  <ssdef.h>

#define   MAXBUF        256
#define   MBXNAM         40
#define   MAX_DCL_LINE  132
#define   MAX_DCL_CMD   255

#define   SEND(b)    if (_mbx_write(outchan, b, strlen(b)) == -1) return (-1)

/*
 *  "escape string" is used to tell us when the DCL subprocess is finished
 *   processing a command.
 */
static char escape_string[] = "!<DCL_ESCAPE>!",
	    escape_buf[]    = "$ WRITE SYS$OUTPUT \"!<DCL_ESCAPE>!\"",
	    escape_string_len = sizeof (escape_string) - 1;
/* 
 * string to get the status back from the DCL subprocess 
 */
static char status_string[] = "  $STATUS == \"%X",
	    status_buf[]    = "$ SHOW SYMBOL $STATUS",
	    status_string_len = sizeof (status_string) - 1;

static int inchan = 0, outchan = 0, out_err_diff;
static char buf[MAXBUF+1];
static long pid = 0;

static struct {		/* for exit handler (stop_dcl()) */
    int   link;
    int   (*handler_address)();
    char  n;
    char  n2;
    short n3;
    int   result_address;   
} _exit_block = { 0, 0, 1, 0, 0, 0 };

static int  _exit_code = 0;

/*  Start a DCL subprocess,  return pid  or -1 */

static int
start_dcl ()
{
   static  long    flags = '\001';     			/*  "spawn/nowait" */
   char    mbx_in[MBXNAM],  mbx_out[MBXNAM];
   struct  dsc$descriptor_s  din,  dout;
   static int stop_dcl();

   if (( inchan = _mbx_open (mbx_in )) == -1)  return (-1);
   if ((outchan = _mbx_open (mbx_out)) == -1)  return (-1);

/* install stop_dcl() as an exit handler, to make sure that subprocess dies
 *  when the image exits.
 */
   _exit_block.handler_address = stop_dcl;
   _exit_block.result_address  = &_exit_code;   
   SYS$DCLEXH (&_exit_block);

   din.dsc$w_length   = strlen (mbx_in);
   din.dsc$a_pointer  = mbx_in;
   dout.dsc$w_length  = strlen (mbx_out);
   dout.dsc$a_pointer = mbx_out;
   din.dsc$b_dtype  =  dout.dsc$b_dtype  =  DSC$K_DTYPE_T;
   din.dsc$b_class  =  dout.dsc$b_class  =  DSC$K_CLASS_S;

/*  Spawn off a DCL "co-process" with I/O from the mailboxes */
   if (LIB$SPAWN (0, &dout, &din, &flags, 0, &pid, 0, 0, 0, 0) != SS$_NORMAL)  
       return(-1);

/* make sure subprocess doesn't verify everything */
   SEND("$ SET NOVERIFY");
/* make sure subprocess stays alive even if errors occur */
   SEND("$ SET NOON");

/* are stdout and stderr different ? */
   out_err_diff = strcmp (getenv("SYS$OUTPUT")+4, getenv("SYS$ERROR")+4);

   return (pid);
}

/* Stop a DCL subprocess and deassign mbx channels. */

static int
stop_dcl ()
{
   if (pid)     SYS$DELPRC (&pid, 0);
   if (inchan)  SYS$DASSGN (inchan);
   if (outchan) SYS$DASSGN (outchan);
   pid = inchan = outchan = 0;
   return (_exit_code);
}
/*
 * Send a DCL command to the subprocess.
 */
static int
dcl(cmd)
char  *cmd;
{
   char *malloc();
   int len, return_status;
   static int start_dcl();

   while(*cmd == ' ' || *cmd == '\t') cmd++;
   if(*cmd == '\0')  return(0);

   if(!pid) 				/* start up subprocess if not there */
       if (start_dcl() == -1) return (-1);

   if (strlen(cmd) <= MAX_DCL_LINE) {
       SEND(cmd);

   } else {	/* break up command so DCL can handle it; define some local
                 *   symbols and then concatenate them to execute the command.
                 *   (not very clean, but it works!!) 
                 */
       char *s1;
       int  bp;
       if((s1 = malloc (MAX_DCL_LINE + 1)) == NULL) {
 	  msg("Can't malloc s1 in dcl\n");
	  return(-1);
       }
       bp = MAX_DCL_LINE - 6;			/* break point in cmd line */

       (void)sprintf(s1, "s1:=\"%.*s\"", bp, cmd); /* first part of command */
       SEND(s1);
       (void)sprintf(s1,"s2:=\"%.*s\"",strlen(cmd)-bp,&cmd[bp]); /* 2nd part */
       SEND(s1);
       (void)strcpy (s1, "$ 's1''s2'");		/* concatenated command */
       SEND(s1);
       (void)strcpy(s1,"$ delete/symbol/local s1"); /* delete the symbols */
       SEND(s1);
       (void)strcpy(s1, "$ delete/symbol/local s2");
       SEND(s1);

       free (s1);
   }

   SEND(status_buf);		/* check status of DCL command */
   SEND(escape_buf);		/* send back message at end of processing */

   while (1) {
       if ((len = _mbx_read (inchan, buf, MAXBUF)) == -1)  return (-1);
       buf[len] = 0;
       if(!strncmp (status_string, buf, status_string_len)) {
	   (void)sscanf(&buf[status_string_len], "%X", &return_status);
           continue;
       }
       if(!strncmp(escape_string, buf, escape_string_len)) break;
       msg_1s("%s\n",buf);
       if (out_err_diff)  msg_1s( "%s\n", buf);
   }
   return(return_status);
}

/*  _mbx_open  -  create a mailbox and assign a channel to it.  return
 *                the channel number and get the name of the mailbox.
 */

static _mbx_open (name)
char  *name;
{
   short  len;
   int    chan;
   struct item_list {			/* Definition of an item list. */
        short  it_len;
        short  it_code;
        char   *it_buff;
        short  *it_blen;
   } itmlst[2];

   if (SYS$CREMBX (0, &chan, MAXBUF, 0, 0, 0, 0) != SS$_NORMAL)  return (-1);

   itmlst[0].it_len  = MBXNAM;			/* Set up the item list.*/
   itmlst[0].it_code = DVI$_DEVNAM;
   itmlst[0].it_buff = name;
   itmlst[0].it_blen = &len;
   itmlst[1].it_code = 0;
   itmlst[1].it_len  = 0;
   SYS$GETDVI ( 5, chan, 0, itmlst, 0, 0, 0, 0);	/* get mailbox name */
   SYS$WAITFR ( 5 );
   name[len] = 0;
   return (chan);
}

/*  _mbx_write  -  write a buffer to the mailbox, without waiting for the
 *                 subprocess to read it.
 */

static _mbx_write (chan, buffer, size)
int   chan, size;
char  *buffer;
{
   if ( SYS$QIO ( 5, chan, IO$_WRITEVBLK | IO$M_NOW, 0, 0, 0, 
                    buffer, size, 0, 0, 0, 0 ) != SS$_NORMAL) 
       return (-1);
   return (0);
}

/*  _mbx_read  -  read next record from the mailbox.  return the byte count. */

static _mbx_read (chan, buffer, size)
int   chan, size;
char  *buffer;
{
   struct iosb {
     short io_status;
     short io_count;
     long  io_device;
   }  stat;
   int     status;

   status = SYS$QIOW ( 0, chan, IO$_READVBLK, &stat, 0, 0,
                         buffer, size, 0, 0, 0, 0 );
   if (status != SS$_NORMAL || stat.io_status != SS$_NORMAL) 
       return (-1);
   return ((int)stat.io_count);
}
#else
/*
 * We don't have a system() call, and haven't faked one. Maybe this'll
 * compile, or at least provide a template
 */
#if 1
#include <signal.h>
#include <sys/wait.h>

system(str)
char *str;
{
   int dead_pid,
       pid,
       ret,
       (*sigint)(),
       (*sigquit)();
   union wait stat;

   if((pid = vfork()) == 0) {		/* child */
      execl("/bin/sh","sh","-c",str,0);
      _exit(-1);
   }
   sigint = signal(SIGINT,SIG_IGN);	/* ignore signals */
   sigquit = signal(SIGQUIT,SIG_IGN);

   for(;;) {				/* wait until process finishes */
      dead_pid = wait(&stat);
      if(dead_pid == pid) {		/* that was our one */
	 ret = stat.w_status;
	 break;
      } else if(dead_pid == -1) {	/* no children left */
	 ret = -1;
	 break;
      }
   }

   signal(SIGINT,sigint);		/* reinstate signals */
   signal(SIGQUIT,sigquit);

   return(ret);
}
#else
system(str)
char str[];
{
   msg_1s("I don't know how to pass a string to the O/S. String =\n%s",str);
   return(-1);
}
#endif
#endif
#endif
/*
 * Include a symbol for picky linkers
 */
#ifndef lint
static void linker_func() { linker_func(); }
#endif

#ifdef	_Windows
#include	<windows.h>
system( char *s )
{
	char	command[200];
	int	result;
	char	*getenv( char * );

	while( *s == ' ' || *s == '\t')
		s++;
	sprintf( command, *s ? "%s /c%s" : "%s", getenv("COMSPEC"), s );
	result = WinExec( command, SW_SHOW );
	return	result > 32 ? 0 : result+1;
}
#endif
