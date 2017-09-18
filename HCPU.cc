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

/*
This program does the following.
1) Create handlers for two signals.
2) Create an idle process which will be executed when there is nothing
   else to do.
3) Create a send_signals process that sends a SIGALRM every so often

When run, it should produce the following output (approximately):

$ ./a.out
in CPU.cc at 247 main pid = 26428
state:    1
name:     IDLE
pid:      26430
ppid:     0
slices:   0
switches: 0
started:  0
in CPU.cc at 100 at beginning of send_signals getpid () = 26429
in CPU.cc at 216 idle getpid () = 26430
in CPU.cc at 222 going to sleep
in CPU.cc at 106 sending signal = 14
in CPU.cc at 107 to pid = 26428
in CPU.cc at 148 stopped running->pid = 26430
in CPU.cc at 155 continuing tocont->pid = 26430
in CPU.cc at 106 sending signal = 14
in CPU.cc at 107 to pid = 26428
in CPU.cc at 148 stopped running->pid = 26430
in CPU.cc at 155 continuing tocont->pid = 26430
in CPU.cc at 106 sending signal = 14
in CPU.cc at 107 to pid = 26428
in CPU.cc at 115 at end of send_signals
Terminated
---------------------------------------------------------------------------
Add the following functionality.
1) Change the NUM_SECONDS to 20.

2) Take any number of arguments for executables, and place each on new_list.
    The executable will not require arguments themselves.

3) When a SIGALRM arrives, scheduler() will be called. It calls
    choose_process which currently always returns the idle process. Do the
    following.
    a) Update the PCB for the process that was interrupted including the
        number of context switches and interrupts it had, and changing its
        state from RUNNING to READY.
    b) If there are any processes on the new_list, do the following.
        i) Take the one off the new_list and put it on the processes list.
        ii) Change its state to RUNNING, and fork() and execl() it.
    c) Modify choose_process to round robin the processes in the processes
        queue that are READY. If no process is READY in the queue, execute
        the idle process.

4) When a SIGCHLD arrives notifying that a child has exited, process_done() is
    called. process_done() currently only prints out the PID and the status.
    a) Add the printing of the information in the PCB including the number
        of times it was interrupted, the number of times it was context
        switched (this may be fewer than the interrupts if a process
        becomes the only non-idle process in the ready queue), and the total
        system time the process took.
    b) Change the state to TERMINATED.
    c) Start the idle process to use the rest of the time slice.
*/

#define NUM_SECONDS 20

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

/*
** a signal handler for those signals delivered to this process, but
** not already handled.
*/
void grab (int signum) { dprint (signum); }

// c++decl> declare ISV as array 32 of pointer to function (int) returning
// void
void (*ISV[32])(int) = {
/*        00    01    02    03    04    05    06    07    08    09 */
/*  0 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 10 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 20 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 30 */ grab, grab
};

struct PCB
{
    STATE state;
    const char *name;   // name of the executable
    int pid;            // process id from fork();
    int ppid;           // parent process id
    int interrupts;     // number of times interrupted
    int switches;       // may be < interrupts
    int started;        // the time this process started
};

/*
** an overloaded output operator that prints a PCB
*/
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

PCB *running;
PCB *idle;

// http://www.cplusplus.com/reference/list/list/
list<PCB *> new_list;
list<PCB *> processes;

int sys_time;

/*
**  send signal to process pid every interval for number of times.
*/
void send_signals (int signal, int pid, int interval, int number)
{
    dprintt ("at beginning of send_signals", getpid ());

    for (int i = 1; i <= number; i++)
    {
        sleep (interval);

        dprintt ("sending", signal);
        dprintt ("to", pid);

        if (kill (pid, signal) == -1)
        {
            perror ("kill");
            return;
        }
    }
    dmess ("at end of send_signals");
}

struct sigaction *create_handler (int signum, void (*handler)(int))
{
    struct sigaction *action = new (struct sigaction);

    action->sa_handler = handler;
/*
**  SA_NOCLDSTOP
**  If  signum  is  SIGCHLD, do not receive notification when
**  child processes stop (i.e., when child processes  receive
**  one of SIGSTOP, SIGTSTP, SIGTTIN or SIGTTOU).
*/
    if (signum == SIGCHLD)
    {
        action->sa_flags = SA_NOCLDSTOP;
    }
    else
    {
        action->sa_flags = 0;
    }

    sigemptyset (&(action->sa_mask));

    assert (sigaction (signum, action, NULL) == 0);
    return (action);
}

