/*
 * heat.h
 *
 * Iterative solver for heat distribution
 */


#include <stdio.h>
#include <stdlib.h>

#include "input.h"
#include "heat.h"
#include "timing.h"

#include <papi.h>


void usage( char *s )
{
    fprintf(stderr, 
	    "Usage: %s <input file> [result file]\n\n", s);
}


int main( int argc, char *argv[] )
{
    unsigned iter;
    FILE *infile, *resfile;
    char *resfilename;

    // algorithmic parameters
    algoparam_t param;
    int np;

    double runtime, flop;
    double residual;




    // check arguments
    if( argc < 2 )
    {
	usage( argv[0] );
	return 1;
    }

    // check input file
    if( !(infile=fopen(argv[1], "r"))  ) 
    {
	fprintf(stderr, 
		"\nError: Cannot open \"%s\" for reading.\n\n", argv[1]);
      
	usage(argv[0]);
	return 1;
    }
  

    // check result file
    resfilename= (argc>=3) ? argv[2]:"heat.ppm";

    if( !(resfile=fopen(resfilename, "w")) )
    {
	fprintf(stderr, 
		"\nError: Cannot open \"%s\" for writing.\n\n", 
		resfilename);
      
	usage(argv[0]);
	return 1;
    }

    // check input
    if( !read_input(infile, &param) )
    {
	fprintf(stderr, "\nError: Error parsing input file.\n\n");
      
	usage(argv[0]);
	return 1;
    }

    print_params(&param);

    // set the visualization resolution
    param.visres = param.max_res;

    param.u     = 0;
    param.uhelp = 0;
    param.uvis  = 0;

    param.act_res = param.initial_res;
    int retval;
    // loop over different resolutions
    while(1) {
      
	// free allocated memory of previous experiment
        if (param.u != 0)
	    finalize(&param);

	if( !initialize(&param) )
	{
	    fprintf(stderr, "Error in Jacobi initialization.\n\n");

	    usage(argv[0]);
	}

	fprintf(stderr, "Resolution: %5u\r", param.act_res);

	// full size (param.act_res are only the inner points)
	np = param.act_res + 2;
    
	// starting time
	runtime = wtime();
	residual = 999999999;

	iter = 0;
	/* initialize papi
	 */
	//init PAPI Library
	//fprintf(stderr, "Optimal length %d\n", PAPI_num_counters()); //Debug
	//PAPI_library_init(PAPI_VER_CURRENT);
	int num_events = 3; //must be adjusted to actual num
	long long values [num_events];
	int events []= {PAPI_TOT_CYC, PAPI_TOT_INS,PAPI_FP_OPS};//,  PAPI_L2_TCA, PAPI_L2_TCM, PAPI_L3_TCA, PAPI_L3_TCM};
	//check events, needs PAPi_lib
	/*int a;
	for (a=0; a<3; a++)
	  {
	    if ((PAPI_query_event(events[a]))!=PAPI_OK)
	      fprintf(stderr, "Error with Papi event at Pos %d\n", a);
	  }
	*/
	if ((retval=(PAPI_start_counters(events, num_events)))!=PAPI_OK)
	  fprintf(stderr, "Error starting PAPI counters: Returns %i \n", retval);
	
	while(1) {
	  

	    switch( param.algorithm ) {

		case 0: // JACOBI
      
		    relax_jacobi(param.u, param.uhelp, np, np);
		    residual = residual_jacobi( param.u, np, np);
		    break;

		case 1: // GAUSS

		    relax_gauss(param.u, np, np);
		    residual = residual_gauss( param.u, param.uhelp, np, np);
		    break;
	    }
	    
	    iter++;

	    // solution good enough ?
	    if (residual < 0.000005) break;

	    // max. iteration reached ? (no limit with maxiter=0)
	    if (param.maxiter>0 && iter>=param.maxiter) break;
	    
	    if (iter % 100 == 0)
		fprintf(stderr, "residual %f, %d iterations\n", residual, iter);
	}
	if (((PAPI_stop_counters(values, num_events)))!=PAPI_OK)
	  fprintf(stderr, "Error stopping PAPI Counters: Returns %d\n", retval);
	fprintf(stderr, "Val 1: %lld\nVal 2: %lld\nVal 3: %lld\n", values[0], values[1],values[2]);


	// Flop count after <i> iterations
	flop = iter * 11.0 * param.act_res * param.act_res;
	// stopping time
	runtime = wtime() - runtime;

	fprintf(stderr, "Resolution: %5u, ", param.act_res);
	fprintf(stderr, "Time: %04.3f ", runtime);
	fprintf(stderr, "(%3.3f GFlop => %6.2f MFlop/s, ", 
		flop/1000000000.0,
		flop/runtime/1000000);
	fprintf(stderr, "residual %f, %d iterations)\n", residual, iter);

	// for plot...
	printf("%5d %f\n", param.act_res, flop/runtime/1000000);

	if (param.act_res + param.res_step_size > param.max_res) break;
	param.act_res += param.res_step_size;
    }

    coarsen( param.u, np, np,
	     param.uvis, param.visres+2, param.visres+2 );
  
    write_image( resfile, param.uvis,  
		 param.visres+2, 
		 param.visres+2 );

    finalize( &param );

    return 0;
}
