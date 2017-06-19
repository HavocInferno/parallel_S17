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
	double globresid;
	// set the visualization resolution
	param.visres = 100;

	// mpi variables
	
	MPI_Status status;
	int nprocs, myid;
	int root = 0;
	MPI_Comm comm_2d;
	int dim[2], period[2], coords[2], reorder,  id;
	if ((ret=MPI_Init(&argc, &argv))!=MPI_SUCCESS)
	  fprintf(stderr, "Error during init of MPI, Errorcode %i", ret);
	
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

	MPI_Comm_rank(MPI_COMM_WORLD, &myid);
	
	period[0]=period[1]=0;
	reorder=0;
	
	// check arguments
	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}
	//if (myid==root)
	// check input file
	int file_free=0;
	{
	  if (myid==root)
	    file_free=1;
	  else
	    MPI_Recv(&file_free, 1, MPI_INT, myid-1, 1, MPI_COMM_WORLD, &status);
	  if (file_free==1)
	    {

	      if (!(infile = fopen(argv[1], "r"))) {
		fprintf(stderr, "\nError: Cannot open \"%s\" for reading.\n\n", argv[1]);
		
		usage(argv[0]);
		return 1;
	      }
	      if (myid==root){
		// check result file
		resfilename = (argc >= 3) ? argv[2] : "heat.ppm";
		
		if (!(resfile = fopen(resfilename, "w"))) {
		  fprintf(stderr, "\nError: Cannot open \"%s\" for writing.\n\n", resfilename);
		  
		  usage(argv[0]);
		  return 1;
		}
	      }
	      // check input
	      if (!read_input(infile, &param)) {
		fprintf(stderr, "\nError: Error parsing input file.\n\n");
		
		usage(argv[0]);
		return 1;
	      }
	    }
	  if (myid!=nprocs-1)
	    {
	      MPI_Send(&file_free, 1, MPI_INT, myid+1, 1, MPI_COMM_WORLD);
	    }
	}
	dim[0]= param.proc_x; dim[1]=param.proc_y;
	MPI_Cart_create(MPI_COMM_WORLD, 2, dim, period, reorder, &comm_2d);
	MPI_Cart_coords(comm_2d, myid, 2, coords);
	param.row=coords[0];
	param.col=coords[1];


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
		globresid = residual;
		np = param.act_res + 2;

		t0 = gettime();

		for (iter = 0; iter < param.maxiter; iter++) {
			residual = relax_jacobi(&(param.u), &(param.uhelp), np, np);
			/*	
				the residual used to be a condition to break. because we use allreduce, all processes have the correct residual and this
				could very easy be reimplemented. otherwise, reduce would be sufficient to just have a chance to read the residual.
			*/
			MPI_Allreduce(&residual, &globresid, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
		}

		t1 = gettime();
		time[exp_number] = wtime() - time[exp_number];

		printf("\n\nResolution: %u\n", param.act_res);
		printf("===================\n");
		printf("Execution time: %f\n", time[exp_number]);
		printf("Residual: %f\n", residual);
		printf("Global Residual: %f\n\n", globresid);
		printf("megaflops:  %.1lf\n", (double) param.maxiter * (np - 2) * (np - 2) * 7 / time[exp_number] / 1000000);
		printf("  flop instructions (M):  %.3lf\n", (double) param.maxiter * (np - 2) * (np - 2) * 7 / 1000000);

		exp_number++;
	}
	if (myid==root)
	  {
	    param.act_res = param.act_res - param.res_step_size;
	    
	    coarsen(param.u, param.act_res + 2, param.act_res + 2, param.uvis, param.visres + 2, param.visres + 2);
	    
	    write_image(resfile, param.uvis, param.visres + 2, param.visres + 2);
	  }
	if (myid!=root)
	  free(param.heatsrcs);

	finalize(&param);
	MPI_Finalize();
	fprintf(stderr, "\nProcess %d is done\n", myid);
	return 0;
}
