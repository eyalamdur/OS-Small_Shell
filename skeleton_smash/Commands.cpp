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

SmallShell::SmallShell() : m_prompt("smash"),  m_plastPwd(nullptr),  
                        m_jobList(new JobsList()), m_proceed(true), m_alias(new map<string, string>){}

SmallShell::~SmallShell() {
    delete m_jobList;
    delete m_alias;
}

void SmallShell::setPrompt(const string str) {
    m_prompt = str;
}

string SmallShell::getPrompt() const {
    return m_prompt;
}

bool SmallShell::toProceed() const {
    return m_proceed;
}

void SmallShell::quit() {
    m_proceed = false;
}

void SmallShell::addAlias(string name, string command) {
    if (m_alias->find(name) == m_alias->end() && COMMANDS.find(name) == COMMANDS.end())
        (*m_alias)[name] = command;
    else
        cout << "smash error: alias: " << name << " already exists or is a reserved command" << endl;
}

void SmallShell::removeAlias(vector<string> args) {
    for (int i = 1; i < (int)args.size(); i++){
        if (m_alias->find(args[i]) != m_alias->end())
            m_alias->erase(args[i]);
        else{
            cout << "smash error: unalias: " << args[i] << " alias does not exist" << endl;
            break;
        }
    }
}

void SmallShell::printAlias() {
    for (auto& pair : *m_alias){
        cout << pair.first << "='" << pair.second << "'" << endl;
    }
}

char* SmallShell::extractCommand(const char* cmd_l,string &firstWord){
    // Make a modifiable copy of the command line and remove & if exists
    char* newCmdLine = strdup(cmd_l);
    _removeBackgroundSign(newCmdLine);

    // Find first word (the command)
    string cmd_s = _trim(string(newCmdLine));
    firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    char* cmd_line = strdup(newCmdLine);
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
    // Check if the command line ends with "&" for background execution
    string firstWord;
    char* newCmdLine = extractCommand(cmd_line, firstWord);

    if (firstWord.compare("pwd") == 0) 
        return new GetCurrDirCommand(cmd_line, newCmdLine);
    else if (firstWord.compare("chprompt") == 0)
        return new ChangePromptCommand(cmd_line, newCmdLine);
    else if (firstWord.compare("showpid") == 0)
        return new ShowPidCommand(cmd_line, newCmdLine);
    else if (firstWord.compare("cd") == 0) 
        return new ChangeDirCommand(cmd_line, newCmdLine, getPlastPwdPtr());
    else if (firstWord.compare("quit") == 0) 
        return new QuitCommand(cmd_line, newCmdLine, getJobsList());
    else if (firstWord.compare("alias") == 0)
        return new aliasCommand(cmd_line, newCmdLine);
    else if (firstWord.compare("unalias") == 0)
        return new unaliasCommand(cmd_line, newCmdLine);
    else if (firstWord.compare("jobs") == 0) 
        return new JobsCommand(cmd_line, newCmdLine, getJobsList());
    else if (firstWord.compare("fg") == 0) 
        return new ForegroundCommand(cmd_line, newCmdLine, getJobsList());
    else if (firstWord.compare("kill") == 0) 
        return new KillCommand(cmd_line, newCmdLine, getJobsList());
    else
        return new ExternalCommand(cmd_line, newCmdLine, _isBackgroundCommand(cmd_line));

  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    Command* cmd = CreateCommand(cmd_line);
    // Invalid Command
    if (cmd == nullptr){
        printToTerminal("Unknown Command");
        return;
    }


    if(cmd->isExternalCommand()){
        // Fork a new process
        pid_t pid = fork();

        if (pid == ERROR_VALUE) {
            cerr << "Error forking process." << endl;
            return;
        }

        // Parent process
        if (pid > CHILD_ID){
            if(cmd->isBackgroundCommand())
                getJobsList()->addJob(cmd, pid);
            else{
                int status;
                waitpid(pid, &status,0);
            }
            return;
        }
    }

    // Execute command
    cmd->execute();
}

