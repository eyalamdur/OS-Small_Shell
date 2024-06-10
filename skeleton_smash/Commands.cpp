#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

const string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == string::npos) ? "" : s.substr(start);
}

string _rtrim(const string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    istringstream iss(_trim(string(cmd_line)).c_str());
    for (string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

/*---------------------------------------------------------------------------------------------------*/
/*----------------------------------------- SmallShell Class ----------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/

SmallShell::SmallShell() {
// TODO: add your implementation
}

SmallShell::~SmallShell() {
// TODO: add your implementation
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
  
    if (firstWord.compare("pwd") == 0) 
        return new GetCurrDirCommand(cmd_line);
    else if (firstWord.compare("cd") == 0) 
        return new ChangeDirCommand(cmd_line, getPlastPwdPtr());
  
  /*
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  else if ...
  .....
  else {
    return new ExternalCommand(cmd_line);
  }
  */
  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    Command* cmd = CreateCommand(cmd_line);

    if (cmd != nullptr)
        cmd->execute();
    else
        printToTerminal("Unknown Command");

    // Please note that you must fork smash process for some commands (e.g., external commands....)
}

/* Gets pointer to the last path of working directory */
char** SmallShell::getPlastPwdPtr() {
    return &m_plastPwd;
}

/* Prints give line to terminal */
void SmallShell::printToTerminal(string line){
    cout << line << endl;
}

/*---------------------------------------------------------------------------------------------------*/
/*-------------------------------------- General Command Class --------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/
/* C'tor & D'tor for Command Class*/
Command::Command(const char *cmd_line) : m_cmd_string(cmd_line){}

// Virtual destructor implementation
Command::~Command() {} 

/* Method to count the number of arguments, assuming arguments are space-separated */
int Command::getArgCount() const {
    int count = 0;
    const char* cmd = m_cmd_string; // m_cmd_string holds the command string
    while (*cmd != '\0') {
        if (*cmd != ' ') {
            count++;
            
            while (*cmd != ' ' && *cmd != '\0')     // Skip to the next argument
                cmd++;
        } 
        else
            cmd++;
    }

    return count;
}

/* Method to get the command arguments as a vector */
vector<string> Command::getArgs() const {
    vector<string> arguments;
    string cmdLine(m_cmd_string); //  m_cmd_string holds the command line

    // Tokenize the command line based on spaces
    istringstream iss(cmdLine);
    for (string arg; iss >> arg;) 
        arguments.push_back(arg);

    return arguments;
}

/*---------------------------------------------------------------------------------------------------*/
/*---------------------------------------- Built-in Commands ----------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/
/* C'tor for BuiltInCommand Class*/
BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line){}


/* Constructor implementation for GetCurrDirCommand */
GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

/* Execute method to get and print the current working directory. */
void GetCurrDirCommand::execute() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != nullptr)
        cout << cwd << endl;
}


/* Constructor implementation for ChangeDirCommand */
ChangeDirCommand::ChangeDirCommand(const char *cmd_line, char **plastPwd) : BuiltInCommand(cmd_line), plastPwd(plastPwd) {}

/* Execute method to change the current working directory. */
void ChangeDirCommand::execute() {
    char *newDir = nullptr;

    // 1 argument - go to the given directory
    if (getArgCount() == CD_COMMAND_ARGS_NUM){
        if (getArgs()[1] == "-"){
            if (*plastPwd != nullptr)
                newDir = *plastPwd;
            else
                cerr << "smash error: cd: OLDPWD not set" << endl;
        }
        else if (getArgs()[1] == "..")
            newDir = dirname(getcwd(NULL, 0));
        else
            newDir = strdup(getArgs()[1].c_str());
    }

    // 2 args or more
    else if (getArgCount() > CD_COMMAND_ARGS_NUM){
        cerr << "smash error: cd: too many arguments" << endl;
        return;
    }

    // Change directory using chdir
    *plastPwd = getcwd(NULL, 0);
    chdir(newDir);
}