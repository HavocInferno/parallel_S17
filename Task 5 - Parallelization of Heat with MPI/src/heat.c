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
  if ((param.visres+2)%param.proc_x==0)
    param.visresx=((param.visres+2)/param.proc_x);
  else param.visresx =((param.visres+2)/param.proc_x+1);

  if ((param.visres+2)%param.proc_y==0) 
    param.visresy = ((param.visres+2)/param.proc_y);
  else param.visresy=((param.visres+2)/param.proc_y+1);

  param.visresglobx=param.visresx*param.proc_x;
  param.visresgloby=param.visresy*param.proc_y;
    
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
    /*
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
      */
    
    
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
    MPI_Type_free(&north_south_type);
    MPI_Type_free(&east_west_type);
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
	printf("Visres x: %d Visres y: %d, Visres globx: %d Visresgloby: %d\n", param.visresx, param.visresy, param.visresglobx, param.visresgloby);
	printf("Residual: %f\n", residual);
	printf("Global Residual: %f\n\n", globresid);
	printf("megaflops:  %.1lf\n", (double) param.maxiter * (np - 2) * (np - 2) * 7 / time[exp_number] / 1000000);
	printf("  flop instructions (M):  %.3lf\n", (double) param.maxiter * (np - 2) * (np - 2) * 7 / 1000000);
      }
    
    
    exp_number++;
  }
  param.act_res = param.act_res - param.res_step_size;
  
  
  
  double *uloc=(double*)calloc(param.visresx*param.visresy, sizeof(double));
  coarsen(param.u, param.act_res + 2, param.act_res + 2, uloc, param.visresx, param.visresy, param.len_x+2, param.len_y+2);
  
  // Now we need to gather the data. Sending part is simple - (visresx*visresy) MPI_Doubles
  // Receiving is more difficult. We need to do MPI_Type_create_subarray and resize them, so we can use doubles as offsets.
  MPI_Datatype subarray, subarray_resized;
  int sizes[2];
  sizes [0] = param.visresgloby;
  sizes [1] = param.visresglobx;
  int subsizes[2];
  subsizes [0] = param.visresy;
  subsizes [1] = param.visresx;
  int starts[2]= {0,0};
  int order;
  int* displs=(int*) malloc (nprocs*sizeof(int));
  int* counts=(int*) malloc (nprocs*sizeof(int));
  int cords[2];
  
  int a;
  for (a=0; a<nprocs; a++)
    {
      counts[a]=1;
      MPI_Cart_coords(comm_2d, a, 2, cords);
      displs[a]=cords[0]*param.visresx+cords[1]*param.visresy*param.visresglobx/*TODO*/; //TODO
    }
  MPI_Type_create_subarray(2, sizes, subsizes, starts, MPI_ORDER_C, MPI_DOUBLE, &subarray);
  MPI_Type_commit(&subarray);
  MPI_Type_create_resized(subarray, 0, 1*sizeof(double), &subarray_resized);
  MPI_Type_commit(&subarray_resized);
  
  MPI_Gatherv(uloc, param.visresx*param.visresy, MPI_DOUBLE, param.uvis, counts, displs, subarray_resized, root, comm_2d);
  
  MPI_Type_free(&subarray_resized);
  MPI_Type_free(&subarray);
  MPI_Barrier(comm_2d);
  file_free=0;
  if (myid==root)	{
    if(param.act_res * param.act_res < 200) {
      fprintf(stderr,"\np%d: my coarsed partial array is\n",myid);
      for (i = 0; i < param.visresy; i++) {
	for (j = 0; j < param.visresx; j++) {
	  fprintf(stderr,"%f ", uloc[i * param.visresx + j]);
	}
	fprintf(stderr,"\n");
      }
      fprintf(stderr,"\n\n");
    }
  }
  MPI_Barrier(comm_2d);
  //---DEBUG ONLY
  if (param.col==1&&param.row==1)
    {
      if(param.act_res * param.act_res < 200) {
	fprintf(stderr,"\np%d: my coarsed partial array is\n",myid);
	for (i = 0; i < param.visresy; i++) {
	  for (j = 0; j < param.visresx; j++) {
	    fprintf(stderr,"%f ", uloc[i * param.visresx + j]);
	  }
	  fprintf(stderr,"\n");
	}
	fprintf(stderr,"\n\n");
      }
	}
  MPI_Barrier(comm_2d);
  //---DEBUG ONLY
  if (myid==root)
    {
      if(param.act_res * param.act_res < 200) {
	fprintf(stderr,"\np%d: my coarsed partial array is\n",myid);
	for (i = 0; i < param.visresgloby; i++) {
	  for (j = 0; j < param.visresglobx; j++) {
	    fprintf(stderr,"%f ", param.uvis[i * param.visresglobx + j]);
	  }
	  fprintf(stderr,"\n");
	}
	fprintf(stderr,"\n\n");
      }
	}
  MPI_Barrier(comm_2d);
  
  
  
  if(myid == root)
    {
      write_image(resfile, param.uvis, param.visresglobx, param.visresgloby);
    }
  free (displs);
  free (counts);
  free (uloc);
  if (myid==root)
    finalize(&param, myid);
  MPI_Finalize();
  
  return 0;
}
