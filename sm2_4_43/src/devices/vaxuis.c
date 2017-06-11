#include	"copyright.h"
#include	"options.h"

#if defined(VAXUIS)
#include	<uisentry.h>
#include	<uisusrdef.h>
#include	<descrip.h>
#include	"sm_declare.h"
#include	"sm.h"
#include	<stdio.h>
#include	<ctype.h>
#include	<math.h>

unsigned long int	vd_id, wd_id;	/* Virtual display and Window IDs	*/
long unsigned		atb = 1;	/* Attribute block for lines	*/
long unsigned		fatb = 2;	/* Attribute block for filled points	*/
long int		cur_x_pos = 0;	/* Current plot positions	*/
long int		cur_y_pos = 0; 
float			width = 0;	/* Width of display window, 0 if uninitialized	*/
float			height = 0;	/* Height of display window, 0 if uninitializewd	*/
long int		numxpixels;	/* Num of x pixels on workstation screen	*/
long int		numypixels;	/* Num of y pixels on workstation screen	*/

#define		FUDGE	(float)0.4	/* Fudge to make filled points same size as others	*/

unsigned long int	line_type[  ] =
	{
		0xFFFFFFFF,	/* Ltype 0 - solid line			*/
		0x55555555,	/* Ltype 1 - dotted line		*/
		0xF8F8F8F8,	/* Ltype 2 - short dash			*/
		0xFFF0FFF0,	/* Ltype 3 - long dash			*/
		0xCECECECE,	/* Ltype 4 - dot - short dash		*/
		0xCFFCCFFC,	/* Ltype 5 - dot - long dash		*/
		0xF3FCF3FC,	/* Ltype 6 - short dash - long dash	*/
		0,
		0,
		0
	};

int	uis_setup( char *str )
{
	float			screenwidth,	/* Width of workstation screen in cm	*/
				screenheight,	/* Height of workstation screen in cm	*/
				xres,		/* Horiz. screen resolution in pixel/cm	*/
				yres,		/* Vert. screen resolution in pixel/cm	*/
				x_window_cm,	/* Req. width of graphics window in cm	*/
				y_window_cm;	/* Req. height of graphics window in cm */


	float			x1 = 0.0,	/* Dummy coord for call to uis$create_display	*/
				x2 = 1.0,
				y1 = 0.0,
				y2 = 1.0;

	long unsigned		atb_template= 0;/* Number of attribute block with */
	long int		linemode = UIS$C_WIDTH_PIXELS;
	long int		solid_fill = PATT$C_FOREGROUND;
	float			deflinewidth = 1.0;	/* Default line width	*/

		
			/* First create the window WINXPIX x WINYPIX	*/
	$DESCRIPTOR( wsdescrip, "SYS$WORKSTATION" );	/* Generate descriptor	*/
							/* for workstation	*/
	$DESCRIPTOR( tdescrip, "SM" );
	$DESCRIPTOR( uis_fill_patterns, "UIS$FILL_PATTERNS" );

				/* Get information about the user's workstation	*/
	uis$get_display_size( &wsdescrip,&screenwidth, &screenheight, &xres, &yres, &numxpixels, &numypixels );
	if( yres == 0 || xres == 0 || screenwidth == 0 || screenheight == 0 )	/* Bail out if get_display_size failed	*/
		return -1;

				/* Try to accomodate requested graphics window size */
	if ( sscanf( str, "%f %f", &x_window_cm, &y_window_cm ) == 2 )	/* Only use requested coordinates if both height and	*/
		{							/* width are specified and are reasonable	*/
		if ( x_window_cm >= 1.0 && x_window_cm < screenwidth
			&& y_window_cm >= 1.0 && y_window_cm < screenheight )
			{
			width = x_window_cm;
			height = y_window_cm;
			}
		else
			puts( "\n Error in requested window size - default values used\n");
		}
	else
		{	/* If no arguements or wrong number are given use last value	*/
		if( height == 0.0 || width == 0.0 )
			{
			height = screenheight / 2;
			width = screenwidth / 2;
			}
		}
	vd_id = uis$create_display( &x1, &y1, &x2, &y2, &width, &height );
	wd_id = uis$create_window( &vd_id, &wsdescrip, &tdescrip );
	
	xscreen_to_pix = ( width * xres ) / (float) SCREEN_SIZE;	/* Tell SM about our transformation values	*/
	yscreen_to_pix = ( height * yres ) / (float) SCREEN_SIZE;
	aspect = (float) height / width;		/* The aspect ratio	*/
	ldef = 1 / xscreen_to_pix;			/* The minimum linewidth	*/
	termout = 1;					/* The device is a terminal	*/

		/* Use attribute block atb for lines	*/
	uis$set_line_width( &vd_id, &atb_template, &atb, &deflinewidth, &linemode );

		/* Use attribute block fatb fot filled points	*/
	uis$set_font( &vd_id, &atb_template, &fatb, &uis_fill_patterns );
	uis$set_fill_pattern( &vd_id, &fatb, &fatb, &solid_fill );

	return 0;
 }

