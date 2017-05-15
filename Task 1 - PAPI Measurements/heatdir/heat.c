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

/*
PAPI_MODE_FLOPS = flops measuring mode, 
PAPI_MODE_CACHE = cache measuring mode
 */
#define PAPI_MODE_CACHE 

void usage( char *s )
{
    fprintf(stderr, 
	    "Usage: %s <input file> [result file]\n\n", s);
}
void relax_jacobi_optCA (double *u, double *utmp, unsigned sizex, unsigned sizey);
double residual_jacobi_optCA (double *u, double *utmp, unsigned sizex, unsigned sizey);
void relax_jacobi_optPS (double **u, double **utmp, unsigned sizex, unsigned sizey);
double relax_jacobi_optAIO (double **u, double **utmp, unsigned sizex, unsigned sizey);
double relax_jacobi_optAIO_tiled(double **u, double **utmp, unsigned sizex, unsigned sizey);
void arraySwap(double** a, double** b, int sizex, int sizey);

int main( int argc, char *argv[] )
{
    unsigned iter;
    FILE *infile, *resfile;
    char *resfilename;

    // algorithmic parameters
    algoparam_t param;
    int np, i, j;

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

		/* IMPORTANT - initialize uhelp with the data from u, 
		so we can swap pointers without having to manually 
		fix the border values each iteration */
		for (i = 0; i < np; i++) {
			for (j = 0; j < np; j++) {
				(param.uhelp)[i * np + j] = (param.u)[i * np + j];
			}
		}
		
		/* ----------** PAPI **----------
		 */
		/*
		//apparently there is only a small subset of events which can be measured together. 
		test with papi_command_line event1 event2...
		cf page 18 of the papi documentation linked on moodle
		 */
		#ifdef PAPI_MODE_FLOPS
		/* use for ipc, flips and flops calc */
		float rtime;
		float ptime;
		long long flpops;
		float mflops;
		int err;
		if((err=PAPI_flops(&rtime,&ptime,&flpops,&mflops))<PAPI_OK)
			fprintf(stderr, "%d", err);
		#endif
		#ifdef PAPI_MODE_CACHE
		/* use for cache */
		int num_events = 2; //must be adjusted to actual num
		long long values [num_events];
		//change to PAPI_L2_TC* for L2 rates
		int events []= {       
		PAPI_L2_TCA,
		PAPI_L2_TCM
		};
		if ((retval=(PAPI_start_counters(events, 2)))<PAPI_OK)
			fprintf(stderr, "Error starting PAPI counters: Returns %i \n", retval);
		#endif
		/* ----------** PAPI **----------
		 */
		
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
					
				case 2: // JACOBI_OPT_cacheAligned
					relax_jacobi_optCA(param.u, param.uhelp, np, np);
					residual = residual_jacobi_optCA (param.u, param.uhelp, np, np);
					break;
					
				case 3: // JACOBI_OPT_wPointerSwap
					relax_jacobi_optPS(&(param.u), &(param.uhelp), np, np);
					residual = residual_jacobi_optCA (param.u, param.uhelp, np, np);
					break;
					
				case 4: // JACOBI_OPT_AIO
					//if(iter==0) {
					  //relax_jacobi_optCA(param.u, param.uhelp, np, np);
					  /*
					  for (i = 1; i < np - 1; i++) {
							for (j = 1; j < np - 1; j++) {
								(param.uhelp)[i * np + j] = (param.u)[i * np + j];
							}
							}*/
						//}
					residual = relax_jacobi_optAIO (&(param.u), &(param.uhelp), np, np);
					break;
					
				case 5: // JACOBI_OPT_AIO_tiling
					//if(iter==0) {
					  //relax_jacobi_optCA(param.u, param.uhelp, np, np);
					  /*
					  for (i = 1; i < np - 1; i++) {
							for (j = 1; j < np - 1; j++) {
								(param.uhelp)[i * np + j] = (param.u)[i * np + j];
							}
							}*/
						//}
					residual = relax_jacobi_optAIO_tiled (&(param.u), &(param.uhelp), np, np);
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
		
		/* ----------** PAPI **----------
		 */
		#ifdef PAPI_MODE_FLOPS
		/* output of flips, flops and ipc calc */
		//flop calculcation by PAPI, actually works. prints #flops MFlop/s
		if ((err=PAPI_flops(&rtime,&ptime,&flpops,&mflops))<PAPI_OK)
			fprintf(stderr,"Error reading flops %d", err);
		printf("%lld %f \n", flpops, mflops);
		#endif
		#ifdef PAPI_MODE_CACHE
		/* output of Cache counters */
		if (((PAPI_stop_counters(values, num_events)))<PAPI_OK)
			fprintf(stderr, "Error stopping PAPI Counters: Returns %d\n", retval);
		printf("Acce: %lld\nMiss: %lld\nRate: %2.2f%%\n", 
			values[0], 
			values[1],
			(100.0*values[1])/values[0]
			);
		#endif
		/* ----------** PAPI **----------
		 */
		  

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
		// commented out as assignment 1_5 asks for other plots      
		// for plot...
		//printf("%5d %f\n", param.act_res, flop/runtime/1000000);

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

/*	FIRST OPTIMIZATION
 * One Jacobi iteration step
 */
double residual_jacobi_optCA(double *u, double *utmp, unsigned sizex, unsigned sizey) {
	unsigned i, j;
	double unew, diff, sum = 0.0;
	//ideas for optimization: 	- go through rows instead of columns
	//							- instead of recomputing unew, compare u to utmp -> reuse result from previous relaxation
	//							- vectorization
	for (i = 1; i < sizey - 1; i++) {
		for (j = 1; j < sizex - 1; j++) {
			unew = 0.25 * (u[i * sizex + (j - 1)] +  // left
						u[i * sizex + (j + 1)] +  // right
						u[(i - 1) * sizex + j] +  // top
						u[(i + 1) * sizex + j]); // bottom

			diff = unew - u[i * sizex + j];
			//diff = utmp[i+j*sizey] - u[i+j*sizey];
			sum += diff * diff;
		}
	}

	return sum;
}

void relax_jacobi_optCA(double *u, double *utmp, unsigned sizex, unsigned sizey) {
	int i, j;
	/*
	ideas for optimization: - array padding (fewer conflict misses)
							- go through rows instead of columns
							- manual vectorization
							- loop unrolling
	 */
	for (i = 1; i < sizey - 1; i++) {
		for (j = 1; j < sizex - 1; j++) {
			utmp[i * sizex + j] = 0.25 * (u[i * sizex + (j - 1)] +  // left
						u[i * sizex + (j + 1)] +  // right
						u[(i - 1) * sizex + j] +  // top
						u[(i + 1) * sizex + j]); // bottom
		}
	}
	
	// copy from utmp to u
	// idea for optimization: - instead of copying from utmp to u, just swap the pointers
	arraySwap(&u, &utmp, sizex, sizey);
}

/*	SECOND OPTIMIZATION
 * One Jacobi iteration step, but swapping array pointers instead of copying contents
 * 		
 */
void relax_jacobi_optPS(double **u, double **utmp, unsigned sizex, unsigned sizey) {
	int i, j;
	/*
	ideas for optimization: - array padding (fewer conflict misses)
							- go through rows instead of columns
							- manual vectorization
							- loop unrolling
	 */
	double *a, *atmp;
	a = *u;
	atmp = *utmp;
	
	for (i = 1; i < sizey - 1; i++) {
		for (j = 1; j < sizex - 1; j++) {
			atmp[i * sizex + j] = 0.25 * (a[i * sizex + (j - 1)] +  // left reeeeeeeeeeeeee
						a[i * sizex + (j + 1)] +  // right
						a[(i - 1) * sizex + j] +  // top
						a[(i + 1) * sizex + j]); // bottom
		}
	}
	
	// optimization: instead of copying from utmp to u, just swap the pointers
	double *temp = *u;
	*u = *utmp;
	*utmp = temp;
}

/*	THIRD OPTIMIZATION
 * One Jacobi iteration step plus residual integrated / All in one
 * 		
 */
double relax_jacobi_optAIO(double **u, double **utmp, unsigned sizex, unsigned sizey) {
	int i, j;
	double diff, sum = 0.0;
	/*
	ideas for optimization: - array padding (fewer conflict misses)
							- go through rows instead of columns
							- manual vectorization
							- loop unrolling
	 */ 
	 
	// optimization: instead of copying from utmp to u, just swap the pointers
	/* 
	 *	SWAP 
	 */
	arraySwap(u,utmp, sizex, sizey);
	
	
	/*
	 * RESIDUAL (+ RELAXATION)
	 */
	double *a, *atmp;
	a = *u;
	atmp = *utmp;
	
	for (i = 1; i < sizey - 1; i++) {
		for (j = 1; j < sizex - 1; j++) {
			atmp[i * sizex + j] = 0.25 * (a[i * sizex + (j - 1)] +  // left
						a[i * sizex + (j + 1)] +  // right
						a[(i - 1) * sizex + j] +  // top
						a[(i + 1) * sizex + j]); // bottom
			
			/* Do residual calculation right inside the relaxation loop.
			 * Drawback: technically this returns the residual for timestep t,
			 * while the relaxation advanced to t+1 already, no?
			 */
			diff = atmp[i * sizex + j] - a[i * sizex + j];
			sum += diff * diff;
		}
	}
	
	return sum;
}

/*	FOURTH OPTIMIZATION
 * One Jacobi iteration step plus residual integrated / All in one
 * with tiled computation
 * 		
 */
double relax_jacobi_optAIO_tiled(double **u, double **utmp, unsigned sizex, unsigned sizey) {
	int i, j;
	double diff, sum = 0.0;
	/*
	ideas for optimization: - array padding (fewer conflict misses)
							- go through rows instead of columns
							- manual vectorization
							- loop unrolling
	 */ 
	 
	// optimization: instead of copying from utmp to u, just swap the pointers
	/* 
	 *	SWAP 
	 */
	arraySwap(u,utmp, sizex, sizey);
	
	
	/*
	 * RESIDUAL (+ RELAXATION)
	 */
	double *a, *atmp;
	a = *u;
	atmp = *utmp;
	int tilex, tiley;
	int tilesize = 7998;
	for (tiley = 1; tiley < sizey-1; tiley += tilesize){
		for (tilex = 1; tilex < sizex-1; tilex += tilesize){
			for (i = tiley; (i < sizey - 1) && (i < tiley+tilesize); i++) {
				for (j = tilex; (j < sizex - 1) && (j < tilex+tilesize); j++) {
					atmp[i * sizex + j] = 0.25 * (a[i * sizex + (j - 1)] +  // left
								a[i * sizex + (j + 1)] +  // right
								a[(i - 1) * sizex + j] +  // top
								a[(i + 1) * sizex + j]); // bottom
					
					/* Do residual calculation right inside the relaxation loop.
					 * Drawback: technically this returns the residual for timestep t,
					 * while the relaxation advanced to t+1 already, no?
					 */
					diff = atmp[i * sizex + j] - a[i * sizex + j];
					sum += diff * diff;
				}
			}
		}
	}
	
	return sum;
}

void arraySwap(double** a, double** b, int sizex, int sizey) {
	/*
	int i,j;

	for (i = 1; i < sizey - 1; i++) {
		for (j = 1; j < sizex - 1; j++) {
			(*a)[i * sizex + j] = (*b)[i * sizex + j];
		}
	}
	 */
	
	double *temp = *a;
	*a = *b;
	*b = temp;
}
