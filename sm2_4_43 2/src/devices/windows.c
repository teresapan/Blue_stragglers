#include	"copyright.h"
/*
 * SM driver for MW-Windows
 *
 * Written by Laurent Bartholdi (lbartho@uni2a.unige.ch)
 */
#include	"options.h"
#if defined(MSWINDOWS)
#include	<stdio.h>
#include	<windows.h>
#include	"sm.h"
#include	"sm_declare.h"

static	HWND	hPlot, hMain;
static	HBRUSH	hBrush;
static	HDC	hDC;
static	HBITMAP	hBitmap;
static	HPEN	hPen, hOldPen;
static	int	penstyle, penwidth;
static	COLORREF	pencolor;
static	volatile enum { M_NONE, M_ENTER, M_QUIT } cursorbut;
static	POINT	cursorpos;
static	enum { M_SAVE, M_DONE } dirty;
static	int	width, height;

static void
setpen()
{
	SelectObject( hDC, hOldPen );
	DeleteObject( hPen );
	hPen = CreatePen( penstyle, penwidth, pencolor );
	SelectObject( hDC, hPen );
	SetTextColor( hDC, pencolor );
}

static int
getx( int x )
{
	return	x * xscreen_to_pix;
}

static int
gety( int y )
{
	return	(SCREEN_SIZE - y) * yscreen_to_pix;
}

static int
putx( int x )
{
	return	x / xscreen_to_pix;
}

static int
puty( int y )
{
	return	SCREEN_SIZE - (y / yscreen_to_pix);
}

static void
win_restore( HDC hDC )
{
	HDC	hMemoryDC = CreateCompatibleDC( hDC );
	HBITMAP	hOldBitmap;

	hOldBitmap = SelectObject( hMemoryDC, hBitmap );
	BitBlt( hDC, 0, 0, width, height, hMemoryDC, 0, 0, SRCCOPY );
	SelectObject( hMemoryDC, hOldBitmap );

	DeleteDC( hMemoryDC );
}

static void
win_save( HDC hDC )
{
	HDC	hMemoryDC = CreateCompatibleDC( hDC );
	HBITMAP	hOldBitmap;

	hOldBitmap = SelectObject( hMemoryDC, hBitmap );
	BitBlt( hMemoryDC, 0, 0, width, height, hDC, 0, 0, SRCCOPY );
	SelectObject( hMemoryDC, hOldBitmap );

	DeleteDC( hMemoryDC );
}