void	uis_enable( void )
{;}

void	uis_line( int x1, int y1, int x2, int y2 )
{
	long int	lx1, lx2, ly1, ly2;

	lx1 = xscreen_to_pix * x1;	/* Convert to long signed format for uisdc calls	*/
	lx2 = xscreen_to_pix * x2;
	ly1 = yscreen_to_pix * y1;
	ly2 = yscreen_to_pix * y2;
	uisdc$line( &wd_id, &atb, &lx1, &ly1, &lx2, &ly2);
	cur_x_pos = lx2;
	cur_y_pos = ly2;
}

void	uis_reloc( int x, int y )
{
	cur_x_pos = xscreen_to_pix * x;
	cur_y_pos = yscreen_to_pix * y;
}

void	uis_draw( int x, int y )
{
	long int	new_x_pos, new_y_pos;

	new_x_pos = xscreen_to_pix * x;
	new_y_pos = yscreen_to_pix * y;
	uisdc$line( &wd_id, &atb, &cur_x_pos, &cur_y_pos, &new_x_pos, &new_y_pos );
	cur_x_pos = new_x_pos;
	cur_y_pos = new_y_pos;
}

int	uis_char( char *pstr, int x, int y )
{
	long int	lx, ly;
	struct	dsc$descriptor_d strdscrp;

	if( pstr == NULL )
		return 0;

	strdscrp.dsc$w_length = ( unsigned short )strlen( pstr );
	strdscrp.dsc$b_dtype = DSC$K_DTYPE_T;
	strdscrp.dsc$b_class = DSC$K_CLASS_D;
	strdscrp.dsc$a_pointer = pstr;

	lx = xscreen_to_pix * x;
	ly = ( yscreen_to_pix * y ) + 8;
	uisdc$text( &wd_id, &atb, &strdscrp, &lx, &ly );
	return 0;
}

int	uis_ltype( int i )
{
	if( i >= 0 && i <= 6 )
		{
		uis$set_line_style( &vd_id, &atb, &atb, line_type + i );
		return 0;
		}
	else
		return -1;
}

int	uis_lweight( double di )
{
	float		new_width =  di;
	long int	line_mode = UIS$C_WIDTH_PIXELS;

	uis$set_line_width( &vd_id, &atb, &atb, &new_width, &line_mode);
	return 0;
}
void	uis_erase(int raise_on_erase)
{
   volatile int roe = raise_on_erase; roe++;
	
	uis$erase( &vd_id );
}

void	uis_idle( void )
{;}

