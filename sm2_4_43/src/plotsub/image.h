#define INC_IMAGE_H			/* this file has been included */
typedef struct {
   char name[40];
   int nx,ny;				/* dimensions of image */
   char *space;				/* storage for image */
   float **ptr,				/* pointers to rows of image */
	 xmin,ymin,			/* limits of image */
	 xmax,ymax;
} IMAGE;

