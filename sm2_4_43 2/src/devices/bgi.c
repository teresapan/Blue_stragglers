#include	"copyright.h"
/*
 * SM driver for PC, using Borland's graphics
 *
 * Written by Laurent Bartholdi (lbartho@uni2a.unige.ch)
 */
#include	"options.h"
#if defined(BGI)
#include	<graphics.h>
#include	<svga16.h>
#include	<svga256.h>
#include	<stdio.h>
#include	"sm.h"
#include	"sm_declare.h"

extern int sm_verbose;

static int _driver, _mode;

static int
sm2bgix( int x )
{
	return	x * xscreen_to_pix;
}

static int
sm2bgiy( int y )
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

#define	BUFSIZE	0x1000
#define	CHRSIZE	0x2000
static unsigned	oldcursor;
static char far oldscreen[BUFSIZE], far oldchars[CHRSIZE];
static enum SWAPMODES { SWAP, SWITCH, NONE }
	swap = SWAP;		/* swap and save screens ? */

#define	VIDEOMODE	(*((unsigned volatile char far *) 0x00400049))
#define	EXTVMODE	(*((unsigned volatile char far *) 0x00400087))
#define	SHIFTSTATE	(*((unsigned volatile char far *) 0x00400017))
#define	RIGHTSHIFT	0x01
#define	LEFTSHIFT	0x02

static unsigned char	textmode, graphmode;

#define	EXTNUM	1000		/* big magic number */

static struct	TABLE
{
	char	*s;
	int	driver, mode;
}
	_modes[] = {
	
	{"DETECT", DETECT, },

	{"CGA", CGA, CGAHI },
	{"CGAC0", CGA, CGAC0 },
	{"CGAC1", CGA, CGAC1 },
	{"CGAC2", CGA, CGAC2 },
	{"CGAC3", CGA, CGAC3 },
	{"CGAHI", CGA, CGAHI },

	{"MCGA", MCGA, MCGAHI },
	{"MCGAC0", MCGA, MCGAC0 },
	{"MCGAC1", MCGA, MCGAC1 },
	{"MCGAC2", MCGA, MCGAC2 },
	{"MCGAC3", MCGA, MCGAC3 },
	{"MCGAMED", MCGA, MCGAMED },
	{"MCGAHI", MCGA, MCGAHI },

	{"EGA", EGA, EGAHI },
	{"EGALO", EGA, EGALO },
	{"EGAHI", EGA, EGAHI },

	{"EGA64", EGA64, EGA64HI },
	{"EGA64LO", EGA64, EGA64LO },
	{"EGA64HI", EGA64, EGA64HI },

	{"EGAMONO", EGAMONO, EGAMONOHI },
	{"EGAMONOHI", EGAMONO, EGAMONOHI },

	{"HERCMONO", HERCMONO, HERCMONOHI },
	{"HERCMONOHI", HERCMONO, HERCMONOHI },

	{"ATT400", ATT400, ATT400HI },
	{"ATT400C0", ATT400, ATT400C0 },
	{"ATT400C1", ATT400, ATT400C1 },
	{"ATT400C2", ATT400, ATT400C2 },
	{"ATT400C3", ATT400, ATT400C3 },
	{"ATT400MED", ATT400, ATT400MED },
	{"ATT400HI", ATT400, ATT400HI },

	{"VGA", VGA, VGAHI },
	{"VGALO", VGA, VGALO },
	{"VGAMED", VGA, VGAMED },
	{"VGAHI", VGA, VGAHI },

	{"PC3270", PC3270, PC3270HI },
	{"PC3270HI", PC3270, PC3270HI },

	{"IBM8514", IBM8514, IBM8514HI },
	{"IBM8514LO", IBM8514, IBM8514LO },
	{"IBM8514HI", IBM8514, IBM8514HI },

	{"320x200x16", EXTNUM, SVGA320x200x16 },
	{"640x200x16", EXTNUM, SVGA640x200x16 },
	{"640x350x16", EXTNUM, SVGA640x350x16 },
	{"640x480x16", EXTNUM, SVGA640x480x16 },
	{"800x600x16", EXTNUM, SVGA800x600x16 },
	{"1024x768x16", EXTNUM, SVGA1024x768x16 },

	{"320x200x256", EXTNUM+1, SVGA320x200x256 },
	{"640x400x256", EXTNUM+1, SVGA640x400x256 },
	{"640x480x256", EXTNUM+1, SVGA640x480x256 },
	{"800x600x256", EXTNUM+1, SVGA800x600x256 },
	{"1024x768x256", EXTNUM+1, SVGA1024x768x256 },

	{ NULL, }
	};

static char	*newdrivers[] = {
	"SVGA16", "SVGA256"
	};

static struct	MODENTRY
{
	char	*s;
	enum SWAPMODES	num;
}
	swapmodes[] = {	
	{"none", NONE },
	{"switch", SWITCH },
	{"swap", SWAP },
	{ NULL, }
	};

static unsigned	
getcurpos()
{
asm {
	mov	ah, 0x0f
	int	10h		/* get page nr. */
	mov	ah, 0x03
	int	10h
	}
	return	_DX;
}

