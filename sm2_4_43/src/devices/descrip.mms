#
# Default rules for compiling code.
#
.c.obj :
        @- delete $*.obj.*
        cc $(CFLAGS) $*.c
        lib/replace [-]libdevices.olb $*.obj
#
#
# loader flags
#
#LDFLAGS = /debug
LDFLAGS =
#
# Object files
#
TTY_OFILES = 	ttyindex.obj, ttyget.obj, ttyopen.obj, ttyputs.obj, ttyio.obj
OFILES =	stgsetup.obj, stgencode.obj, stgerase.obj, stgcursor.obj, \
		stgclose.obj, stgline.obj, stgreloc.obj, stgfillpt.obj, \
		stgchar.obj, stglist.obj, imagen.obj, sunwindows.obj, \
		x10.obj, x11.obj, raster.obj, vaxuis.obj, metafile.obj
UTIL_OFILES =	get1char.obj, gstrcpy.obj
#
# Update the library in the directory above
#
all : $(OFILES), $(TTY_OFILES), $(UTIL_OFILES)
	@ wait 0
#
# Clean up directory
#
clean :
	- delete *.obj.*, *_rej..*, *.*_rej.*, *_orig..*, *.*_orig.*
#
# Dependencies
#
#START_DEPEND
stgsetup.obj :	[-]devices.h
stgsetup.obj :	[-]options.h
stgsetup.obj :	[-]sm.h
stgsetup.obj :	[-]stdgraph.h
stgsetup.obj :	[-]tty.h
stgencode.obj :	[-]charset.h
stgencode.obj :	[-]options.h
stgencode.obj :	[-]stdgraph.h
stgencode.obj :	[-]tty.h
stgerase.obj :	[-]options.h
stgerase.obj :	[-]sm.h
stgerase.obj :	[-]stdgraph.h
stgerase.obj :	[-]tty.h
stgcursor.obj :	[-]charset.h
stgcursor.obj :	[-]options.h
stgcursor.obj :	[-]sm.h
stgcursor.obj :	[-]stdgraph.h
stgcursor.obj :	[-]tty.h
stgclose.obj :	[-]options.h
stgclose.obj :	[-]stdgraph.h
stgclose.obj :	[-]tree.h
stgclose.obj :	[-]tty.h
stgline.obj :	[-]options.h
stgline.obj :	[-]sm.h
stgline.obj :	[-]stdgraph.h
stgline.obj :	[-]tty.h
stgreloc.obj :	[-]options.h
stgreloc.obj :	[-]sm.h
stgreloc.obj :	[-]stdgraph.h
stgreloc.obj :	[-]tty.h
stgchar.obj :	[-]options.h
stgchar.obj :	[-]sm.h
stgchar.obj :	[-]stdgraph.h
stgchar.obj :	[-]tty.h
stglist.obj :	[-]options.h
stgfillpt.obj :	[-]options.h
stgfillpt.obj :	[-]sm.h
stgfillpt.obj :	[-]stdgraph.h
stgfillpt.obj :	[-]tty.h
imagen.obj :	[-]options.h
imagen.obj :	[-]sm.h
imagen.obj :	[-]stdgraph.h
imagen.obj :	[-]tty.h
raster.obj :	[-]devices.h
raster.obj :	[-]options.h
raster.obj :	[-]raster.h
raster.obj :	[-]sm.h
raster.obj :	[-]stdgraph.h
raster.obj :	[-]tty.h
raster.obj :	[-]vectors.h
sgi.obj :	[-]devices.h
sgi.obj :	[-]options.h
sgi.obj :	[-]sm.h
sunview.obj :	SM_icon.h
sunview.obj :	[-]options.h
sunview.obj :	[-]sm.h
sunwindows.obj :	[-]options.h
sunwindows.obj :	[-]sm.h
x10.obj :	cursor.h
x10.obj :	[-]options.h
x10.obj :	[-]sm.h
x11.obj :	SM_icon.h
x11.obj :	[-]devices.h
x11.obj :	[-]options.h
x11.obj :	[-]sm.h
x11.obj :	[-]stdgraph.h
x11.obj :	[-]tty.h
unixpc.obj :	[-]options.h
unixpc.obj :	[-]sm.h
unixpc.obj :	[-]stdgraph.h
unixpc.obj :	[-]tty.h
unixpc.obj :	upcrast.c
metafile.obj :	[-]devices.h
metafile.obj :	[-]options.h
metafile.obj :	[-]sm.h
svgalib.obj :	[-]options.h
svgalib.obj :	[-]sm.h
ttyindex.obj :	[-]options.h
ttyindex.obj :	[-]tty.h
ttyget.obj :	[-]options.h
ttyget.obj :	[-]tty.h
ttyopen.obj :	cacheg.dat
ttyopen.obj :	[-]options.h
ttyopen.obj :	[-]tty.h
ttyputs.obj :	[-]options.h
ttyputs.obj :	[-]tty.h
ttyio.obj :	[-]options.h
ttyio.obj :	[-]sm.h
ttyio.obj :	[-]stdgraph.h
ttyio.obj :	[-]tty.h
get1char.obj :	[-]charset.h
get1char.obj :	[-]devices.h
get1char.obj :	[-]options.h
gstrcpy.obj :	[-]options.h
