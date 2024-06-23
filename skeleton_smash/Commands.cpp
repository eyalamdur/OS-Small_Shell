#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <regex>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syscall.h>
#include <fstream>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>


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

/*---------------------------------------------------------------------------------------------------*/
/*---------------------------------------------- Utils ----------------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/

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

bool _isBackgroundCommand(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);

    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);

    // if all characters are spaces / command line does not end with & / empty - then return
    if (idx == string::npos || cmd_line[0] == '\0' || cmd_line[idx] != '&') 
        return;

    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = '\0';
}

string _removeBackgroundSignForString(string cmd){
    // find last character other than spaces
    unsigned int idx = cmd.find_last_not_of(WHITESPACE);
    
    // if the command line does end with & then replace it
    if (cmd[idx] == '&')
        cmd.erase(idx);
    return cmd;
}

/*---------------------------------------------------------------------------------------------------*/
/*----------------------------------------- SmallShell Class ----------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/
const set<string> SmallShell::COMMANDS = {"chprompt", "showpid", "pwd", "cd", "jobs", "fg",
"quit", "kill", "alias", "unalias", "ls", "mkdir", "cp", "mv", "rm", "touch", "cat", "grep",
 "find", "awk", "sed", "sort", "wc", "head", "tail", "chmod", "chown", "ps", "top", "tar",
  "gzip", "unzip", "ssh", "scp", "chmod", "chown", "echo", "basename", "dirname", "type",
   "which", "source", "export", "history", "date", "cal", "man", "pushd", "popd", "bg",
    "nohup", "killall", "ifconfig", "netstat", "ping", "traceroute", "nc", "telnet","lsof",
     "less", "more", "df", "du", "free", "uname", "w", "who", "last", "uptime", "banner", "notify-send", "shutdown", "sleep"
};

SmallShell::SmallShell() : m_fg_process(ERROR_VALUE), m_prompt("smash"),  m_plastPwd(nullptr),  
                        m_jobList(new JobsList()), m_proceed(true), m_stopWatch(false), m_alias(new map<string, string>),
                        m_aliasToPrint(vector<string>()) {}

SmallShell::~SmallShell() {
    if (m_plastPwd)
        free(m_plastPwd);
    delete m_jobList;
    delete m_alias;
}

void SmallShell::setPrompt(const string str) {
    m_prompt = str;
}

string SmallShell::getPrompt() const {
    return m_prompt;
}

void SmallShell::setStopWatch(bool status){
    m_stopWatch = status;
}

bool SmallShell::getStopWatch() const{
    return m_stopWatch;
}

void SmallShell::setForegroundProcess(pid_t pid){
    m_fg_process = pid;
}

pid_t SmallShell::getForegroundProcess() const{
    return m_fg_process;
}

bool SmallShell::toProceed() const {
    return m_proceed;
}

void SmallShell::quit() {
    m_proceed = false;
}

void SmallShell::addAlias(string name, string command, string originCommand) {
    if (m_alias->find(name) == m_alias->end() && COMMANDS.find(name) == COMMANDS.end()){
        (*m_alias)[name] = command;
        m_aliasToPrint.push_back(name);
        int equals = originCommand.find_first_of('='); // position of the '=' sign
        string printCommand = originCommand.substr(equals, originCommand.size()-equals);
        m_aliasToPrint.push_back(printCommand);
    }
    else
        cerr << "smash error: alias: " << name << " already exists or is a reserved command" << endl;
}

void SmallShell::removeAlias(vector<string> args) {
    for (int i = 1; i < (int)args.size(); i++){
        // checking if the arguments are aliases
        auto it = m_alias->find(args[i]);
        //case an argument is an alias
        if (it != m_alias->end()){
            //finding the alias in the printing vector
            auto aliasIt = find(m_aliasToPrint.begin(), m_aliasToPrint.end(), it->first);
            //removing the match 'name', 'command' from the printing vector
            if (aliasIt != m_aliasToPrint.end() && next(aliasIt) != m_aliasToPrint.end()){
                m_aliasToPrint.erase(next(aliasIt));
                m_aliasToPrint.erase(aliasIt);
            }
            //removing the tuple (name,command) from the map
            m_alias->erase(args[i]);
        }
        //case an argument isn't an alias
        else{
            cerr << "smash error: unalias: " << args[i] << " alias does not exist" << endl;
            break;
        }
    }
}

void SmallShell::printAlias() {
    //printing the aliases by the order of appending
    for (int i = 0; i < (int)m_aliasToPrint.size(); i+=2)
        cout << m_aliasToPrint[i] << m_aliasToPrint[i+1] << endl;
}

char* SmallShell::extractCommand(const char* cmd_l,string &firstWord){
    // Make a modifiable copy of the command line and remove & if exists
    char* newCmdLine = strdup(cmd_l);

    _removeBackgroundSign(newCmdLine);

    // Find first word (the command)
    string cmd_s = _trim(string(newCmdLine));
    firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    if(m_alias->find(firstWord) != m_alias->end()){
        string command = m_alias->find(firstWord)->second,  rest = "";
        
        // Extract the rest of the command
        if (firstWord.compare(cmd_s)!=0)
            rest = cmd_s.substr(cmd_s.find_first_of(" \n"),cmd_s.find_first_of('\0'));

        // build new command into newCmdLine & firstWord
        cmd_s = command + rest;
        firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
        copy(cmd_s.begin(), cmd_s.end(), newCmdLine);
    }
    return newCmdLine;
}

/* Creates and returns a pointer to Command class which matches the given command line (cmd_line) */
Command *SmallShell::CreateCommand(const char *cmd_line) {
    string firstWord;
    char* newCmdLine = extractCommand(cmd_line, firstWord);
    //case the alias command is background, need to trim the '&'
    bool isBg = _isBackgroundCommand(newCmdLine);
    _removeBackgroundSign(newCmdLine);
    if (firstWord.compare("alias") == 0)
        return new aliasCommand(cmd_line, newCmdLine);
    else if (string(newCmdLine).find('>') != string::npos)
        return new RedirectionCommand(cmd_line, newCmdLine);
    else if (string(newCmdLine).find('|') != string::npos)
        return new PipeCommand(cmd_line, newCmdLine);
    else if (firstWord.compare("pwd") == 0)
        return new GetCurrDirCommand(cmd_line, newCmdLine);
    else if (firstWord.compare("chprompt") == 0)
        return new ChangePromptCommand(cmd_line, newCmdLine);
    else if (firstWord.compare("showpid") == 0)
        return new ShowPidCommand(cmd_line, newCmdLine);
    else if (firstWord.compare("cd") == 0)
        return new ChangeDirCommand(cmd_line, newCmdLine, getPlastPwdPtr());
    else if (firstWord.compare("quit") == 0)
        return new QuitCommand(cmd_line, newCmdLine, getJobsList());
    else if (firstWord.compare("unalias") == 0)
        return new unaliasCommand(cmd_line, newCmdLine);
    else if (firstWord.compare("jobs") == 0) 
        return new JobsCommand(cmd_line, newCmdLine, getJobsList());
    else if (firstWord.compare("fg") == 0) 
        return new ForegroundCommand(cmd_line, newCmdLine, getJobsList());
    else if (firstWord.compare("kill") == 0) 
        return new KillCommand(cmd_line, newCmdLine, getJobsList());
    else if (firstWord.compare("listdir") == 0) 
        return new ListDirCommand(cmd_line, newCmdLine);
    else if (firstWord.compare("getuser") == 0)
        return new GetUserCommand(cmd_line, newCmdLine);
    else if (firstWord.compare("watch") == 0)
        return new WatchCommand(cmd_line, newCmdLine);
    else
        return new ExternalCommand(cmd_line, newCmdLine, _isBackgroundCommand(cmd_line) || isBg);

  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    Command* cmd = CreateCommand(cmd_line);

    // Remove all finshed background jobs.
    m_jobList->removeFinishedJobs();

    // Invalid Command
    if (cmd == nullptr){
        printToTerminal("Unknown Command");
        return;
    }

 if(cmd->isExternalCommand()){

        // Fork a new process
        pid_t pid = fork();

        if (pid == ERROR_VALUE) {
            perror("smash error: fork failed");
            delete cmd;
            return;
        }

        // Child process
        if (pid == CHILD_ID)
            setpgrp();

        // Parent process
        if (pid > CHILD_ID){
            if(cmd->isBackgroundCommand())
                getJobsList()->addJob(cmd, pid);
            else{
                // Set process in as foreground process
                setForegroundProcess(pid);
                
                int status;
                waitpid(pid, &status,0);
                
                // Set foreground process as empty
                setForegroundProcess(ERROR_VALUE);

                // Free allocated memory from parent process 
                delete cmd;

            }
            return;
        }
    }

    // Execute command
    cmd->execute();
    delete cmd;
}

