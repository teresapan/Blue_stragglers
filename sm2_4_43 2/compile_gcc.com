$!
$! This command script assumes that the symbol CURRENT is set to the main
$! SM directory, i.e. the one containing this file,
$! but without a trailing ]. e.g. "CURRENT :== disk$cannon:[sm"
$! (If it isn't, it'll get it for you)
$!
$! You can specify options to the compiler/linker be DEFINEing the logicals
$! cflags/ldflags, or by editing the file [sm.src]options.h. You should
$! read it!
$!
$! To (just) compile Bison, say "@compile bison"
$! To (just) compile and link SM, say "@compile sm"
$! To (just) compile Src, say "@compile src"
$! To (just) compile Devices and make libdevices.olb, say "@compile devices"
$! To (just) compile Plotsub and make libplotsub.olb, say "@compile plotsub"
$! To (just) compile Utils and make libutils.olb, say "@compile utils"
$! To (just) link, say "@compile link"
$! To (just) prepare the fonts, say "@compile fonts"
$! To (just) prepare patch, say "@patch"
$! To (just) prepare miscellaneous things, say "@compile misc"
$! Otherwise everything is made
$!
$! Current directory
$!
$ if("''CURRENT'" .nes. "") then goto start
$    current = f$environment("default")
$    current = f$extract(0,f$length(current)-1,current) ! remove trailing ]
$ start:
$ on control_y then goto abort
$ on error then goto abort
$!
$! Options for  GCC
$    cflags := "D NODEFS NOUNDEFS"
$!
$! Options for linker
$!
$ if("''ldflags'" .nes. "") then goto got_ldflags
$    ldflags := ""
$ got_ldflags:
$!
$ if("''p1'" .eqs. "FONTS") then goto fonts
$ if("''p1'" .eqs. "LINK") then goto link
$ if("''p1'" .eqs. "MISC") then goto misc
$ if("''p1'" .eqs. "DEVICES") then goto devices
$ if("''p1'" .eqs. "PLOTSUB") then goto plotsub
$ if("''p1'" .eqs. "UTILS") then goto utils
$ if("''p1'" .eqs. "SRC")then goto after_bison
$ if("''p1'" .eqs. "SM") then goto after_bison
$ if("''p1'" .eqs. "PATCH") then goto patch
$ if("''p1'" .eqs. "" .or. "''p1'" .eqs. "BISON") then goto after_failure
$       write sys$output "Unknown option for compile : ''p1'"
$       exit
$ after_failure:
$       set def 'CURRENT'.src.bison]
$       gcc  LR0.C 'cflags'
$       gcc  ALLOCATE.C 'cflags'
$       gcc  CLOSURE.C 'cflags'
$       gcc  CONFLICTS.C 'cflags'
$       gcc  DERIVES.C 'cflags'
$       gcc  files.c 'cflags'
$       gcc  GETARGS.C 'cflags'
$       gcc  GRAM.C 'cflags'
$       gcc  LALR.C 'cflags'
$       gcc  LEX.C 'cflags'
$       gcc  MAIN.C 'cflags'
$       gcc  NULLABLE.C 'cflags'
$       gcc  OUTPUT.C 'cflags'
$       gcc  PRINT.C 'cflags'
$       gcc  READER.C 'cflags'
$       gcc  SYMTAB.C 'cflags'
$       gcc  WARSHALL.C 'cflags'
$       gcc  UTILS.C 'cflags'
$       link/exe=bison  lr0.obj, allocate.obj, closure.obj, conflicts.obj, -
        derives.obj,files.obj, getargs.obj, gram.obj, lalr.obj, lex.obj, -
        main.obj, nullable.obj, output.obj, print.obj, reader.obj, -
        symtab.obj,warshall.obj, utils.obj, gnu_cc:[000000]gcclib/lib,-
        sys$library:vaxcrtl/lib
