#include "copyright.h"
/*
 * Compile entries from the graphcap file into CACHE. If you don't have a
 * valid CACHE file, you won't be able to link this programme (ttyopen.c
 * won't compile). Don't worry, compile this programme with BOOTSTRAP defined
 * to the C preprocessor and specify no devices. You will now have a valid,
 * if null, CACHE file, so make this programme again and prepare a real CACHE
 */
#define STANDALONE			/* this is a stand-alone executable */
#include "options.h"
#include <stdio.h>
#include "tty.h"
#include "sm_declare.h"

#define CACHE "cacheg.dat"
#define NDEV 10			/* max number of devices to compile */
#define SIZE 40			/* size of device names */
/*********************************************************************/
/*
 * We have to declare the variables and functions from devices.a. Most
 * are not used.
 */
#include "dummies.h"
int devices[1];

#ifdef lint
int
main()
#else
int
main(argc,argv)				/* fit prototype */
int argc;
char *argv[];
#endif
{
   char c,
	devices[NDEV][SIZE],	/* Devices to compile */
        graphcap[80];		/* name of graphcap file */
   FILE *fil;			/* the output file */
   int i,j,ndev,
       offset,			/* total number of characters writen to `buf'*/
       offsets[NDEV + 1];	/* offset at start of each device */
   TTY *tty = NULL;

   if((fil = fopen(CACHE,"w")) == NULL) {
      fprintf(stderr,"Can't open %s\n",CACHE);
      exit(EXIT_BAD);
   }
   
#if defined(BOOTSTRAP)
   graphcap[0] = '\0';
   ndev = 0;
   offsets[0] = 0;
#else
   printf("Complete pathname of graphcap file to compile:\n");
   scanf("%s%*c",graphcap);
   printf("Enter names of devices to compile, terminated by blank line\n");
   for(i = 0;i < NDEV;i++) {
      printf(": ");
      if(fgets(devices[i],SIZE,stdin) == NULL || devices[i][0] == '\n') break;
      devices[i][strlen(devices[i]) - 1] = '\0';	/* remove \n */
   }
   ndev = i;
#endif
/*
 * Now write the cache file
 */
   fprintf(fil,"/*\n");
   fprintf(fil," * Compiled graphcap entries\n");
   fprintf(fil," */\n");
   fprintf(fil,"#define SIZE %d\n",SIZE);
   fprintf(fil,"\n");
   fprintf(fil,"static char termcap_filename[] = \"%s\";\n",graphcap);
/*
 * Now write out the device names
 */
   fprintf(fil,"static char devices[][SIZE] = {\n");
   for(i = 0;i < ndev;i++) {
      fprintf(fil,"	\"%s\",\n",devices[i]);
   }
   fprintf(fil,"	\"\"\n");
   fprintf(fil,"};\n");
/*
 * Now the contents of the graphcap entries
 */
   fprintf(fil,"static char buf[] = {\n");
   offset = 0;
   for(i = 0;i < ndev;i++) {
      char *av[1];

      av[0] = devices[i];
      offsets[i] = offset;
      if((tty = ttyopen(graphcap,1,av,(int (*)())NULL)) == NULL) {
         fprintf(stderr,"Can't get graphcap entry for %s\n",devices[i]);
	 exit(EXIT_BAD);
      }
      fprintf(fil,"	");
      for(j = 0;(c = tty->t_caplist[j]) != '\0';j++,offset++) {
	 fprintf(fil,"%3d,",c);
	 if(j%18 == 17) fprintf(fil,"\n\t");
      }
      fprintf(fil,"	0,\n");		/* NUL terminate string */
      offset++;
   }
   offsets[i] = offset;
   fprintf(fil,"	0\n");		/* NUL terminate string */
   fprintf(fil,"};\n");
/*
 * And the offsets of each device within buf
 */
   fprintf(fil,"static int offsets[] = {\n");
   for(i = 0;i <= ndev;i++) {
      fprintf(fil,"	%d,\n",offsets[i]);
   }
   fprintf(fil,"	0\n");
   fprintf(fil,"};\n");

   fclose(fil);

   printf("Copy the file %s to the devices directory, and\n",CACHE);
#ifdef BOOTSTRAP
   printf("remake SM and then compile_g\n");
#else
   printf("remake SM\n");
#endif
   exit(EXIT_OK);
   /*NOTREACHED*/
}

#ifdef BOOTSTRAP
TTY *
ttyopen()
{
  return(NULL);
}
#endif
