#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    // TODO: Add your implementation
}

void childProcessHandler(int signum) {
    // Handle the SIGCHLD signal here
    // Extract the job ID
    // Handle the termination of the child process with the given job ID
    //getJobsList()->removeJobById(jobID);
}