/* Gets pointer to the last path of working directory */
char* SmallShell::getPlastPwdPtr() {
    return m_plastPwd;
}

/* Gets pointer to the last path of working directory */
void SmallShell::setPlastPwdPtr(char * newPwd) {
    m_plastPwd = newPwd;
}

JobsList* SmallShell::getJobsList(){
    return m_jobList;
}

/* Prints give line to terminal */
void SmallShell::printToTerminal(string line){
    cout << line << endl;
}

/*---------------------------------------------------------------------------------------------------*/
/*-------------------------------------- General Command Class --------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/
/* C'tor & D'tor for Command Class*/
Command::Command(const char *origin_cmd_line, const char *cmd_line, bool isBgCmd) : m_origin_cmd_string(string(origin_cmd_line)),
 m_cmd_string(string(cmd_line)), m_bgCmd(isBgCmd) {
    // Free allocated memory in extract command
    if (cmd_line)
        free(const_cast<char*>(cmd_line));
 }

Command::~Command() {}

/* Method to count the number of arguments, assuming arguments are space-separated */
int Command::getArgCount() const {
    int count = 0;
    const char* cmd = m_cmd_string.c_str(); // m_cmd_string holds the command string
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

string Command::getCommand() const {
    return m_cmd_string;
}

string Command::getOriginalCommand() const {
    return m_origin_cmd_string;
}

void Command::setCommand(string cmd){
    m_cmd_string = cmd;
}

bool Command::isBackgroundCommand() const{
    return m_bgCmd;
}

bool Command::isExternalCommand() const {
    return false; // Default implementation indicates it's not an ExternalCommand
}

/*---------------------------------------------------------------------------------------------------*/
/*---------------------------------------- Built-in Commands ----------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/
/* C'tor for BuiltInCommand Class*/
BuiltInCommand::BuiltInCommand(const char* origin_cmd_line, const char *cmd_line) : Command(origin_cmd_line, cmd_line){}

/* Constructor implementation for GetCurrDirCommand */
GetCurrDirCommand::GetCurrDirCommand(const char* origin_cmd_line, const char *cmd_line) : BuiltInCommand(origin_cmd_line, cmd_line) {}

/* Execute method to get and print the current working directory. */
void GetCurrDirCommand::execute() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != nullptr)
        cout << cwd << endl;
}