/* Gets pointer to the last path of working directory */
char** SmallShell::getPlastPwdPtr() {
    return &m_plastPwd;
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
 m_cmd_string(string(cmd_line)), m_bgCmd(isBgCmd) {}
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

/* Implement the clone method for GetCurrDirCommand */
Command* GetCurrDirCommand::clone() const {
    return new GetCurrDirCommand(*this);
}

/* Execute method to get and print the current working directory. */
void GetCurrDirCommand::execute() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != nullptr)
        cout << cwd << endl;
}


/* C'tor for changePromptCommand*/
ChangePromptCommand::ChangePromptCommand(const char* origin_cmd_line, const char *cmd_line) : BuiltInCommand(origin_cmd_line, cmd_line) {}

/* Implement the clone method for ChangeDirCommand */
Command* ChangePromptCommand::clone() const {
    return new ChangePromptCommand(*this);
}

void ChangePromptCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    string str = (getArgCount() > 1) ? getArgs()[1] : "smash";
    smash.setPrompt(str);
}


/* C'tor for ShowPidCommand Class*/
ShowPidCommand::ShowPidCommand(const char* origin_cmd_line, const char *cmd_line) : BuiltInCommand(origin_cmd_line, cmd_line) {}

/* Implement the clone method for ChangeDirCommand */
Command* ShowPidCommand::clone() const {
    return new ShowPidCommand(*this);
}

void ShowPidCommand::execute() {
    pid_t pid = getpid();
    cout << "smash pid is " << pid << endl;
}


/* C'tor for quit command*/
QuitCommand::QuitCommand(const char* origin_cmd_line, const char *cmd_line, JobsList *jobs) : BuiltInCommand(origin_cmd_line, cmd_line) , m_jobsList(jobs){}

/* Implement the clone method for ChangeDirCommand */
Command* QuitCommand::clone() const {
    return new QuitCommand(*this);
}

void QuitCommand::execute() {
    if (getArgCount() > 1 && getArgs()[1].compare("kill") == 0){
        // Print a message & joblist before exiting
        if (m_jobsList != nullptr){
            cout << "smash: smashing " << m_jobsList->getNumRunningJobs() << " jobs:" << endl;
            m_jobsList->printJobsListWithPid();
        }
        else
            cout << "smash: smashing 0 jobs:" << endl;
            
        // Print the list of jobs that were killed
        while(m_jobsList != nullptr && !m_jobsList->isEmpty())
            m_jobsList->killAllJobs();
        
    }
    // End the execution of the shell
    SmallShell::getInstance().quit();
}


/* C'tor for aliasCommamd class */
aliasCommand::aliasCommand(const char* origin_cmd_line, const char *cmd_line) : BuiltInCommand(origin_cmd_line, cmd_line) {
    int space = 0;
    for (; space < m_cmd_string.length() && m_cmd_string[space]!=' ' ; space++) {}
    int equals = space;
    for (; equals < m_cmd_string.length() && m_cmd_string[equals]!='='; equals++) {}
    //cout << "space: " << space <<", equals: " << equals << endl;
    int length = m_cmd_string.length() - equals - 3;
    //cout << "length: " << (int)strlen(m_cmd_string) << endl;
    char command[length+1];
    for (int i = 0 ; i < length ; i++){
        command[i] = m_cmd_string[equals + 2 + i];
    }
    command[length] = '\0';
    length = equals - space - 1;
    char name[length + 1];
    for (int i = 0; i < length; i++){
        name[i] = m_cmd_string[space + 1 + i];
    }
    name[length] = '\0';
    //cout << "name: " << name << endl;
    //cout << "command: " << command << endl;
    this->name=name;
    this->command=command;
}

/* Implement the clone method for ChangeDirCommand */
Command* aliasCommand::clone() const {
    return new aliasCommand(*this);
}