static void	
setcurpos( unsigned curpos )
{
asm {
	mov	ah, 0x0f
	int	10h		/* get page nr. */
	mov	ah, 0x02
	mov	dx, curpos
	int	10h
	}
}

static void	
setvideomode( unsigned char mode )
{
asm {
	mov	al, mode	
	mov	ah, 0
	or	al, 80h		/* set hi bit, so the screen ain't blanked */
	int	10h
	}
	if( VIDEOMODE & 0x80 )	/* hi bit put here */
		VIDEOMODE &= 0x7f;	/* put the new mode w/o the high bit set */
	else
		EXTVMODE &= 0x7f;	/* some drivers set this high bit instead !?! */
}

static void	
swapscreens()
{
	char far	*p, *q, c;	/* i KNOW this is dirty ! */

	for( q = oldscreen, p = (char far *) 0xb8000000; 
		q < oldscreen + BUFSIZE; 
		c = *p, *p++ = *q, *q++ = c );
}

int
bgi_setup(disp)
char *disp;
{
	char	*stdmode;
	char	*bgipath;
	struct TABLE	*p;
	struct MODENTRY	*m;
	char	dname[66], swmode[10];

	if( !*disp && (*(stdmode = print_var("stdmode")) != '\0') )
		disp = stdmode;

	*dname = 0;
	if( sscanf( disp, "%s %9s", dname, swmode ) == 2 )
	{
		for( m = swapmodes; m->s; m++ )
		if( stricmp( m->s, swmode ) == 0 )
		{
			swap = m->num;
			break;
		}
		if( !m->s )
		{
			msg_1s("Unknown swap mode '%s'\n", swmode );
			msg("Your options are:\n");
			for( m = swapmodes; m->s != NULL; m++) {
				msg_1s("\t%s\n",m->s);
			}
			return	-1;
		}
	}

	if( stricmp( dname, "?") == 0 )
	{
		for( p = _modes; p->s; p++ ) {
			msg_2s("%-19s%s",p->s,((p - _modes)%4) == 3 ? "\n" : " ");
		}
		msg("\n");
		return	-1;
	}

	if( *dname != '\0')
	{
		for( p = _modes; p->s; p++ )
		if( !stricmp( p->s, dname ) )
		{
			if( p->driver >= EXTNUM )	/* extended driver */
				_driver = installuserdriver( 
					newdrivers[p->driver-EXTNUM], NULL );
			else	_driver = p->driver;
			_mode = p->mode;
			break;
		}
		if( !p->s )
		{
			char	driver[66];
			if( sscanf( dname, "%[^.]s.%d", driver, &_mode ) == 2 )
				_driver = installuserdriver( driver, NULL );
			else {
				msg_1s("bgi: unknown driver/mode '%s'\n", dname );
				msg("use `device bgi ?' for a list of valid drivers\n");
				return	-1;
			}
		}
	} else
		_driver = DETECT;
	
	if( swap == SWAP )
		swapscreens();	/* we don't care if we put garbage on the screen, */
				/* we only want to read the video buffer */
	if( swap != NONE )
	{
		oldcursor = getcurpos();
			/* save coords, as the're scratched out by initgraph */
		textmode = VIDEOMODE;	/* and get the text mode, so we can restore it */
	}

	if( (bgipath = getenv("BGIPATH")) == NULL )
	{
		if( *(bgipath = print_var("bgipath")) == '\0' )
			bgipath = "";
	}
	if(*bgipath == '\0' && sm_verbose > 0) {
	   msg("You don't have a bgipath set\n");
	}

	initgraph( &_driver, &_mode, bgipath );
	if( graphresult() < 0 )
	{
		msg_1s("bgi: can't open driver/mode '%s'\n", dname );
		return	-1;
	}

	termout = 1;
	ldef = SCREEN_SIZE / (float) getmaxx();
	xscreen_to_pix = (float) getmaxx() / SCREEN_SIZE;
	yscreen_to_pix = (float) getmaxy() / SCREEN_SIZE;
	aspect = (float) getmaxy() / getmaxx();
	default_ctype("white");
	settextjustify( LEFT_TEXT, CENTER_TEXT );

	if( swap != NONE )
	{
		setcurpos( oldcursor );
		graphmode = VIDEOMODE;		/* get the graphmode, too */
	}

	return	0;
}

void
bgi_enable()
{
	if( VIDEOMODE == graphmode || swap == NONE )
		return;

	oldcursor = getcurpos();
	if( swap == SWAP )
		swapscreens();
	setvideomode( graphmode );
}

void
bgi_line( int x1, int y1, int x2, int y2 )
{
	moveto( sm2bgix(x1), sm2bgiy(y1) );
	lineto( sm2bgix(x2), sm2bgiy(y2) );
}

void
bgi_reloc( int x, int y )
{
	moveto( sm2bgix(x), sm2bgiy(y) );
}

void
bgi_draw( int x, int y )
{
	lineto( sm2bgix(x), sm2bgiy(y) );
}

