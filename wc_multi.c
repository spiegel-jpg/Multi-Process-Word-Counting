#include "wc.h"
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char **argv)
{
		long fsize;
		FILE *fp;
		count_t count, buf;
		struct timespec begin, end;
		int nChildProc=0;
        int i;int j;
		
		/* 1st arg: filename */
		if(argc < 2) {
				printf("usage: wc <filname> [# processes] [crash rate]\n");
				return 0;
		}
		
		/* 2nd (optional) arg: number of child processes */
		if (argc > 2) {
				nChildProc = atoi(argv[2]);
				if(nChildProc < 1) nChildProc = 1;
				if(nChildProc > 10) nChildProc = 10;
		}

		/* 3rd (optional) arg: crash rate between 0% and 100%. Each child process has that much chance to crash*/
		if(argc > 3) {
				crashRate = atoi(argv[3]);
				if(crashRate < 0) crashRate = 0;
				if(crashRate > 50) crashRate = 50;
				printf("crashRate RATE: %d\n", crashRate);
		}
		
		printf("# of Child Processes: %d\n", nChildProc);
		printf("crashRate RATE: %d\n", crashRate);

		count.linecount = 0;
		count.wordcount = 0;
		count.charcount = 0;

  // start to measure time
		clock_gettime(CLOCK_REALTIME, &begin);

		// Open file in read-only mode
		fp = fopen(argv[1], "r");

		if(fp == NULL)
		{
				printf("File open error: %s\n", argv[1]);
				printf("usage: wc <filname>\n");
				return 0;
		}

		
		// get a file size
		fseek(fp, 0L, SEEK_END);
		fsize = ftell(fp);
		fclose(fp);
		//Calculate offset and size to read for each child
		int a = nChildProc;
		int offset = fsize / a;
		int off[a];
		off[0] = 0;
		for (i=1; i<a; i++){
			off[i]=off[i-1] + offset;
		}
		
		/* word_count() has 3 arguments.
			* 1st: file descriptor
			* 2nd: starting offset
			* 3rd: number of bytes to count from the offset
			*/
		
		//open pipes for all child processes
		int pipes[a][2];
		int pid[a];
		for (i = 0; i < a; i++) {

            if (pipe(pipes[i]) == -1)
            {
                return 3;
            }

			pid[i] = fork(); 
			if (pid[i] == -1)
			{
				printf("Fork failed.\n");
				return 1;
			} 
			else if(pid[i] == 0) //forking only from the main process
			{ 
				close(pipes[i][0]); //close the read end of the pipe
			
				//Child
				printf("Created child process:%d\n", getpid());
				fp = fopen(argv[1], "r");
				if (i==a-1)
				{
					count = word_count(fp, off[a-1], fsize-off[a-1]); //couting the number of words, lines and char
				}
				else
				{
					count = word_count(fp, off[i], off[i+1]-off[i]); //couting the number of words, lines and char
				}

				if ((write(pipes[i][1], &count, sizeof(count))) == -1) //writing to pipe the count
				{ 
					printf("Writing into pipe failed.\n");
					return 4;
				}

				close(pipes[i][1]); //closing the write end of the pipe
				fclose(fp);
				return 0;
			}
		}
		
		//Checking the exitt status of each process and rerunning if crashed.
		int waiting = 1;
		int failure = 0;
		while (waiting==1)
		{

			failure = 0;
			int wstatus;
			for(i=0;i<a;i++)
			{
				waitpid(pid[i] , &wstatus, 0);
				printf("Exit status of the child = %d\n", wstatus);
					if (wstatus != 0)
					{
						// create new child process with same index and rerun
						pid[i] = fork();
						if (pid[i] == -1)
						{
							printf("Fork failed.\n");
							return 1;
						}
						else if(pid[i] == 0) //forking only from the main process
						{
							close(pipes[i][0]);

							//Child
							printf("Created child process:%d\n", getpid());
							fp = fopen(argv[1], "r");
							if (i==a-1)
							{
								count = word_count(fp, off[a-1], fsize-off[a-1]); //couting the number of words, lines and char
							}
							else
							{
								count = word_count(fp, off[i], off[i+1]-off[i]); //couting the number of words, lines and char
							}

							if ((write(pipes[i][1], &count, sizeof(count))) == -1) //writing to pipe the count
							{
								printf("Writing into pipe failed.\n");
								return 4;
							}
							close(pipes[i][1]); //closing the write end of the pipe
							fclose(fp);
							return 0;
						}
						failure = 1;
					}	
			}	
			waiting = failure;	
		}

		for (i=0;i<a;i++)
		{
			wait(NULL);
		}
    
    	for (j=0; j < a; j++) 
		{
			close(pipes[j][1]);
                
        	if ((read(pipes[j][0], &buf, sizeof(buf)) ) ==-1) //reading from the pipe and storing the result to buf
			{ 
            	printf("Reading from pipe failed.\n");
            	exit(1);
        	}
			close(pipes[j][0]);
            	printf("Parent process %d successfully read - (%'d lines, %'d words, %'d chars) \n",getpid(), buf.linecount, buf.wordcount, buf.charcount);

            	count.linecount += buf.linecount;
            	count.wordcount += buf.wordcount;
            	count.charcount += buf.charcount;	
        
    }
		clock_gettime(CLOCK_REALTIME, &end);
		long seconds = end.tv_sec - begin.tv_sec;
		long nanoseconds = end.tv_nsec - begin.tv_nsec;
		double elapsed = seconds + nanoseconds*1e-9;

		printf("\n========= %s =========\n", argv[1]);
		printf("Total Lines : %d \n", count.linecount);
		printf("Total Words : %d \n", count.wordcount);
		printf("Total Characters : %d \n", count.charcount);
		printf("======== Took %.3f seconds ========\n", elapsed);

		return(0);
}