$       dele/nocon *.obj;*
$       set def 'CURRENT']
$       if("''p1'" .eqs. "BISON") then exit
$ after_bison:
$!
$! We have completed (or skipped) bison, now make SM
$!
$ set def 'CURRENT'.src]
$! Options for  GCC
$    cflags := "D NODEFS NOUNDEFS ''CURRENT.src]' -fwritable-strings"
$ gcc MAIN.C  'cflags'
$ delete control.c.*, y_tab.*.*
$ bison := $ 'CURRENT'.src.bison]bison
$ write sys$output "Expect 15 shift/reduce and 6 reduce/reduce conflicts"
$ bison -dy control.y
$ delete b_act.*.*, b_tab.*.*, b_attrs.*.*
$ rename y_tab.c control.c
$ gcc CMP.c 'cflags'
$ link cmp.obj, gnu_cc:[000000]gcclib/lib,sys$library:vaxcrtl/lib
$ cmp := $ 'CURRENT'.src]cmp.exe
$ @issame y_tab.h control.h
$ gcc  control.c  'cflags'
$ gcc  DO_FOR.c  'cflags'
$ gcc  EDITOR.c  'cflags'
$ gcc  FFT.c     'cflags'
$ gcc  GETLIST.c  'cflags'
$ gcc  HELP.c  'cflags'
$ gcc  HISTORY.c  'cflags'
$ gcc  INPUT.c  'cflags'
$ gcc  LEXAN.c  'cflags'
$ gcc  KEYMAC.c  'cflags'
$ gcc  MACROS.c  'cflags'
$ gcc  MACROED.c  'cflags'
$ gcc  MATH.c  'cflags'
$ gcc  MORE.c  'cflags'
$ gcc  MSCANF.c  'cflags'
$ gcc  NAME.c  'cflags'
$ gcc  NUMWORD.c  'cflags'
$ gcc  POW.c  'cflags'
$ gcc  PRINTV.c 'cflags'
$ gcc  READCOL.c  'cflags'
$ gcc  READ_OLD.c  'cflags'
$ gcc  RESTORE.c  'cflags'
$ gcc  SPLINE.c  'cflags'
$ gcc  TOKENS.c  'cflags'
$ gcc  USERFN.c  'cflags'
$ gcc  VARIABLE.c  'cflags'
$ gcc  VEC_ARITH.c  'cflags'
$ gcc  VEC_LOGICAL.c  'cflags'
$ gcc  VECTORS.c  'cflags'
$ gcc MAKEYYL.c
$ link makeyyl.obj, gnu_cc:[000000]gcclib/lib,sys$library:vaxcrtl/lib
$ delete yylex.c.*
$ makeyyl := $ 'CURRENT'.src]makeyyl.exe
$ makeyyl control.h lexan yylex
$ gcc  YYLEX.c  'cflags'
$ set def 'CURRENT]
$if("''p1'" .eqs. "SRC") then exit
$!
$! Now the subdirectories. We'll need some include files from here
$!
$ devices:
$    cflags := "D NODEFS NOUNDEFS ''CURRENT'.src] -fwritable-strings"
$ set def 'CURRENT'.src]
$ if("''F$SEARCH("libdevices.olb")'" .eqs. "") then -
                lib/create libdevices.olb
$!
$ set def [.devices]
$ gcc  STGSETUP.c  'cflags'
$ lib/replace [-]libdevices.olb STGSETUP.obj
$ gcc  STGENCODE.c  'cflags'
$ lib/replace [-]libdevices.olb STGENCODE.obj
$ gcc  STGERASE.c  'cflags'
$ lib/replace [-]libdevices.olb STGERASE.obj
$ gcc  STGCURSOR.c  'cflags'
$ lib/replace [-]libdevices.olb STGCURSOR.obj
$ gcc  STGCLOSE.c  'cflags'
$ lib/replace [-]libdevices.olb STGCLOSE.obj
$ gcc  STGLINE.c  'cflags'
$ lib/replace [-]libdevices.olb STGLINE.obj
$ gcc  STGFILLPT.c  'cflags'
$ lib/replace [-]libdevices.olb STGFILLPT.obj
$ gcc  STGRELOC.c  'cflags'
$ lib/replace [-]libdevices.olb STGRELOC.obj
$ gcc  STGCHAR.c  'cflags'
$ lib/replace [-]libdevices.olb STGCHAR.obj
$ gcc  STGLIST.c  'cflags'
$ lib/replace [-]libdevices.olb STGLIST.obj
$ gcc  RASTER.c  'cflags'
$ lib/replace [-]libdevices.olb RASTER.obj
$ gcc  IMAGEN.c  'cflags'
$ lib/replace [-]libdevices.olb IMAGEN.obj
$ gcc  SUNWINDOWS.c  'cflags'
$ lib/replace [-]libdevices.olb SUNWINDOWS.obj
$ gcc  X10.c  'cflags'
$ lib/replace [-]libdevices.olb X10.obj
$ gcc X11.c   'cflags' 
$ lib/replace [-]libdevices.olb X11.obj
$ gcc  TTYINDEX.c  'cflags'
$ lib/replace [-]libdevices.olb TTYINDEX.obj
$ gcc  TTYGET.c  'cflags'
$ lib/replace [-]libdevices.olb TTYGET.obj
$ gcc  TTYOPEN.c  'cflags'
$ lib/replace [-]libdevices.olb TTYOPEN.obj
$ gcc  TTYPUTS.c  'cflags'
$ lib/replace [-]libdevices.olb TTYPUTS.obj
$ gcc  TTYIO.c  'cflags'
$ lib/replace [-]libdevices.olb TTYIO.obj
$ gcc  GET1CHAR.c  'cflags'
$ lib/replace [-]libdevices.olb GET1CHAR.obj
$ gcc  GSTRCPY.c   'cflags'
$ lib/replace [-]libdevices.olb GSTRCPY.obj
$ dele/nocon *.obj;*
$ set def 'CURRENT']
$       if("''p1'" .eqs. "DEVICES") then exit
$ plotsub:
$    cflags := "D NODEFS NOUNDEFS ''CURRENT'.src] -fwritable-strings"
$ set def 'CURRENT'.src]
$ if("''F$SEARCH("libplotsub.olb")'" .eqs. "") then -
                lib/create libplotsub.olb
