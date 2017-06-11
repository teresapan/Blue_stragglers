$!
$! You can specify options to the compiler/linker be defining the symbols
$! cflags/ldflags, or by editing the file [sm.src]options.h. You should
$! read it!
$!
$! To compile an Alpha system with OpenVMS, say "@compile alpha"
$! To (just) compile Bison, say "@compile bison" (or "@compile alpha bison")
$! To (just) compile and link SM, say "@compile sm" (or "@compile alpha sm")
$! To (just) compile Src, say "@compile src" (as above)
$! To (just) compile Devices and make libdevices.olb, say "@compile devices"
$! To (just) compile Plotsub and make libplotsub.olb, say "@compile plotsub"
$! To (just) compile Utils and make libutils.olb, say "@compile utils"
$! To (just) compile Parser and make libparser.olb, say "@compile parser"
$! To (just) link, say "@compile link"
$! To (just) prepare the fonts, say "@compile fonts"
$! To (just) prepare patch, say "@patch"
$! To (just) prepare miscellaneous things, say "@compile misc"
$! To (just) prepare compile_g, say "@compile compile_g"
$! To (just) prepare get_grammar, say "@compile get_grammar"
$! Otherwise everything is made
$!
$! Current directory
$!
$ if("''CURRENT'" .nes. "") then goto start
$    current = f$environment("default")
$    current = f$extract(0,f$length(current)-1,current)	! remove trailing ]
$ start:
$ on control_y then goto abort
$ on error then goto abort
$ if("''p1'" .eqs. "ALPHA") 
$ then
    SMOPT := 'CURRENT'.src]sm_alpha.opt
$ else
    SMOPT := 'CURRENT'.src]sm.opt
$ endif
$!
$! Options for CC
$!
$ if("''cflags'" .nes. "") then goto got_cflags
$    cflags := /opt=nodis/nolis
$ if("''p1'" .eqs. "ALPHA") 
$ then 
  cflags := /decc/prefix_library_entries=all_entries 
$ endif
$ got_cflags:
$!
$! Options for linker
$!
$ if("''ldflags'" .nes. "") then goto got_ldflags
$    ldflags := ""
$ got_ldflags:
$!
$ if("''p1'".eqs."FONTS" .or. ("''p1'".eqs."ALPHA".and."''p2'".eqs."FONTS")) then goto fonts
$ if("''p1'".eqs."LINK" .or. ("''p1'".eqs."ALPHA".and."''p2'".eqs."LINK")) then goto link
$ if("''p1'".eqs."MISC" .or. ("''p1'".eqs."ALPHA".and."''p2'".eqs."MISC")) then goto misc
$ if("''p1'".eqs."DEVICES" .or. ("''p1'".eqs."ALPHA".and."''p2'".eqs."DEVICES")) then goto devices
$ if("''p1'".eqs."PARSER" .or. ("''p1'".eqs."ALPHA".and."''p2'".eqs."PARSER")) then goto parser
$ if("''p1'".eqs."PLOTSUB" .or. ("''p1'".eqs."ALPHA".and."''p2'".eqs."PLOTSUB")) then goto plotsub
$ if("''p1'".eqs."UTILS" .or. ("''p1'".eqs."ALPHA".and."''p2'".eqs."UTILS")) then goto utils
$ if("''p1'".eqs."SRC" .or. ("''p1'".eqs."ALPHA".and."''p2'".eqs."SRC")) then goto after_bison
$ if("''p1'".eqs."SM" .or. ("''p1'".eqs."ALPHA".and."''p2'".eqs."SM")) then goto after_bison
$ if("''p1'" .eqs. "GET_GRAMMAR".or. ("''p1'".eqs."ALPHA".and."''p2'".eqs."GET_GRAMMAR")) then goto get_grammar
$ if("''p1'" .eqs. "COMPILE_G".or. ("''p1'".eqs."ALPHA".and."''p2'".eqs."COMPILE_G")) then goto compile_g
$ if("''p1'" .eqs. "PATCH".or. ("''p1'".eqs."ALPHA".and."''p2'".eqs."PATCH")) then goto patch
$ if("''p1'" .eqs. "" .or. "''p1'" .eqs. "BISON".or. ("''p1'".eqs."ALPHA".and."''p2'".eqs."BISON")) then goto after_failure
$	write sys$output "Unknown option for compile : ''p1'"
$	exit
$ after_failure:
$	set def 'CURRENT'.src.bison]
$	cc 'cflags'  LR0.C
$	cc 'cflags'  ALLOCATE.C
$	cc 'cflags'  CLOSURE.C
$	cc 'cflags'  CONFLICTS.C
$	cc 'cflags'  DERIVES.C
$	cc 'cflags'  'no_alloca'/define=("XPFILE=""[.bison]bison.simple""",-
	     	"XPFILE1=""[.bison]bison.hairy""", -
		"TDIR=""[]""") files.c
