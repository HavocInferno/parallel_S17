#include <stdio.h>
#include "mpi.h"

#define RUNS 1000


int main (int argc, char** argv)
{
  int retval, np, myid;
  double start, end;
  if (argc<2)
    {
      fprintf(stderr, "Usage: %s mode [0=latency, 1=bandwidth]\n\n", argv[0]);
      return 1;
    }
  /* Initialize MPI */
  if( (retval=MPI_Init (&argc, &argv))!=MPI_SUCCESS)
    fprintf(stderr, "Error initializing MPI. Errorcode %i\n", retval);
  MPI_Comm_size(MPI_COMM_WORLD, &np);
  MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    
  if (atoi(argv[1])==0)
    {
      //code for latency
      printf("Measuring latency\n\n");
      uint32_t send=0;
      uint32_t recv;
      MPI_Status status;
      int ctr;
      if (retval=(sizeof(send)!=4))
	{
	  fprintf(stderr, "Data does not meet size requirement of 4 byte. It has %i bytes.\n\n", retval);
	  return 1;
	}
      MPI_Barrier(MPI_COMM_WORLD);
      start=MPI_Wtime ();
      
      if (myid==0)
	{
	  //loop for main thread
	  for (ctr=0; ctr<RUNS; ctr++)
	    {
	      MPI_Send(&send, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
	      MPI_Recv(&recv, 1, MPI_INT, 1, 1, MPI_COMM_WORLD, &status);
	    }
	  
	}
      else if (myid==1)
	{
	  //loop for second thread
	  for (ctr=0; ctr<RUNS; ctr++)
	    { 
	      MPI_Recv(&recv, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
	      MPI_Send(&send, 1, MPI_INT, 0, 1, MPI_COMM_WORLD); 
	    }
	}
      if (myid==0)
	{
	  end=MPI_Wtime();
	  printf("starttime: %f\nendtime %f\nduration %f\n", start, end, ((end-start)/RUNS));

	}
    }
  else if (atoi(argv[1])==1)
    {
      //measure bandwidth
      printf("Measuring bandwidth\n\n");

    }
  
  MPI_Finalize();
}