int	uis_cursor( int get_pos, int *px, int *py )
{
	int			ret_val;
	char			c;
	long int		xpos, ypos;	/* Long int's for UIS calls	*/
	unsigned long int	buttons;

	for( ret_val = 0; !ret_val; )
		{
		if( uisdc$get_pointer_position( &wd_id, &xpos, &ypos ) )
			{
			uis$get_buttons( &wd_id, &buttons );			
			if( !( buttons & UIS$M_POINTER_BUTTON_1 ) )
				ret_val = (int) 'e';
			else if ( !( buttons & UIS$M_POINTER_BUTTON_2 ) )
				ret_val = (int) 'p';
			else if ( !( buttons & UIS$M_POINTER_BUTTON_3 ) )
				ret_val = (int) 'q';

			}
/*		if( !feof( stdin ) )
			ret_val = getchar();
		printf( "feof = %d ", feof( stdin ) );*/
		
		}

	*px = xpos / xscreen_to_pix;
	*py = ypos / yscreen_to_pix;

	return ret_val;
}

void	uis_close( void )
{
	uis$delete_window( &wd_id );
	uis$delete_display( &vd_id );
}

int	uis_dot( int x, int y )
{ 
	
	cur_x_pos = xscreen_to_pix * x;	/* First move current position	*/
	cur_y_pos = yscreen_to_pix * y;
	uisdc$line( &wd_id, &atb, &cur_x_pos, &cur_y_pos, &cur_x_pos,
			 &cur_y_pos ); /* Draw a point	*/
	return 0;
}


void	uis_ctype( int r, int g, int b )
{;}

int	uis_set_ctype( color, n )
COLOR	*color;
int	n;
{
	return -1;
}

void	uis_gflush( void )
{;}

int uis_fill_pt( int n)
                          /*  n - number of sides */
{
	float 		dtheta, theta;
  	static float 	old_ang;		/* old values of angle */
   	int 		i, xpsize, ypsize;	/* scale for points */
      
   	static int 	num = -1;    		/* number of vertices used last time */
	static int	old_xp,old_yp;		/* old values of xpsize, ypsize */
	long int	vlistx[ MAX_POLYGON + 1];	/* corners of point	*/
	long int	vlisty[ MAX_POLYGON + 1];
	static long int	vlist0x[ MAX_POLYGON + 1];	/* corners of point at ( 0,0 )	*/
	static long int	vlist0y[ MAX_POLYGON + 1];
	long int	num_plus_one;			/* Number of points to plot	*/

   if(n < 2) 
	{
      	uisdc$line(&vd_id, &atb, &cur_x_pos, &cur_y_pos, &cur_x_pos, &cur_y_pos );
      	return(0);
   	}

   dtheta = 2 * PI / n;
   xpsize = ypsize =((float)PDEF*(float)numxpixels*eexpand*FUDGE )/SCREEN_SIZE;

   if(aspect > 1.0)                    /* allow for aspect ratio of screen */
      	ypsize /= aspect;
   else if(aspect < 1.0)
      	xpsize *= aspect;

   if(n > MAX_POLYGON) 
	num = MAX_POLYGON;
   else 
	num = n;

   if(n != num || aangle != old_ang || xpsize != old_xp || ypsize != old_yp)
	{
      	theta = 3*PI/2 + dtheta/2 + aangle*PI/180;

      	old_ang = aangle;
      	old_xp = xpsize;
      	old_yp = ypsize;

      	for( i = 0; i < num; i++ )
	 	{
	 	vlist0x[i] =  xpsize * cos(theta) + 0.5 ;
		vlist0y[i] = ypsize * sin(theta) + 0.5 ;
		vlistx[i] = vlist0x[i] + cur_x_pos;
		vlisty[i] = vlist0y[i] + cur_y_pos;
		theta += dtheta;
      		}
   	}
    else 
	{				/* no need for more trig. */
      	for( i = 0; i < num; i++ ) 
		{
		vlistx[i] = vlist0x[i] + cur_x_pos;
		vlisty[i] = vlist0y[i] + cur_y_pos;
      		}
   }

	
   vlistx[i] = vlistx[0];	/* Last point to close the polygon	*/
   vlisty[i] = vlisty[0];

   num_plus_one = num + 1;	/* Plot and fill the point	*/
   uisdc$plot_array( &wd_id, &fatb, &num_plus_one, vlistx, vlisty );

   return 0;
}
#endif