int
bgi_char( char *str, int x, int y )
{
	if( str )
		outtextxy( sm2bgix(x), sm2bgiy(y), str );
	return	0;
}

static unsigned	pattern[] =
	{
	0xffff,		/* ____ */
	0xaaaa,		/* .... */
	0xeeee,		/* ---- */
	0xfcfc,		/* _ _  */
	0x9c9c,		/* ._._ */
	0xc7f8,		/* . __ */
	0xf1f8		/* _ __ */
	};

int
bgi_ltype( int i )
{
	static	oldcolor;

	switch( i )
	{
	case	10:
		oldcolor = getcolor();
		setcolor( BLACK );
		break;
	case	11:
		setcolor( oldcolor );
		break;
	default:
		setlinestyle( USERBIT_LINE, pattern[i], NORM_WIDTH );
	}
	return	0;
}

int
bgi_lweight( int i )
{
	struct linesettingstype	l;

	getlinesettings( &l );

	switch( i )
	{
	case	0:
		setlinestyle( USERBIT_LINE, l.upattern, NORM_WIDTH );
		return	0;
	case	2:
		setlinestyle( USERBIT_LINE, l.upattern, THICK_WIDTH );
		return	0;
	default:
		return	1;
	}
}

void
bgi_erase(int raise_on_erase)
{
   volatile int roe = raise_on_erase; roe++;

	cleardevice();
}

void
bgi_idle()
{
	if( swap == NONE )
		return;

	setvideomode( textmode );
	if( swap == SWAP )
		swapscreens();
	setcurpos( oldcursor );
}

static unsigned	
get_char()
{
asm {
	mov	ax, 0000h
	int	16h
	}
	return	_AX;
}

#define	SIZE	30

static void	
showcursor( int x, int y )
{
	int	oldcolor;

	x = sm2bgix(x);
	y = sm2bgiy(y);		/* BGI coordinates, please */

	oldcolor = getcolor();
	setwritemode( XOR_PUT );
	setcolor( WHITE );

	line( x-SIZE, y, x+SIZE, y );
	line( x, y-SIZE, x, y+SIZE );
	
	setwritemode( COPY_PUT );
	setcolor( oldcolor );
}

int
bgi_cursor( int get_pos, int *x, int *y )
{
	unsigned	c, speed = ldef, incr = ldef;
	
	long	X = SCREEN_SIZE / 2, Y = SCREEN_SIZE / 2;
	
	for(;;)
	{
		showcursor( X, Y );
		c = get_char();
		showcursor( X, Y );

		switch( c & 0xff )
		{
		case	'q':
		case	'Q':
		case	'e':
		case	'E':
			*x = X, *y = Y;
			return	(c & 0xff) | 0x20;	/* lower-case */
		case	0:
		case	'2':
		case	'4':
		case	'6':
		case	'8':
			switch( c >> 8 )
			{
			case	'H':
				Y += speed;
				break;
			case	'P':
				Y -= speed;
				break;
			case	'K':
				X -= speed;
				break;
			case	'M':
				X += speed;
			}
		}
		if( X > SCREEN_SIZE )
			X = SCREEN_SIZE;
		if( X < 0 )
			X = 0;
		if( Y >= SCREEN_SIZE )
			Y = SCREEN_SIZE;
		if( Y < 0 )
			Y = 0;

		if( SHIFTSTATE & LEFTSHIFT )
			speed += incr;
		if( SHIFTSTATE & RIGHTSHIFT && speed > incr )
			speed -= incr;
	}
}

void
bgi_close()
{
	if( VIDEOMODE == textmode && swap != NONE )
	{
		oldcursor = getcurpos();
		if( swap == SWAP )
			swapscreens();
	}

	closegraph();

	if( swap == SWAP )
		swapscreens();
	if( swap != NONE )
		setcurpos( oldcursor );
}

int
bgi_dot( int x, int y )
{
	putpixel( sm2bgix(x), sm2bgiy(y), getcolor() );
	return	0;
}

#pragma	argsused
int
bgi_fill_pt( int n )
{
	return	-1;
}

void
bgi_ctype( int r, int g, int b )
{
   r = (r < 128) ? 0 : 1;
   g = (g < 128) ? 0 : 1;
   b = (b < 128) ? 0 : 1;
   
   if(r) {
      if(g) {
         if(b) {
            setcolor(WHITE);
         } else {
            setcolor(YELLOW);
         }
      } else {
         if(b) {
            setcolor(MAGENTA);
         } else {
            setcolor(RED);
         }
      }
   } else {
      if(g) {
         if(b) {
            setcolor(CYAN);
         } else {
            setcolor(GREEN);
         }
      } else {
         if(b) {
            setcolor(BLUE);
         } else {
            setcolor(BLACK);
         }
      }
   }  
}

#pragma	argsused
int
bgi_set_ctype( COLOR *col, int n )
{
	return	-1;
}

void
bgi_gflush()
{
}

void
bgi_page()
{
}

#pragma	argsused
void
bgi_redraw( int fildes )
{
}
#else
   main();				/* shut up compiler */
#endif
