#include <stdio.h>

#include "input.h"
#include "heat.h"
#include "timing.h"
#include "omp.h"
#include "mmintrin.h"
#include <papi.h>
#include "mpi.h"

double* time;

double gettime() {
	return ((double) PAPI_get_virt_usec() * 1000000.0);
}

void usage(char *s) {
	fprintf(stderr, "Usage: %s <input file> [result file]\n\n", s);
}

int main(int argc, char *argv[]) {
	int i, j, k, ret;
	FILE *infile, *resfile;
	char *resfilename;
	int np, iter, chkflag;
	double rnorm0, rnorm1, t0, t1;
	double tmp[8000000];

	// algorithmic parameters
	algoparam_t param;

	// timing

	double residual;

	// set the visualization resolution
	param.visres = 100;
	
	//MPI shith
	MPI_Status status;
	int np, myid;
	MPI_Comm comm;
	int dim[2], period[2], reorder;
    int coord[2], id;
	
	  if ((retval=MPI_Init (&argc, &argv))!=MPI_SUCCESS)
    fprintf(stderr, "Error initializing MPI, Errorcode %i", retval); 
	MPI_Comm_size(MPI_COMM_WORLD, &np);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);
	//TODO: adjust dims to useful value from args[]
	dim[0]= 2; dim[1]=2;
	//grid -> non periodic
    period[0]=0; period[1]=0;
    reorder=0;
	
	MPI_Cart_create(MPI_COMM_WORLD, 2, dim, period, reorder, &comm);
	MPI_Cart_coords(comm, myid, 2, coord);
	
	#TODO: MPI_INIT, MPI_Cart_create (2D grid), blablabla 
	// check arguments
	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	#TODO:only proc1?{
	// check input file
	if (!(infile = fopen(argv[1], "r"))) {
		fprintf(stderr, "\nError: Cannot open \"%s\" for reading.\n\n", argv[1]);

		usage(argv[0]);
		return 1;
	}

	// check result file
	resfilename = (argc >= 3) ? argv[2] : "heat.ppm";
	if (!(resfile = fopen(resfilename, "w"))) {
		fprintf(stderr, "\nError: Cannot open \"%s\" for writing.\n\n", resfilename);

		usage(argv[0]);
		return 1;
	}

	// check input
	if (!read_input(infile, &param)) {
		fprintf(stderr, "\nError: Error parsing input file.\n\n");

		usage(argv[0]);
		return 1;
	}
	#TODO: broadcast param to all other procs}
	#TODO: all other procs receive param and save
	print_params(&param);
	time = (double *) calloc(sizeof(double), (int) (param.max_res - param.initial_res + param.res_step_size) / param.res_step_size);

	int exp_number = 0;

	for (param.act_res = param.initial_res; param.act_res <= param.max_res; param.act_res = param.act_res + param.res_step_size) {
		if (!initialize(&param)) {
			fprintf(stderr, "Error in Jacobi initialization.\n\n");

			usage(argv[0]);
		}

		for (i = 0; i < param.act_res + 2; i++) {
			for (j = 0; j < param.act_res + 2; j++) {
				param.uhelp[i * (param.act_res + 2) + j] = param.u[i * (param.act_res + 2) + j];
			}
		}

		// starting time
		time[exp_number] = wtime();
		residual = 999999999;
		np = param.act_res + 2;

		t0 = gettime();
		#TODO: compute chunksize in x, y and chunk offset
		for (iter = 0; iter < param.maxiter; iter++) {
			#TODO: give chunksize and chunkoffset
			residual = relax_jacobi(&(param.u), &(param.uhelp), np, np);
			#TODO: send borders to neighbours
		}
		#TODO: gather residual
		t1 = gettime();
		time[exp_number] = wtime() - time[exp_number];

		printf("\n\nResolution: %u\n", param.act_res);
		printf("===================\n");
		printf("Execution time: %f\n", time[exp_number]);
		printf("Residual: %f\n\n", residual);

		printf("megaflops:  %.1lf\n", (double) param.maxiter * (np - 2) * (np - 2) * 7 / time[exp_number] / 1000000);
		printf("  flop instructions (M):  %.3lf\n", (double) param.maxiter * (np - 2) * (np - 2) * 7 / 1000000);

		exp_number++;
	}

	param.act_res = param.act_res - param.res_step_size;

	coarsen(param.u, param.act_res + 2, param.act_res + 2, param.uvis, param.visres + 2, param.visres + 2);
	#TODO:gather image
	write_image(resfile, param.uvis, param.visres + 2, param.visres + 2);

	finalize(&param);
	MPI_Finalize();
	return 0;
}