/* C'tor for changePromptCommand*/
ChangePromptCommand::ChangePromptCommand(const char* origin_cmd_line, const char *cmd_line) : BuiltInCommand(origin_cmd_line, cmd_line) {}

void ChangePromptCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    string str = (getArgCount() > 1) ? getArgs()[1] : "smash";
    smash.setPrompt(str);
}


/* C'tor for ShowPidCommand Class*/
ShowPidCommand::ShowPidCommand(const char* origin_cmd_line, const char *cmd_line) : BuiltInCommand(origin_cmd_line, cmd_line) {}

void ShowPidCommand::execute() {
    pid_t pid = getpid();
    cout << "smash pid is " << pid << endl;
}


/* C'tor for quit command*/
QuitCommand::QuitCommand(const char* origin_cmd_line, const char *cmd_line, JobsList *jobs) : BuiltInCommand(origin_cmd_line, cmd_line) , m_jobsList(jobs){}

void QuitCommand::execute() {
    if (getArgCount() > 1 && getArgs()[1].compare("kill") == 0){
        // Print a message & joblist before exiting
        if (m_jobsList != nullptr){
            cout << "smash: sending SIGKILL signal to " << m_jobsList->getNumRunningJobs() << " jobs:" << endl;
            m_jobsList->printJobsListWithPid();
        }
        else
            cout << "smash: sending SIGKILL signal to 0 jobs:" << endl;
            
        // Print the list of jobs that were killed
        while(m_jobsList != nullptr && !m_jobsList->isEmpty())
            m_jobsList->killAllJobs();
        
    }
    // End the execution of the shell
    SmallShell::getInstance().quit();
}


/* C'tor for aliasCommamd class */
aliasCommand::aliasCommand(const char* origin_cmd_line, const char *cmd_line) : BuiltInCommand(origin_cmd_line, cmd_line) {
    unsigned int space = 0, equals = 0;
    for (; space < m_cmd_string.length() && m_cmd_string[space]!=' ' ; space++) {}
    equals = space;
    for (; equals < m_cmd_string.length() && m_cmd_string[equals]!='='; equals++) {}

    int length = m_cmd_string.length() - equals - 3;
    char command[length+1];

    for (int i = 0 ; i < length ; i++)
        command[i] = m_cmd_string[equals + 2 + i];

    command[length] = '\0';
    //trimming the command line from '&' and ' '
    int j = 1;
    while (command[length-j] == '\'' || command[length-j] == ' '){
        command[length-j] = '\0';
        j++;
    }

    length = equals - space - 1;

    char name[length + 1];
    for (int i = 0; i < length; i++)
        name[i] = m_cmd_string[space + 1 + i];

    name[length] = '\0';
    m_name = name;
    m_command = command;
}

void aliasCommand::execute() {
    const regex aliasRegex("^alias [a-zA-Z0-9_]+='[^']*'$");
    SmallShell &smash = SmallShell::getInstance();
    // Print alias commands list
    if (getArgCount() == 1)
        smash.printAlias();
    
    // Add new alias command
    else{
        m_cmd_string = _trim(m_cmd_string);
        string first = m_command.substr(0, m_command.find_first_of(" \n"));
        if (regex_match(m_cmd_string, aliasRegex))
            smash.addAlias(m_name, m_command, m_origin_cmd_string);
        else
            cerr << "smash error: alias: invalid alias format" << endl;
    }
}


