/*
 * relax_gauss.c
 *
 * Gauss-Seidel Relaxation
 *
 */

#include "heat.h"

/*
 * Residual (length of error vector)
 * between current solution and next after a Gauss-Seidel step
 *
 * Temporary array utmp needed to not change current solution
 *
 * Flop count in inner body is 7
 */

double residual_gauss(double *u, double *utmp, unsigned sizex, unsigned sizey) {
	unsigned i, j;
	double unew, diff, sum = 0.0;

	// first row (boundary condition) into utmp
	for (j = 1; j < sizex - 1; j++)
		utmp[0 * sizex + j] = u[0 * sizex + j];
	// first column (boundary condition) into utmp
	for (i = 1; i < sizey - 1; i++)
		utmp[i * sizex + 0] = u[i * sizex + 0];

	for (j = 1; j < sizex - 1; j++) {
		for (i = 1; i < sizey - 1; i++) {
			unew = 0.25 * (utmp[i * sizex + (j - 1)] +  // new left
						u[i * sizex + (j + 1)] +  // right
						utmp[(i - 1) * sizex + j] +  // new top
						u[(i + 1) * sizex + j]); // bottom

			diff = unew - u[i * sizex + j];
			sum += diff * diff;

			utmp[i * sizex + j] = unew;
		}
	}

	return sum;
}

/*
 * One Gauss-Seidel iteration step
 *
 * Flop count in inner body is 4
 */
void relax_gauss(double *u, unsigned sizex, unsigned sizey) {
	unsigned i, j;

	for (j = 1; j < sizex - 1; j++) {
		for (i = 1; i < sizey - 1; i++) {
			u[i * sizex + j] = 0.25 * (u[i * sizex + (j - 1)] + u[i * sizex + (j + 1)] + u[(i - 1) * sizex + j] + u[(i + 1) * sizex + j]);
		}
	}
}
