//
// input.c
// 

#include <stdio.h>
#include <stdlib.h>

#include "input.h"

#define BUFSIZE 100

int read_input( FILE *infile, algoparam_t *param )
{
  int i, n;
  char buf[BUFSIZE];

  fgets(buf, BUFSIZE, infile);
  n = sscanf( buf, "%u", &(param->maxiter) );
  if( n!=1 )
    return 0;

  fgets(buf, BUFSIZE, infile);
  n = sscanf( buf, "%u", &(param->initial_res) );
  if( n!=1 )
    return 0;

  fgets(buf, BUFSIZE, infile);
  n = sscanf( buf, "%u", &(param->max_res) );
  if( n!=1 )
    return 0;

  fgets(buf, BUFSIZE, infile);
  n = sscanf( buf, "%u", &(param->res_step_size) );
  if( n!=1 )
    return 0;


  fgets(buf, BUFSIZE, infile);
  n = sscanf(buf, "%d", &(param->algorithm) );
  if( n!=1 )
    return 0;

  fgets(buf, BUFSIZE, infile);
  n = sscanf(buf, "%u", &(param->numsrcs) );
  if( n!=1 )
    return 0;

  (param->heatsrcs) = 
    (heatsrc_t*) malloc( sizeof(heatsrc_t) * (param->numsrcs) );
  
  for( i=0; i<param->numsrcs; i++ )
    {
      fgets(buf, BUFSIZE, infile);
      n = sscanf( buf, "%f %f %f %f",
		  &(param->heatsrcs[i].posx),
		  &(param->heatsrcs[i].posy),
		  &(param->heatsrcs[i].range),
		  &(param->heatsrcs[i].temp) );

      if( n!=4 )
	return 0;
    }

  return 1;
}


void print_params( algoparam_t *param )
{
  int i;

  fprintf(stderr, "Resolutions       : (%u, %u, ... %u)\n",
	  param->initial_res,
	  param->initial_res + param->res_step_size,
	  param->max_res);
  fprintf(stderr, "Iterations        : %u\n", param->maxiter);
  fprintf(stderr, "Algorithm         : %d (%s)\n",
	  param->algorithm,
	  (param->algorithm == 0) ? "Jacobi":"Gauss-Jacobi" );
  fprintf(stderr, "Num. Heat sources : %u\n", param->numsrcs);

  for( i=0; i<param->numsrcs; i++ )
    {
      fprintf(stderr, "  %2d: (%2.2f, %2.2f) %2.2f %2.2f \n",
	     i+1,
	     param->heatsrcs[i].posx,
	     param->heatsrcs[i].posy,
	     param->heatsrcs[i].range,
	     param->heatsrcs[i].temp );
    }
}