$	cc 'cflags'  GETARGS.C
$	cc 'cflags'  GRAM.C
$	cc 'cflags'  LALR.C
$	cc 'cflags'  LEX.C
$	cc 'cflags'  MAIN.C
$	cc 'cflags'  NULLABLE.C
$	cc 'cflags'  OUTPUT.C
$	cc 'cflags'  PRINT.C
$	cc 'cflags'  READER.C
$	cc 'cflags'  SYMTAB.C
$	cc 'cflags'  WARSHALL.C
$	cc 'cflags'  UTILS.C
$	link/exe=bison  lr0.obj, allocate.obj, closure.obj, conflicts.obj, -
	derives.obj,files.obj, getargs.obj, gram.obj, lalr.obj, lex.obj, -
	main.obj, nullable.obj, output.obj, print.obj, reader.obj, -
	symtab.obj,warshall.obj, utils.obj, 'SMOPT'/opt
$	dele/nocon *.obj;*
$	set def 'CURRENT']
$	if("''p1'" .eqs. "BISON") then exit
$ after_bison:
$!
$! We have completed (or skipped) bison, now make SM
$!
$ set def 'CURRENT'.src]
$ cc 'cflags' MAIN.c
$ cc 'cflags' SM_MAIN.c
$ delete control.c.*, y_tab.*.*
$ bison := $ 'CURRENT'.src.bison]bison
$ write sys$output "Expect 15 shift/reduce and 6 reduce/reduce conflicts"
$ bison -dy control.y
$ delete b_act.*.*, b_tab.*.*, b_attrs.*.*
$ rename y_tab.c control.c
$ cc 'cflags' CMP.c
$ link cmp.obj, 'SMOPT'/opt
$ cmp := $ 'CURRENT'.src]cmp.exe
$ @issame y_tab.h control.h
$ cc 'cflags' control.c
$ cc 'cflags' DO_FOR.c
$ cc 'cflags' FFT.c
$ cc 'cflags' EDITOR.c
$ cc 'cflags' GETLIST.c
$ cc 'cflags' HELP.c
$ cc 'cflags' HISTORY.c
$ cc 'cflags' INPUT.c
$ cc 'cflags' LEXAN.c
$ cc 'cflags' KEYMAC.c
$ cc 'cflags' MACROS.c
$ cc 'cflags' MACROED.c
$ cc 'cflags' MATH.c
$ cc 'cflags' MORE.c
$ cc 'cflags' MSCANF.c
$ cc 'cflags' NAME.c
$ cc 'cflags' NUMWORD.c
$ cc 'cflags' POW.c
$ cc 'cflags' PRINTV.c
$ cc 'cflags' RANDOM.c
$ cc 'cflags' READCOL.c
$ cc 'cflags' READ_OLD.c
$ cc 'cflags' RESTORE.c
$ cc 'cflags' SPLINE.c
$ cc 'cflags' TOKENS.c
$ cc 'cflags' USERFN.c
$ cc 'cflags' VARIABLE.c
$ cc 'cflags' VEC_ARITH.c
$ cc 'cflags' VEC_LOGICAL.c
$ cc 'cflags' VEC_MISC.c
$ cc 'cflags' VEC_STRINGS.c
$ cc 'cflags'  MAKEYYL.c
$ link makeyyl.obj, 'SMOPT'/opt
$ delete yylex.c.*
$ makeyyl := $ 'CURRENT'.src]makeyyl.exe
$ makeyyl control.h lexan yylex
$ cc 'cflags' YYLEX.c
$ set def 'CURRENT]
$if("''p1'" .eqs. "SRC") then exit
$!
$! Now the subdirectories. We'll need some include files from here
$!
$ devices:
$ define C$INCLUDE 'CURRENT'.src]
$ define DECC$USER_INCLUDE 'CURRENT'.src]
$ set def 'CURRENT'.src]
$ if("''F$SEARCH("libdevices.olb")'" .eqs. "") then -
		lib/create libdevices.olb
