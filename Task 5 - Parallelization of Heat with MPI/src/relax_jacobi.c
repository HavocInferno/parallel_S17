
/*
 * relax_jacobi.c
 *
 * Jacobi Relaxation
 *
 */
#define nonblocking
#include "heat.h"
#include "mpi.h"

double relax_jacobi( double **u1, double **utmp1,
		     int  sizex, int sizey,int len_x, int len_y,
		     MPI_Comm comm_2d, MPI_Datatype* north_south_type, MPI_Datatype* east_west_type)
{
  int i, j;
  double *help,*u, *utmp,factor=0.5;
  MPI_Status status;
  int north, south, west, east;
  MPI_Cart_shift(comm_2d, 1, 1, &north, &south);
  MPI_Cart_shift(comm_2d, 0, 1, &west, &east);
  
  
  utmp=*utmp1;
  u=*u1;
  double unew, diff, sum=0.0;
 
  //  Send recv, nonblocking

  
  MPI_Request reqs[8];
  
  MPI_Isend(&u[1+sizex] , 1, *north_south_type, north, 9, comm_2d, &reqs[0]);
  
  MPI_Isend(&u[1+(sizey-2)*sizex] , 1, *north_south_type, south, 9, comm_2d, &reqs[1]);
  
  MPI_Irecv(&u[1], 1, *north_south_type, north, 9, comm_2d, &reqs[2]);
  
  MPI_Irecv(&u[1+(sizey-1)*sizex], 1, *north_south_type, south, 9, comm_2d, &reqs[3]);
  
  MPI_Isend(&u[1+sizex], 1, *east_west_type, west, 9, comm_2d, &reqs[4]);
  
  MPI_Isend(&u[2*sizex-2], 1, *east_west_type, east, 9, comm_2d, &reqs[5]);
  
  MPI_Irecv(&u[sizex], 1, *east_west_type, west, 9, comm_2d, &reqs[7]);
  
  MPI_Irecv(&u[2*sizex-1], 1, *east_west_type, east, 9, comm_2d, &reqs[6]);

	
#ifndef nonblocking
      MPI_Waitall(8, reqs, MPI_STATUS_IGNORE);
#endif
  
  // calculate inner points, which do not depend on borders
  for( i=2; i<len_y+0; i++ ) {
    int ii=i*sizex;
    int iim1=(i-1)*sizex;
    int iip1=(i+1)*sizex;
    
    
    
#pragma ivdep
    for( j=2; j<len_x+0; j++ )
      {
	unew = 0.25 * (u[ ii+(j-1) ]+
			 u[ ii+(j+1) ]+
			 u[ iim1+j ]+
			 u[ iip1+j ]);
	diff = unew - u[ii + j];
	utmp[ii+j] = unew;
	sum += diff * diff;
	
      }
  }
  

  
#ifdef nonblocking
  // now we need the bordervalues, so that we can calc the outer points
  MPI_Waitall(8, reqs, MPI_STATUS_IGNORE);
#endif
  
  for (i=1; i<len_x+1; i++)
    {
      // top row
      unew = 0.25 * (u[i]+u[sizex-1+i]+u[sizex+1+i]+u[2*sizex+i]);
      diff = unew - u[sizex+i];
      utmp [sizex+i] = unew;
      sum+=diff*diff;
      
	// bottom row
      unew = 0.25 * (u[i+(len_y-1)*sizex]+u[len_y*sizex+i-1]+u[len_y*sizex+i+1]+u[(len_y+1)*sizex+i]);
      diff = unew - u[(len_y-1)*sizex+sizex+i];
      utmp [(len_y-1)*sizex+sizex+i] = unew;
      sum+=diff*diff;
      
    }
  
  for (i=2; i<len_y; i++)
    {
      int ii=i*sizex;
      int iim1=(i-1)*sizex;
      int iip1=(i+1)*sizex;
      
	// left column
      unew = 0.25 * (u[iim1+1]+u[ii-1+1]+u[ii+1+1]+u[iip1+1]);
      diff = unew - u[ii+1];
      utmp [ii+1] = unew;
      sum+=diff*diff;		      
      
      //right column
      unew = 0.25 * (u[iim1+len_x]+u[iip1+len_x]+u[ii+len_x+1]+u[ii+len_x-1]);
      diff = unew - u[ii+len_x];
      utmp [ii+len_x] = unew;
      sum+=diff*diff;		      
      
    }
  
    *u1=utmp;
    *utmp1=u;
    return(sum);
}


