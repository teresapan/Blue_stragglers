$ !
$ ! This command takes two filenames as arguments.
$ ! If the files are the same, move the second file onto the first.
$ ! If they are different, then delete the second file.
$ !
$ ! Due to a bug in the VMS C-library, files opened read-only have their
$ ! modification dates updated, so make a copy of p2 called temp.hh for the
$ ! comparison. Delete it after use.
$ !
$ on severe_error then exit
$ copy 'p2' temp.hh
$ cmp 'p1' temp.hh
$ if $status then goto same
$    delete temp.hh.*
$    delete 'p2'.*
$    rename 'p1' 'p2'
$    exit 1
$ same :
$    delete temp.hh.*
$    delete 'p1'.*
$    exit 1
