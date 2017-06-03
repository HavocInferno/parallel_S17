#include <stdio.h>
#include "mpi.h"
#include <math.h>


int main (int argc, char** argv)
{
  int ctr, retval, np, myid, arraysize;
  int sum=0;
  MPI_Status status;
  if (argc<2)
    {
      fprintf(stderr, "Usage: %s arraysize\n", argv[0]);
      return 1;
    }
  int totalsize=atoi(argv[1]);
  if ((retval=MPI_Init (&argc, &argv))!=MPI_SUCCESS)
    fprintf(stderr, "Error initializing MPI, Errorcode %i", retval);
  MPI_Comm_size(MPI_COMM_WORLD, &np);
  MPI_Comm_rank(MPI_COMM_WORLD, &myid);
  MPI_Errhandler_set(MPI_COMM_WORLD,MPI_ERRORS_RETURN);
  int buf=0;


  if (totalsize%np==0)
    {
      arraysize=totalsize/np;
    }
  else if (myid+1<np)
    {
      arraysize=(totalsize/np)+1;
    }
  else 
    {
      arraysize=totalsize%((totalsize/np)+1);
    }


    
  
  int* array = malloc(arraysize*sizeof(int));
  //initialize arrays
  printf("Process %i reports arraysize %i \n", myid, arraysize);
  if ((myid<np-1)||totalsize%np==0)
    {
      for (ctr=0; ctr<arraysize; ctr++)
	{
	  array[ctr]=(myid*arraysize)+ctr;
	}
    }
  else 
    {
      for (ctr=0; ctr<(arraysize); ctr++)
	{
	  array[ctr]=(myid*(arraysize+1))+ctr;
	}
    }
  printf("Process %i reports minelem %i maxelem %i\n", myid, *array, array[arraysize-1]);
  //calc local sum
  for (ctr=0; ctr<arraysize; ctr++)
    {
      sum+=array[ctr];
    }
  if (myid==1)
    printf("Process %i still alive, has sum %i\n", myid,sum);
  //printf("Number of runs %i\n", runs);
  ctr=0;
  while(1)
    {
      //MPI_Barrier(MPI_COMM_WORLD);
      //process recvs
      if ((myid%(pow2(ctr+1)))==0)
	{
	  retval=MPI_Recv(&buf, 1, MPI_INT,  myid+pow2(ctr),MPI_ANY_TAG, MPI_COMM_WORLD, &status);
	  //allows process0 to break out of loop
	  if (retval!=MPI_SUCCESS)
	    {
	      break;
	    }
	  printf("Process %i recvs %i from %i\n", myid, buf ,myid+pow2(ctr));
	  sum+=buf;
	}
      //process sends      
      else
	{
	  MPI_Send(&sum, 1, MPI_INT,  myid-pow2(ctr),0, MPI_COMM_WORLD);
	  if (myid==1)
	    printf("Process %i still alive\n", myid);
	  printf("Process %i sends %i to %i\n", myid, sum,myid-pow2(ctr));
	  break;
	} 

      ctr++;
    }
  //MPI_Barrier(MPI_COMM_WORLD);
  if (myid==0)
    {
      printf("Reduction sum: %i. Correct value is %d.\n", sum, totalsize*(totalsize-1)/2);
    }
  free (array);
  array=0;
  printf("Process %d reached end\n", myid);
}
int pow2 (int exp)
{ 
  int result=1;
  int ctr=0;
  for (ctr;ctr<exp;ctr++)
    {
      result*=2;
    }
  return result;
}