PCB* choose_process ()
{
    int f;
    int p = getpid();
    //cout <<"Number of interrupts before increase: " <<running->interrupts <<endl;
    running->interrupts++;
    //cout <<"Running was interrupted! Times: " <<running->interrupts <<endl;
    //running -> switches++;
    running->state = READY;
    if (running != idle)
    {
	//This code was gotten from https://stackoverflow.com/questions/24139428/check-if-element-is-in-the-list-contains
	if (std::find(std::begin(processes), std::end(processes), running) != std::end(processes))
	{
	} else {
	    processes.push_back(running);
	}

	//cout <<"Running was not idle and not already in the queue!" <<endl;
    }
    //cout <<"Size of processes after pushing running onto back: " << processes.size() <<endl;
    if (new_list.size() != 0) 
    {
	PCB* hold = new (PCB); 
	hold = new_list.front();
	new_list.pop_front();
	//cout <<"Size of new_list after pop: " <<new_list.size() <<endl;
	processes.push_back(hold);
	//cout <<"Size of process after pushing new process on: " <<processes.size() <<endl;
	hold -> state = RUNNING;
	cout <<hold;
	f = fork();
	int status;
	if (f == 0) 
	{
	   hold->started = sys_time;
	   hold->state = READY;
	   execl(hold -> name, hold -> name, NULL);
	   perror("Execl");
	} /*else {
	   waitpid(f, &status, 0);
	}	*/       	
	//running = hold;
	hold->pid = f;
	hold->ppid = p;
	hold->state = READY;
	//cout <<running;
    }
    PCB* temp = new (PCB);
    for (int x = 0; x < processes.size(); x++)
    {
	temp = processes.front();
	if (temp->state == READY)
	{
	   if (temp->pid != running->pid)
	   {
	       running->switches++;
	   }
	   processes.push_back(temp);
	   //cout <<"Size of processes after pushing on new processes" <<processes.size() <<endl;
	   processes.pop_front();
	   //cout <<"Size of processes after popping off front: " <<processes.size() <<endl;
	   running = temp;
	   //cout <<"Processes with switch" <<endl <<processes <<"end" <<endl;
	   return running;
	} else {
           processes.push_back(temp);
	   processes.pop_front();
	   //cout <<"Processes without switch"<< endl <<processes <<"end" <<endl;
	}
    }
    cout<<"Returning idle!"<< endl;
    return idle;
}

void scheduler (int signum)
{
    assert (signum == SIGALRM);
    sys_time++;

    PCB* tocont = choose_process();

    dprintt ("continuing", tocont->pid);
    if (kill (tocont->pid, SIGCONT) == -1)
    {
        perror ("kill");
        return;
    }
}

void process_done (int signum)
{
    assert (signum == SIGCHLD);

    int status, cpid;

    cpid = waitpid (-1, &status, WNOHANG);

    dprintt ("in process_done", cpid);
    cout << "Number of times interrupted: " <<running->interrupts << endl;
    cout <<"Number of context switches: " <<running->switches <<endl;
    int tot = sys_time - running->started;
    cout <<"Total time running: " <<tot <<endl;
    running->state = TERMINATED;
    running = idle;

    if  (cpid == -1)
    {
        perror ("waitpid");
    }
    else if (cpid == 0)
    {
        if (errno == EINTR) { return; }
        perror ("no children");
    }
    else
    {
        dprint (WEXITSTATUS (status));
    }
}

/*
** stop the running process and index into the ISV to call the ISR
*/
void ISR (int signum)
{
    if (kill (running->pid, SIGSTOP) == -1)
    {
        perror ("kill");
        return;
    }
    dprintt ("stopped", running->pid);

    ISV[signum](signum);
}

/*
** set up the "hardware"
*/
void boot (int pid)
{
    ISV[SIGALRM] = scheduler;       create_handler (SIGALRM, ISR);
    ISV[SIGCHLD] = process_done;    create_handler (SIGCHLD, ISR);

    // start up clock interrupt
    int ret;
    if ((ret = fork ()) == 0)
    {
        // signal this process once a second for three times
        send_signals (SIGALRM, pid, 1, NUM_SECONDS);

        // once that's done, really kill everything...
        kill (0, SIGTERM);
    }

    if (ret < 0)
    {
        perror ("fork");
    }
}

void create_idle ()
{
    int idlepid;

    if ((idlepid = fork ()) == 0)
    {
        dprintt ("idle", getpid ());

        // the pause might be interrupted, so we need to
        // repeat it forever.
        for (;;)
        {
            dmess ("going to sleep");
            pause ();
            if (errno == EINTR)
            {
                dmess ("waking up");
                continue;
            }
            perror ("pause");
        }
    }
    idle = new (PCB);
    idle->state = RUNNING;
    idle->name = "IDLE";
    idle->pid = idlepid;
    idle->ppid = getpid();
    idle->interrupts = 0;
    idle->switches = 0;
    idle->started = sys_time;
}

int main (int argc, char **argv)
{
    int pid = getpid();
    dprintt ("main", pid);

    sys_time = 0;

    for(int i = 1; i < argc; i++) {
	PCB* add = new (PCB);
	add -> state = NEW;
	add -> name = argv[i];
	add -> ppid = 0;
	add -> interrupts = 0;
	add -> switches = 0;
	new_list.push_back(add);
	cout <<"Size of new_list: " <<new_list.size() <<endl;
    }

    boot (pid);

    // create a process to soak up cycles
    create_idle ();
    running = idle;

    cout << running;

    // we keep this process around so that the children don't die and
    // to keep the IRQs in place.
    for (;;)
    {
        pause();
        if (errno == EINTR) { continue; }
        perror ("pause");
    }
}
