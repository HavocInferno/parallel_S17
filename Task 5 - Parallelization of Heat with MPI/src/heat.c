#include <stdio.h>

#include "input.h"
#include "heat.h"
#include "timing.h"
//#include "omp.h"
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
	
	//MPI shith
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

	
	//TODO: MPI_INIT, MPI_Cart_create (2D grid), blablabla 
	// check arguments
	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	//TODO:only proc1?{
	// check input file
	if(myid==root)	{
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
	
	MPI_Bcast(&(param.maxiter), 1, MPI_UNSIGNED, root, MPI_COMM_WORLD);
	MPI_Bcast(&(param.act_res), 1, MPI_UNSIGNED, root, MPI_COMM_WORLD);
	MPI_Bcast(&(param.max_res), 1, MPI_UNSIGNED, root, MPI_COMM_WORLD);
	MPI_Bcast(&(param.initial_res), 1, MPI_UNSIGNED, root, MPI_COMM_WORLD);
	MPI_Bcast(&(param.res_step_size), 1, MPI_UNSIGNED, root, MPI_COMM_WORLD);
	MPI_Bcast(&(param.numsrcs), 1, MPI_UNSIGNED, root, MPI_COMM_WORLD);
	MPI_Bcast(&(param.heatsrcs), sizeof(heatsrc_t), MPI_BYTE, root, MPI_COMM_WORLD);
	MPI_Bcast(&(param.proc_x), 1, MPI_INT, root, MPI_COMM_WORLD);
	MPI_Bcast(&(param.proc_y), 1, MPI_INT, root, MPI_COMM_WORLD);
	dim[0]= param.proc_x; dim[1]=param.proc_y;
	
	MPI_Cart_create(MPI_COMM_WORLD, 2, dim, period, reorder, &comm_2d);
	MPI_Cart_coords(comm_2d, myid, 2, coord);	
	//TODO: broadcast param to all other procs}
	//TODO: all other procs receive param and save
	print_params(&param);
	time = (double *) calloc(sizeof(double), (int) (param.max_res - param.initial_res + param.res_step_size) / param.res_step_size);

	int source, north, south, east, west;
	MPI_Cart_shift(comm_2d, 0, 1, &west, &east);
	MPI_Cart_shift(comm_2d, 1, 1, &north, &south);
	printf("Shifted cart");
	
	int exp_number = 0;

	for (param.act_res = param.initial_res; param.act_res <= param.max_res; param.act_res = param.act_res + param.res_step_size) {
		if(myid == root)
			printf("Initializing. ");
		if (!initialize(&param)) {
			fprintf(stderr, "Error in Jacobi initialization.\n\n");
			usage(argv[0]);
		}
		if(myid == root)
			printf("Initialized. ");

		for (i = 0; i < param.act_res + 2; i++) {
			for (j = 0; j < param.act_res + 2; j++) {
				param.uhelp[i * (param.act_res + 2) + j] = param.u[i * (param.act_res + 2) + j];
			}
		}
		
		// starting time
		time[exp_number] = wtime();
		residual = 999999999;
		np = param.act_res + 2;
		
		int tileSizeX=(np/dim[0])+2, tileSizeY=(np/dim[1])+2;
		int tileOffsetX = coord[0]*(tileSizeX-2), tileOffsetY = coord[1]*(tileSizeY-2);
		
		if(coord[0]==dim[0]-1)
			tileSizeX+=np%dim[0];
		if(coord[1]==dim[1]-1)
			tileSizeY+=np%dim[1];
		
		MPI_Datatype north_south_type;
		MPI_Type_contiguous(tileSizeX-2, MPI_DOUBLE, &north_south_type);
		MPI_Type_commit(&north_south_type);
		// create east-west type
		MPI_Datatype east_west_type;
		MPI_Type_vector(tileSizeY-2,1,np,MPI_DOUBLE, &east_west_type);
		MPI_Type_commit(&east_west_type);

		
		t0 = gettime();
		//TODO: compute chunksize in x, y and chunk offset
		for (iter = 0; iter < param.maxiter; iter++) {
			//TODO: give chunksize and chunkoffset
			residual = relax_jacobi(&(param.u), &(param.uhelp), tileSizeX, tileSizeY, tileOffsetX, tileOffsetY, np);
			//TODO: send borders to neighbours
			if(myid == root)
				printf("Relaxed. ");
			MPI_Request reqs[8];
			MPI_Isend(&param.u[tileOffsetX+(tileOffsetY+1)*np+1] /* north */, 1, north_south_type, north, 9, comm_2d, &reqs[0]);
			MPI_Isend(&param.u[tileOffsetX+(tileOffsetY+tileSizeY-2)*np] /* south */, 1, north_south_type, south, 9, comm_2d, &reqs[1]);
			MPI_Isend(&param.u[tileOffsetX+(tileOffsetY+1)*np+1] /* east */, 1, east_west_type, east, 9, comm_2d, &reqs[2]);
			MPI_Isend(&param.u[tileOffsetX+tileSizeX-2+(tileOffsetY+1)*np] /* west */, 1, east_west_type, west, 9, comm_2d, &reqs[3]);
			MPI_Irecv(&param.u[tileOffsetX+(tileOffsetY)*np+1] /* north */, 1, north_south_type, north, 9, comm_2d, &reqs[4]);
			MPI_Irecv(&param.u[tileOffsetX+(tileOffsetY+tileSizeY-1)*np] /* south */, 1, north_south_type, south, 9, comm_2d, &reqs[5]);
			MPI_Irecv(&param.u[tileOffsetX+(tileOffsetY+1)*np] /* west */, 1, east_west_type, east, 9, comm_2d, &reqs[6]);
			MPI_Irecv(&param.u[tileOffsetX+tileSizeX-1+(tileOffsetY+1)*np] /* east */, 1, east_west_type, west, 9, comm_2d, &reqs[7]);
			if(myid == root)
				printf("Waiting. ");
			
			MPI_Waitall(8, reqs, MPI_STATUS_IGNORE);
			if(myid == root)
				printf("Swapped borders. ");
		}
		if(myid == root)
			printf("Done with Jacobi. \n");
		//TODO: gather residual
		t1 = gettime();
		double actualResidual = 5;
		MPI_Reduce(&residual, &actualResidual,1,MPI_DOUBLE,MPI_SUM, root, MPI_COMM_WORLD);
		time[exp_number] = wtime() - time[exp_number];
		if(myid == root)
		{
			printf("\n\nResolution: %u\n", param.act_res);
			printf("===================\n");
			printf("Execution time: %f\n", time[exp_number]);
			printf("Residual: %f\n\n", actualResidual);

			printf("megaflops:  %.1lf\n", (double) param.maxiter * (np - 2) * (np - 2) * 7 / time[exp_number] / 1000000);
			printf("  flop instructions (M):  %.3lf\n", (double) param.maxiter * (np - 2) * (np - 2) * 7 / 1000000);
		}
		exp_number++;
	}

	param.act_res = param.act_res - param.res_step_size;

	coarsen(param.u, param.act_res + 2, param.act_res + 2, param.uvis, param.visres + 2, param.visres + 2);
	//TODO:gather image
	write_image(resfile, param.uvis, param.visres + 2, param.visres + 2);

	finalize(&param);
	MPI_Finalize();
	return 0;
}
