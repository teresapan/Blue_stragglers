$ verify = f$verify (0)
$ !
$ ! Command file for use on VMS to spawn a SM process
$ ! that can be suspended with C-z and will not go away
$ ! when other programs are run.
$ !
$ ! Assumes that p1 is a foreign command that runs SM
$ !
$ ! (modified from) Joe Kelsey
$ ! FlexComm Corp.
$ !
$ ! September, 1985
$ !
$ ! Run or attach to SM in a kept fork.
$ !
$ name		= p1 + " " + f$trnlnm ( "TT" ) - ":"
$ priv_list	= f$setprv ("NOWORLD, NOGROUP")
$ pid 		= 0
$ loop:
$ 	proc		= f$getjpi ( f$pid ( pid ), "PRCNAM")
$ 	if proc .eqs. name then goto attach
$ 	if pid .ne. 0 then goto loop
$! end_loop
$! if(spawn) then
$	args = p2 + " " + p3 + " " + p4 + " " + p5 + " " + -
    	       p6 + " " + p7 + " " + p8
$	priv_list = f$setprv( priv_list )
$	write sys$error "[Spawning a new Kept SM]"
$	define/user sys$input sys$command
$	spawn /process="''NAME'"/nolog 'p1' 'args'
$	goto quit
$ attach:
$ 	priv_list = f$setprv( priv_list )
$ 	write sys$error "[Attaching to process ''NAME']"
$ 	write sys$error " "
$ 	define/user sys$input sys$command
$ 	attach "''NAME'"
$ quit:
$ write sys$error -
"[Attached to DCL in directory ''F$TRNLNM("SYS$DISK")'''F$DIRECTORY()']"
$ if verify then set verify
$ exit
