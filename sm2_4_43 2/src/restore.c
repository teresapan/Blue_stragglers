#include "copyright.h"
/*
 * Restore or save everything from an SM session
 */
#include "options.h"
#include <stdio.h>
#include <ctype.h>
#include "vectors.h"
#include "sm_declare.h"

#define BSIZE 151
#define RETURN { (void)fclose(fil); return; }

extern int sm_verbose;			/* Shall I be chatty? */

void
restore(file)
char *file;				/* name of file to restore from */
{
   char *all,				/* pointer to macro `all' */
	*bptr,				/* pointer to buff */
   	buff[BSIZE],			/* Buffer to read lines of save file */
	c,
	*ret,				/* return value from fgets */
   	s[20],				/* buffer to read dimension.q */
        word[BSIZE];			/* buffer for a word */
   FILE *fil;
   int all_len,				/* length of macro `all' */
       i,j,
       type;				/* type of vector */
   VECTOR *vector;
   
   if(file[0] == '\0') {
      if(*(file = print_var("save_file")) == '\0') {
	 file = "sm.dmp";
      }
   }
   if(sm_verbose > 0) msg_1s("dump file is %s\n",file);

   if((fil = fopen(file,"r")) == NULL) {
      msg_1s("Can't open %s in restore()\n",file);
      return;
   }

   (void)fgets(buff,BSIZE,fil);		/* Read first line */
   if(strlen(buff) != 31 ||		/* Check length of first line */
			 strncmp(buff,"Dump, ",6) != 0) {
      (void)fseek(fil,0L,0);		/* no header, rewind file */
      read_hist(fil);			/* and read as history file */
   } else {
/*
 * There is a header, so this isn't just a history file
 */
      if(sm_verbose > 0) msg_1s("Date of dump is %s",&buff[6]);
      
      while((ret = fgets(buff,BSIZE,fil)) != NULL && buff[0] == '\n') continue;
      if(ret == NULL) {
	 RETURN;
      }

      if(!strcmp(buff,"Variables:\n")) {
	 while((c = getc(fil)) == '\n') continue;
	 (void)ungetc(c,fil);
	 
	 while((ret = fgets(buff,BSIZE,fil)) != NULL) {
	    if(buff[0] == '\n') {
	       while((ret = fgets(buff,BSIZE,fil)) != NULL && buff[0] == '\n'){
		  continue;
	       }
	       break;
	    } else if(!strcmp("Vectors:\n",buff) ||
						!strcmp("Macros:\n",buff)) {
	       break;
	    }
	    (void)str_scanf(buff,"%s",word);	/* variable name */
	    for(bptr = buff;*bptr != '\0' &&
		!isspace(*bptr);bptr++) continue; /* skip name */
	    for(;*bptr != '\0' && isspace(*bptr);bptr++) { /* find defn */
	       continue;
	    }
	    bptr[strlen(bptr) - 1] = '\0';	/* trim off \n */
	    setvar(word,bptr);
	 }
      }
      if(ret == NULL) {
	 RETURN;
      }
/*
 * Now for vectors
 */
      if(!strcmp(buff,"Vectors:\n")) {
	 while((c = getc(fil)) == '\n') continue;
	 (void)ungetc(c,fil);

	 while((ret = fgets(buff,BSIZE,fil)) != NULL) {
	    if(buff[0] == '\n') {
	       while((ret = fgets(buff,BSIZE,fil)) != NULL && buff[0] == '\n'){
		  continue;
	       }
	       break;
	    } else if(!strcmp("Macros:\n",buff)) {	/* end of macros */
	       break;
	    }
	    (void)str_scanf(buff,"%s : dimension = %s\n",word,s);
	    type = parse_qualifier(s);
	    if((vector = make_vector(word,atoi(s),type)) == NULL) {
	       break;
	    }
	    (void)fgets(buff,BSIZE,fil);
	    (void)strncpy(vector->descrip,&buff[7],VEC_DESCRIP_SIZE);
	    vector->descrip[strlen(vector->descrip) - 1] = '\0'; /* strip \n */
	    
	    if(type == VEC_FLOAT) {
	       char *fmt = "%f";
	       
	       if(sizeof(REAL) == sizeof(double)) {
		  fmt = "%lf";
	       }
	       
	       for(i = 0;i < vector->dimen;i++) { /* read data */
		  (void)fscanf(fil,fmt,&(vector->vec.f[i]));
	       }
	       (void)fgets(buff,BSIZE,fil); /* read rest of line, inc. \n */
	    } else {
	       for(i = 0;i < vector->dimen;i++) { /* read data */
		  if((ret = fgets(buff,BSIZE,fil)) == NULL) {
		     break;
		  }
		  for(j = 0;j < VEC_STR_SIZE - 1;j++) {
		     if(buff[j] == '\n') {
			vector->vec.s.s[i][j] = '\0';
			break;
		     } else {
			vector->vec.s.s[i][j] = buff[j];
		     }
		  }
		  buff[j] = '\0';
	       }
	    }
	 }
      }
      if(ret == NULL) {
	 RETURN;
      }
/*
 * Variables and Vectors are read, now for macros.
 */
      if(strcmp(buff,"Macros:\n") != 0) { /* No Macros: line */
	 msg_1s("Error in SAVE file %s at line:\n",file);
	 msg_1s("%s",buff);
	 RETURN;
      }
/*
 * check if there's an `all' macro already
 */
      if((all = get_macro("all",(int *)NULL,(int *)NULL,(int *)NULL)) != NULL){
	 all_len = strlen(all);
      } else {
	 all_len = 0;
      }
      
      while((c = getc(fil)) == '\n') continue;
      (void)ungetc(c,fil);

      (void)read_macros(fil);
/*
 * Put macro "all" onto history buffer, if it exists
 */
      if((all = get_macro("all",(int *)NULL,(int *)NULL,(int *)NULL))!= NULL &&
						      strlen(all) != all_len) {
	 if(sm_verbose > 1) msg("replacing history list with \"all\"\n");
	 delete_history(0,10000,0);
	 (void)read_history_macro("all");
      }
   }
   
   (void)fclose(fil);
}

