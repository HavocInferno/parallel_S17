/*
 * relax_jacobi.c
 *
 * Jacobi Relaxation
 *
 */

#include "heat.h"

/*
 * Residual (length of error vector)
 * between current solution and next after a Jacobi step
 */
double residual_jacobi(double *u, unsigned sizex, unsigned sizey) {
	unsigned i, j;
	double unew, diff, sum = 0.0;
	//idea for optimization: 	go through rows instead of columns
	//							instead of recomputing unew, compare u to utmp
	//							vectorization
	for (j = 1; j < sizex - 1; j++) {
		for (i = 1; i < sizey - 1; i++) {
			unew = 0.25 * (u[i * sizex + (j - 1)] +  // left
						u[i * sizex + (j + 1)] +  // right
						u[(i - 1) * sizex + j] +  // top
						u[(i + 1) * sizex + j]); // bottom

			diff = unew - u[i * sizex + j];
			sum += diff * diff;
		}
	}

	return sum;
}

/*
 * One Jacobi iteration step
 */
void relax_jacobi(double *u, double *utmp, unsigned sizex, unsigned sizey) {
	int i, j;
	//idea for optimization: 	array padding (less conflict misses)
	//							go through rows instead of columns
	//							manual vectorization
	//							loop unrolling
	for (j = 1; j < sizex - 1; j++) {
		for (i = 1; i < sizey - 1; i++) {
			utmp[i * sizex + j] = 0.25 * (u[i * sizex + (j - 1)] +  // left
						u[i * sizex + (j + 1)] +  // right
						u[(i - 1) * sizex + j] +  // top
						u[(i + 1) * sizex + j]); // bottom
		}
	}

	// copy from utmp to u
	//idea for optimization: instead of copying from utmp to u, just swap the pointers

	for (j = 1; j < sizex - 1; j++) {
		for (i = 1; i < sizey - 1; i++) {
			u[i * sizex + j] = utmp[i * sizex + j];
		}
	}
}


double residual_jacobi_optimized(double *u, double *uold, unsigned sizex, unsigned sizey) {
	unsigned i, j;
	double unew, diff, sum = 0.0;
	//idea for optimization: 	go through rows instead of columns
	//							instead of recomputing unew, compare u to utmp
	//							vectorization
	
	
	for (j = 1; j < sizex - 1; j++) {
		for (i = 1; i < sizey - 1; i++) {
			
			unew = 0.25 * (u[i + (j - 1)*sizey] +  // left
						u[i + (j + 1)*sizey] +  // right
						u[(i - 1) + j*sizey] +  // top
						u[(i + 1) + j*sizey]); // bottom

						
						
	//		diff = uold[i + j*sizey] - u[i + j*sizey];
			diff = unew - u[i + j*sizey];
			sum += diff * diff;
		}
	}

	return sum;
}

/*
 * One Jacobi iteration step
 */
void relax_jacobi_optimized(double *u, double *utmp, unsigned sizex, unsigned sizey) {
	int i, j;
	//idea for optimization: 	array padding (less conflict misses)
	//							go through rows instead of columns
	//							manual vectorization
	//							loop unrolling
	for (j = 1; j < sizex - 1; j++) {
		for (i = 1; i < sizey - 1; i++) {
			utmp[i + j * sizey] = 0.25 * (u[i + (j - 1)*sizey] +  // left
						u[i + (j + 1)*sizey] +  // right
						u[(i - 1) + j*sizey] +  // top
						u[(i + 1) + j*sizey]); // bottom
		}
	}

	// copy from utmp to u
	//idea for optimization: instead of copying from utmp to u, just swap the pointers
	
	double* temp = u;
	u = utmp;
	utmp = temp;
	
	/*
	for (j = 1; j < sizex - 1; j++) {
		for (i = 1; i < sizey - 1; i++) {
			u[i * sizex + j] = utmp[i * sizex + j];
		}
	}
	*/
}