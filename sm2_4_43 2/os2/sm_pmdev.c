#define INCL_WIN
#define	INCL_DOSPROCESS
#define	INCL_DOSSESMGR
#define	INCL_DOSNMPIPES
#define	INCL_DOSPROCESS
#define INCL_GPI
#include	<os2.h>
#include	<string.h>
#include	<stdlib.h>

#define		BUFF_SIZE		2048
#define		TIMEOUT			10000
#define		PM_SET_CTYPE_RESET	0
#define		PM_SET_CTYPE_COLOR	1
#define		PM_SET_CTYPE_LOAD	2

struct sm_msg		/* Struct used to communicate with command line session */
{
	char	cmd;		/* command character			*/
	int	a, b, c, d;	/* FOur ints for various parameters	*/
	char	str[128];
};

HPS		hps;					/* Presentation space handle	*/
void		update_screen( ULONG parameter );
HWND		hwnd;					/* Window handle		*/
int		mouse_active;				/* Mouse status			*/

MRESULT EXPENTRY window_func(HWND, ULONG, MPARAM, MPARAM );
HAB	hand_ab;
char	pipe_name[32];
HPIPE	PipeHandle;
LONG	drawing_mode;

main( int argc, char *argv[])
{
	extern HAB	hand_ab;
	HMQ		hand_mq;
	HWND		hand_frame;
	QMSG		q_mess;
	ULONG	flFlags;
	unsigned char class[] = "smshell";
	
	if( argc != 3 )
		return 0;	
	
	strcpy( pipe_name, argv[1] );
	
	if( strchr( argv[2], 'R' ))		/* If the retain flag is sent */
	    drawing_mode = DM_DRAWANDRETAIN;
	else
	    drawing_mode = DM_DRAW;
	
	flFlags = FCF_TITLEBAR | FCF_SIZEBORDER | FCF_MINMAX | FCF_SHELLPOSITION | FCF_ICON;

	hand_ab = WinInitialize( 0 );
	hand_mq = WinCreateMsgQueue( hand_ab, 0 );

	if( !WinRegisterClass( hand_ab, (PSZ)class, (PFNWP)window_func,
					CS_SIZEREDRAW, 0 ))
		exit(1);

	hand_frame = WinCreateStdWindow( 	HWND_DESKTOP, 
							WS_VISIBLE,
							&flFlags,
							(PSZ)class,
							(PSZ)"SM",
							WS_VISIBLE,
							0,
							1,
							NULL);
	
	while( WinGetMsg( hand_ab, &q_mess, 0L, 0, 0 ))
		WinDispatchMsg( hand_ab, &q_mess );

	WinDestroyWindow( hand_frame );
	WinDestroyMsgQueue( hand_mq );
	WinTerminate( hand_ab );
	return 0;
}