/**************************************************/
#define SIZE 30

void
save(file,what)
char *file;			/* name of file to dump to */
int what;			/* what to save? E.g. 101 */
{
   char buff[SIZE];		/* response of user */
   FILE *fil;
   TIME_T t;
   
   if(file[0] == '\0') {
      if(*(file = print_var("save_file")) == '\0') {
	 file = "sm.dmp";
      }
   }
   if(sm_verbose > 0) msg_1s("Save file is %s\n",file);
   if((fil = fopen(file,"w")) == NULL) {
      msg_1s("Can't open %s in save()\n",file);
      return;
   }
   (void)time(&t);
   fprintf(fil,"Dump, %s\n",ctime(&t));
/*
 * First the variables
 */
   if(what != 0) {
      buff[0] = ((what/100)%10) + '0';
   } else {
      prompt_user("save variables?           [y/n] ");
      (void)mscanstr(buff,SIZE);
   }
   if(buff[0] != 'n' && buff[0] != '0') {
      fprintf(fil,"Variables:\n\n");
      save_variables(fil);
   }
/*
 * next the vectors
 */
   if(what != 0) {
      buff[0] = ((what/10)%10) + '0';
   } else {
      prompt_user("save vectors?             [y/n] ");
      (void)mscanstr(buff,SIZE);
   }
   if(buff[0] != 'n' && buff[0] != '0') {
      fprintf(fil,"\nVectors:\n\n");
      save_vectors(fil);
   }
/*
 * Variables and Vectors are saved; now for macros.
 * First get an up-to-date version of `all' from history list,
 * then write macros to disk
 */
   if(what != 0) {
      buff[0] = (what%10) + '0';
   } else {
      prompt_user("save history and macros?  [y/n] ");
      (void)mscanstr(buff,SIZE);
   }
   if(buff[0] != 'n' && buff[0] != '0') {
      fprintf(fil,"\nMacros:\n\n");
      (void)define_history_macro("all",0,10000);
      (void)write_macros(fil,0);	/* don't write macros starting
					   with two comment characters */
   }
   (void)fclose(fil);
}
