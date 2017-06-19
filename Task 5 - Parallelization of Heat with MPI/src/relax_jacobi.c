/*
 * relax_jacobi.c
 *
 * Jacobi Relaxation
 *
 */

#include "heat.h"

//TODO: add x and y offset and chunksize into signature
double relax_jacobi( double **u1, double **utmp1,
         unsigned sizex, unsigned sizey,unsigned ofx, unsigned ofy , int gridsize)
{
  int i, j;
  double *help,*u, *utmp,factor=0.5;

  utmp=*utmp1;
  u=*u1;
  double unew, diff, sum=0.0;

  //TODO: i = chunkoffset_y, i<chunkoffset_y+chunksize_y-1 && i<sizey-1
  for( i=ofy; i<ofy+sizey-1 && i <gridsize-1; i++ ) {
  	int ii=i*sizex;
  	int iim1=(i-1)*sizex;
  	int iip1=(i+1)*sizex;
#pragma ivdep
	//TODO: j = chunkoffset_x, i<chunkoffset_x+chunksize_x-1 && i<sizex-1
    for( j=ofx; j<ofx+sizex-1 && j<gridsize-1; j++ ){
       unew = 0.25 * (u[ ii+(j-1) ]+
        		            u[ ii+(j+1) ]+
        		            u[ iim1+j ]+
        		            u[ iip1+j ]);
		    diff = unew - u[ii + j];
		    utmp[ii+j] = unew;
		    sum += diff * diff;

       }
    }

  *u1=utmp;
  *utmp1=u;
  return(sum);
}


