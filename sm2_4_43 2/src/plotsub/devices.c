#include "copyright.h"
/*
 *  These are the definitions of all devices currently known to SM.
 * Each device is described as an element in the array DEVICES devices[].
 * The current device is element devnum of the array.
 *
 *  To add another, define another element in devices[].
 * The first element ( devices[i].name ) is the name by which the device will
 * be known to SM.
 * This array defines any information that the routines which talk to the 
 * device may need to know. This should include the names of the routines
 * to e.g. erase the display. Any unimplemented calls should be specified
 * as the call from "nodevice" (e.g. no_ctype). You should also add the
 * declarations to declare.h (protected by #ifdef's, of course)
 *
 * To provide a new function, add it to the definition of DEVICES, in
 * devices.h (along with a macro to access the function on the current device)
 * and add an entry into the initialisation of each element of devices[]
 * in this file.
 */
#include <stdio.h>
#include "options.h"
#include "devices.h"
#include "sm.h"
#include "sm_declare.h"
/*
 * Functions for nodevice
 */
int no_char() { return(-1); }
void no_close() { ; }
void no_ctype() { ; }
int no_cursor() { return(-1); }
int no_dot() { return(-1); }
void no_draw() { ; }
void no_enable() { ; }
void no_erase() { ; }
int no_fill_pt() { return(-1); }
int no_fill_polygon() { return(-1); }
void no_gflush() { ; }
void no_idle() { ; }
void no_line() { ; }
int no_ltype() { return(0); }
int no_lweight() { return(-1); }
void no_page() { ; }
void no_redraw() { ; }
void no_reloc() { ; }
int no_setup() { return(0); }
int no_set_ctype() { return(-1); }

