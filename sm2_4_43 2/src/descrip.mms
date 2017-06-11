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
#   If you have trouble with this Makefile, consult the Unix one which
# has been more thoroughly tested.
#
#
#   We have now separated the compilation options from the Make options,
# see the file options.h for how to compile in devices and (maybe) placate
# your compiler
#
#   You may not need to define MAKE at all
#
# Default rules for compiling code
#
.c.obj :
	@- delete $*.obj.*
	cc $(CFLAGS) $*.c
#
# cc flags
#
# cc flags  For alphas running "open vms", uncomment the top line.  for old
# vms, uncomment the second line
#CFLAGS = /decc/prefix_library_entries=all_entries/define=("NO_ALLOCA")
#CFLAGS = /nolist/opt=nodisjoint/define=("NO_ALLOCA")
#
# current directory 
#
#CURRENT = disk$cannon:[sm.src
#
# for alphas running "open vms", uncomment this line
#SMOPT = $(CURRENT)]sm_alpha.opt
#
# for old vms, uncomment this line
#SMOPT = $(CURRENT)]sm.opt
#
# Write to the standard output
#
ECHO = write sys$output
#
# linker flags
#
#LDFLAGS = /debug
LDFLAGS =
#
OBJECT = \
        main.obj, sm_main.obj, control.obj, do_for.obj, editor.obj, fft.obj, \
        getlist.obj, help.obj, history.obj, input.obj, lexan.obj, \
        keymac.obj, macros.obj, macroed.obj, math.obj, more.obj, mscanf.obj, \
        name.obj, numword.obj, pow.obj, printv.obj, random.obj, readcol.obj, \
	read_old.obj, restore.obj, spline.obj, tokens.obj, \
        userfn.obj, variable.obj, vec_arith.obj, vec_logical.obj, \
	vec_misc.obj, vec_strings.obj, yylex.obj
#
# Make all targets
#
all : bison sm.exe
	@ wait 0
#
# Make bison first
#
bison :
        set def [.bison]
	$(MMS)/macro=("CFLAGS=$(CFLAGS)") bison.exe
        set def [-]
#
# Make SM
#
sm.exe : $(OBJECT) devices plotsub utils cmp.exe
        @- delete sm.exe.*
        - link/exe=sm.exe $(LDFLAGS) $(OBJECT), libplotsub/lib, \
                libdevices/lib, libutils/lib, $(SMOPT)
#
# This rule actually links SM
#
link : cmp.exe
        @- delete sm.exe.*
        - link/exe=sm.exe $(LDFLAGS) $(OBJECT), libplotsub/lib, \
                libdevices/lib, libutils/lib, $(SMOPT)
#
# make control.c from control.y
# is_same sees if control.h is changed. if not, don't update it
#
control.c : control.y
        @- delete control.c.*, y_tab.*.*
        @ bison := $ $(CURRENT).bison]bison
        @- $(ECHO) "Expect 16 shift/reduce and 6 reduce/reduce conflicts"
        bison -dy control.y
        @- delete b_act.*.*, b_tab.*.*, b_attrs.*.*
        @ rename y_tab.c control.c
        @ cmp := $ $(CURRENT)]cmp.exe
        @ @issame y_tab.h control.h
#
# cmp is used in issame.com
#
cmp.exe : cmp.obj
        link cmp.obj, sys$library:vaxcrtl/lib
#
# make yylex.c from the list of tokens in control.h,
# and using lexan() as a source for tokens.
#
makeyyl.exe : makeyyl.obj
	  link makeyyl.obj, sys$library:vaxcrtl/lib
#
yylex.c : control.h makeyyl.exe
	@- delete yylex.c.*
	@ makeyyl := $ $(CURRENT)]makeyyl.exe
	makeyyl control.h lexan yylex
#
#
devices :
	@ if("''F$SEARCH("libdevices.olb")'" .eqs. "") then -
		lib/create libdevices.olb
	@ define C$INCLUDE $(CURRENT)]
	@ define DECC$USER_INCLUDE $(CURRENT)]
	set def [.devices]
	$(MMS)/macro=("CFLAGS=$(CFLAGS)","LDFLAGS=$(LDFLAGS)") all
	set def [-]
	@ deassign C$INCLUDE
	@ deassign DECC$USER_INCLUDE
plotsub :
	@ if("''F$SEARCH("libplotsub.olb")'" .eqs. "") then -
		lib/create libplotsub.olb
	@ define C$INCLUDE $(CURRENT)]
	@ define DECC$USER_INCLUDE $(CURRENT)]
	set def [.plotsub]
	$(MMS)/macro=("CFLAGS=$(CFLAGS)","LDFLAGS=$(LDFLAGS)") all
	set def [-]
	@ deassign C$INCLUDE
	@ deassign DECC$USER_INCLUDE
