###################################################################
#								  #
#		    SSSS       M       M			  #
#		   S    S      MM     MM			  #
#		   S	       M M   M M			  #
#		    SSSS       M  M M  M			  #
#		        S      M   M   M			  #
#		   S     S     M       M			  #
#		    SSSS       M       M			  #
#								  #
#								  #
###################################################################
#
# This makefile makes various standalone programmes, and then
# SM itself, in subdirectory src. See the makefile in src
# to modify the ways that SM is compiled
#
#
# Default rules for compiling code
#
.c.obj :
	@- delete $*.obj.*
	cc $(CFLAGS) $*.c
#
# cc flags  For alphas running "open vms", uncomment the top line.  for old
# vms, uncomment the second line
#CFLAGS = /decc/prefix_library_entries=all_entries 
#CFLAGS = /nolist/debug/opt=nodisjoint
#
# current directory
#
#CURRENT = disk$cannon:[monger.sm
#
# for alphas running "open vms", uncomment this line
#SMOPT = $(CURRENT).src]sm_alpha.opt
#
# for old vms, uncomment this line
#SMOPT = $(CURRENT).src]sm.opt
#
# Write to the standard output
#
ECHO = write sys$output
#
# Installation directory
#
DESTDIR = $(CURRENT)]
DESTLIB = $(CURRENT)]
#
# linker flags
#
LDFLAGS = /debug
#LDFLAGS =
#
# Make all targets
#
notice :
	@- $(ECHO) ""
	@- $(ECHO) "SM is copyright 1987, 1989, 1990, 1992 Robert Lupton and Patricia Monger."
	@- $(ECHO) "You may not distribute either source or executables without"
	@- $(ECHO) "specific permission from the authors. See the file copyright.h"
	@- $(ECHO) "for more details."
	@- $(ECHO) ""
all : notice fonts.bin sm misc
        @- $(ECHO) "SM is made"
#
# Make SM
#
sm :
        set def [.src]
        $(MMS)/macro=("CURRENT=$(CURRENT).src","CC=$(CC)","CFLAGS=$(CFLAGS)") all
        set def [-]
#
# Copy executable to current release version.
#
install :
        @- $(ECHO) "Installing SM"
	set def [.src]
        - link/exe=ism $(OBJECT), libplotsub/lib, -
                                libdevices/lib, libutils/lib, $(SMOPT)
        @- rename ism.exe $(DESTDIR)sm.exe
        @- set protection=(o=rwed,w=re) $(DESTDIR)sm.exe
        @- copy rasterise.exe $(DESTDIR)rasterise.exe
        @- set protection=(o=rwed,w=re) $(DESTDIR)rasterise.exe
        @- copy graphcap $(DESTLIB)
        @- copy filecap $(DESTLIB)
        @- copy fonts.bin $(DESTLIB)
        @- purge $(DESTDIR)sm.exe, $(DESTLIB)graphcap, -
                $(DESTLIB)filecap, $(DESTLIB)*fonts.bin
        @- $(ECHO) "SM is installed"
#
# Now various stand-alone programmes:
#
# Convert the fonts to binary format
#
read_fonts.exe : read_fonts.c
	@ define C$INCLUDE $(CURRENT).src]
	@ define DECC$USER_INCLUDE $(CURRENT).src]
        cc read_fonts.c
	link/exe=read_fonts.exe read_fonts.obj, sys$library:vaxcrtl/lib
fonts.bin : hershey_oc.dat read_fonts.exe font_index. tex.def
	@- read_fonts := $$(CURRENT)]read_fonts
	read_fonts hershey_oc.dat font_index tex.def fonts.bin
#
# Various things need compiling:
#
misc : rasterise.exe
	set def [.info]
	cc texindex.c
	link texindex.obj, sys$library:vaxcrtl/lib
	@- delete texindex.obj.*
	set def [-]
#
# Bootstrap compile_g without a valid cacheg.dat file
# (not made by default)
#
boot_g.exe : compile_g.c [.src]tty.h
	@ define C$INCLUDE $(CURRENT).src]
	@ define DECC$USER_INCLUDE $(CURRENT).src]
	cc/def=BOOTSTRAP compile_g.c
	link compile_g.obj, sys$library:vaxcrtl/lib
	@- delete compile_g.obj;
	@ deassign C$INCLUDE
	@ deassign DECC$USER_INCLUDE
#
# The programme to compile graphcap entries
#
compile_g.exe : compile_g.c [.src]libdevices.olb [.src]tty.h
	@ define C$INCLUDE $(CURRENT).src]
	@ define DECC$USER_INCLUDE $(CURRENT).src]
	cc compile_g.c
	link compile_g.obj, [.src]libdevices/lib, sys$library:vaxcrtl/lib
	@- delete compile_g.obj;
	@ deassign C$INCLUDE
	@ deassign DECC$USER_INCLUDE
#
# Extract the grammar from a yacc/bison file (a foreign command)
#
get_grammar.exe : get_grammar.c
	@ define C$INCLUDE $(CURRENT).src]
	@ define DECC$USER_INCLUDE $(CURRENT).src]
	cc $(CFLAGS) get_grammar.c
	link get_grammar.obj, sys$library:vaxcrtl/lib
#
# Rasterise a plot (this must be declared a foreign command)
#
rasterise.exe : rasterise.obj
	@ define C$INCLUDE $(CURRENT).src]
	@ define DECC$USER_INCLUDE $(CURRENT).src]
	cc $(CFLAGS) rasterise.c
	link rasterise.obj, [.src]pow.obj,[.src]libdevices.olb/lib, -
		[.src]libutils.olb, sys$library:vaxcrtl/lib
	@ deassign C$INCLUDE
	@ deassign DECC$USER_INCLUDE
#
# patch the source, given a file produced by `diff'
#
patch :
	set def [.patch]
	$(MMS) patch
	set def [-]
#
# clean out all object code
#
empty : clean
	- delete fonts.bin.*, read_fonts.exe.*, \
		get_grammar.exe.*, compile_g.exe.*, boot_g.exe.*, TAGS..*, \
		[.doc]index..*
	set def [.src]
	$(MMS) empty
	set def [-.patch]
	$(MMS) empty
	set def [-]
clean :
	- delete *.obj.*
	set def [.src]
	$(MMS) clean
	set def [-.patch]
	$(MMS) clean
	set def [-]
tidy :
	- purge [...]*.*
	- delete [.doc]*.log.*, *.dvi.*, index.tex.*, contents.tex.*, \
		index1.tex.*, contents1.tex.*
	set def [.src]
	$(MMS) tidy
	set def [-.patch]
	$(MMS) tidy
	set def [-]
#
# Here are all the dependencies:
#
#START_DEPEND
read_fonts.o :	[.src]fonts.h


