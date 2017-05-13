# Intel compiler
CC =  icc
CFLAGS = -g

MPICC = mpicc

all: heat

heat : heat.o input.o misc.o timing.o relax_gauss.o relax_jacobi.o
	$(CC) $(CFLAGS) -o $@ $+ -lm $(PAPI_LIB)

%.o : %.c %.h
	$(CC) $(CFLAGS) $(PAPI_INC) -c -o $@ $<

clean:
	rm -f *.o heat *~ *.ppm

remake : clean all