$ set def [.plotsub]
$ gcc  AXIS.c  'cflags'
$ library/replace [-]libplotsub.olb AXIS.obj
$ gcc  CHOPPER.c  'cflags'
$ library/replace [-]libplotsub.olb CHOPPER.obj
$ gcc  COLOUR.c  'cflags'
$ library/replace [-]libplotsub.olb COLOUR.obj
$ gcc  CONNECT.c  'cflags'
$ library/replace [-]libplotsub.olb CONNECT.obj
$ gcc  CONTOUR.c  'cflags'
$ library/replace [-]libplotsub.olb CONTOUR.obj
$ gcc  CURSOR.c  'cflags'
$ library/replace [-]libplotsub.olb CURSOR.obj
$ gcc  DECLARE.c  'cflags'
$ library/replace [-]libplotsub.olb DECLARE.obj
$ gcc  DEVICES.c  'cflags'
$ library/replace [-]libplotsub.olb DEVICES.obj
$ gcc  images.c  'cflags'
$ library/replace [-]libplotsub.olb images.obj
$ gcc  INTERFACE.c  'cflags'
$ library/replace [-]libplotsub.olb INTERFACE.obj
$ gcc  LABEL.c  'cflags'
$ library/replace [-]libplotsub.olb LABEL.obj
$ gcc  LINE.c  'cflags'
$ library/replace [-]libplotsub.olb LINE.obj
$ gcc  POINT.c  'cflags'
$ library/replace [-]libplotsub.olb POINT.obj
$ gcc  RELOCATE.c  'cflags'
$ library/replace [-]libplotsub.olb RELOCATE.obj
$ gcc  SET.c  'cflags'
$ library/replace [-]libplotsub.olb SET.obj
$ gcc  SHADE.c  'cflags'
$ library/replace [-]libplotsub.olb SHADE.obj
$ gcc  STRING.c  'cflags'
$ library/replace [-]libplotsub.olb STRING.obj
$ dele/nocon *.obj;*
$ set def 'CURRENT']
$       if("''p1'" .eqs. "PLOTSUB") then exit
$ utils:
$    cflags := "D NODEFS NOUNDEFS ''CURRENT'.src] -fwritable-strings"
$ set def 'CURRENT'.src]
$ if("''F$SEARCH("libutils.olb")'" .eqs. "") then -
		lib/create libutils.olb