void aliasCommand::execute() {
    const regex aliasRegex("^alias [a-zA-Z0-9_]+='[^']*'$");
    SmallShell &smash = SmallShell::getInstance();

    // Print alias commands list
    if (getArgCount() == 1)
        smash.printAlias();
    
    // Add new alias command
    else{
        string first = command.substr(0, command.find_first_of(" \n"));
        if (regex_match(m_cmd_string, aliasRegex))
            smash.addAlias(name, command);
        else
            cout << "smash error: alias: invalid alias format" << endl;
    }
}


/* C'tor for unaliasCommand class. */
unaliasCommand::unaliasCommand(const char* origin_cmd_line, const char *cmd_line) : BuiltInCommand(origin_cmd_line, cmd_line){}

/* Implement the clone method for ChangeDirCommand */
Command* unaliasCommand::clone() const {
    return new unaliasCommand(*this);
}

void unaliasCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    if (getArgCount() == 1){
        cout << "smash error: unalias: not enough arguments" << endl;
    }
    else
        smash.removeAlias(getArgs());
}


/* Constructor implementation for ChangeDirCommand */
ChangeDirCommand::ChangeDirCommand(const char* origin_cmd_line, const char *cmd_line, char **plastPwd) : BuiltInCommand(origin_cmd_line, cmd_line), plastPwd(plastPwd) {}

/* Implement the clone method for ChangeDirCommand */
Command* ChangeDirCommand::clone() const {
    return new ChangeDirCommand(*this);
}

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
    if (newDir != nullptr){
        *plastPwd = getcwd(NULL, 0);
        chdir(newDir);
    }
}


JobsCommand::JobsCommand(const char* origin_cmd_line, const char* cmd_line, JobsList* jobs) : BuiltInCommand(origin_cmd_line, cmd_line), m_jobsList(jobs) {}

/* Implement the clone method for JobsCommand */
Command* JobsCommand::clone() const {
    return new JobsCommand(*this);
}

void JobsCommand::execute() {
    m_jobsList->printJobsList();
}


ForegroundCommand::ForegroundCommand(const char* origin_cmd_line, const char* cmd_line, JobsList* jobs) : BuiltInCommand(origin_cmd_line, cmd_line), m_jobsList(jobs){}

/* Implement the clone method for JobsCommand */
Command* ForegroundCommand::clone() const {
    return new ForegroundCommand(*this);
}

void ForegroundCommand::execute() {
    // Check if the jobs list is empty
    if (m_jobsList->isEmpty()) {
        cout << "smash error: fg: jobs list is empty" << endl;
        return;
    }
    
    int jobID;
    // No job ID specified, select the job with the maximum job ID
    if (getArgs().size() == 1)
        jobID = m_jobsList->getNextJobID() - 1;
    else if (getArgs().size() == 2) {
        try {
            jobID = stoi(getArgs()[1]);
            if (m_jobsList->getJobById(jobID) == nullptr) {
                cout << "smash error: fg: job-id " << jobID << " does not exist" << endl;
                return;
            }
        } 
        catch (const invalid_argument& e) {
            cout << "smash error: fg: invalid arguments" << endl;
            return;
        }
    } 
    else {
        cout << "smash error: fg: invalid arguments" << endl;
        return;
    }

    // Get the job and remove it from the jobs list
    JobsList::JobEntry* jobEntry = m_jobsList->getJobById(jobID);
    m_jobsList->removeJobById(jobID);

    // Bring the job to the foreground and print the command line along with the PID
    cout << jobEntry->getCommand()->getCommand() << " " << jobEntry->getProcessID() << endl;

    // Wait for the process to finish, bringing it to the foreground
    int status;
    waitpid(jobEntry->getProcessID(), &status, 0);
}


/* Constructor implementation for KillCommand */
KillCommand::KillCommand(const char* origin_cmd_line, const char *cmd_line, JobsList* jobs) : BuiltInCommand(origin_cmd_line, cmd_line), m_jobsList(jobs){}

