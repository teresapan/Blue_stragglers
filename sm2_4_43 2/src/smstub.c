#include	<process.h>
#include	<dir.h>
#include	<stdio.h>

main( int argc, char *argv[] )
{
	char	*newargv[20], drive[MAXDRIVE], dir[MAXDIR], new[MAXPATH];

	fnsplit( argv[0], drive, dir, NULL, NULL );
	sprintf( new, "%s%sSM.EXE", drive, dir );

	fprintf( stderr, "Spawning 'win %s' ...\n", new );

	newargv[0] = "win.com";
	newargv[1] = new;
	while( argc )
	{
		newargv[argc+1] = argv[argc];
		argc--;
	}

	execvp("win.com", newargv );
	fprintf( stderr, "Spawn failed\nThis program requires Microsoft Windows 3.0 or later\n");
	return	1;
}
