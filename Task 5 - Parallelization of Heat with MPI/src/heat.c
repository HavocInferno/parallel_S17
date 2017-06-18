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
        int i, j, k, ret, retval;
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
	
	//MPI vars
	MPI_Status status;
	int nprocs, myid;
	int root = 0;
	MPI_Comm comm_2d;
	int dim[2], period[2], reorder;
	int coord[2], id;
	int myx = 0, myy = 0;
	if ((retval=MPI_Init (&argc, &argv))!=MPI_SUCCESS)
		fprintf(stderr, "Error initializing MPI, Errorcode %i", retval); 
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);
	
	//TODO: adjust dims to useful value from args[]
	// Moved down, after al processes know the topology
	//dim[0]= 2; dim[1]=2;
	//grid -> non periodic
	period[0]=0; period[1]=0;
	reorder=0;
	
	MPI_Cart_create(MPI_COMM_WORLD, 2, dim, period, reorder, &comm_2d);
	MPI_Cart_coords(comm_2d, myid, 2, coord);
	
	//TODO: MPI_INIT, MPI_Cart_create (2D grid), blablabla 
	// check arguments
	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	//only proc0 checks file
	if(myid==0)	{
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
		
		  //broadcast param to all other procs
		  /* REMINDER
		     typedef struct
		{
			unsigned maxiter;       // maximum number of iterations
			unsigned act_res;
			unsigned max_res;       // spatial resolution
			unsigned initial_res;
			unsigned res_step_size;
			unsigned visres;        // visualization resolution
		  
			double *u, *uhelp;
			double *uvis;

			unsigned   numsrcs;     // number of heat sources
			heatsrc_t *heatsrcs;
		}
		algoparam_t;
	*/
		//to avoid performance deterioration due to a single pointer, 
		// we send a copy of each attribute
		
        }
	MPI_Bcast(&(param.maxiter), 1, MPI_UNSIGNED, root, comm_2d);
	MPI_Bcast(&(param.act_res), 1, MPI_UNSIGNED, root, comm_2d);
	MPI_Bcast(&(param.max_res), 1, MPI_UNSIGNED, root, comm_2d);
	MPI_Bcast(&(param.initial_res), 1, MPI_UNSIGNED, root, comm_2d);
	MPI_Bcast(&(param.res_step_size), 1, MPI_UNSIGNED, root, comm_2d);
	MPI_Bcast(&(param.numsrcs), 1, MPI_UNSIGNED, root, comm_2d);
	MPI_Bcast(&(param.heatsrcs), sizeof(heatsrc_t), MPI_BYTE, root, comm_2d);
	MPI_Bcast(&(param.proc_x), 1, MPI_UNSIGNED, root, comm_2d);
	MPI_Bcast(&(param.proc_y), 1, MPI_UNSIGNED, root, comm_2d);
	if (np!=((param.proc_x)*(param.proc_y)))
	  {
	    fprintf(stderr, "\n Error: Number of processes does not equal number of processes in grid definition");
	    return 1;
	  }
	  //TODO: set u, uhelp, uvis according to own chunksize?
	  //Konrad: u and uhelp yes. uvis is only needed in process0, as it is the visualization
	dim[0]= param.proc_x; dim[1]=param.proc_y;
	// declare local arrays
	double* local = malloc (sizeof(double)*myx*myy);
	double* local_help = malloc (sizeof(double)*myx*myy);
	
	print_params(&param);
	time = (double *) calloc(sizeof(double), (int) (param.max_res - param.initial_res + param.res_step_size) / param.res_step_size);

	int exp_number = 0;
	int* sendcounts=malloc((sizeof(int))*param.proc_x*param.proc_y);
	int* displs=malloc((sizeof(int))*param.proc_x*param.proc_y);
	for (param.act_res = param.initial_res; param.act_res <= param.max_res; param.act_res = param.act_res + param.res_step_size) {
	        // calculate gridsize of process
	        myx=param.act_res/param.proc_x;
		myy=param.act_res/param.proc_y;
		
		// implement handling for act_res%proc_n!=0
		if ((param.act_res%param.proc_x)<myid)
		  myx++;
		if ((param.act_res%param.proc_y)<myid)
		  myy++;
		// scatter initialized values from process root
		int a;
		int cord [2];
		for (a=0; a<(param.proc_x*param.proc_y); a++)
		  {
		    MPI_Cart_coords(comm_2d, a, 2, cord);
		    displs[a]=cord[0]*myx+cord[1]*param.act_res*myy; // same as below
		    sendcounts[a]=myx; // CHANGE TO ACTUAL LENGTH OF ARRAY, WRONG FOR BORDERS
		  } 
		
		MPI_Scatterv(&(param.u), sendcounts, displs, MPI_DOUBLE, local, myx, MPI_DOUBLE, root, comm_2d); 

		
		if (myid==root)
		  {
		    if (!initialize(&param)) {
		      fprintf(stderr, "Error in Jacobi initialization.\n\n");
		      
		      usage(argv[0]);
		    }
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
		//TODO: compute chunksize in x, y and chunk offset
		for (iter = 0; iter < param.maxiter; iter++) {
		  //TODO: give chunksize and chunkoffset
		  residual = relax_jacobi(&(param.u), &(param.uhelp), np, np);
		  //TODO: send borders to neighbours
		  //TODO: receive borders from neighbors
		  //potential deadlock here?
		}
		//TODO: gather residual, ideally with MPI_Reduce
		t1 = gettime();
		time[exp_number] = wtime() - time[exp_number];
		
		if(myid == root) {
		  printf("\n\nResolution: %u\n", param.act_res);
		  printf("===================\n");
		  printf("Execution time: %f\n", time[exp_number]);
		  printf("Residual: %f\n\n", residual);

		  printf("megaflops:  %.1lf\n", (double) param.maxiter * (np - 2) * (np - 2) * 7 / time[exp_number] / 1000000);
		  printf("  flop instructions (M):  %.3lf\n", (double) param.maxiter * (np - 2) * (np - 2) * 7 / 1000000);
		}
		
		exp_number++;
	}
	free(sendcounts);
	free(displs);
	param.act_res = param.act_res - param.res_step_size;
	
	coarsen(param.u, param.act_res + 2, param.act_res + 2, param.uvis, param.visres + 2, param.visres + 2);
	
	//gather image
	if(myid != root) {
	  //TODO: all non-root processes send their image parts to p0
	} else {
	  //TODO: recv image parts, MPI_Gather?
		
		write_image(resfile, param.uvis, param.visres + 2, param.visres + 2);
	}

	finalize(&param);
	MPI_Finalize();
	return 0;
}