/* Implement the clone method for KillCommand */
Command* KillCommand::clone() const {
    return new KillCommand(*this);
}

/* Execute method to kill given jobID. */
void KillCommand::execute() {
    vector<string> args = getArgs();

    // Validate the number of arguments
    if (args.size() != 3) {
        cout << "smash error: kill: invalid arguments" << endl;
        return;
    }

    // Get signal number and job ID from command arguments
    int signum = stoi(args[1].substr(1));
    int jobID = stoi(args[2]);

    // Get job entry by job ID
    JobsList::JobEntry* jobEntry = m_jobsList->getJobById(jobID);
    if (jobEntry == nullptr) {
        cout << "smash error: kill: job-id " << jobID << " does not exist" << endl;
        return;
    }

    // Send the specified signal to the job
    if (kill(jobEntry->getProcessID(), signum) == 0)
        cout << "signal number " << signum << " was sent to pid " << jobEntry->getProcessID() << endl;
    else
        perror("kill error");
}

/*---------------------------------------------------------------------------------------------------*/
/*---------------------------------------- External Commands ----------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/

/* Constructor implementation for ExternalCommand */
ExternalCommand::ExternalCommand(const char* origin_cmd_line, const char *cmd_line, bool isBgCmd) : Command(origin_cmd_line, cmd_line, isBgCmd){
    if (isBgCmd)
        setCommand(getCommand()+"&");
}

/* Implement the clone method for KillCommand */
Command* ExternalCommand::clone() const {
    return new ExternalCommand(*this);
}

/* Execute method to get and print the current working directory. */
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

    // Execute the command using execvp
    execvp(args[0], args);

    // Handle execvp failure
    cerr << "External command execution failed" << endl;
    exit(1);
}

/* Run a complex external command by executing bash */
void ExternalCommand::runComplexCommand(const string& cmd) {
    string bashCmd = "bash -c \"" + cmd + "\"";
    system(bashCmd.c_str());
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
/*------------------------------------------- Jobs Methods ------------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/

/* C'tor for JobEntry & Setters/Getters */
JobsList::JobEntry::JobEntry(int id, int pid, Command* cmd, bool stopped) : m_jobID(id), m_processID(pid), m_command(cmd), m_isStopped(stopped) {}

void JobsList::JobEntry::setJobID(int id){
    m_jobID = id;
}

void JobsList::JobEntry::setProcessID(int id){
    m_processID = id;
}

void JobsList::JobEntry::setCommand(Command* cmd){
    m_command = cmd;
}

void JobsList::JobEntry::setStopped(bool stopped){
    m_isStopped = stopped;
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

bool JobsList::JobEntry::isStopped() const{
    return m_isStopped;
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
    Command* cmd = command->clone(); // virtual clone method
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

// Rearrange the jobs oreder and returns an iterator pointing to the new "end" of the range.
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

/* Method for printint job list to terminal */
 void JobsList::printJobsList() {
    // Sort the job entries based on job ID
    sort(m_jobEntries->begin(), m_jobEntries->end(), [](const JobEntry& a, const JobEntry& b) {
        return a.getJobID() < b.getJobID();
    });

    // Print the jobs list in the required format
    for (const auto& job : *m_jobEntries) {
        if (!job.isStopped()) {
            cout << "[" << job.getJobID() << "] " << job.getCommand()->getOriginalCommand() << endl;
        }
    }
}

/* Method for printint job list to terminal */
 void JobsList::printJobsListWithPid() {
    // Sort the job entries based on job ID
    sort(m_jobEntries->begin(), m_jobEntries->end(), [](const JobEntry& a, const JobEntry& b) {
        return a.getJobID() < b.getJobID();
    });

    // Print the jobs list in the required format
    for (const auto& job : *m_jobEntries) {
        if (!job.isStopped()) {
            cout << job.getProcessID() << ": " << job.getCommand()->getOriginalCommand() << endl;
        }
    }
}

bool JobsList::isEmpty(){
    return m_jobEntries->empty();
}