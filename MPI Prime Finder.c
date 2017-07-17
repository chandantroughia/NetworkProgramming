//  prime.c
//
//
//  Created by Chandan Troughia and Sidharth Prabhakaran on 4/17/17.
//  ******************************************************
//  || Details:  Name                    RCS Id         ||
//  ||           Chandan Troughia        trougc@rpi.edu ||
//  ||           Sidharth Prabhakaran    prabhs2@rpi.edu||
//  ******************************************************
//
//  MPI Prime Finder
//
//
//

#include <mpi.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>

int end_now = 0;
//unsigned int count = 0;
void sig_handler(int signo)
{
    if (signo == SIGUSR1) {
        end_now = 1;
    }
    //printf("time\n");
}

int main(int argc, char **argv)

{
     
    
    //int iCount = 0;
	int ranks=0;int id=0;

 	MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &ranks);
    MPI_Comm_rank(MPI_COMM_WORLD, &id);
    //MPI_Comm comm;
    signal(SIGUSR1, sig_handler);

    unsigned int primes=0;
    unsigned int max=0;
    unsigned int num2;
    unsigned int i=0;
    unsigned int j=0;
    int flag;
    unsigned int count = 0;
    //unsigned int lastPrime = 0;
    //double start, end=0; //Used for MPI Wtime
    num2=4294967295;
    
    /*
	    Count is 0 for every rank other than 0
	    Algorithm does't take even numbers. 2(special prime) should be counted too.
	    Hence the increment at rank 0.
	*/
    if(id==0){
    	count++;
    }
    int tens=10;
    int inc_tens=10;
    int first=1;
    if(id == 0){
        printf("\t\tN\t\tPrimes\n");
        //To print time
        //printf("\t\tN\t\tPrimes\tTime\n");
    }
    while (i<=num2) {

    	//if(inc_tens>1000000)
	    // printf(" i %d\n",i );
	    /*
	        start value will be set through this each rank will get a unique start value
	        Also, even numbers are omitted in this.
	    */
    	if(first==1){
    		i = 3+(id*2);
    		first=0;
    	}
        
        //MPI Wtime start
        /*if(id==0)
            start=MPI_Wtime();*/
        
        
        /*
          i = i + (ranks*2) ensures each rank gets a unique odd number to test
          e.g. for 2 ranks rank 0 will get 3, 7, 11 and so on.
                and for rank 1 it will be  5, 9, 13 and so on.
            based on number of processes(ranks) this calculation varies accordingly.
        */
        for (; i <= inc_tens; i = i + (ranks*2))
    	{
	        flag = 0;
	        /*
	            omitting even numbers becausse,
	            1. Except for 2 none of other even numbers are prime.
	            2. As even numbers will not divide an odd number equally.
	            
	        */
	        for (j = 3; j*j <= i; j = j+2)
	        {
	        	//if(inc_tens>=1000000)
	        		//printf(" i %d\n",i );
	            if ((i % j) == 0)
	            {
	                flag = 1;
	                break;
	            }
	            if (end_now == 1) {
	            	//printf("break innermost\n");
            		break;
        		}
	        }

	        if (flag == 0)
	        {
	            //printf("rank:%d %d\n",id, i);
	            count++;
                //lastPrime = i;

	        }
	        if (end_now == 1) {
	            	//printf("break INTER\n");

            	break;
        	}

	        
    	}
        
        
        //MPI Wtime end
        /*
        if(id==0)
            end=MPI_Wtime();*/
        
        
        
    
        /*
            MPI_Reduce is an effective way to reduce the operation across processes.
            MPI_SUM will get total number of prime numbers across processes(ranks).
            MPI_MAX will find the upper limit to print in the end.
        */
        MPI_Reduce ( &count, &primes, 1, MPI_INT, MPI_SUM, 0,MPI_COMM_WORLD );
        MPI_Reduce ( &i, &max, 1, MPI_INT, MPI_MAX, 0,MPI_COMM_WORLD );
        
        
        
        if (end_now == 1) {
            if(id == 0){
                printf("<Signal Received>\n");
                printf("\t%9d\t%12d\n",max, primes);
                //To print time
                //printf("\t%9d\t%12d\t%f\n",max, primes,end-start );
                
            }
            fflush(stdout);
            	break;
        }

		if(id==0){
			printf("\t%9d\t%12d\n",inc_tens, primes);
            //To print time
            //printf("\t%9d\t%12d\t%f\n",inc_tens, primes,end-start);
            fflush(stdout);
		}
        
		inc_tens=inc_tens*tens;
        
    }
    
    MPI_Finalize();
    return EXIT_SUCCESS;
    
}
