#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"
#include <unistd.h>
 
using namespace std;

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    
    SmallShell &smash = SmallShell::getInstance();

    // Stop watch case
    smash.setStopWatch(true);
    
    // No fg process
    if (smash.getForegroundProcess() == ERROR_VALUE)
        return;
    
    // Kill fg process
    cout << "smash: process " << smash.getForegroundProcess() << " was killed." << endl;
    kill(smash.getForegroundProcess(),SIGKILL);
    
    // Reset to no fg process
    smash.setForegroundProcess(ERROR_VALUE);
}
