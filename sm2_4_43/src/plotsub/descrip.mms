#
# Default rules for compiling code.
#
.c.obj :
        @- delete $*.obj.*
        cc $(CFLAGS) $*.c
        library/replace [-]libplotsub.olb $*.obj
#
OBJECT = \
	compress.obj, dummies.obj, \
        axis.obj, chopper.obj, colour.obj, connect.obj, contour.obj, \
        cursor.obj, declare.obj, devices.obj, \
        interface.obj, label.obj, line.obj, point.obj, images.obj, \
        relocate.obj, set.obj, shade.obj, string.obj, surfaces.obj
#
#
# The target to update the library
#
all : $(OBJECT)
        @ wait 0
#
clean :
        - delete *.obj.*, *_rej..*, *.*_rej.*, *_orig..*, *.*_orig.*
#
# dependencies
#
#START_DEPEND
compress.obj :	[-]defs.h
compress.obj :	[-]options.h
compress.obj :	[-]sm.h
dummies.obj :	[-]defs.h
dummies.obj :	[-]devices.h
dummies.obj :	[-]options.h
dummies.obj :	[-]sm.h
dummies.obj :	[-]tree.h
dummies.obj :	[-]vectors.h
axis.obj :	[-]defs.h
axis.obj :	[-]options.h
axis.obj :	[-]sm.h
axis.obj :	[-]vectors.h
chopper.obj :	[-]defs.h
chopper.obj :	[-]devices.h
chopper.obj :	[-]options.h
chopper.obj :	[-]sm.h
chopper.obj :	[-]stdgraph.h
chopper.obj :	[-]tty.h
colour.obj :	[-]devices.h
colour.obj :	[-]options.h
colour.obj :	[-]sm.h
colour.obj :	[-]vectors.h
connect.obj :	[-]defs.h
connect.obj :	[-]options.h
connect.obj :	[-]sm.h
contour.obj :	image.h
contour.obj :	[-]options.h
contour.obj :	[-]sm.h
cursor.obj :	[-]charset.h
cursor.obj :	[-]devices.h
cursor.obj :	[-]options.h
cursor.obj :	[-]sm.h
cursor.obj :	[-]vectors.h
declare.obj :	[-]options.h
declare.obj :	[-]sm.h
devices.obj :	[-]devices.h
devices.obj :	[-]options.h
devices.obj :	[-]sm.h
dither.obj :	image.h
dither.obj :	[-]options.h
dither.obj :	[-]vectors.h
interface.obj :	[-]defs.h
interface.obj :	[-]devices.h
interface.obj :	[-]options.h
interface.obj :	[-]sm.h
interface.obj :	[-]tree.h
label.obj :	[-]defs.h
label.obj :	[-]options.h
label.obj :	[-]sm.h
line.obj :	[-]defs.h
line.obj :	[-]devices.h
line.obj :	[-]options.h
line.obj :	[-]sm.h
point.obj :	[-]defs.h
point.obj :	[-]devices.h
point.obj :	[-]options.h
point.obj :	[-]sm.h
point.obj :	[-]vectors.h
point.obj :	[-]yaccun.h
images.obj :	image.h
images.obj :	[-]options.h
images.obj :	[-]sm.h
images.obj :	[-]tree.h
images.obj :	[-]tty.h
images.obj :	[-]vectors.h
relocate.obj :	[-]defs.h
relocate.obj :	[-]devices.h
relocate.obj :	[-]options.h
relocate.obj :	[-]sm.h
set.obj :	[-]defs.h
set.obj :	[-]devices.h
set.obj :	[-]options.h
set.obj :	[-]sm.h
set.obj :	[-]vectors.h
shade.obj :	[-]defs.h
shade.obj :	[-]devices.h
shade.obj :	[-]options.h
shade.obj :	[-]sm.h
string.obj :	[-]defs.h
string.obj :	[-]devices.h
string.obj :	[-]fonts.h
string.obj :	[-]options.h
string.obj :	[-]sm.h
string.obj :	[-]tree.h
surfaces.obj :	image.h
surfaces.obj :	[-]options.h
surfaces.obj :	[-]sm.h
vec_image.obj :	image.h
vec_image.obj :	[-]options.h
vec_image.obj :	[-]vectors.h