MRESULT EXPENTRY window_func( HWND handle, ULONG mess, 
						MPARAM parm1, MPARAM parm2 )
{
	TID		ThreadID;
	PFNTHREAD	ThreadAddr = (PFNTHREAD)update_screen;
	ULONG		ThreadArg;
	ULONG		ThreadFlags = 0;
	ULONG		StackSize = 4096;
	ULONG		j;
	POINTL		coords,ptl;
	RECTL		rectClient;
	HDC		hdc;
	SIZEL		pagesize = { 32768L, 32768L };
	extern HAB	hand_ab;
	extern HPS	hps;
	struct sm_msg	msg;
		
	switch( mess )
		{
		case	WM_CREATE:
			hwnd = handle;
			hdc = WinOpenWindowDC( handle );
			hps = GpiCreatePS( hand_ab, hdc, &pagesize,
				PU_LOENGLISH | GPIT_NORMAL | GPIA_ASSOC );
			GpiSetDrawingMode( hps, drawing_mode);
			WinQueryWindowRect( handle, &rectClient );
			GpiSetPageViewport( hps, &rectClient );
			GpiCreateLogColorTable( hps, 0L, LCOLF_RGB,0L, 0L, NULL); 
			GpiOpenSegment( hps, 1L );     
			GpiSetColor( hps, 0L );
			ThreadArg = (ULONG)handle;
			DosCreateThread( &ThreadID, ThreadAddr,
				ThreadArg, ThreadFlags, StackSize );
			mouse_active = FALSE;
			break;

		case	WM_ERASEBACKGROUND:
			return (MRESULT)TRUE;

		case	WM_DESTROY:
			GpiDestroyPS( hps );
			break;

		case	WM_PAINT:
			DosEnterCritSec();
		 	WinQueryWindowRect( handle, &rectClient );
           		WinInvalidateRect( handle, NULL, FALSE );
			WinBeginPaint( handle, hps, NULL );
			GpiDrawSegment( hps, 1L);   
			WinEndPaint( handle );
			DosExitCritSec(); 
			break;

		case WM_SIZE:
		case WM_MINMAXFRAME:
			WinQueryWindowRect( handle, &rectClient );
			GpiSetPageViewport( hps, &rectClient ); 
			break;

		case WM_BUTTON1DOWN:
			msg.cmd = 'e';
			ptl.x = (LONG)SHORT1FROMMP(parm1);
			ptl.y = (LONG)SHORT2FROMMP(parm1);
			GpiConvert( hps,CVTC_DEVICE, CVTC_WORLD, 1L,&ptl);
			msg.a = ptl.x;
			msg.b = ptl.y;
			if( mouse_active )
				{
				DosWrite( PipeHandle, &msg, sizeof(msg), &j );
				mouse_active = FALSE;
				}
			break;

		case WM_BUTTON2DOWN:
			msg.cmd = 'q';
			ptl.x = (LONG)SHORT1FROMMP(parm1);
			ptl.y = (LONG)SHORT2FROMMP(parm1);
			GpiConvert( hps,CVTC_DEVICE, CVTC_WORLD, 1L,&ptl);
			msg.a = ptl.x;
			msg.b = ptl.y;
			if( mouse_active )
				{
				DosWrite( PipeHandle, &msg, sizeof(msg), &j );
				mouse_active = FALSE;
				}
			break;

		case WM_BUTTON3DOWN:			
			msg.cmd = 'p';
			ptl.x = (LONG)SHORT1FROMMP(parm1);
			ptl.y = (LONG)SHORT2FROMMP(parm1);
			GpiConvert( hps,CVTC_DEVICE, CVTC_WORLD, 1L,&ptl);
			msg.a = ptl.x;
			msg.b = ptl.y;
			if( mouse_active )
				{
				DosWrite( PipeHandle, &msg, sizeof(msg), &j );
				mouse_active = FALSE;
				}
			break;

		default:
			return WinDefWindowProc( handle, mess, parm1, parm2 );
		}

	return (MRESULT)FALSE;
}

void	update_screen( parameter )
ULONG	parameter;
{
	ULONG		j, Action;
	LONG		old_x, old_y;
	POINTL		coords;
	char		buf2[100];
	struct sm_msg 	msg;
	static long	lweight, lcolor;
		
	DosOpen( pipe_name,
		&PipeHandle,
		&Action,
		0,			/* File size		*/
		FILE_NORMAL,		/* File attribute	*/
		FILE_OPEN,
		OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYNONE, (PEAOP) NULL);
	
	while(1)
		{
		DosRead( PipeHandle, &msg, sizeof(msg), &j );
		coords.x = msg.a;
		coords.y = msg.b;
		switch (msg.cmd)
			{
			case 'r':
				GpiSetCurrentPosition( hps, &coords );
				break;

			case 'd':
				if( lweight )
				    GpiBeginPath( hps, 1L );	
				GpiLine( hps, &coords );
				if( lweight )
				    {
				    GpiEndPath( hps );
				    GpiStrokePath(hps, 1L, 0L);	
				    }
				break;

			case 'l':
				old_x = coords.x;
				old_y = coords.y;
				GpiSetCurrentPosition( hps, &coords );
				coords.x = msg.c;
				coords.y = msg.d;
				if( lweight )
				    GpiBeginPath( hps, 1L );	
				GpiLine( hps, &coords );
				if( lweight )
				    {
				    GpiEndPath( hps );
				    GpiStrokePath(hps, 1L, 0L);	
				    }
				coords.x = old_x;
				coords.y = old_y;
				GpiSetCurrentPosition( hps, &coords );
				break;

			case 'e':

				GpiErase( hps );
				GpiCloseSegment( hps );
				GpiDeleteSegment( hps, 1L );
				GpiOpenSegment( hps, 1L );
				GpiSetColor( hps, lcolor );
				GpiSetLineWidthGeom( hps, lweight );    
				break;
			case 'c':

				GpiCharStringAt( hps, &coords, strlen(msg.str), msg.str );
				break;
			case 'f':
				WinInvalidateRect( hwnd, NULL, FALSE );
				break;
			case 'm':
				mouse_active = TRUE;
				break;
			case 'w':
				lweight = (LONG)msg.a;
				GpiSetLineWidthGeom( hps, lweight);
				break;
		           
             		case 'C':
				lcolor = msg.a * 65536 + msg.b * 256 + msg.c;
                		GpiSetColor( hps, lcolor );
                		break;

			case 'q':
				DosClose( PipeHandle );
				break;
			}
		}
}
