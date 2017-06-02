GCC = gcc
CFLAGS = -O3
MPICC = mpicc

all: pingpong.c
	$(MPICC) $(CFLAGS)  -o pingpong $<

clean:
	rm -f *.o pingpong *~


remake: clean all
