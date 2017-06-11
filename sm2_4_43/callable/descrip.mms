.c.obj :
	cc $(CFLAGS) $*.c
.for.obj :
	for $(FFLAGS) $*.for
#
# cc flags
#
CFLAGS = /debug/opt=nodis
#
# f77 flags
#
FFLAGS = /debug/noopt
#
# Hardware option (on, e.g., a Sun)
#
#HARD = -f68881
HARD =
#
# linker flags
#
#XLIB = -lX
XLIB =
#
#SUNLIB = -lcore -lsunwindow -lpixrect
SUNLIB =
#
LDFLAGS = /debug
LIBS = [-.src]plotsub.olb/lib, [-.src]devices.olb/lib, \
	[-.src]utils.olb/lib, $(XLIB) $(SUNLIB) sys$library:vaxcrtl/lib
#
tst : tst.obj
	link/exe=tst $(LDFLAGS) tst.obj, $(LIBS)
tstc : tstc.obj
	link/exe=tstc $(LDFLAGS) tstc.obj, $(LIBS)
cont : cont.obj
	link/exe=cont $(LDFLAGS) cont.obj, $(LIBS)
contc : contc.obj
	link/exe=contc $(LDFLAGS) contc.obj, $(LIBS)
