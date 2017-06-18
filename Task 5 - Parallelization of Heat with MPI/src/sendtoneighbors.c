		  //TODO: give chunksize and chunkoffset
			residual = relax_jacobi(&(param.u), &(param.uhelp), sizex, sizey);
			MPI_Request dummyRequest;
			if(cords[0]<dim[0]-1)
			{
				//MPI_SEND send bottom row down
				//receive bottom row from down
				int down;
				MPI_Cart_rank(comm_2d, {cords[0]+1,cords[1]}, &down)
				MPI_Isend(&(param.u)+sizex*(sizey-2)+1, sizex-2, MPI_DOUBLE, down,0, MPI_COMM_WORLD, &dummyRequest);
				MPI_Irecv(&(param.u)+sizex*(sizey-1)+1, sizex-2, MPI_DOUBLE, down,0, MPI_COMM_WORLD, &dummyRequest);
			}
			if(cord[0]!=0)
			{
				//MPI_SEND send top row up
				//receive top row from above
				int up;
				MPI_Cart_rank(comm_2d, {cords[0]-1,cords[1]}, &up)
				MPI_Isend(&(param.u)+sizex+1, sizex-2, MPI_DOUBLE, up,1, MPI_COMM_WORLD, &dummyRequest);
				MPI_Irecv(&(param.u)+1, sizex-2, MPI_DOUBLE, up,1, MPI_COMM_WORLD, &dummyRequest);
			}
			if(cords[1]<dim[1]-1)
			{
				//MPI_SEND send right row right
				//receive right row from right
				int right;
				MPI_Cart_rank(comm_2d, {cords[0],cords[1]+1}, &right)
				for (int i = 1; i<sizey)
				{
					MPI_Isend(&(param.u)+sizex*i+sizey-2, 1, MPI_DOUBLE, right,2, MPI_COMM_WORLD, &dummyRequest);
					MPI_Irecv(&(param.u)+sizex*i+sizey-1, 1, MPI_DOUBLE, right,2, MPI_COMM_WORLD, &dummyRequest);
				}
			}
			if(cord[1]!=0)
			{
				//MPI_SEND send left row left
				//receive left row left above
				int left;
				MPI_Cart_rank(comm_2d, {cords[0],cords[1]-1}, &left)
				for (int i = 1; i<sizey)
				{
					MPI_Isend(&(param.u)+sizex*i+1, 1, MPI_DOUBLE, left,3, MPI_COMM_WORLD, &dummyRequest);
					MPI_Irecv(&(param.u)+sizex*i, 1, MPI_DOUBLE, left,3, MPI_COMM_WORLD, &dummyRequest);
				}
			}