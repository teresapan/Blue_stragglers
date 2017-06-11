#include <stdio.h>
#include <math.h>
#include "options.h"
#include "sm.h"
#include "defs.h"
#include "sm_declare.h"

/***************************************************************************/
/*                                                                         */
/* Copyright (C) 1989, 1990 Rick McGonegal, Steven Smith and               */
/* Canada-France-Hawaii Telescope Corp.                                    */
/*									   */
/*    This routine is distributed in the hope that it will be useful,      */
/*    but WITHOUT ANY WARRANTY.  No author or distributor accepts          */
/*    responsibility to anyone for the consequences of using it or for     */
/*    whether it serves any particular purpose or works at all, unless     */
/*    he says so in writing. 						   */
/*									   */
/* If you find any bugs, or wish to suggest any enhancements, please send  */
/* mail to steve@cfht.hawaii.edu			 		   */
/*									   */
/* Algorithm was gleened from "IEEE Computer Graphics and Applications"    */
/* March 1989, article titled "Faster Plots by Fan Data-Compression" by	   */
/* Richard A. Fowell and David D. McNeil          			   */
/*									   */
/***************************************************************************/


#define Pvecx(i)  (ffx + fsx * xaray[i])
#define Pvecy(i)  (ffy + fsy * yaray[i])

void
fan_compress (xaray, yaray, nin, eps, karay, nout)
REAL xaray[], yaray[];
int nin;
double eps;
int karay[], *nout;
{
	int keep, 
		i = 0, 
		k, 
		o
		;
	double	d1, d2;
	float 	distance,
			eps1,
			Pui, Pui1, Pvi1,
			Mi, delMi
			;
	
	struct vector2 {
		float x;
		float y;
	} Uvec, Vvec;

	struct vector3 {
		float x;
		float y;
		float z;
	} Fvec;

	*nout = (nin > 0) ? 0 : nin;
	if ( nin <= 0 ) return;

	karay[(*nout)++] = i;
	o = i;
	k = nin;

	if (nin == 1) return;
	eps1 = (eps > 0) ? eps : 0.0;

	while (i < (nin-1)) {
		i++;
		if (i < k) {
			d1 = Pvecx(i) - Pvecx(o);
			d2 = Pvecy(i) - Pvecy(o);
			distance = sqrt ( d1*d1 + d2*d2);
			if (distance > eps1) {
				k = i;
				Pui = distance;
				Uvec.x = (Pvecx(i) - Pvecx(o)) / Pui;
				Uvec.y = (Pvecy(i) - Pvecy(o)) / Pui;
				Vvec.x = - Uvec.y;
				Vvec.y = Uvec.x;
				Fvec.x = Pui;
				Fvec.y = eps1 / Pui;
				Fvec.z = - eps1 / Pui;
			}
		}

		if (i >= k ) {
			keep = 1;
			Pui1 = (Pvecx(i+1) - Pvecx(o)) * Uvec.x + 
					(Pvecy(i+1) - Pvecy(o)) * Uvec.y;
			Pvi1 = (Pvecx(i+1) - Pvecx(o)) * Vvec.x + 
					(Pvecy(i+1) - Pvecy(o)) * Vvec.y;
			if (Pui1 >= Fvec.x) {
				Mi = Pvi1 / Pui1;
				if ((Mi <= Fvec.y) && (Mi >= Fvec.z)) {
					delMi = eps1 / Pui1;
					keep = 0;
					Fvec.x = Pui1;
					if (Fvec.y > (Mi + delMi)) 
						Fvec.y = (Mi + delMi);
					if (Fvec.z < (Mi - delMi)) 
						Fvec.z = (Mi - delMi);
				}
			}
			if (keep) {
				karay[(*nout)++] = i;
				o = i;
				k = nin;
			}
		}
	}
	karay[(*nout)++] =  nin-1;
}
