#include <stdio.h>
#include "mpi.h"
#include <math.h>

#define RUNS 1000


void init (char* arr, int size);

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
  MPI_Status status;    
  
  if (atoi(argv[1])==0)
    {
      //code for latency
      printf("Measuring latency\n\n");
      uint32_t send=0;
      uint32_t recv;
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
	  end=MPI_Wtime();
	  printf("starttime: %f\nendtime %f\nduration %f usec\n", start, end, 1000000*(((end-start)/(2*RUNS))));
	  
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
    }
  else if (atoi(argv[1])==1)
    {
      //measure bandwidth
      printf("Measuring bandwidth\n\nMSGSize [kb] Bandwidth [MB/sec]\n");
      unsigned size = 0;
      for (size;size<=20;size++)
	{
	  //different data sizes
	  int length= pow(2,size);
	  char* src = malloc(length* sizeof(char));
	  char* dst = malloc(length* sizeof(char));
	  int ctr=0;
	  init(src, length);
	  init(dst, length);
	  MPI_Barrier(MPI_COMM_WORLD);
	  start=MPI_Wtime ();
	  if (myid==0)
	    {
	      //loop for main thread
	      for (ctr=0; ctr<RUNS; ctr++)
		{
		  MPI_Send(src, length, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
		  MPI_Recv(dst, length, MPI_BYTE, 1, 1, MPI_COMM_WORLD, &status);
		}
	      end=MPI_Wtime();
	      printf("%4.4f %f\n", 
		     (double)(length*sizeof(char))/1024.0,
		     (sizeof(char)*length*RUNS*2)/(pow(2,20)*(end-start))
		     );
	      
	    }
	  else if (myid==1)
	    {
	      //loop for second thread
	      for (ctr=0; ctr<RUNS; ctr++)
		{ 
		  MPI_Recv(dst, length, MPI_BYTE, 0, 0, MPI_COMM_WORLD, &status);
		  MPI_Send(src, length, MPI_BYTE, 0, 1, MPI_COMM_WORLD); 
		}
	    }
	  free(src);
	  free(dst);
	  src=0;
	  dst=0;
	}
    }
  

  MPI_Finalize();
}
void init (char* arr, int size)
{
  int ctr=0;
  for (ctr; ctr<size;ctr++)
    {
      arr[ctr]=ctr;
    }
}