$!
$ set def [.devices]
$ cc 'cflags' STGSETUP.c
$ lib/replace [-]libdevices.olb STGSETUP.obj
$ cc 'cflags' STGENCODE.c
$ lib/replace [-]libdevices.olb STGENCODE.obj
$ cc 'cflags' STGERASE.c
$ lib/replace [-]libdevices.olb STGERASE.obj
$ cc 'cflags' STGCURSOR.c
$ lib/replace [-]libdevices.olb STGCURSOR.obj
$ cc 'cflags' STGCLOSE.c
$ lib/replace [-]libdevices.olb STGCLOSE.obj
$ cc 'cflags' STGLINE.c
$ lib/replace [-]libdevices.olb STGLINE.obj
$ cc 'cflags' STGFILLPT.c
$ lib/replace [-]libdevices.olb STGFILLPT.obj
$ cc 'cflags' STGRELOC.c
$ lib/replace [-]libdevices.olb STGRELOC.obj
$ cc 'cflags' STGCHAR.c
$ lib/replace [-]libdevices.olb STGCHAR.obj
$ cc 'cflags' STGLIST.c
$ lib/replace [-]libdevices.olb STGLIST.obj
$ cc 'cflags' RASTER.c
$ lib/replace [-]libdevices.olb RASTER.obj
$ cc 'cflags' IMAGEN.c
$ lib/replace [-]libdevices.olb IMAGEN.obj
$ cc 'cflags' SUNWINDOWS.c
$ lib/replace [-]libdevices.olb SUNWINDOWS.obj
$ cc 'cflags' X10.c
$ lib/replace [-]libdevices.olb X10.obj
$ cc 'cflags' X11.c
$ lib/replace [-]libdevices.olb X11.obj
$ cc 'cflags' VAXUIS.c
$ lib/replace [-]libdevices.olb VAXUIS.obj
$ cc 'cflags' METAFILE.c
$ lib/replace [-]libdevices.olb METAFILE.obj
$ cc 'cflags' TTYINDEX.c
$ lib/replace [-]libdevices.olb TTYINDEX.obj
$ cc 'cflags' TTYGET.c
$ lib/replace [-]libdevices.olb TTYGET.obj
$!
$! The VMS compiler breaks ttyopen if optimised, so don't
$! cc 'cflags' TTYOPEN.c
$ cc 'cflags'  /noopt TTYOPEN.c
$ lib/replace [-]libdevices.olb TTYOPEN.obj
$ cc 'cflags' TTYPUTS.c
$ lib/replace [-]libdevices.olb TTYPUTS.obj
$ cc 'cflags' TTYIO.c
$ lib/replace [-]libdevices.olb TTYIO.obj
$ cc 'cflags' GET1CHAR.c
$ lib/replace [-]libdevices.olb GET1CHAR.obj
$ cc 'cflags' GSTRCPY.c
$ lib/replace [-]libdevices.olb GSTRCPY.obj
$ deassign C$INCLUDE
$ deassign DECC$USER_INCLUDE
$ dele/nocon *.obj;*
$ set def 'CURRENT']
$	if("''p1'" .eqs. "DEVICES") then exit
$ plotsub:
$ define C$INCLUDE 'CURRENT'.src]
$ define DECC$USER_INCLUDE 'CURRENT'.src]
$ set def 'CURRENT'.src]
$ if("''F$SEARCH("libplotsub.olb")'" .eqs. "") then -
		lib/create libplotsub.olb
$ set def [.plotsub]
$ cc 'cflags' AXIS.c
$ library/replace [-]libplotsub.olb AXIS.obj
$ cc 'cflags' CHOPPER.c
$ library/replace [-]libplotsub.olb CHOPPER.obj
$ cc 'cflags' COLOUR.c
$ library/replace [-]libplotsub.olb COLOUR.obj
$ cc 'cflags' COMPRESS.c
$ library/replace [-]libplotsub.olb COMPRESS.obj
$ cc 'cflags' CONNECT.c
$ library/replace [-]libplotsub.olb CONNECT.obj
$ cc 'cflags' CONTOUR.c
$ library/replace [-]libplotsub.olb CONTOUR.obj
$ cc 'cflags' CURSOR.c
$ library/replace [-]libplotsub.olb CURSOR.obj
$ cc 'cflags' DECLARE.c
$ library/replace [-]libplotsub.olb DECLARE.obj
$ cc 'cflags' DEVICES.c
$ library/replace [-]libplotsub.olb DEVICES.obj
$ cc 'cflags' DITHER.c
$ library/replace [-]libplotsub.olb DITHER.obj
$ cc 'cflags' DUMMIES.c
$ library/replace [-]libplotsub.olb DUMMIES.obj
$ cc 'cflags' IMAGES.c
$ library/replace [-]libplotsub.olb images.obj
$ cc 'cflags' INTERFACE.c
$ library/replace [-]libplotsub.olb INTERFACE.obj
$ cc 'cflags' LABEL.c
$ library/replace [-]libplotsub.olb LABEL.obj
$ cc 'cflags' LINE.c
$ library/replace [-]libplotsub.olb LINE.obj
$ cc 'cflags' POINT.c
$ library/replace [-]libplotsub.olb POINT.obj
$ cc 'cflags' RELOCATE.c
$ library/replace [-]libplotsub.olb RELOCATE.obj
$ cc 'cflags' SET.c
$ library/replace [-]libplotsub.olb SET.obj
$ cc 'cflags' SHADE.c
$ library/replace [-]libplotsub.olb SHADE.obj
$ cc 'cflags' STRING.c
$ library/replace [-]libplotsub.olb STRING.obj
$ cc 'cflags' SURFACES.c
$ library/replace [-]libplotsub.olb SURFACES.obj
$ cc 'cflags' VEC_IMAGE.c
$ library/replace [-]libplotsub.olb VEC_IMAGE.obj
$ dele/nocon *.obj;*
$ deassign C$INCLUDE
$ deassign DECC$USER_INCLUDE
$ set def 'CURRENT']
$	if("''p1'" .eqs. "PLOTSUB") then exit
$ utils:
$ define C$INCLUDE 'CURRENT'.src]
$ define DECC$USER_INCLUDE 'CURRENT'.src]
$ set def 'CURRENT'.src]
$ if("''F$SEARCH("libutils.olb")'" .eqs. "") then -
		lib/create libutils.olb
