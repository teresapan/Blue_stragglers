#include "copyright.h"
/*
 * This routine returns the name of the user of the programme.
 * Underscores are replaced by spaces.
 */
#include "options.h"
#include <stdio.h>
#include "sm_declare.h"
#define SIZE 100

static char *read_passwd();

char *
get_name()
{
   char *ptr;
   static char nam[30];

   if((ptr = get_val("name")) != NULL) {
      (void)strcpy(nam,ptr);
   } else if((ptr = read_passwd()) != NULL) {
      (void)strcpy(nam,ptr);
   } else {
      (void)printf("Please enter the name that you wish me to use\n");
      (void)scanf("%s",nam);
      (void)printf("Thank you, %s.\n",nam);
      if(put_val("name",nam) < 0) {
         (void)printf("I'm afraid that I shall forget your name, sorry.\n");
      }
   }

   for(ptr = nam;*ptr != '\0';ptr++) {
      if(*ptr == ' ') {
	 *ptr = '\0';
	 break;
      } else if(*ptr == '_') {
	 *ptr = ' ';
      }
   }
   return(nam);
}


/**********************************/
/*
 * Here's a routine to try to get the name from the password file
 */
#ifdef unix
#include <ctype.h>
#include <pwd.h>

/*
 * Try to find a user's home directory
 */
char *
get_home(name)
char *name;
{
#if !defined(_POSIX_SOURCE)
   struct passwd *getpwnam();
#endif
   struct passwd *pw;

   if((pw = getpwnam(name)) == NULL) {
      return(NULL);
   }
   return(pw->pw_dir);
}

static char *
read_passwd()
{
   char c,
	*idname;
   static char name_buf[SIZE];
   int j,k;
#if !defined(_POSIX_SOURCE) && !defined(__GNUC__)
   struct passwd *getpwuid();
#endif
   struct passwd *pw;

   if((pw = getpwuid(getuid())) == NULL) { /* lint has "uid_t getuid()" */
      return(NULL);
   }

   for(j = k = 0;(c = pw->pw_gecos[j]) != ',' && c != '\0';j++,k++) {
      name_buf[k] = c;
      if(name_buf[k] == '&') {
	 idname = pw->pw_name;
	 name_buf[k] = *idname;
	 if(isalpha(name_buf[k]) && islower(name_buf[k])) {
	    name_buf[k] = toupper(name_buf[k]);
	 }
	 while(*++idname != '\0') name_buf[++k] = *idname;
      }
   }
   name_buf[k] = '\0';

   return(name_buf);
}
#else
char *
get_home(name)
char *name;
{ return(name == NULL ? name : NULL); }

static char *
read_passwd()
{ return(NULL);}
#endif