/* C'tor for unaliasCommand class. */
unaliasCommand::unaliasCommand(const char* origin_cmd_line, const char *cmd_line) : BuiltInCommand(origin_cmd_line, cmd_line){}

void unaliasCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    if (getArgCount() == 1){
        cerr << "smash error: unalias: not enough arguments" << endl;
    }
    else
        smash.removeAlias(getArgs());
}


/* Constructor implementation for ChangeDirCommand */
ChangeDirCommand::ChangeDirCommand(const char* origin_cmd_line, const char *cmd_line, char *plastPwd) : BuiltInCommand(origin_cmd_line, cmd_line), plastPwd(plastPwd) {}

ChangeDirCommand::~ChangeDirCommand(){
    free(plastPwd);
}

/* Execute method to change the current working directory. */
void ChangeDirCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();
    char *newDir = nullptr;
    bool lessArg = false;
    // 1 argument - go to the given directory
    if (getArgCount() == CD_COMMAND_ARGS_NUM){
        if (getArgs()[1] == "-"){
            if (plastPwd != nullptr)
                newDir = plastPwd;
            else
                cerr << "smash error: cd: OLDPWD not set" << endl;
        }
        else if (getArgs()[1] == ".."){
            newDir = dirname(getcwd(NULL, 0));
            lessArg = true;
        }
           
        else{
            newDir = strdup(getArgs()[1].c_str());
            lessArg = true;
        }
    }
    
    // 2 args or more
    else if (getArgCount() > CD_COMMAND_ARGS_NUM){
        cerr << "smash error: cd: too many arguments" << endl;
        return;
    }

    // Change directory using chdir
    if (newDir != nullptr){
        plastPwd = getcwd(nullptr, 0);
        if(chdir(newDir) <= ERROR_VALUE){
            perror("smash error: chdir failed");
        }
        else
            smash.setPlastPwdPtr(strdup(plastPwd));
    }
    if (lessArg)
        free(newDir);

}


JobsCommand::JobsCommand(const char* origin_cmd_line, const char* cmd_line, JobsList* jobs) : BuiltInCommand(origin_cmd_line, cmd_line), m_jobsList(jobs) {}

void JobsCommand::execute() {
    m_jobsList->printJobsList();
}


ForegroundCommand::ForegroundCommand(const char* origin_cmd_line, const char* cmd_line, JobsList* jobs) : BuiltInCommand(origin_cmd_line, cmd_line), m_jobsList(jobs){}

void ForegroundCommand::execute() {  
    SmallShell &smash = SmallShell::getInstance();
    int jobID;
    // No job ID specified, select the job with the maximum job ID
    if (getArgs().size() == 1){
        // Check if the jobs list is empty
        if (m_jobsList->isEmpty()) {
            cerr << "smash error: fg: jobs list is empty" << endl;
            return;
        }
        jobID = m_jobsList->getNextJobID() - 1;
    }
    else if (getArgs().size() == 2) {
        try {
            jobID = stoi(getArgs()[1]);
            if (jobID < 0)
                stoi("a");  // Throw error
            if (m_jobsList->getJobById(jobID) == nullptr) {
                cerr << "smash error: fg: job-id " << jobID << " does not exist" << endl;
                return;
            }
        } 
        catch (const invalid_argument& e) {
            cerr << "smash error: fg: invalid arguments" << endl;
            return;
        }
    } 
    else {
        cerr << "smash error: fg: invalid arguments" << endl;
        return;
    }

    // Get the job, sets him as forground and remove it from the jobs list and prints the requested message
    JobsList::JobEntry* jobEntry = m_jobsList->getJobById(jobID);
    int jobPid = jobEntry->getProcessID();
    cout << jobEntry->getCommand()->getCommand() << " " << jobPid << endl;
    smash.setForegroundProcess(m_jobsList->getJobById(jobID)->getProcessID());
    m_jobsList->removeJobById(jobID);

    // Wait for the process to finish, bringing it to the foreground
    int status;
    waitpid(jobPid, &status, 0);

    // No job in foreground
    smash.setForegroundProcess(ERROR_VALUE);
}


/* Constructor implementation for KillCommand */
KillCommand::KillCommand(const char* origin_cmd_line, const char *cmd_line, JobsList* jobs) : BuiltInCommand(origin_cmd_line, cmd_line), m_jobsList(jobs){}

