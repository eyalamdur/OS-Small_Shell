#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    
    SmallShell &smash = SmallShell::getInstance();
    JobsList* jobsList = smash.getJobsList();
    
    JobsList::JobEntry* currentJob = jobsList->getJobByPid(getpid());
    if (currentJob != nullptr && currentJob->getProcessID() != getpid()) {
        kill(currentJob->getProcessID(), SIGKILL);
        cout << "smash: process " << currentJob->getProcessID() << " was killed." << endl;
    }
}