$ set def [.utils]
$ gcc  FTRUNCATE.c  'cflags'
$ library/replace [-]libutils.olb FTRUNCATE.obj
$ gcc  GET_VAL.c  'cflags'
$ library/replace [-]libutils.olb GET_VAL.obj
$ gcc  REGEXP.c  'cflags'
$ library/replace [-]libutils.olb REGEXP.obj
$ gcc  SORT.c  'cflags'
$ library/replace [-]libutils.olb SORT.obj
$ gcc  SYSTEM.c  'cflags'
$ library/replace [-]libutils.olb SYSTEM.obj
$ gcc  STR_SCANF.c  'cflags'
$ library/replace [-]libutils.olb STR_SCANF.obj
$ gcc  TREE.c  'cflags'
$ library/replace [-]libutils.olb TREE.obj
$ gcc  UTILS.c  'cflags'
$ library/replace [-]libutils.olb UTILS.obj
$ gcc  VECTORS.c 'cflags' 
$ library/replace [-]libutils.olb VECTORS.obj
$ dele/nocon *.obj;*
$ set def 'CURRENT']
$	if("''p1'" .eqs. "UTILS") then exit
$!
$! This target just links
$!
$ link:
$ write sys$output "Linking SM (ignore redefinition of system)"
$ set def 'CURRENT'.src]
$ link/exe=sm.exe 'ldflags' main.obj, control.obj, do_for.obj,-
        editor.obj, fft.obj, -
        getlist.obj, help.obj, history.obj, input.obj, -
        keymac.obj,lexan.obj, math.obj, -
        macros.obj, macroed.obj, more.obj, mscanf.obj, name.obj, numword.obj, -
        pow.obj, printv.obj, readcol.obj, read_old.obj, restore.obj,-
        spline.obj, tokens.obj, -
        userfn.obj, variable.obj, vec_arith.obj, -
        vec_logical.obj, yylex.obj, -
        libplotsub/lib, libdevices/lib, libutils/lib, -
        gnu_cc:[000000]gcclib/lib, sys$library:vaxcrtl/lib
$ purge *.exe, *.obj
$ set def 'CURRENT']
$ if("''p1'" .nes. "") then exit
$!
$! Now get the fonts into a binary state for this machine
$!
$ fonts:
$    cflags := "D NODEFS NOUNDEFS ''CURRENT'.src] -fwritable-strings"
$ set def 'CURRENT']
$ gcc READ_FONTS.c 'cflags'
$ link read_fonts.obj, gnu_cc:[000000]gcclib/lib,sys$library:vaxcrtl/lib
$ read_fonts := $ 'CURRENT']read_fonts
$ read_fonts hershey_oc.dat font_index tex.defs fonts.bin
$ dele read_fonts.obj;*
$ if("''p1'" .nes. "") then exit
$ misc:
$    cflags := "D NODEFS NOUNDEFS ''CURRENT'.src] -fwritable-strings"
$ set def 'CURRENT']
$ gcc GET_GRAMMAR.c 'cflags'
$ link GET_GRAMMAR.obj, gnu_cc:[000000]gcclib/lib,sys$library:vaxcrtl/lib
$ gcc rasterise.c 'cflags'
$ link rasterise.obj, [.src]libdevices/lib, [.src]libutils/lib, -
                        gnu_cc:[000000]gcclib/lib,sys$library:vaxcrtl/lib
$ gcc compile_g.c 'cflags'
$ link/exe=boot_g compile_g.obj, [.src]libdevices/lib, gnu_cc:[000000]gcclib/lib,-
                                sys$library:vaxcrtl/lib
$ gcc compile_g.c 'cflags'
$ link compile_g.obj, [.src]libdevices/lib, gnu_cc:[000000]gcclib/lib,-
                                sys$library:vaxcrtl/lib
$ dele compile_g.obj;*, get_grammar.obj;*, rasterise.obj;*
$ set def 'CURRENT'.doc]
$ gcc INDEX.c 'cflags'
$ link INDEX.obj, gnu_cc:[000000]gcclib/lib,sys$library:vaxcrtl/lib
$ dele index.obj;*
$ set def 'CURRENT']
$ exit
$!
$! By default, DO NOT make patch. It'll require a little work to
$! set up the config.h file, anyway
$!
$ patch:
$    cflags := "D NODEFS NOUNDEFS ''CURRENT'.src] -fwritable-strings"
$ set def 'CURRENT'.patch]
$ gcc inp.c 'cflags'
$ gcc patch.c 'cflags'
$ gcc pch.c 'cflags'
$ gcc util.c 'cflags'
$ gcc version.c 'cflags'
$ link/exe=patch.exe 'ldflags' inp.obj, patch.obj, pch.obj, util.obj, -
        version.obj, gnu_cc:[000000]gcclib/lib,sys$library:vaxcrtl.olb/lib
$ delete/noconf *.obj;*
$ set def 'CURRENT']
$ patch :== $ 'current'.patch]patch
$ if("''p1'" .nes. "") then exit
$ exit
$ abort:
$ set def 'CURRENT']
$ exit
