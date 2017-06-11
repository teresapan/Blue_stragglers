#include "copyright.h"
#include "options.h"
#if defined(OS2PM)
#define 	__OS2PM__
#define	INCL_DOSNMPIPES
#define	INCL_DOSFILEMGR
#define	INCL_DOSSESMGR
#define INCL_DOSPROCESS
#include	<os2.h> 
#include 	<stdio.h>
#include 	<ctype.h> 
#include	<math.h>
#include 	"devices.h" 
#include 	"sm_declare.h"

	/* These are declared here since we cannot include <sm.h>	*/
	/* due to a name conflict with the IBM API's COLOR structure.	*/

extern int   termout;		/* True for term, false for print  	*/
extern float xscreen_to_pix,	/* scale SCREEN to window coords   	*/
  	     yscreen_to_pix;
extern int   ldef;		/* scale spacing for lweight       	*/

ULONG		idSession;	/* ID for this session			*/

#define		PM_SET_CTYPE_RESET	0
#define		PM_SET_CTYPE_COLOR	1
#define		PM_SET_CTYPE_LOAD	2


struct	sm_msg {		/* Structure used to exchange messages with	*/
				/* SM command graphics window session.		*/
	char		cmd;		/* The command identifier 		*/
	int		a, b, c, d;	/* Four integers for various parameters */
	char		str[128];	/* A buffer for strings 		*/
};

static	HPIPE PipeHandle;	/* Handle for named pipe to graphics session	*/
PID	pid;			/* Process ID for child (graphics) process 	*/

/****************************************************************/
int pm_setup(s)
char *s;
{
 	STARTDATA	sdata;
	char		buff[CCHMAXPATH];
	char		*gs;
	static int	s_xpix = 600, s_ypix = 400; /* Default graphics window dim	*/
	int		xpix, ypix;
	char		*retain_str;	
	static		pipe_created = 0;

	APIRET	retcode, rc;
	extern HPIPE	PipeHandle;
	ULONG	OpenMode = NP_ACCESS_DUPLEX | NP_INHERIT;
	ULONG	PipeMode = NP_NOWAIT | NP_TYPE_MESSAGE | NP_READMODE_MESSAGE | 0x01;
	ULONG	InBuffSize = 4096;
	ULONG	OutBuffSize = 4096;
	ULONG	TimeOut = 1000;
	PPIB	pppib;
	PTIB	pptib;
	char	buf2[32];

	/* First process any arguments	*/

	strupr( s );			/* Accept flags in upper and lower case	*/
	if( strstr( s, "-N" ) )		/* Set retain_str according to	*/
	    retain_str = " N";		/* arguments passed to device	*/
	else				/* command			*/
	    retain_str = " R";


	if( gs = strstr( s, "-G" ) )	/* If geometry flag ..		*/
	    if( sscanf( gs + 2, "%dX%d", &xpix, &ypix) == 2 )
		{			/* and correct parameters...	*/
		s_xpix = xpix;		/* update graphics window size.	*/
		s_ypix = ypix;
		}

	/* Now start new PM session for graphics output */
	    
	DosGetInfoBlocks( &pptib, &pppib );		/* Get PID of this session and	*/
	sprintf( buf2, "\\PIPE\\sm_%d", pppib->pib_ulpid ); /* made a unique pipe name	*/

		/* Create pipe if it doesn't already exist	*/
	if( !pipe_created )				
	    rc = DosCreateNPipe( buf2, &PipeHandle, 
			OpenMode, PipeMode,
			OutBuffSize, InBuffSize, 
			TimeOut );	
	if( rc == 0 )
	    pipe_created = 1;
		
		/* Get pipe ready to connect, but don't wait since child	*/	
		/* process doesn't exist yet					*/
	rc = DosSetNPHState( PipeHandle, NP_NOWAIT | NP_READMODE_MESSAGE );	
	rc = DosConnectNPipe( PipeHandle );

		/* We do want pipe to wait for other calls though..	*/
	rc = DosSetNPHState( PipeHandle, NP_WAIT | NP_READMODE_MESSAGE );
	
	strcat( buf2, retain_str );	/* Append retain info to pipe name */

		/* Data structure to start new session	*/
		/* Pass pipe name and retain mode on command line	*/
	sdata.Length = sizeof( sdata );
	sdata.Related = SSF_RELATED_CHILD;
	sdata.FgBg = SSF_FGBG_BACK;
	sdata.TraceOpt = SSF_TRACEOPT_NONE;
	sdata.PgmTitle = "SM commands";
	sdata.PgmName = "sm_pmdev.exe";
	sdata.PgmInputs = buf2;
	sdata.TermQ = NULL;
	sdata.Environment = NULL;
	sdata.InheritOpt = SSF_INHERTOPT_PARENT;
	sdata.SessionType = SSF_TYPE_PM;
	sdata.IconFile = NULL;
	sdata.PgmHandle = 0;
	sdata.PgmControl = SSF_CONTROL_SETPOS;
	sdata.InitXPos = 50;
	sdata.InitYPos = 50;
	sdata.InitXSize = s_xpix;
	sdata.InitYSize = s_ypix;
	sdata.ObjectBuffer = buff;
	sdata.ObjectBuffLen = sizeof( buff );

	DosStartSession( &sdata, &idSession, &pid );
	termout = 1;
	ldef = 10;
	xscreen_to_pix = 1;
	yscreen_to_pix = 1;
	return 0;
}
/****************************************************************/

