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
			dim[0]= param.proc_x; dim[1]=param.proc_y;
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
	MPI_Cart_create(MPI_COMM_WORLD, 2, dim, period, reorder, &comm_2d);
	MPI_Cart_coords(comm_2d, myid, 2, coord);	
		
		
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
	// declare local arrays
	double* local = malloc (sizeof(double)*(myx+2)*(myy+2));
	double* local_help = malloc (sizeof(double)*(myx+2)*(myy+2));
	
	// init local array
	int m, n;
	for (m=0; m<(myy+2); m++)
	  {
	    for (n=0; n<(myx+2); n++)
	    {
	      local[m*myx+n]=0;
	      local_help[m*myx+n]=0;
	    }
	  }
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
	  
	  // TODO!!!
	  
	  // scatter initialized values from process root
	  int a;
	  int cord [2];
	  for (a=0; a<(param.proc_x*param.proc_y); a++)
		  {
		    MPI_Cart_coords(comm_2d, a, 2, cord);   // only works if every chunk has same size! 
		    displs[a]=cord[0]*myx+cord[1]*param.act_res*myy;   // only works if every chunk has same size! 
		    sendcounts[a]=myx; // only works if every line in local array has same length, which should be true
		  } 
	  int line;
	  for (line=0; line<myy; line++)
	    {
	      MPI_Scatterv((&(param.u)+line*param.act_res), sendcounts, displs, MPI_DOUBLE, &(local[1+myx+line*myx]), myx, MPI_DOUBLE, root, comm_2d);  // only works if every chunk has same size! 
	    }
	  
	  if (myid==root)
	    {
	      if (!initialize(&param)) {
		      fprintf(stderr, "Error in Jacobi initialization.\n\n");
		      
		      usage(argv[0]);
	      }
	    }
	  /*
	    for (i = 0; i < param.act_res + 2; i++) {
	    for (j = 0; j < param.act_res + 2; j++) {
		    param.uhelp[i * (param.act_res + 2) + j] = param.u[i * (param.act_res + 2) + j];
		    }
		    }
	  */
	  // starting time
	  time[exp_number] = wtime();
	  residual = 999999999;
	  np = param.act_res + 2;
	  
	  t0 = gettime();
		//TODO: compute chunksize in x, y and chunk offset
	  for (iter = 0; iter < param.maxiter; iter++) {
		  //TODO: give chunksize and chunkoffset
			residual = relax_jacobi(&(param.u), &(param.uhelp), myx, myy);
			MPI_Request dummyRequest;
			if(coord[0]<dim[0]-1)
			{
				//MPI_SEND send bottom row down
				//receive bottom row from down
				int down;
				int downc[] = {coord[0]+1,coord[1]};
				MPI_Cart_rank(comm_2d, downc, &down);
				MPI_Isend(&(param.u)+myx*(myy-2)+1, myx-2, MPI_DOUBLE, down,0, MPI_COMM_WORLD, &dummyRequest);
				MPI_Irecv(&(param.u)+myx*(myy-1)+1, myx-2, MPI_DOUBLE, down,0, MPI_COMM_WORLD, &dummyRequest);
			}
			if(cord[0]!=0)
			{
				//MPI_SEND send top row up
				//receive top row from above
				int up;
				int upc[] = {coord[0]-1,coord[1]};
				MPI_Cart_rank(comm_2d, upc, &up);
				MPI_Isend(&(param.u)+myx+1, myx-2, MPI_DOUBLE, up,1, MPI_COMM_WORLD, &dummyRequest);
				MPI_Irecv(&(param.u)+1, myx-2, MPI_DOUBLE, up,1, MPI_COMM_WORLD, &dummyRequest);
			}
			int i;
			if(coord[1]<dim[1]-1)
			{
				//MPI_SEND send right row right
				//receive right row from right
				int right;
				int rightc[] = {coord[0],coord[1]+1};
				MPI_Cart_rank(comm_2d, rightc, &right);
				for (i = 1; i<myy;i++)
				{
					MPI_Isend(&(param.u)+myx*i+myy-2, 1, MPI_DOUBLE, right,2, MPI_COMM_WORLD, &dummyRequest);
					MPI_Irecv(&(param.u)+myx*i+myy-1, 1, MPI_DOUBLE, right,2, MPI_COMM_WORLD, &dummyRequest);
				}
			}
			if(cord[1]!=0)
			{
				//MPI_SEND send left row left
				//receive left row left above
				int left;
				int leftc[] = {coord[0],coord[1]-1};
				MPI_Cart_rank(comm_2d, leftc, &left);
				for (i = 1; i<myy;i++)
				{
					MPI_Isend(&(param.u)+myx*i+1, 1, MPI_DOUBLE, left,3, MPI_COMM_WORLD, &dummyRequest);
					MPI_Irecv(&(param.u)+myx*i, 1, MPI_DOUBLE, left,3, MPI_COMM_WORLD, &dummyRequest);
				}
			}
	    //TODO: send borders to neighbours
	    //TODO: receive borders from neighbors
	    //potential deadlock here?
	  }
		//TODO: gather residual, ideally with MPI_Reduce
		double actualResidual;
		MPI_Reduce(&residual, &actualResidual,1,MPI_DOUBLE,MPI_SUM, root, MPI_COMM_WORLD);
	  t1 = gettime();
	  time[exp_number] = wtime() - time[exp_number];
	  
	  if(myid == root) {
	    printf("\n\nResolution: %u\n", param.act_res);
	    printf("===================\n");
		  printf("Execution time: %f\n", time[exp_number]);
		  printf("Residual: %f\n\n", actualResidual);
		  
		  printf("megaflops:  %.1lf\n", (double) param.maxiter * (np - 2) * (np - 2) * 7 / time[exp_number] / 1000000);
		  printf("  flop instructions (M):  %.3lf\n", (double) param.maxiter * (np - 2) * (np - 2) * 7 / 1000000);
	  }
	  
		exp_number++;
	}
	free(sendcounts);
	free(displs);
	free(local);
	free(local_help);
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
