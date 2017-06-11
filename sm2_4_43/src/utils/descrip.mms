#
# Default rules for compiling code.
#
.c.obj :
        @- delete $*.obj.*
        cc $(CFLAGS) $*.c
        library/replace [-]libutils.olb $*.obj
#
OBJECT = \
	ftruncate.obj, get_val.obj, regexp.obj, sort.obj, str_scanf.obj, \
	system.obj, tree.obj, utils.obj, vectors.obj
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
ftruncate.obj :	[-]options.h
get_val.obj :	[-]options.h
numword.obj :	[-]options.h
regexp.obj :	[-]options.h
sort.obj :	[-]options.h
str_scanf.obj :	[-]options.h
tree.obj :	[-]options.h
tree.obj :	[-]tree.h
utils.obj :	[-]sm.h
utils.obj :	[-]options.h
vectors.obj :	[-]options.h
vectors.obj :	[-]tree.h
vectors.obj :	[-]vectors.h
