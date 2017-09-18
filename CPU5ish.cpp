#include <iostream>
#include <list>
#include <iterator>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <algorithm>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#define NUM_SECONDS 20
// Code from main.cc from Dr. Beaty
#define READ_END 0
#define WRITE_END 1

#define NUM_CHILDREN 5
#define NUM_PIPES NUM_CHILDREN*2

#define P2K i
#define K2P i+1
#define WRITE(a) { const char *foo = a; write (1, foo, strlen (foo)); }

// make sure the asserts work
#undef NDEBUG
#include <assert.h>

#define EBUG
#ifdef EBUG
#   define dmess(a) cout << "in " << __FILE__ << \
    " at " << __LINE__ << " " << a << endl;

#   define dprint(a) cout << "in " << __FILE__ << \
    " at " << __LINE__ << " " << (#a) << " = " << a << endl;

#   define dprintt(a,b) cout << "in " << __FILE__ << \
    " at " << __LINE__ << " " << a << " " << (#b) << " = " \
    << b << endl
#else
#   define dprint(a)
#endif /* EBUG */

using namespace std;

enum STATE { NEW, RUNNING, WAITING, READY, TERMINATED };
int pipes[NUM_PIPES][2];
int child_count = 0;

struct PCB
{
    STATE state;
    const char *name;   // name of the executable
    int pid;            // process id from fork();
    int ppid;           // parent process id
    int interrupts;     // number of times interrupted
    int switches;       // may be < interrupts
    int started;        // the time this process started
    int K2C [2];        // The pipe for the child to kernel	
    int C2K [2];	// The pipe for the kernel to child
    
};

ostream& operator << (ostream &os, struct PCB *pcb)
{
    os << "state:        " << pcb->state << endl;
    os << "name:         " << pcb->name << endl;
    os << "pid:          " << pcb->pid << endl;
    os << "ppid:         " << pcb->ppid << endl;
    os << "interrupts:   " << pcb->interrupts << endl;
    os << "switches:     " << pcb->switches << endl;
    os << "started:      " << pcb->started << endl;
    return (os);
}

/*
** an overloaded output operator that prints a list of PCBs
*/
ostream& operator << (ostream &os, list<PCB *> which)
{
    list<PCB *>::iterator PCB_iter;
    for (PCB_iter = which.begin(); PCB_iter != which.end(); PCB_iter++)
    {
        os << (*PCB_iter);
    }
    return (os);
}

list<PCB *> new_list;
list<PCB *> processes;

int sys_time;

void child_done (int signum)
{
    assert (signum == SIGCHLD); // assert the signal that was recieved was sigchild. 
    WRITE("---- entering child_done\n"); // printing out that we are done with the child process. 

    for (;;) // continue forever.
    {
        int status, cpid;
        cpid = waitpid (-1, &status, WNOHANG); // waiting for all child processes to finished. 

        if (cpid < 0)
        {
            WRITE("cpid < 0\n"); // if there was an error we kill the parent process. 
            kill (0, SIGTERM);
        }
        else if (cpid == 0)
        {
            WRITE("cpid == 0\n"); // if its 0 we write then continue loop. 
            break;
        }
        else
        {
            char buf[64]; // create a buffer of size 10. 
            //assert (eye2eh (cpid, buf, 10, 10) != -1); // assert there was no error in eye2eh
            WRITE("process exited:"); // write that the process exited into pipe. . 
            WRITE(buf); // write the buffer into pipe. 
            WRITE("\n");
            child_count++; // added to the number of children we have
            if (child_count == NUM_CHILDREN) // if we reached the max number of children kill the parent. 
            {
                kill (0, SIGTERM);
            }
        }
    }

    WRITE("---- leaving child_done\n"); // then write were done with the child. 
}

void process_trap (int signum)
{
    // Code modified by main.cc by Dr. Beaty
    assert (signum == SIGTRAP);
    
    WRITE("---- entering process_trap\n"); // write into pipe that were in this process. 

    /*
    ** poll all the pipes as we don't know which process sent the trap, nor
    ** if more than one has arrived.
    */
    for (int i = 0; i < NUM_PIPES; i+=2)
    {
        char buf[1024]; // create a large buffer for all possible inputs. 
        int num_read = read (pipes[P2K][READ_END], buf, 1023); // make sure there is something to read. 							READ_END is is the parent process part of pipe.
 
        if (num_read > 0) // if something was read...
        {
            buf[num_read] = '\0'; // Add the terminating char at end of buffer
            WRITE("kernel read: "); // write to screen what was read
            WRITE(buf);// then what was read
            WRITE("\n");
	    string str(buf);	
	    const char *message;     

	    if (buf == "TIME")
	    {
		message = "The system time is " + sys_time;
	    } 
	    else if (buf == "PROCESSES") 
	    {
		std::string hold;
		hold = "The name of the processes running are ";
	        list<PCB *> copy = new_list;
		for (int x = 0; x < new_list.size(); x++) 
		{
		    hold += copy.front() -> name;
		    copy.pop_front();
		}
		//This code was gotten from http://www.cplusplus.com/forum/general/4422/
		char *a=new char[hold.size()+1];
		a[hold.size()]=0;
		memcpy(a,hold.c_str(),hold.size());
		const char *poof = a;
		message = poof;
	    } 
	    else if (buf == "SURPRISE")
            {
		message = "Ha cha cha!";
	    } 
	    else 
	    {
		message = "Could not find the information you requested";
	    }

            write (pipes[K2P][WRITE_END], message, strlen (message)); // whats written
        }
    }

    WRITE("---- leaving process_trap\n"); // done!    
}

int main (int argc, char** argv)
{
    //cout << "Argc: " << argc << endl;
    struct sigaction child_action; // creating a signaction struct.
    child_action.sa_handler = child_done; // giving the handler as child_done
    child_action.sa_flags = 0; // flags set to 0
    sigemptyset (&child_action.sa_mask); // making the mask the empty set, so accepts all signals
    //cout << sigaction (SIGCHLD, &child_action, NULL) << endl;
    assert (sigaction (SIGCHLD, &child_action, NULL) == 0); // make sure this was sucessful. 

    struct sigaction trap_action; // creating another sigaction struct
    trap_action.sa_handler = process_trap; // making this ones handler the process_trap
    trap_action.sa_flags = 0; // flags set to 0
    sigemptyset (&trap_action.sa_mask); // mask to empty set
    //cout <<sigaction (SIGTRAP, &trap_action, NULL) << endl;
    assert (sigaction (SIGTRAP, &trap_action, NULL) == 0); // making sure it was a sucess.

    // create the pipes
    for (int i = 0; i < (argc * 2) - 2; i+=2)
    {
        // i is from process to kernel, K2P from kernel to process
	//cout << pipe (pipes[P2K]) << endl;
        assert (pipe (pipes[P2K]) == 0); // making sure each pipe was created correctly.
	//cout << pipe (pipes[K2P]) << endl;
        assert (pipe (pipes[K2P]) == 0);

        // make the read end of the kernel pipe non-blocking.
	//cout << fcntl (pipes[P2K][READ_END], F_SETFL,
            //fcntl(pipes[P2K][READ_END], F_GETFL) | O_NONBLOCK) << endl;

        assert (fcntl (pipes[P2K][READ_END], F_SETFL,
            fcntl(pipes[P2K][READ_END], F_GETFL) | O_NONBLOCK) == 0);
    }

    for (int u = 0; u < 10; u++)
    {
	for (int y = 0; y < 2; y++)
	{
	 //   cout <<"Pipes at " << u << " " << y << " : "<< pipes[u][y] << endl;
	}
    }

    int j = 0;

    for(int k = 1; k < argc; k++) {
	PCB* add = new (PCB);
	add -> state = NEW;
	add -> name = argv[k];
	add -> ppid = 0;
	add -> interrupts = 0;
	add -> switches = 0;
	add -> C2K[0] = pipes[j][READ_END];
	add -> C2K[1] = pipes[j][WRITE_END];
	add -> K2C[0] = pipes[j + 1][READ_END];
	add -> K2C[1] = pipes[j + 1][WRITE_END];

	new_list.push_front(add);
	//cout << add;
	//cout <<"Size of new_list: " <<new_list.size() <<endl;
	//cout << "Input gotten from C2K: " << add -> C2K[0] << " and "<< add -> C2K[1] << endl;
	//cout << "Input gotten from K2C: " << add -> K2C[0] << " and "<< add -> K2C[1] << endl;
	j += 2;
    }    
    /*for (int h = 0; h < new_list.size(); h++)
    {
	cout << "got in here!" << endl;
	cout << new_list.front() -> name << endl;
	new_list.pop_front();
    } */

    int c = 0;
    PCB* hold;
    for (int m = 0; m < new_list.size(); m++)
    {
        int child;
        if ((child = fork()) == 0) // make a new child process
        {
	    //printf("CHILD PROCESS!\n");
	    hold = new_list.front();
	    //cout << "Hold: \n" << hold;
	    new_list.pop_front();
            close (pipes[c][READ_END]); // close ends of pipes for read only and write only.
            close (pipes[c + 1][WRITE_END]);

            // assign fildes 3 and 4 to the pipe ends in the child
            dup2 (pipes[c][WRITE_END], 3); // create new ones with a new file 
            dup2 (pipes[c + 1][READ_END], 4);     //descriptor....
	    c += 2;
	    //cout << "Got here!" << endl;
	    //cout << hold -> name << endl;

            execl(hold -> name, hold -> name, NULL); // execl to child process.
	    perror("execl");
        }
    }
    //delete(hold);

    for (;;)
    {
        pause();
        // will be unblocked by signals, but continue to pause
        if (errno == EINTR)
        {
            continue;
        }
    }

    exit (0);

}
