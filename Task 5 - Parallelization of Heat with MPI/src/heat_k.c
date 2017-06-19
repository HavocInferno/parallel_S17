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

	// mpi variables
	
	MPI_Status status;
	int nprocs, myid;
	int root = 0;
	MPI_Comm comm_2d;
	int dim[2], period[2], reorder, coord [2], id;
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
	if (myid==root)
	// check input file
	  {
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
	  }
	
	MPI_Bcast(&(param.maxiter), 1, MPI_UNSIGNED, root, MPI_COMM_WORLD);
	//MPI_Bcast(param.act_res, 1, MPI_UNSIGNED, root, MPI_COMM_WORLD);
	MPI_Bcast(&(param.max_res), 1, MPI_UNSIGNED, root, MPI_COMM_WORLD);
	MPI_Bcast(&(param.initial_res), 1, MPI_UNSIGNED, root, MPI_COMM_WORLD);
	MPI_Bcast(&(param.res_step_size), 1, MPI_UNSIGNED, root, MPI_COMM_WORLD);
	MPI_Bcast(&(param.numsrcs), 1, MPI_UNSIGNED, root, MPI_COMM_WORLD);


	/* DEFINE TYPE FOR HEATSRCS
	   MPI_Datatype heatsrctype;
	   MPI_Datatype oldtypes [4];
	   int count = 4;
	   int blocklens [4] = {1,1,1,1};
	   int displs [4];
	   oldtypes [0]= MPI_FLOAT; displs[0]=(MPI_Aint)0;
	   oldtypes [1]= MPI_FLOAT; displs[1]=(MPI_Aint)0+sizeof(float);
	   oldtypes [2]= MPI_FLOAT; displs[2]=(MPI_Aint)0+2*sizeof(float);
	   oldtypes [3]= MPI_FLOAT; displs[3]=(MPI_Aint)0+3*sizeof(float);
	   
	   MPI_Type_struct (count, blocklens, displs, oldtypes, &heatsrctype);
	   // allocate array for heatsrcs, bcast one by one? or create type.
	   MPI_Type_commit (&heatsrctype);
	*/
	if (myid!=root)
	  {
	    // this should work, right?
	    (param.heatsrcs)=(heatsrc_t*) malloc(sizeof(heatsrc_t)*param.numsrcs);
	    
	  }  
	int z;
	
	for (z=0; z<param.numsrcs; z++)
	  {
	    if (myid!=root)
	      {
		param.heatsrcs[z].posx=0;
		param.heatsrcs[z].posy=0;
		param.heatsrcs[z].range=0;
		param.heatsrcs[z].temp=0;
	      }
	    //MPI_Bcast(&(param.heatsrcs[z]), 1, heatsrctype, root, MPI_COMM_WORLD);		
	    // some how, some way, distribute heatsrcs to all processes
	  }
	
	
	MPI_Bcast(&(param.proc_x), 1, MPI_INT, root, MPI_COMM_WORLD);
	MPI_Bcast(&(param.proc_y), 1, MPI_INT, root, MPI_COMM_WORLD);
	dim[0]= param.proc_x; dim[1]=param.proc_y;
	//param.numsrcs=0;
	


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

		for (iter = 0; iter < param.maxiter; iter++) {
			residual = relax_jacobi(&(param.u), &(param.uhelp), np, np);
		}

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