int pm_char(s,x,y)
char *s;
int x,y;
{
	struct sm_msg 	msg;
	ULONG		j;
	size_t		msg_size =  (ULONG)&msg.str - (ULONG)&msg.cmd;

	if( s == NULL )
		return 0;

	msg_size += strlen(s) + 1;	
	msg.a = x;
	msg.b = y;
	msg.cmd = 'c';

	strcpy( msg.str, s); 
	DosWrite( PipeHandle, (void*)&msg, msg_size, &j );
	return 0;
}
 
/****************************************************************/

void pm_close()
{
	struct sm_msg 	msg;
	ULONG		j;
	size_t		msg_size =  (ULONG)&msg.a - (ULONG)&msg.cmd;
	
	msg.cmd = 'q';
	DosWrite( PipeHandle, (void*)&msg, msg_size, &j );

	DosDisConnectNPipe(PipeHandle);
	
	DosStopSession( STOP_SESSION_SPECIFIED, idSession );
}
 
/****************************************************************/

int pm_cursor(get_pos,x,y)
int get_pos;				/* simply return the current position */
int *x,*y;
{	
	struct sm_msg 	msg;
	ULONG		j;
	size_t		msg_size1 =  (LONG)&msg.a - (LONG)&msg.cmd;
	size_t		msg_size2 =  (ULONG)&msg.c - (ULONG)&msg.cmd;

	msg.cmd = 'm';
	DosWrite( PipeHandle, (void*)&msg, msg_size1, &j );
	DosRead(  PipeHandle, (void*)&msg, msg_size2, &j );
	*x = msg.a;
	*y = msg.b;

       return msg.cmd;
}

/****************************************************************/

void  pm_draw(x,y)
int x,y;
{
	struct sm_msg 	msg;
	ULONG		j;
	size_t		msg_size =  (ULONG)&msg.c - (ULONG)&msg.cmd;

	msg.a = x;
	msg.b = y;
	msg.cmd = 'd';

	DosWrite( PipeHandle, (void*)&msg, msg_size, &j );
	
}

/****************************************************************/

void pm_erase(int raise_on_erase)
{
   volatile int roe = raise_on_erase; roe++;

	struct sm_msg 	msg;
	ULONG		j;
	size_t		msg_size =  (ULONG)&msg.a - (ULONG)&msg.cmd;

	msg.cmd = 'e';

	DosWrite( PipeHandle, (void*)&msg, msg_size, &j );
}

/****************************************************************/

int  pm_fill_pt(n)
int n;                          /* number of sides */
{
       return -1;
}

/****************************************************************/

int  pm_dot(x,y)
int x,y;
{
   return -1;
}

/****************************************************************/

void  pm_gflush()
{	
	struct sm_msg 	msg;
	ULONG		j;
	size_t		msg_size =  (ULONG)&msg.a - (ULONG)&msg.cmd;

	msg.cmd = 'f';
	DosWrite( PipeHandle, (void*)&msg, msg_size, &j );
}

/****************************************************************/

void pm_idle()
{
  ;
}

/****************************************************************/

void  pm_line(x1,y1,x2,y2)
int x1,y1,x2,y2;
{
	struct sm_msg 	msg;
	ULONG		j;
	size_t		msg_size =  (ULONG)&msg.str - (ULONG)&msg.cmd;

	msg.a = x1;
	msg.b = y1;
	msg.c = x2;
	msg.d = y2;
	msg.cmd = 'l';

	DosWrite( PipeHandle, (void*)&msg, msg_size, &j );
}	
/****************************************************************/  
int pm_ltype(lt)
int lt;
{
   return(-1);
}

/****************************************************************/

int pm_lweight(lw)
double lw;
{
	struct sm_msg	msg;
	ULONG		j;
	size_t		msg_size =  (ULONG)&msg.b - (ULONG)&msg.cmd;

	msg.a = (int)lw * 80.0;	
	msg.cmd = 'w';
	DosWrite( PipeHandle, (void*)&msg, msg_size, &j);
	
      	return 0 ;
}

/****************************************************************/

void  pm_page()
{
  ; 
}

/****************************************************************/

void  pm_redraw(fd)
int fd;					
{
 ;    
}

/****************************************************************/

void  pm_reloc(x,y)
int x,y;
{
	struct sm_msg 	msg;
	ULONG		j;
	size_t		msg_size =  (ULONG)&msg.c - (ULONG)&msg.cmd;
	
	msg.a = x;
	msg.b = y;
	msg.cmd = 'r';

	DosWrite( PipeHandle, (void*)&msg, msg_size, &j );
}

/****************************************************************/

int  pm_set_ctype(colors,ncolors)
COLOR *colors;
int ncolors;
{		
   return(-1);
}

/****************************************************************/

void  pm_ctype(r,g,b)
int r,g,b;
{
	ULONG		j;
	struct sm_msg	msg;
	size_t		msg_size =  (ULONG)&msg.d - (ULONG)&msg.cmd;

	msg.cmd = 'C';
	msg.a = r;
	msg.b = g;
	msg.c = b;
	DosWrite( PipeHandle, (void*)&msg, msg_size, &j );

}
/****************************************************************/
void  pm_enable()
{
  ; 
}
#endif
/*
 * Include a symbol for picky linkers
 */
#ifndef lint
static void linker_func() { linker_func(); }
#endif


 