long _export FAR PASCAL
PlotWindowProc( HWND hWnd, unsigned message, WORD wParam, LONG lParam )
{
	int	repeat = 1;
	POINT	ptCursor;
	RECT	Rect;

	switch( message )
	{
	case	WM_KEYDOWN:
		GetCursorPos( &ptCursor );
		ScreenToClient( hWnd, &ptCursor );
		switch( wParam )
		{
		case	'E':
			cursorbut = M_ENTER;
			repeat = 1;
			return	0;
		case	'Q':
			cursorbut = M_QUIT;
			repeat = 1;
			return	0;
		case	VK_RETURN:
			{
				win_save( hDC );

				if( OpenClipboard( hWnd ) )
				{
					EmptyClipboard();
					SetClipboardData( CF_BITMAP, hBitmap );
					CloseClipboard();
				}
			}
			break;
		case	VK_LEFT:
			ptCursor.x -= repeat;
			break;
		case	VK_RIGHT:
			ptCursor.x += repeat;
			break;
		case	VK_UP:
			ptCursor.y -= repeat;
			break;
		case	VK_DOWN:
			ptCursor.y += repeat;
			break;
		}
		GetClientRect( hWnd, &Rect );
		if( ptCursor.x >= Rect.right )	ptCursor.x = Rect.right - 1;
		if( ptCursor.x < Rect.left )	ptCursor.x = Rect.left;
		if( ptCursor.y >= Rect.bottom )	ptCursor.y = Rect.bottom - 1;
		if( ptCursor.y < Rect.top )	ptCursor.y = Rect.top;
		ClientToScreen( hWnd, &ptCursor );
		SetCursorPos( ptCursor.x, ptCursor.y );
		repeat++;
		break;
	case	WM_KEYUP:
		repeat = 1;
		break;
	case	WM_MOUSEMOVE:
		cursorpos = MAKEPOINT(lParam);
		break;
	case	WM_LBUTTONDOWN:
		cursorbut = M_ENTER;
		break;
	case	WM_RBUTTONDOWN:
		cursorbut = M_QUIT;
		break;
	case	WM_PAINT:
		{
			PAINTSTRUCT	ps;
			HDC	hDC = BeginPaint( hWnd, &ps );
			if( hBitmap )
				win_restore( hDC );
			EndPaint( hWnd, &ps );
		}
		break;
	case	WM_ACTIVATE:
		if( wParam )
		{
			HDC	hDC = GetDC( hPlot );
			if( hBitmap )
				win_restore( hDC );
			ReleaseDC( hPlot, hDC );
		} else	if( dirty == M_SAVE )
		{
			HDC	hDC = GetDC( hPlot );
			win_save( hDC );
			ReleaseDC( hPlot, hDC );
			dirty = M_DONE;
		}
		break;
	case	WM_CLOSE:
		break;	/* forbid destroy */
	default:
		return	DefWindowProc( hWnd, message, wParam, lParam );
	}
	return	0;
}

int
msw_setup(disp)
char *disp;
{
	static first = 1;
	extern HANDLE	__hInstance;
	RECT	Rect;

	if( sscanf( disp, "%d%d", &width, &height ) != 2 )
		width = CW_USEDEFAULT, height = CW_USEDEFAULT;

	if( first )
	{
		WNDCLASS	wndClass;
		WORD	style;
		first = 0;

		wndClass.style = CS_VREDRAW | CS_HREDRAW;
		wndClass.lpfnWndProc = PlotWindowProc;
		wndClass.cbClsExtra = 0;
		wndClass.cbWndExtra = 0;
		wndClass.hInstance = __hInstance;
		wndClass.hIcon = NULL;
		wndClass.hCursor = LoadCursor( NULL, IDC_ARROW );
		wndClass.hbrBackground = hBrush = GetStockObject( LTGRAY_BRUSH );
		wndClass.lpszMenuName = NULL;
		wndClass.lpszClassName = "PlotWindowClass";

		if( !RegisterClass(&wndClass) )
			return	-1;
		printf("<Plot Window Defined>\n");
			/* writing this message actually isn't philanthropy:
				we have to make sure the main window is visible ... */
		hMain = SetFocus( 0 );
	}

	hBitmap = 0;	/* so nothing will be painted during the first WM_PAINT */

	hPlot = CreateWindow( "PlotWindowClass", "SM Plot",
		WS_OVERLAPPEDWINDOW, 
		CW_USEDEFAULT,CW_USEDEFAULT, width, height,
		NULL, NULL, __hInstance, NULL );
	ShowWindow( hPlot, SW_SHOW );
	UpdateWindow( hPlot );

	hDC = GetDC( hPlot );
	hPen = CreatePen( PS_SOLID, 1, RGB(0,0,0) );
	hOldPen = SelectObject( hDC, hPen );
	SelectObject( hDC, hBrush );
	GetClientRect( hPlot, &Rect );
	width = Rect.right - Rect.left + 1;
	height = Rect.bottom - Rect.top + 1;
	SetTextAlign( hDC, TA_LEFT | TA_BASELINE );
	SetBkMode( hDC, TRANSPARENT );

	hBitmap = CreateCompatibleBitmap( hDC, width, height );
	{
		HDC	hMemoryDC = CreateCompatibleDC( hDC );
		HBITMAP	hOldBitmap;

		hOldBitmap = SelectObject( hMemoryDC, hBitmap );
		BitBlt( hMemoryDC, 0, 0, width, height, hDC, 0, 0, SRCCOPY );
		SelectObject( hMemoryDC, hOldBitmap );

		DeleteDC( hMemoryDC );
	}

	ldef = SCREEN_SIZE / (float) (width > height ? width : height);
	xscreen_to_pix = (float) width / SCREEN_SIZE;
	yscreen_to_pix = (float) height / SCREEN_SIZE;
	aspect = (float) height / width;
	default_ctype("black");
	dirty = M_DONE;

	return	0;
}

