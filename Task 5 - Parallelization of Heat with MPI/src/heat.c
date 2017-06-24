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
  reorder=1;
  
	// check arguments
  if (argc < 2) {
    usage(argv[0]);
    return 1;
  }
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
  param.row=coords[1];
  param.col=coords[0];
  if (myid == nprocs-1||1)
    print_params(&param);
  time = (double *) calloc(sizeof(double), (int) (param.max_res - param.initial_res + param.res_step_size) / param.res_step_size);



  //  int north, south, west, east=0;
  //  MPI_CART_SHIFT(comm_2d, 0, 1, &north, &south);
  //  MPI_CART_SHIFT(comm_2d, 1, 1, &west, &east);

  int exp_number = 0;
  
  for (param.act_res = param.initial_res; param.act_res <= param.max_res; param.act_res = param.act_res + param.res_step_size) {
    if (!initialize(&param)) {
      fprintf(stderr, "Error in Jacobi initialization.\n\n");
      
      usage(argv[0]);
    }
    
      //---DEBUG ONLY, FOR TESTING OF INIT VALUES
    
      file_free=0;
      if (myid==root)
      file_free=1;
      else
      MPI_Recv(&file_free, 1, MPI_INT, myid-1, 1, MPI_COMM_WORLD, &status);
      if (file_free=1)
      {
      if(param.act_res * param.act_res < 200) {
      fprintf(stderr,"\np%d: my partial array is\n",myid);
      for (i = 0; i < param.arraysize_y + 2; i++) {
      if(i==1)
      fprintf(stderr,"---------\n");
      for (j = 0; j < param.arraysize_x + 2; j++) {
      if(j==param.arraysize_x+1 || j==1)
      fprintf(stderr,"| ");
      fprintf(stderr,"%f ", param.u[i * (param.arraysize_x + 2) + j]);
      }
      fprintf(stderr,"\n");
      if(i==param.arraysize_y)
      fprintf(stderr,"---------\n");
      }
      fprintf(stderr,"\n\n");
      }
      if (myid!=nprocs-1)
      MPI_Send (&file_free, 1, MPI_INT, myid+1, 1, MPI_COMM_WORLD);
      }
      MPI_Barrier(comm_2d);
      //---DEBUG ONLY

    
    
    // changed from act_res
      for (i = 0; i < param.arraysize_y + 2; i++) {
	for (j = 0; j < param.arraysize_x + 2; j++) {
	  param.uhelp[i * (param.arraysize_x + 2) + j] = param.u[i * (param.arraysize_x + 2) + j];
	}
      }
      
    // starting time
    time[exp_number] = wtime();
    residual = 999999999;
    globresid = residual;
    np = param.act_res + 2;

    
    
    
    int npx = param.arraysize_x + 2;
    int npy = param.arraysize_y + 2;
  
    MPI_Datatype north_south_type;
    MPI_Type_contiguous(npx-2, MPI_DOUBLE, &north_south_type);
    MPI_Type_commit(&north_south_type);
    // create east-west type

    MPI_Datatype east_west_type;
   
    MPI_Type_vector(npy-2,1,npx,MPI_DOUBLE, &east_west_type);
    MPI_Type_commit(&east_west_type);

    t0 = gettime();
    for (iter = 0; iter < param.maxiter; iter++) {
      residual = relax_jacobi(&(param.u), &(param.uhelp), npx, npy, param.len_x, param.len_y, comm_2d, &north_south_type, &east_west_type);
      // prints array after 1 iteration and communication, debug
      /*      if (iter==1000)
	{file_free=0;
	  if (myid==root)
	    file_free=1;
	  else
	    MPI_Recv(&file_free, 1, MPI_INT, myid-1, 1, MPI_COMM_WORLD, &status);
	  if (file_free=1)
	    {
	      if(param.act_res * param.act_res < 200) {
		fprintf(stderr,"\np%d: my partial array is\n",myid);
		for (i = 0; i < param.arraysize_y + 2; i++) {
		  if(i==1)
		    fprintf(stderr,"---------\n");
		  for (j = 0; j < param.arraysize_x + 2; j++) {
		    if(j==param.arraysize_x+1 || j==1)
		      fprintf(stderr,"| ");
		    fprintf(stderr,"%f ", param.u[i * (param.arraysize_x + 2) + j]);
		  }
		  fprintf(stderr,"\n");
		  if(i==param.arraysize_y)
		    fprintf(stderr,"---------\n");
		}
		fprintf(stderr,"\n\n");
	      }
	      if (myid!=nprocs-1)
		MPI_Send (&file_free, 1, MPI_INT, myid+1, 1, MPI_COMM_WORLD);
	    }
	}
      */      
      
	/*	
		the residual used to be a condition to break. because we use allreduce, all processes have the correct residual and this
		could very easy be reimplemented. otherwise, reduce would be sufficient to just have a chance to read the residual.
      */
      MPI_Allreduce(&residual, &globresid, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    }
    
    t1 = gettime();
    time[exp_number] = wtime() - time[exp_number];
    if(myid == root)
      {
	printf("\n\nResolution: %u\n", param.act_res);
	printf("===================\n");
	printf("Execution time: %f\n", time[exp_number]);
	printf("Row: %d Column %d\n", param.row, param.col);
	//	printf("Neighbours: North %d, South %d, West %d, East %d", north, south, west, east)
	printf("Offset x: %d Offset y: %d\n",param.offs_x,param.offs_y);
	printf("Len x: %d Len y: %d\n",param.arraysize_x,param.arraysize_y);
	printf("Size x: %d Size y: %d\n",param.len_x,param.len_y);
	printf("Residual: %f\n", residual);
	printf("Global Residual: %f\n\n", globresid);
	printf("megaflops:  %.1lf\n", (double) param.maxiter * (np - 2) * (np - 2) * 7 / time[exp_number] / 1000000);
	printf("  flop instructions (M):  %.3lf\n", (double) param.maxiter * (np - 2) * (np - 2) * 7 / 1000000);
      }
    
    
    exp_number++;
  }
  // change to ==root, if this should work
  param.act_res = param.act_res - param.res_step_size;
  
  
  //offsets for coarsen
  int ofx = param.offs_x;
  int ofy = param.offs_y;
  //lengths for coarsen
  int lx = param.len_x;
  int ly = param.len_y;
  
  //if tile in first row, increase length, else increase offset
  if(coords[0] == 0)	{lx+=1;}
  else 					{ofx+=1;}
  //if tile in last row, increase length
  if(coords[0] == (dim[0]-1)) {lx+=1;}

  //same for columns
  if(coords[1] == 0)	{ly+=1;}
  else 					{ofy+=1;}
  if(coords[1] == (dim[1]-1)){ly+=1;}
  coarsen(param.u, param.act_res + 2, param.act_res + 2, param.uvis, param.visres + 2, param.visres + 2,ofx,ofy,lx,ly,param.arraysize_x + 2,param.arraysize_y + 2);
  
  double* globalvis;
  globalvis  = 	(double*)calloc( sizeof(double),((param.visres+2)*(param.visres+2)) );
  
  MPI_Allreduce(param.uvis, globalvis, ((param.visres+2)*(param.visres+2)), MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

  if(myid == root)
  {
	printf("Process %d is writing the file\n", myid);
	write_image(resfile, globalvis, param.visres+2, param.visres+2);
  }

  free(globalvis);
  finalize(&param, myid);
  MPI_Finalize();
  fprintf(stderr, "\nProcess %d is done\n", myid);
  printf("Process %d is done\n", myid);

  return 0;
}