$ set def [.utils]
$ cc 'cflags' FTRUNCATE.c
$ library/replace [-]libutils.olb FTRUNCATE.obj
$ cc 'cflags' GET_VAL.c
$ library/replace [-]libutils.olb GET_VAL.obj
$ cc 'cflags' REGEXP.c
$ library/replace [-]libutils.olb REGEXP.obj
$ cc 'cflags' SORT.c
$ library/replace [-]libutils.olb SORT.obj
$ cc 'cflags' SYSTEM.c
$ library/replace [-]libutils.olb SYSTEM.obj
$ cc 'cflags' STR_SCANF.c
$ library/replace [-]libutils.olb STR_SCANF.obj
$ cc 'cflags' TREE.c
$ library/replace [-]libutils.olb TREE.obj
$ cc 'cflags' UTILS.c
$ library/replace [-]libutils.olb UTILS.obj
$ cc 'cflags' VECTORS.c
$ library/replace [-]libutils.olb VECTORS.obj
$ dele/nocon *.obj;*
$ deassign C$INCLUDE
$ deassign DECC$USER_INCLUDE
$ set def 'CURRENT']
$	if("''p1'" .eqs. "UTILS") then exit
$!
$! This target just links
$!
$ link:
$ write sys$output "Linking SM (ignore redefinition of system)"
$ set def 'CURRENT'.src]
$ link/exe=sm.exe 'ldflags' main.obj, sm_main.obj, control.obj, do_for.obj,-
        editor.obj, fft.obj, -
	getlist.obj, help.obj, history.obj, input.obj, -
	keymac.obj,lexan.obj, math.obj, -
	macros.obj, macroed.obj, more.obj, mscanf.obj, name.obj, numword.obj, -
	pow.obj, printv.obj, readcol.obj, read_old.obj, restore.obj,-
        spline.obj, tokens.obj, -
	userfn.obj, variable.obj, vec_arith.obj, vec_logical.obj, -
	vec_misc.obj, vec_strings.obj, -
	yylex.obj, -
	libplotsub/lib, libdevices/lib, libutils/lib, -
	'SMOPT'/opt
$ purge *.exe, *.obj
$ set def 'CURRENT']
$ if("''p1'" .nes. "") then exit
$!
$! Now get the fonts into a binary state for this machine
$!
$ fonts:
$ define C$INCLUDE 'CURRENT'.src]
$ define DECC$USER_INCLUDE 'CURRENT'.src]
$ set def 'CURRENT']
$ cc 'cflags' READ_FONTS.c
$ link read_fonts.obj, 'SMOPT'/opt
$ read_fonts := $ 'CURRENT']read_fonts
$ read_fonts hershey_oc.dat font_index tex.def fonts.bin
$ dele read_fonts.obj;*
$ deassign C$INCLUDE
$ deassign DECC$USER_INCLUDE
$ if("''p1'" .nes. "") then exit
$ misc:
$ define C$INCLUDE 'CURRENT'.src]
$ define DECC$USER_INCLUDE 'CURRENT'.src]
$ set def 'CURRENT']
$ cc 'cflags' rasterise.c
$ link rasterise.obj, [.src]pow.obj,[.src]libdevices.olb/lib, -
		[.src]libutils.olb/lib, 'SMOPT'/opt