void
msw_enable()
{
	SetFocus( hPlot );
}

void
msw_line( int x1, int y1, int x2, int y2 )
{
	MoveTo( hDC, getx(x1), gety(y1) );
	LineTo( hDC, getx(x2), gety(y2) );
	dirty = M_SAVE;
}

void
msw_reloc( int x, int y )
{
	MoveTo( hDC, getx(x), gety(y) );
}

void
msw_draw( int x, int y )
{
	LineTo( hDC, getx(x), gety(y) );
	dirty = M_SAVE;
}

int
msw_char( char *str, int x, int y )
{
	if( str )
		TextOut( hDC, getx(x), gety(y), str, strlen(str) ), dirty = M_SAVE;
	return	0;
}

unsigned	style[] =
	{
	PS_SOLID,
	PS_DOT,
	PS_DASH,
	PS_DASH,
	PS_DASHDOT,
	PS_DASHDOTDOT,
	PS_DASHDOT
	};

int
msw_ltype( int i )
{
	static	oldcolor;

	switch( i )
	{
	case	10:
		oldcolor = pencolor;
		pencolor = RGB(0,0,0);
		setpen();
		break;
	case	11:
		pencolor = oldcolor;
		setpen();
		break;
	default:
		penstyle = style[i];
		setpen();
	}
	return	0;
}

int
msw_lweight( int i )
{
	penwidth = i;
	setpen();
	return	0;
}

void
msw_erase(int raise_on_erase)
{
   volatile int roe = raise_on_erase; roe++;

	RECT	Rect;
	HPEN	hOldPen = SelectObject( hDC, GetStockObject( NULL_PEN ) );
	GetClientRect( hPlot, &Rect );
	Rectangle( hDC, Rect.left, Rect.top, Rect.right, Rect.bottom );
	SelectObject( hDC, hOldPen );

	dirty = M_SAVE;
}

void
msw_idle()
{
	SetFocus( hMain );
}

int
msw_cursor( int get_pos, int *x, int *y )
{
	cursorbut = M_NONE;
	while( cursorbut == M_NONE )
	{
		MSG	msg;
		if( GetMessage( &msg, NULL, NULL, NULL ) )
			DispatchMessage( &msg );
	}

	*x = putx( cursorpos.x );
	*y = puty( cursorpos.y );
	return	(cursorbut == M_ENTER) ? 'e' : 'q';
}

void
msw_close()
{
	SelectObject( hDC, hOldPen );
	DeleteObject( hPen );
	ReleaseDC( hPlot, hDC );
	DeleteObject( hBitmap );
	DestroyWindow( hPlot );
}

#pragma	argsused
int
msw_dot( int x, int y )
{
	SetPixel( hDC, getx(x), gety(y), pencolor );
	dirty = M_SAVE;
	return	0;
}

#pragma	argsused
int
msw_fill_pt( int n )
{
	return	-1;
}

void
msw_ctype( int r, int g, int b )
{
	pencolor = RGB( r, g, b );
	setpen();
}

#pragma	argsused
int
msw_set_ctype( COLOR *col, int n )
{
	return	-1;
}

void
msw_gflush()
{
}

void
msw_page()
{
}

#pragma	argsused
void
msw_redraw( int fildes )
{
}
#else
   main();				/* shut up compiler */
#endif
