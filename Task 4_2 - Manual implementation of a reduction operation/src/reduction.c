#include <stdio.h>
#include "mpi.h"
#include <math.h>
/* Lab Course Efficient Programming of Multicore Processors and Supercomputers
 * Assignment 4.2: Manual Implementation of a reduction operation
 * Group 1: Jonas Mayer, Paul Preißner, Konrad Proell
 */


/* We were not quite sure how to understand that the number of processes should only be known at initialization time. We thought it could mean a) that we can only use MPI_Comm_size at initialization time, but reuse the variable, so
 * that we can not use np==1 as a break condition in our loop, after we updated np during each iteration (we could not do that anyway, because there is no way to synchronize that, and we would have to shut down all processes,
 * which is pretty ugly. However, from this understanding, we would be allowed to use log2(np) as upper bound for the number of iterations, as this only relies on the number of processes at initialization time.
 * We thought it could also mean, that b) we can not use any variable related to np as upper bound for the loop. Default behaviour of our code is a), if we are supposed to use b, add #define solutionb
*/

//#define solutionb

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
  /*This forces MPI to not crash on errors, but return errorcodes, which can be used as breakconditions*/
#ifdef solutionb
  MPI_Errhandler_set(MPI_COMM_WORLD,MPI_ERRORS_RETURN);
#endif
  int buf=0;

  /*calculate the size of the local array*/
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
  //initialize array
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
	  /*only for last array, if totalsize is not divisible by np, the size of the other arrays is higher than the size of the local array. thus size+1*/
	  array[ctr]=(myid*(arraysize+1))+ctr;
	}
    }

  //calc local sum
  for (ctr=0; ctr<arraysize; ctr++)
    {
      sum+=array[ctr];
    }
  ctr=0;
#ifdef solutionb
  /* This implementation uses two separate break conditions: 1. A node has sent its data to the parentnode 2. The node tries to receive data from a node that does not exist
   * Condition 1. terminates processes {1;np}, while process 0 never sends its data and thus does never meet that condition. However, once process 0 has received the data from process
   * (np/2)-1, and thus the reduction is completed, it tries to receive data from process np, which does not exist. Due to the changed Errorhandler setting, this does not crash the program
   * but returns a value != MPI_SUCCESS, which can be used as break condition. However, due to the ugliness of this code, we prefer the default solution.
   */
  while(1)
    {
      //process recvs
      if ((myid%(pow2(ctr+1)))==0)
	{
	  retval=MPI_Recv(&buf, 1, MPI_INT,  myid+pow2(ctr),MPI_ANY_TAG, MPI_COMM_WORLD, &status);
	  //allows process0 to break out of loop
	  if (retval!=MPI_SUCCESS)
	    {
	      printf("Process %i tried to receive data from process %i, does not exist, thus reduction is finished\n", myid, myid+pow2(ctr));
	      break;
	    }
	  printf("Process %i recvs %i from %i\n", myid, buf, myid+pow2(ctr));
	  sum+=buf;
	}
      //process sends      
      else
	{
	  MPI_Send(&sum, 1, MPI_INT, myid-pow2(ctr),0, MPI_COMM_WORLD);
	  printf("Process %i sends %i to %i\n", myid, sum, myid-pow2(ctr));
	  break;
	} 
      ctr++;
    }
#else
  for (ctr=0; ctr<((int) log2(np)); ctr++)
    {
      //process recvs
      if ((myid%(pow2(ctr+1)))==0)
	{
	  retval=MPI_Recv(&buf, 1, MPI_INT,  myid+pow2(ctr),MPI_ANY_TAG, MPI_COMM_WORLD, &status);
	  printf("Process %i recvs %i from %i\n", myid, buf ,myid+pow2(ctr));
	  sum+=buf;
	}
      //process sends      
      else
	{
	  MPI_Send(&sum, 1, MPI_INT,  myid-pow2(ctr),0, MPI_COMM_WORLD);
	  printf("Process %i sends %i to %i\n", myid, sum,myid-pow2(ctr));
	  break;
	} 
    }
#endif
  if (myid==0)
    {
      printf("Reduction sum: %d. Correct value is %d.\n", sum, totalsize*(totalsize-1)/2);
    }
  MPI_Finalize();
  free (array);
  array=0;
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