utils :
	@ if("''F$SEARCH("libutils.olb")'" .eqs. "") then -
		lib/create libutils.olb
	@ define C$INCLUDE $(CURRENT)]
	@ define DECC$USER_INCLUDE $(CURRENT)]
	set def [.utils]
	$(MMS)/macro=("CFLAGS=$(CFLAGS)","LDFLAGS=$(LDFLAGS)") all
	set def [-]
	@ deassign C$INCLUDE
	@ deassign DECC$USER_INCLUDE
#
# clean out all object code and junk files
#
empty : clean
	- delete sm.exe.*, [.bison]bison.exe.*
#
tidy :
	- purge [...]*.*
	- delete y*tab.*.*, y*output.*, *.ckp.*
	- set def [.plotsub]
	- $(MMS) tidy
	- set def [-.devices]
	- $(MMS) tidy
	- set def [.utils]
	- $(MMS) tidy
	- set def [-]
	- delete [.doc]*.log.*, [.doc]*.dvi.*
#
# clean out all object code, and recreate some needed files
#
clean : tidy
	- delete *.obj.*, sm.exe.*, *.olb.*, y_tab.*.*, y*output.*, \
		 b_act.*.*, b_tab.*.*, b_attrs.*.*
	- set def [.plotsub]
	- $(MMS) clean
	- set def [-.devices]
	- $(MMS) clean
	- set def [.utils]
	- $(MMS) clean
	- set def [-.bison]
	- $(MMS) clean
	- set def [-]
#
# Here are all the dependencies:
#
#START_DEPEND
main.obj :	options.h
sm_main.obj :	local.c
sm_main.obj :	[-]AA_Version.h
sm_main.obj :	devices.h
sm_main.obj :	edit.h
sm_main.obj :	options.h
sm_main.obj :	stack.h
control.obj :	control.h
control.obj :	devices.h
control.obj :	options.h
control.obj :	sm.h
control.obj :	stack.h
control.obj :	vectors.h
control.obj :	yaccun.h
do_for.obj :	options.h
do_for.obj :	vectors.h
do_for.obj :	yaccun.h
editor.obj :	charset.h
editor.obj :	devices.h
editor.obj :	edit.h
editor.obj :	options.h
editor.obj :	tty.h
fft.obj :	options.h
fft.obj :	sm.h
getlist.obj :	options.h
getlist.obj :	vectors.h
getlist.obj :	yaccun.h
help.obj :	options.h
history.obj :	charset.h
history.obj :	edit.h
history.obj :	options.h
input.obj :	charset.h
input.obj :	edit.h
input.obj :	options.h
input.obj :	stack.h
lexan.obj :	lex.h
lexan.obj :	options.h
lexan.obj :	vectors.h
lexan.obj :	yaccun.h
keymac.obj :	charset.h
keymac.obj :	edit.h
keymac.obj :	options.h
macros.obj :	charset.h
macros.obj :	options.h
macros.obj :	stack.h
macros.obj :	tree.h
macroed.obj :	charset.h
macroed.obj :	edit.h
macroed.obj :	options.h
math.obj :	options.h
math.obj :	sm.h
more.obj :	charset.h
more.obj :	options.h
mscanf.obj :	options.h
name.obj :	options.h
printv.obj :	options.h
printv.obj :	vectors.h
random.obj :	options.h
readcol.obj :	charset.h
readcol.obj :	options.h
readcol.obj :	sm.h
readcol.obj :	vectors.h
read_old.obj :	options.h
restore.obj :	options.h
restore.obj :	vectors.h
spline.obj :	options.h
spline.obj :	vectors.h
tokens.obj :	control.h
tokens.obj :	options.h
tokens.obj :	vectors.h
tokens.obj :	yaccun.h
userfn.obj :	options.h
userfn.obj :	vectors.h
variable.obj :	options.h
variable.obj :	tree.h
vec_arith.obj :	options.h
vec_arith.obj :	vectors.h
vec_logical.obj :	options.h
vec_logical.obj :	vectors.h
vec_misc.obj :	options.h
vec_misc.obj :	plotsub/image.h
vec_misc.obj :	vectors.h
vec_strings.obj :	options.h
vec_strings.obj :	vectors.h
yylex.obj :	control.h
yylex.obj :	lex.h
yylex.obj :	options.h
yylex.obj :	stack.h
yylex.obj :	vectors.h
yylex.obj :	yaccun.h
