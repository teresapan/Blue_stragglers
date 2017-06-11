# Makefile for bison
#
# Default rules for compiling code.
#
.c.obj :
        @- delete $*.obj.*
        cc $(CFLAGS) $*.c
#
LDFLAGS =     
# Where temp files go
TEMPDIR = []
# where the installed binary goes
#BINDIR = /usr/local
#
# where the parsers go
#PARSERDIR = /usr/local/lib
#
# These are for the use of SM - if you install bison for general use you'd
# probably want to change them back. You'll have to change the SM Makefile
# to find bison, of course.
#
PARSERDIR = [.bison]
BINDIR = $(PARSERDIR)
     
# names of parser files
PFILE = bison.simple
PFILE1 = bison.hairy
     
PFILES = /define=("XPFILE=""$(PARSERDIR)$(PFILE)""",            \
                  "XPFILE1=""$(PARSERDIR)$(PFILE1)""",          \
                  "TDIR=""$(TEMPDIR)""")
     
OBJECTS = LR0.obj, allocate.obj, closure.obj, conflicts.obj, derives.obj, \
          files.obj, getargs.obj, gram.obj, lalr.obj, lex.obj, main.obj, \
          nullable.obj, output.obj, print.obj, reader.obj, symtab.obj, \
          warshall.obj, utils.obj
     
bison.exe : $(OBJECTS)
        link/exe=bison $(LDFLAGS) $(OBJECTS), sys$library:vaxcrtl/lib
     
clean :
        delete/noconfirm *.obj.*
     
install : bison.exe
#       install bison $(BINDIR)
#       install -c $(PFILE) $(PFILE1) $(PARSERDIR)
        copy bison.exe [-]
        purge [-]bison.exe
#
# This file is different to pass the parser file names
# to the compiler.
files.obj : files.c files.h new.h gram.h
        cc $(CFLAGS) $(PFILES) files.c
     
LR0.obj : machine.h new.h gram.h state.h
closure.obj : machine.h new.h gram.h
conflicts.obj : machine.h new.h files.h gram.h state.h
derives.obj : new.h types.h gram.h
getargs.obj : files.h
lalr.obj : machine.h types.h state.h new.h gram.h
lex.obj : files.h symtab.h lex.h
main.obj : machine.h
nullable.obj : types.h gram.h new.h
output.obj : machine.h new.h files.h gram.h state.h
print.obj : machine.h new.h files.h gram.h state.h
reader.obj : files.h new.h symtab.h lex.h gram.h
symtab.obj : new.h symtab.h gram.h
warshall.obj : machine.h
