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

#define READ_END 0
#define WRITE_END 1

#define NUM_CHILDREN 5
#define NUM_PIPES NUM_CHILDREN*2

#define P2K i
#define K2P i+1

#define WRITE(a) { const char *foo = a; write (1, foo, strlen (foo)); }

int pipes[NUM_PIPES][2];
int child_count = 0;

// http://man7.org/linux/man-pages/man7/signal-safety.7.html

/*
** Async-safe integer to a string. i is assumed to be positive. The number
** of characters converted is returned; -1 will be returned if bufsize is
** less than one or if the string isn't long enough to hold the entire
** number. Numbers are right justified. The base must be between 2 and 16;
** otherwise the string is filled with spaces and -1 is returned.
*/
int eye2eh (int i, char *buf, int bufsize, int base)
{
    if (bufsize < 1) return (-1);
    buf[bufsize-1] = '\0';
    if (bufsize == 1) return (0);
    if (base < 2 || base > 16)
    {
        for (int j = bufsize-2; j >= 0; j--)
        {
            buf[j] = ' ';
        }
        return (-1);
    }

    int count = 0;
    const char *digits = "0123456789ABCDEF";
    for (int j = bufsize-2; j >= 0; j--)
    {
        if (i == 0)
        {
            buf[j] = ' ';
        }
        else
        {
            buf[j] = digits[i%base];
            i = i/base;
            count++;
        }
    }
    if (i != 0) return (-1);
    return (count);
}

void child_done (int signum)
{
    assert (signum == SIGCHLD);
    WRITE("---- entering child_done\n");

    for (;;)
    {
        int status, cpid;
        cpid = waitpid (-1, &status, WNOHANG);

        if (cpid < 0)
        {
            WRITE("cpid < 0\n");
            kill (0, SIGTERM);
        }
        else if (cpid == 0)
        {
            WRITE("cpid == 0\n");
            break;
        }
        else
        {
            char buf[10];
            assert (eye2eh (cpid, buf, 10, 10) != -1);
            WRITE("process exited:");
            WRITE(buf);
            WRITE("\n");
            child_count++;
            if (child_count == NUM_CHILDREN)
            {
                kill (0, SIGTERM);
            }
        }
    }

    WRITE("---- leaving child_done\n");
}

void process_trap (int signum)
{
    assert (signum == SIGTRAP);
    WRITE("---- entering process_trap\n");

    /*
    ** poll all the pipes as we don't know which process sent the trap, nor
    ** if more than one has arrived.
    */
    for (int i = 0; i < NUM_PIPES; i+=2)
    {
        char buf[1024];
        int num_read = read (pipes[P2K][READ_END], buf, 1023);
        if (num_read > 0)
        {
            buf[num_read] = '\0';
            WRITE("kernel read: ");
            WRITE(buf);
            WRITE("\n");

            // respond
            const char *message = "from the kernel to the process";
            write (pipes[K2P][WRITE_END], message, strlen (message));
        }
    }
    WRITE("---- leaving process_trap\n");
}

int main (int argc, char** argv)
{
    struct sigaction child_action;
    child_action.sa_handler = child_done;
    child_action.sa_flags = 0;
    sigemptyset (&child_action.sa_mask);
    assert (sigaction (SIGCHLD, &child_action, NULL) == 0);
    printf("Doing something!");
    struct sigaction trap_action;
    trap_action.sa_handler = process_trap;
    trap_action.sa_flags = 0;
    sigemptyset (&trap_action.sa_mask);
    assert (sigaction (SIGTRAP, &trap_action, NULL) == 0);

    // create the pipes
    for (int i = 0; i < NUM_PIPES; i+=2)
    {
        // i is from process to kernel, K2P from kernel to process
        assert (pipe (pipes[P2K]) == 0);
        assert (pipe (pipes[K2P]) == 0);

        // make the read end of the kernel pipe non-blocking.
        assert (fcntl (pipes[P2K][READ_END], F_SETFL,
            fcntl(pipes[P2K][READ_END], F_GETFL) | O_NONBLOCK) == 0);
    }

    for (int i = 0; i < NUM_PIPES; i+=2)
    {
        int child;
        if ((child = fork()) == 0)
        {
            close (pipes[P2K][READ_END]);
            close (pipes[K2P][WRITE_END]);

            // assign fildes 3 and 4 to the pipe ends in the child
            dup2 (pipes[P2K][WRITE_END], 3);
            dup2 (pipes[K2P][READ_END], 4);

            execl ("./child", "./child", NULL);
	    printf("execl failed");
        }
    }

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
