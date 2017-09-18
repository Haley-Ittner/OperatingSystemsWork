#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>

#define READ 0
#define WRITE 1

#define TO_KERNEL 3
#define FROM_KERNEL 4

int main (int argc, char** argv)
{
    printf("Got into child.cc!");
    int pid = getpid(); // get the pids
    int ppid = getppid();
    sleep(1);

    printf ("writing in pid %d\n", pid);
    const char *message;
    
    for (int x = 0; x < 3; x++)
    {
	if (x == 0)
	{
	    message = "TIME\0";
	}
	else if (x == 1)
	{
	    message = "PROCESSES\0";
	}
	else 
	{
	    message = "SURPRISE\0";
	}
    
    write (TO_KERNEL, message, strlen (message)); // write message into the pipe to the kernel.

    printf ("trapping to %d in pid %d\n", ppid, pid);
    kill (ppid, SIGTRAP); // sending a signal to the parent. 
    sleep(1);
    printf ("reading in pid %d\n", pid);
    char buf[1024]; // create a large buffer. 
    int num_read = read (FROM_KERNEL, buf, 1023); //reading everything sent from kernel to process. 
    buf[num_read] = '\0'; // ending with terminating char.
    printf ("process %d read: %s\n", pid, buf); // print it out!
    sleep(3);
    }
    return 0;
    //exit (0); // then exit cuz were done. 
}
