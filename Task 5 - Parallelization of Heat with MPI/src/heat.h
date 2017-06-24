#/*
 * heat.h
 *
 * Global definitions for the iterative solver
 */

#ifndef JACOBI_H_INCLUDED
#define JACOBI_H_INCLUDED

#include <stdio.h>
#include "mpi.h"
// configuration

typedef struct
{
    float posx;
    float posy;
    float range;
    float temp;
}
heatsrc_t;

typedef struct
{
    unsigned maxiter;       // maximum number of iterations
    unsigned act_res;
    unsigned max_res;       // spatial resolution
    unsigned initial_res;
    unsigned res_step_size;
    unsigned visres;        // visualization resolution
    int proc_x;
    int proc_y;
    int row, col;
    int arraysize_x, arraysize_y;
    int len_x, len_y;
    int offs_x, offs_y;
    double *u, *uhelp;
    double *uvis;

    unsigned   numsrcs;     // number of heat sources
    heatsrc_t *heatsrcs;
}
algoparam_t;


// function declarations

// misc.c
int initialize( algoparam_t *param );
int finalize( algoparam_t *param, int myid );
void write_image( FILE * f, double *u,
		  unsigned sizex, unsigned sizey );
int coarsen(double *uold, unsigned oldx, unsigned oldy ,
	    double *unew, unsigned newx, unsigned newy, int offs_x, int offs_y, int len_x, int len_y, int dim_x, int dim_y );

// Gauss-Seidel: relax_gauss.c
double residual_gauss( double *u, double *utmp,
		       unsigned sizex, unsigned sizey );
void relax_gauss( double *u, 
		  unsigned sizex, unsigned sizey  );

// Jacobi: relax_jacobi.c
double residual_jacobi( double *u,
			unsigned sizex, unsigned sizey );
double relax_jacobi( double **u, double **utmp,
		     int sizex, int sizey, int len_x, int len_y, MPI_Comm comm_2d, MPI_Datatype* north_south_type, MPI_Datatype* east_west_type);


#endif // JACOBI_H_INCLUDED