/* Execute method to kill given jobID. */
void KillCommand::execute() {
    vector<string> args = getArgs();
    int signum, jobID;
    // Validate the number of arguments
    if (args.size() != 3) {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    // Get signal number and job ID from command arguments
    try{
        signum = stoi(args[1]);
        jobID = stoi(args[2]);
        signum = signum < 0 ? -signum : MAX_SIGNAL_NUMBER + 1;
        if (signum > MAX_SIGNAL_NUMBER)
            throw InvalidArgument();
    }
    catch(...){
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

        // Get job entry by job ID
        JobsList::JobEntry* jobEntry = m_jobsList->getJobById(jobID);
        if (jobEntry == nullptr) {
            cerr << "smash error: kill: job-id " << jobID << " does not exist" << endl;
            return;
        }

        // Send the specified signal to the job
        if (kill(jobEntry->getProcessID(), signum) == 0)
            cout << "signal number " << signum << " was sent to pid " << jobEntry->getProcessID() << endl;
        else
            perror("smash error: kill failed");
}

/*---------------------------------------------------------------------------------------------------*/
/*---------------------------------------- External Commands ----------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/

/* Constructor implementation for ExternalCommand */
ExternalCommand::ExternalCommand(const char* origin_cmd_line, const char *cmd_line, bool isBgCmd) : Command(origin_cmd_line, cmd_line, isBgCmd){
    if (isBgCmd)
        setCommand(getCommand()+"&");
}

/* Execute method to an external command. */
void ExternalCommand::execute() {

    // Check if the command line contains special characters like '*' or '?'
        string cmd = getCommand();
        cmd = _removeBackgroundSignForString(cmd);

        // Check for Simple/Complex command
        if (cmd.find_first_of("*?") != string::npos) 
            runComplexCommand(cmd);
        else
            runSimpleCommand(cmd);
}

void ExternalCommand::runSimpleCommand(const string& cmd) {
    // Split command into tokens (command and arguments)
    vector<string> tokens = splitCommand(cmd);

    // Construct argument array for execvp
    char* args[tokens.size() + 1];
    for (size_t i = 0; i < tokens.size(); ++i) {
        args[i] = strdup(tokens[i].c_str());
    }
    args[tokens.size()] = nullptr;

    // Free allocated memory from parent process 
    delete this;

    // Execute the command using execvp
    execvp(args[0], args);

    // Handle execvp failure
    perror("smash error: External command failed");
    exit(1);
}

/* Run a complex external command by executing bash */
void ExternalCommand::runComplexCommand(const string& cmd) {    
    // Execute the command using execvp
    execlp("bash", "bash", "-c", cmd.c_str(), (char *)nullptr);

    // If execlp returns, an error occurred
    perror("smash error: External command failed");
    exit(1);
}

vector<string> ExternalCommand::splitCommand(const string& cmd) {
    istringstream iss(cmd);
    vector<string> tokens;
    for (string token; iss >> token;) {
        tokens.push_back(token);
    }
    return tokens;
}
    
bool ExternalCommand::isExternalCommand() const {
    return true;
}

/*---------------------------------------------------------------------------------------------------*/
/*---------------------------------------- Special Commands -----------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/
ListDirCommand::ListDirCommand(const char* origin_cmd_line, const char *cmd_line) : BuiltInCommand(origin_cmd_line, cmd_line) {}

/* Execute method to get and print theListDirCommand */
void ListDirCommand::execute() {
    // Error - to many arguments
    if (getArgCount() > LIST_DIR_COMMAND_ARGS_NUM) {
        cerr << "smash error: listdir: too many arguments" << endl;
        return;
    }
    
    vector<string> args = getArgs();
    const char* directoryPath = (getArgCount() == 2) ? args[1].c_str() : ".";
    
    // Open directory - return if failed.
    int fd = open(directoryPath, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        perror("smash error: open failed");
        return;
    }

    // Trying to read directory data.
    vector<char> buffer(DEFAULT_BUFFER_SIZE);
    int nread = syscall(SYS_getdents, fd, buffer.data(), buffer.size());
    if (nread < 0) {
        perror("smash error: getdents failed");
        return;
    }

    // Sort files in alphabetically order
    vector<string> entries;
    sortEntreysAlphabetically(entries, nread, buffer); 

    // Print files first and than the rest
    printContent(entries, directoryPath, buffer, true);
    printContent(entries, directoryPath, buffer, false);

    close(fd);
}

void ListDirCommand::sortEntreysAlphabetically(vector<string>& dir, int nread, vector<char> buffer){
    // Rearrange order to be alphabetic
    for (int bpos = 0; bpos < nread;){
        linux_dirent* d = (linux_dirent*)(buffer.data() + bpos);
        dir.push_back(d->d_name);
        bpos += d->d_reclen;
    }

    // Sort the directory files alphabetically
    sort(dir.begin(), dir.end());
}

void ListDirCommand::printContent(vector<string> entries, const char* directoryPath, vector<char> buffer, bool files){
        // Print each file in directory.
    for (const auto& entryName : entries) {
        string fullPath = string(directoryPath) + "/" + entryName;
        struct stat fileStat;

        // Error opening file
        if (lstat(fullPath.c_str(), &fileStat) != 0) 
            continue;

        // Classify file mode
        if (files && S_ISREG(fileStat.st_mode))
            cout << "file: " << entryName << endl;
        else if (!files && S_ISDIR(fileStat.st_mode))
            cout << "directory: " << entryName << endl;
        else if (!files && S_ISLNK(fileStat.st_mode)) {
            char link_buffer[buffer.size()];
            ssize_t len = readlink(fullPath.c_str(), link_buffer, buffer.size() - 1);
            if (len != ERROR_VALUE) {
                link_buffer[len] = '\0';
                cout << "link: " << entryName << " -> " << link_buffer << endl;
            } else {
                perror("smash error: readlink failed");
            }
        }
    }
}


RedirectionCommand::RedirectionCommand(const char *origin_cmd_line, const char *cmd_line) :
Command (origin_cmd_line, cmd_line) {}

void RedirectionCommand::execute() {
    // breaking the cmd_line to a command and filename
    SmallShell &smash = SmallShell::getInstance();
    int index = m_cmd_string.find_first_of('>'), start = 1;
    bool isDouble = (m_cmd_string[index+start] == '>');
    const char* command = strdup(m_cmd_string.substr(0,index).c_str());
    if (m_cmd_string[index] == ' ')
        start = 2;
    const char* file = strdup(m_cmd_string.substr(index+2,m_cmd_string.size()-index).c_str());

    // in case using '>>' instead of '>'
    if (isDouble)
        file = strdup(m_cmd_string.substr(index+3,m_cmd_string.size()-index).c_str());
    // forking the process, the son implement the command and writing the output while the parent wait
    pid_t pid = fork();
    if (pid < 0)
        perror("smash error: fork failed");
        
    // son processes
    if (pid == 0){
        int outputFile;
        if (isDouble)
            outputFile = open (file, O_WRONLY | O_CREAT | O_APPEND, 0644);
        else
            outputFile = open (file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    
        if (outputFile < 0){
            perror("smash error: open failed");
            exit(0);
        }
        if (dup2(outputFile, STDOUT_FILENO) < 0){
            perror("smash error: dup2 failed");
            close(outputFile);
            exit(0);
        }
        smash.executeCommand(command);
        close(outputFile);
        //free the allocated memory
        free(const_cast<char*>(command));
        free(const_cast<char*>(file));
        exit(1);
    }

    //parent processes
    else{
        int status;
        waitpid(pid, &status, 0);

        //free the allocated memory
        free(const_cast<char*>(command));
        free(const_cast<char*>(file));
    }
}


/* C'tor for getuser command class */
GetUserCommand::GetUserCommand(const char *origin_cmd_line, const char *cmd_line) :
BuiltInCommand(origin_cmd_line, cmd_line){}

void GetUserCommand::execute() {
    if (getArgCount() > 2){
        cerr << "smash error: getuser: too many arguments" << endl;
        return;
    }

    try {
        if (getArgCount() == 1){
            cerr << "smash error: getuser: process  does not exist" << endl;
            return;
        }
        pid_t pid = static_cast<pid_t>(stoi(getArgs()[1]));
        printUserByPid(pid);
    }
    catch (...){
        cerr << "smash error: getuser: process " << getArgs()[1] << " does not exist" << endl;
    }
}

// changes and additions been made according to piazza @92 and @57_f3 @70
void GetUserCommand::printUserByPid(pid_t pid) {
    //building the path to the status file using the given pid
    string status = "/proc/" + to_string(pid) + "/status";
    // opening the status file
    int fileDirectory = open(status.c_str(), O_RDONLY);
    char buffer [BIG_NUMBER];
    string statusFile;
    ssize_t size;
    do {
        size = read(fileDirectory, buffer, BIG_NUMBER-1);
        buffer[size] = '\0';
        statusFile += buffer;
    }
    while (size > 0);
    close (fileDirectory);
    // Variables to store UID and GID
    uid_t uid = ERROR_VALUE;
    gid_t gid = ERROR_VALUE;

    // Read the file line by line
    istringstream lines (statusFile);
    string line;
    while (getline(lines, line)) {
        if (line.substr(0, 4) == "Uid:") {
            istringstream input(line);
            string uidLabel;
            input >> uidLabel >> uid;
        }
        if (line.substr(0, 4) == "Gid:") {
            istringstream input(line);
            string gidLabel;
            input >> gidLabel >> gid;
        }
    }
    // making pointers to the user and the group to get the names
    struct passwd* user = getpwuid(uid);
    struct group* group = getgrgid(gid);

    //checking validity of pointers and printing the correct message
    if (user && group){
        cout << "User: " << user->pw_name << endl;
        cout << "Group: " << group->gr_name << endl;
    }
    else
        throw exception();

}


/* C'tor for WatchCommand command class */
WatchCommand::WatchCommand(const char *origin_cmd_line, const char *cmd_line) : Command(origin_cmd_line, cmd_line) {}

void WatchCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();

    // Check for interval and command in the arguments
    int interval = DEFAULT_INTERVAL_TIME;
    string command = getWatchCommand(interval);
    
    //Invalid command
    if (command == "")
        return;

    // Make sure to reset stopWatch
    smash.setStopWatch(false);
    while (!smash.getStopWatch()) {
        // Clear the screen before displaying new output
        smash.executeCommand("clear");

        // Execute the specified command
        smash.executeCommand(command.c_str());

        // Wait for the specified interval before executing the command again
        sleep(interval);
    }
}

string WatchCommand::getWatchCommand(int& interval){
    vector<string> args = getArgs();
    int argsNum = getArgCount(), start = 2;
    string command;
    
    // Get interval
    try {
        updateInterval(args[1], interval);
    }
    catch(InvalidInterval& e){
        cerr << "smash error: watch: invalid interval" << endl;
        return "";
    }
    catch (...) {
        start--;
        if (SmallShell::COMMANDS.find(args[1]) == SmallShell::COMMANDS.end()){
            perror("smash error: External command failed");
            return "";
        }
    }

    if (argsNum == 1){
        cerr << "smash error: watch: command not specified" << endl;
        return "";
    }

    extractWatchCommand(command, start, args, argsNum);

    return command;
}

void WatchCommand::extractWatchCommand(string& command, int start, vector<string> args, int argsNum){
    // Concatenate all arguments for the command from start
    for (int i = start; i < argsNum; ++i) {
        command += args[i] + " ";
    }

    // Remove the trailing space at the end if it exists
    command.pop_back(); 
}

void WatchCommand::updateInterval(string value, int& interval){
    // Ensure the string represents a non-negative integer
    size_t pos;
    interval = stoi(value, &pos);
    if (interval <= 0)
        throw InvalidInterval();
}


/* C'tor for pipe command class */
PipeCommand::PipeCommand(const char *origin_cmd_line, const char *cmd_line) : Command(origin_cmd_line, cmd_line){
    // Checking if its '|' or '|&| pipe
    char c ='|';
    bool m_isErr = (m_cmd_string[m_cmd_string.find_first_of(c)+1] == '&');
    if (m_isErr)
        c = '&';

    // Split the commands
    string command1 = (m_isErr ? m_cmd_string.substr(0, m_cmd_string.find_first_of(c)-1)
            : m_cmd_string.substr(0, m_cmd_string.find_first_of(c)));
    string command2 = m_cmd_string.substr(m_cmd_string.find_first_of(c) + 1);
    m_firstCmd = _trim(command1);
    m_secondCmd = _trim(command2) + " ";

}

void PipeCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    string temp;

    // Create pipe
    int fd[2];
    if (pipe(fd) < 0) {
        perror ("pipe failed");
        return;
    }

    // First child process (command1)
    pid_t firstPid = fork();
    if (firstPid < 0) {
        perror("smash error: fork failed");
        close(fd[0]);
        close(fd[1]);
        return;
    }

    if (firstPid == 0) {
        setpgrp();
        close(fd[0]);
        int outputChannel = (m_isErr ? STDERR_FILENO :  STDOUT_FILENO) ;
        if (dup2(fd[1], outputChannel) < 0) { //fd[1] is the writing end
            perror("smash error: dup2 failed");
            close(fd[1]);
            return;
        }
        smash.executeCommand(m_firstCmd.c_str());
        close(fd[1]);
        exit(0);
    }

    // Second child process (command2)
    pid_t secondPid = fork();
    if (secondPid < 0) {
        perror("smash error: fork failed");
        close(fd[0]);
        close(fd[1]);
        return;
    }

    if (secondPid == 0) {
        setpgrp();
        if (dup2(fd[0], STDIN_FILENO) < 0) { //fd[1] is the writing end
            perror("smash error: dup2 failed");
            close(fd[1]);
            return;
        }
        close(fd[0]);
        close(fd[1]);
        smash.executeCommand((m_secondCmd).c_str());
        exit(0);
    }

    //parent process
    int status;
    close(fd[0]);
    close(fd[1]);
    waitpid(firstPid, &status, 0);
    waitpid(secondPid, &status, 0);
}

/*---------------------------------------------------------------------------------------------------*/
/*------------------------------------------- Jobs Methods ------------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/

/* C'tor for JobEntry & Setters/Getters */
JobsList::JobEntry::JobEntry(int id, int pid, Command* cmd, bool stopped) : m_jobID(id), m_processID(pid), m_command(cmd) {}

void JobsList::JobEntry::setJobID(int id){
    m_jobID = id;
}

void JobsList::JobEntry::setProcessID(int id){
    m_processID = id;
}

void JobsList::JobEntry::setCommand(Command* cmd){
    m_command = cmd;
}

int JobsList::JobEntry::getJobID() const{
    return m_jobID;
}

int JobsList::JobEntry::getProcessID() const{
    return m_processID;
}

Command* JobsList::JobEntry::getCommand() const{
    return m_command;
}


/* C'tor & D'tor for JobList*/
JobsList::JobsList() : m_jobEntries(new vector<JobEntry>()), m_nextJobID(DEFAULT_JOB_ID), m_numRunningJobs(DEFAULT_NUM_RUNNING_JOBS) {}
JobsList::~JobsList(){
    delete m_jobEntries;
}

int JobsList::getNextJobID() const{
    return m_nextJobID;
}

int JobsList::getNumRunningJobs() const{
    return m_numRunningJobs;
}

/* Method for adding job for job list */
void JobsList::addJob(Command* command, int jobPid, bool isStopped) {
    // Remove all finshed background jobs.
    removeFinishedJobs();

    m_jobEntries->emplace_back(m_nextJobID++, jobPid, command, isStopped);
    m_numRunningJobs++;
}

JobsList::JobEntry* JobsList::getJobById(int jobId){
    for (auto& job : *m_jobEntries) {
        if (job.getJobID() == jobId) {
            return &job; // Return a pointer to the job with the specified process ID
        }
    }

    return nullptr; // Return nullptr if no job with the specified process ID is found
}

JobsList::JobEntry* JobsList::getJobByPid(int Pid){
    for (auto& job : *m_jobEntries) {
        if (job.getProcessID() == Pid) {
            return &job; // Return a pointer to the job with the specified process ID
        }
    }

    return nullptr; // Return nullptr if no job with the specified process ID is found
}

void JobsList::killAllJobs() {
    for (auto& job : *m_jobEntries){
        kill(job.getProcessID(), SIGKILL); // Send SIGKILL signal to all jobs
        removeJobById(job.getJobID());
    }
}

/* Rearrange the jobs oreder and returns an iterator pointing to the new "end" of the range. */
void JobsList::removeJobById(int jobId){
    auto it = remove_if(m_jobEntries->begin(), m_jobEntries->end(), [jobId](const JobEntry& job){
        return job.getJobID() == jobId;
    });

    // Remove given job.
    if (it != m_jobEntries->end()) {
        m_jobEntries->erase(it, m_jobEntries->end());
        m_numRunningJobs--;

        // Adjust the next job ID if needed
        if (jobId == m_nextJobID - 1)
            m_nextJobID--;
    }


}

void JobsList::removeFinishedJobs(){
    int status, highestJobId = 0;
    // Iterate through the list of background jobs in reverse order to safely erase elements
    for (auto it = m_jobEntries->begin(); it != m_jobEntries->end(); ) {
        pid_t result = waitpid(it->getProcessID(), &status, WNOHANG);

        // The child process of this job has terminated - Erase it from the list.
        if (result == it->getProcessID()){
            // Free allocated memory of command 
            delete it->getCommand();
            it = m_jobEntries->erase(it);
        }
        else{
            // Update the highest job ID
            if (it->getJobID() > highestJobId) 
                highestJobId = it->getJobID();

            // Move to the next job
            ++it;
        }
    }
    // Update the next job ID based on the highest job ID
    m_nextJobID = highestJobId + 1;
    // Update the number of running jobs
    m_numRunningJobs = m_jobEntries->size();
}

/* Method for printint job list to terminal */
 void JobsList::printJobsList() {
    // Remove all finshed background jobs.
    removeFinishedJobs();

    // Sort the job entries based on job ID
    sort(m_jobEntries->begin(), m_jobEntries->end(), [](const JobEntry& a, const JobEntry& b) {
        return a.getJobID() < b.getJobID();
    });

    // Print the jobs list in the required format
    for (const auto& job : *m_jobEntries) {
        cout << "[" << job.getJobID() << "] " << job.getCommand()->getOriginalCommand() << endl;
    }
}

/* Method for printint job list to terminal */
 void JobsList::printJobsListWithPid() {
    // Remove all finshed background jobs.
    removeFinishedJobs();

    // Sort the job entries based on job ID
    sort(m_jobEntries->begin(), m_jobEntries->end(), [](const JobEntry& a, const JobEntry& b) {
        return a.getJobID() < b.getJobID();
    });

    // Print the jobs list in the required format
    for (const auto& job : *m_jobEntries) {
        cout << job.getProcessID() << ": " << job.getCommand()->getOriginalCommand() << endl;
    }
}

bool JobsList::isEmpty(){
    return m_jobEntries->empty();
}
