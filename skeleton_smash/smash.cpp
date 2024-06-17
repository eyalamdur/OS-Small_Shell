#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"
#include <csignal>
#include <cstring>

int main(int argc, char *argv[]) {
    // Ctrl+C signal
    if (signal(SIGINT, ctrlCHandler) == SIG_ERR) 
        perror("smash error: failed to set ctrl-C handler");

    SmallShell &smash = SmallShell::getInstance();
    while (smash.toProceed()) {
        std::cout << smash.getPrompt() << "> ";
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        smash.executeCommand(cmd_line.c_str());
    }
    return 0;
}