DEVICES devices[] = {
	{			/* nodevice - must be first */
   "nodevice",
   no_setup, no_enable, no_line, no_reloc,
   no_draw, no_char, no_ltype, no_lweight,
   no_erase, no_idle, no_cursor, no_close,
   no_dot, no_fill_pt, no_fill_polygon, no_ctype, no_set_ctype,
   no_gflush, no_page, no_redraw
	},
	{				/* stdgraph - must be second */
   "stdgraph",
   stg_setup, stg_enable, stg_line, stg_reloc,
   stg_draw, stg_char, stg_ltype, stg_lweight,
   stg_erase, stg_idle, stg_cursor, stg_close,
   stg_dot, stg_fill_pt, stg_fill_polygon, stg_ctype, stg_set_ctype,
   stg_gflush, stg_page, no_redraw
	},
	{
   "raster",			/* write file for rastering. Must be third */
   ras_setup, no_enable, ras_line, no_reloc,
   ras_draw,  no_char, no_ltype, no_lweight,
   ras_erase, no_idle, no_cursor, ras_close,
   no_dot, no_fill_pt, no_fill_polygon, ras_ctype, ras_set_ctype,
   no_gflush, no_page, no_redraw
	},
	{
   "metafile",				/* plot to a metafile. Must be fourth*/
   meta_setup, meta_enable, meta_line, meta_reloc,
   meta_draw, meta_char, meta_ltype, meta_lweight,
   meta_erase, meta_idle, meta_cursor, meta_close,
   meta_dot, meta_fill_pt, no_fill_polygon, meta_ctype, meta_set_ctype,
   meta_gflush, meta_page, meta_redraw
	},
#ifdef IMAGEN
	{
   "imagen",				/* full page plots on an Imagen */
   im_setup,  no_enable, im_line, no_reloc,
   im_draw,   no_char, no_ltype, im_lweight,
   no_erase,  no_idle, no_cursor, im_close,
   no_dot, im_fill_pt, no_fill_polygon, no_ctype, no_set_ctype,
   no_gflush, im_page, no_redraw
	},
#endif
#ifdef SGI
	{
   "sgi",
   sgi_setup, no_enable, sgi_line, sgi_reloc,
   sgi_draw, no_char, sgi_ltype, sgi_lweight,
   sgi_erase, no_idle, sgi_cursor, no_close,
   no_dot, no_fill_pt, no_fill_polygon, sgi_ctype, sgi_set_ctype,
   sgi_gflush, no_page, sgi_redraw
	},
#endif
#ifdef SUN_VIEW
	{
   "sunview",				/* plot with SunView */
   sunv_setup, sunv_enable, sunv_line, sunv_reloc,
   sunv_draw,  no_char,   sunv_ltype, sunv_lweight,
   sunv_erase, sunv_idle, sunv_cursor, sunv_close,
   no_dot,  sunv_fill_pt, no_fill_polygon, sunv_ctype, sunv_set_ctype,
   sunv_gflush, sunv_page, no_redraw
	},
#endif
#ifdef SUNWINDOWS
	{
   "sunwindows",			/* plot with sunwindows */
   sun_setup, sun_enable, sun_line, sun_reloc,
   sun_draw, sun_char, no_ltype, sun_lweight,
   sun_erase, sun_idle, sun_cursor, sun_close,
   no_dot,   sun_fill_pt, no_fill_polygon, sun_ctype, sun_set_ctype,
   no_gflush, no_page, no_redraw
	},
#endif
#if defined(SVGALIB)
     {
	"svgalib",			/* plot on a linux console */
	svgalib_setup, svgalib_enable, svgalib_line, svgalib_reloc,
	svgalib_draw,  svgalib_char, svgalib_ltype, svgalib_lweight,
	svgalib_erase, svgalib_idle, svgalib_cursor, svgalib_close,
	svgalib_dot,   svgalib_fill_pt, svgalib_fill_polygon, svgalib_ctype,
	svgalib_set_ctype, svgalib_gflush, svgalib_page, svgalib_redraw
     },
#endif
#ifdef XWINDOWS
	{
   "xwindows",				/* plot under x-windows V10 */
   x_setup, no_enable, x_line, x_reloc,
   x_draw,  x_char, x_ltype, no_lweight,
   x_erase, x_idle, x_cursor, x_close,
   no_dot,  x_fill_pt, no_fill_polygon, x_ctype, no_set_ctype,
   x_gflush, no_page, x_redraw
	},
#endif
#ifdef X11
	{
   "x11",				/* plot under x-windows V11 */
   x11_setup, no_enable, x11_line, x11_reloc,
   x11_draw,  x11_char, x11_ltype, x11_lweight,
   x11_erase, x11_idle, x11_cursor, x11_close,
   x11_dot,   x11_fill_pt, x11_fill_polygon, x11_ctype, x11_set_ctype,
   x11_gflush, x11_page, x11_redraw
  },
#endif
#ifdef UNIXPC
	{
   "upc",				/* plot for Unix PC */
   upc_setup, upc_enable, upc_line, upc_reloc,
   upc_draw, upc_char, upc_ltype, upc_lweight,
   upc_erase, upc_idle, upc_cursor, upc_close,
   upc_dot, upc_fill_pt, no_fill_polygon, upc_ctype, upc_set_ctype,
   upc_gflush, no_page, no_redraw
	},
#endif
#ifdef MSWINDOWS
	{
	"msw",
   msw_setup, msw_enable, msw_line, msw_reloc,
   msw_draw, msw_char, msw_ltype, msw_lweight,
   msw_erase, msw_idle, msw_cursor, msw_close,
   msw_dot, msw_fill_pt, no_fill_polygon, msw_ctype, msw_set_ctype,
   msw_gflush, no_page, no_redraw
	},
#endif
#ifdef BGI
	{
	"bgi",
   bgi_setup, bgi_enable, bgi_line, bgi_reloc,
   bgi_draw, bgi_char, bgi_ltype, bgi_lweight,
   bgi_erase, bgi_idle, bgi_cursor, bgi_close,
   bgi_dot, bgi_fill_pt, no_fill_polygon, bgi_ctype, bgi_set_ctype,
   bgi_gflush, no_page, no_redraw
	},
#endif
#ifdef VAXUIS
	{
   "vaxuis",				/* plot for VAX Workstation */
   uis_setup, uis_enable, uis_line, uis_reloc,
   uis_draw, uis_char, uis_ltype, uis_lweight,
   uis_erase, uis_idle, uis_cursor, uis_close,
   uis_dot, uis_fill_pt, no_fill_polygon, uis_ctype, uis_set_ctype,
   uis_gflush, no_page, no_redraw
	},
#endif
#ifdef OS2PM
	{
   "os2pm",				/* plot for OS/2 PC */
   pm_setup, pm_enable, pm_line, pm_reloc,
   pm_draw, pm_char, pm_ltype, pm_lweight,
   pm_erase, pm_idle, pm_cursor, pm_close,
   pm_dot, pm_fill_pt, pm_ctype, pm_set_ctype,
   pm_gflush, no_page, no_redraw
	},
#endif
};

int ndev = (sizeof(devices)/sizeof(DEVICES)),	/* number of devices */
    devnum = NODEVICE;				/* current  plotting device  */

/******************************************/
/*
 * Now a function to persuade the loader to extract this file from the library
 */
void
declare_devs()
{ ; }
