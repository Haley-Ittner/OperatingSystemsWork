#include <iostream>

#include <unistd.h>

#include <sys/wait.h>

#include <assert.h>

#include <signal.h>

#include <string.h>

#include <stdio.h>

#include <stdlib.h>

#include <errno.h>
 
void handler(int);


//using namespace std;



void handler(int signum)

{

	const char *name;

	

	switch(signum)

	{

		case SIGUSR1:	    

			name = "A SIGUSR1 signal was recieved\n";

			write(1, name, strlen(name));

			break;

		case SIGFPE:

			name = "A SIGFPE signal was recieved\n";

			write(1, name, strlen(name));

			break;

		case SIGSYS:

			name = "A SIGSYS signal was recieved\n";

			write(1, name, strlen(name));

			break;

		default:

			name = "An unknown signal was caught";

			write(1, name, strlen(name));

	}

}	



int main(void)

{

    struct sigaction *action = new (struct sigaction);
    action->sa_handler = handler;
    sigemptyset (&(action->sa_mask));
    assert (sigaction (SIGUSR1, action, NULL) == 0);
    assert (sigaction (SIGSYS, action, NULL) == 0);
    assert (sigaction (SIGFPE, action, NULL) == 0);
 
	int sholder;

	const char *s;	

	int hold;

	int i = fork();

	//printf("Printing i: %d\n", i);

	

	if (i == 0)

	{
        sleep(1);
        
		s = "Sending SIGUSR1\n";

		write(1, s, strlen(s));

		kill(getppid(), SIGUSR1);

		//sleep(3);

		s = "Sending SIGFPE\n";

		write(1, s, strlen(s));

		kill(getppid(), SIGFPE);	

		//sleep(3);

		s = "Sending SIGSYS 3 times\n";

		write(1, s, strlen(s));

		kill(getppid(), SIGSYS);

		kill(getppid(), SIGSYS);

		kill(getppid(), SIGSYS);

	} 

	else

	{

	/*	s = "Got to the parent code!\n";

		write(1, s, strlen(s)); */

		do

		{

			//printf("Waiting for the child with the id of %d\n", i);

			hold = waitpid(i, &sholder, 0);

			//printf("hold value %d\n", hold);

		} while(!WIFEXITED(sholder));

	

		if (hold == -1)	

		{

			s = "Error occured not caused by signals";

			write(1,s, strlen(s));

			return -1;

		}

	}

	//free(sholder);

	delete(action);



	return 0;

}