$ deassign C$INCLUDE
$ deassign DECC$USER_INCLUDE
$ dele rasterise.obj;*
$ set def 'CURRENT'.info]
$ cc 'cflags' TEXINDEX.c                             
$ link TEXINDEX.obj, 'SMOPT'/opt
$ dele texindex.obj;*
$ set def 'CURRENT']
$ exit
$!
$! Make the parser library
$!
$ parser:
$ define C$INCLUDE 'CURRENT'.src]
$ define DECC$USER_INCLUDE 'CURRENT'.src]
$ set def 'CURRENT'.src]
$ if("''F$SEARCH("libparser.olb")'" .eqs. "") then -
		lib/create libparser.olb
$ set def [.plotsub]
$ cc 'cflags' INTERFACE.c
$ library/replace [-]libparser.olb INTERFACE.obj
$ dele/nocon *.obj;*
$ set def [..]
$ library/replace libparser.olb sm_main.obj
$ library/replace libparser.olb control.obj
$ library/replace libparser.olb do_for.obj
$ library/replace libparser.olb editor.obj
$ library/replace libparser.olb fft.obj
$ library/replace libparser.olb getlist.obj
$ library/replace libparser.olb help.obj
$ library/replace libparser.olb history.obj
$ library/replace libparser.olb input.obj
$ library/replace libparser.olb lexan.obj
$ library/replace libparser.olb keymac.obj
$ library/replace libparser.olb macros.obj
$ library/replace libparser.olb macroed.obj
$ library/replace libparser.olb math.obj
$ library/replace libparser.olb more.obj
$ library/replace libparser.olb mscanf.obj
$ library/replace libparser.olb name.obj
$ library/replace libparser.olb numword.obj
$ library/replace libparser.olb pow.obj
$ library/replace libparser.olb printv.obj
$ library/replace libparser.olb readcol.obj
$ library/replace libparser.olb read_old.obj
$ library/replace libparser.olb restore.obj
$ library/replace libparser.olb spline.obj
$ library/replace libparser.olb tokens.obj
$ library/replace libparser.olb userfn.obj
$ library/replace libparser.olb variable.obj
$ library/replace libparser.olb vec_arith.obj
$ library/replace libparser.olb vec_logical.obj
$ library/replace libparser.olb vec_misc.obj
$ library/replace libparser.olb vec_strings.obj
$ library/replace libparser.olb yylex.obj
$ deassign C$INCLUDE
$ deassign DECC$USER_INCLUDE
$ set def 'CURRENT']
$	if("''p1'" .eqs. "PARSER") then exit
$ exit
$!
$! By default, DO NOT make patch. It'll require a little work to
$! set up the config.h file, anyway
$!
$ patch:
$ set def 'CURRENT'.patch]
$ cc 'cflags' inp.c
$ cc 'cflags' patch.c
$ cc 'cflags' pch.c
$ cc 'cflags' util.c
$ cc 'cflags' version.c
$ link/exe=patch.exe 'ldflags' inp.obj, patch.obj, pch.obj, util.obj, -
	version.obj, 'SMOPT'/opt
$ delete/noconf *.obj;*
$ set def 'CURRENT']
$ patch :== $ 'current'.patch]patch
$ if("''p1'" .nes. "") then exit
$ exit
$ abort:
$ set def 'CURRENT']
$ exit
$!
$! make a couple of programmes that used to be in misc
$!
$ get_compile_g:
$ define C$INCLUDE 'CURRENT'.src]
$ define DECC$USER_INCLUDE 'CURRENT'.src]
$ set def 'CURRENT']
$ cc 'cflags' /def=BOOTSTRAP compile_g.c
$ link/exe=boot_g compile_g.obj, [.src]libdevices/lib, 'SMOPT'/opt
$ cc 'cflags' compile_g.c
$ link compile_g.obj, [.src]libdevices/lib, 'SMOPT'/opt
$ dele compile_g.obj;*
$ deassign C$INCLUDE
$ deassign DECC$USER_INCLUDE
$ set def 'CURRENT']
$ exit
$ get_grammar:
$ define C$INCLUDE 'CURRENT'.src]
$ define DECC$USER_INCLUDE 'CURRENT'.src]
$ set def 'CURRENT']
$ cc 'cflags' GET_GRAMMAR.c
$ link GET_GRAMMAR.obj, 'SMOPT'/opt
$ dele get_grammar.obj;* 
$ deassign C$INCLUDE
$ deassign DECC$USER_INCLUDE
$ set def 'CURRENT']
$ exit
