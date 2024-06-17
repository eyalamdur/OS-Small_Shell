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
    string firstWord;
    char* newCmdLine = extractCommand(cmd_line, firstWord);
    if (string(newCmdLine).find('>') != string::npos)
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
    else if (firstWord.compare("listdir") == 0) 
        return new ListDirCommand(cmd_line, newCmdLine);
    else if (firstWord.compare("getuser") == 0)
        return new GetUserCommand(cmd_line, newCmdLine);
    else
        return new ExternalCommand(cmd_line, newCmdLine, _isBackgroundCommand(cmd_line));

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
        m_cmd_string = _trim(m_cmd_string);
        string first = command.substr(0, command.find_first_of(" \n"));
        cout << "m_cmd_line:~" << m_cmd_string << "~" << endl;
        cout << regex_match(m_cmd_string, aliasRegex) << endl;
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

/* Implement the clone method for ExternalCommand */
Command* ExternalCommand::clone() const {
    return new ExternalCommand(*this);
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
/*---------------------------------------- Special Commands -----------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/
ListDirCommand::ListDirCommand(const char* origin_cmd_line, const char *cmd_line) : BuiltInCommand(origin_cmd_line, cmd_line) {}

/* Implement the clone method for ListDirCommand */
Command* ListDirCommand::clone() const {
    return new ListDirCommand(*this);
}

/* Execute method to get and print theListDirCommand */
void ListDirCommand::execute() {
    if (getArgCount() > LIST_DIR_COMMAND_ARGS_NUM) {
        cout << "smash error: listdir: too many arguments" << endl;
        return;
    }

    vector<string> args = getArgs();
    const char* directoryPath = (getArgCount() == 2) ? args[1].c_str() : ".";

    DIR* dir = opendir(directoryPath);
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    struct stat fileStat;
    size_t buffer_size = DEFAULT_BUFFER_SIZE;
    char buffer[buffer_size];
    vector<string> entries;

    // Rearrange order to be alphabetic
    while ((entry = readdir(dir)) != NULL) 
        entries.push_back(entry->d_name);

    // Sort the directory entries alphabetically
    sort(entries.begin(), entries.end());

    for (const auto& entryName : entries) {
        string fullPath = string(directoryPath) + "/" + entryName;
        if (lstat(fullPath.c_str(), &fileStat) != 0) {
            perror("lstat");
            continue;
        }

        // Discover type of file/dir/link
        if (S_ISREG(fileStat.st_mode))
            cout << "file: " << entryName << endl;
        else if (S_ISDIR(fileStat.st_mode)) 
            cout << "directory: " << entryName << endl;
        else if (S_ISLNK(fileStat.st_mode)) {
            ssize_t len = readlink(fullPath.c_str(), buffer, buffer_size - 1);
            if (len != -1) {
                buffer[len] = '\0';
                cout << "link: " << entryName << " -> " << buffer << endl;
            } 
            else
                perror("readlink");
        }
    }

    closedir(dir);

}

/* C'tor for redirect command class */
RedirectionCommand::RedirectionCommand(const char *origin_cmd_line, const char *cmd_line) :
        Command (origin_cmd_line, cmd_line) {}

Command *RedirectionCommand::clone() const {
    return new RedirectionCommand(*this);
}

void RedirectionCommand::execute() {
    // breaking the cmd_line to a command and filename
    SmallShell &smash = SmallShell::getInstance();
    int index = m_cmd_string.find_first_of('>');
    bool isDouble = (m_cmd_string[index+1] == '>');
    const char* command = strdup(m_cmd_string.substr(0,index).c_str());
    const char* file = strdup(m_cmd_string.substr(index+1,m_cmd_string.size()-index).c_str());
    // in case using '>>' instead of '>'
    if (isDouble)
        file = m_cmd_string.substr(index+2,m_cmd_string.size()-index).c_str();
    // forking the process, the son implement the command and writing the output while the parent wait
    pid_t pid = fork();
    if (pid < 0)
        cout << "failed to fork" << endl;
    // son processes
    if (pid == 0){
        int outputFile;
        if (isDouble)
            outputFile = open (file, O_WRONLY | O_CREAT | O_APPEND, 0644);
        else
            outputFile = open (file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (outputFile < 0){
            cout << "failed to open" << endl;
            exit(0);
        }
        if (dup2(outputFile, STDOUT_FILENO) < 0){
            cout << "dup2 fail" << endl;
            close(outputFile);
            exit(0);
        }
        smash.executeCommand(command);
        close(outputFile);
        //free the allocated memory
        free(const_cast<char*>(command));
        exit(1);
    }
        //parent processes
    else{
        int status;
        waitpid(pid, &status, 0);
        //free the allocated memory
        free(const_cast<char*>(command));
    }
}


/* C'tor for getuser command class*/
GetUserCommand::GetUserCommand(const char *origin_cmd_line, const char *cmd_line) :
BuiltInCommand(origin_cmd_line, cmd_line){}

Command *GetUserCommand::clone() const {
    return new GetUserCommand(*this);
}

void GetUserCommand::execute() {
    if (getArgCount() > 2){
        cout << "smash error: getuser: too many arguments" << endl;
        return;
    }
    if (getArgCount() == 1) {
        cout << "smash error: getuser: too few arguments" << endl;
        return;
    }
    try {
        pid_t pid = static_cast<pid_t>(stoi(getArgs()[1]));
        printUserByPid(pid);
    }
    catch (const std::exception &e){
        cout << "smash error: getuser: process " << getArgs()[1] << " does not exist" << endl;
    }
}

void GetUserCommand::printUserByPid(pid_t pid) {
    //building the path to the status file using the given pid
    string status = "/proc/" + to_string(pid) + "/status";
    // opening the status file
    ifstream statusFile(status);
    if (!statusFile.is_open()){
        throw exception();
    }

    // Variables to store UID and GID
    uid_t uid = -1;
    gid_t gid = -1;

    // Read the file line by line
    std::string line;
    while (std::getline(statusFile, line)) {
        if (line.substr(0, 4) == "Uid:") {
            //cout << "uid line: " << line << endl;
            std::istringstream input(line);
            std::string uidLabel;
            input >> uidLabel >> uid;
        }
        if (line.substr(0, 4) == "Gid:") {
            //cout << "gid line: " << line << endl;
            std::istringstream input(line);
            std::string gidLabel;
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
    else{
        if (!user){
            cout << "failed to get the information for UID: " << uid << endl;
        }
        if (!group){
            cout << "failed to get the information for GID: " << gid << endl;
        }
    }
}

/* C'tor for pipe command class */
PipeCommand::PipeCommand(const char *origin_cmd_line, const char *cmd_line) : Command(origin_cmd_line, cmd_line){}

Command *PipeCommand::clone() const {
    return new PipeCommand(*this);
}

void PipeCommand::execute() {
    cout << "execute pipe command" << endl;
    SmallShell &smash = SmallShell::getInstance();

    // checking if its '|' or '|&| pipe
    char c ='|';
    bool isErr = (m_cmd_string[m_cmd_string.find_first_of(c)+1] == '&');
    if (isErr)
        c = '&';

    // Split the commands
    string command1 = m_cmd_string.substr(0, m_cmd_string.find_first_of(c));
    string command2 = m_cmd_string.substr(m_cmd_string.find_first_of(c) + 1);
    command1 = _trim(command1);
    command2 = _trim(command2);
    cout << "command1: " << command1 << endl;
    cout << "command2: " << command2 << endl;

    // Create pipe
    int fd[2];
    if (pipe(fd) < 0) {
        perror("pipe failed");
        return;
    }

    // Fork the first child (command1)
    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("fork failed");
        close(fd[0]);
        close(fd[1]);
        return;
    }
    // First child process (command1)
    if (pid1 == 0) {
        cout << "First child process" << endl;
        close(fd[0]);
        if (dup2(fd[1], STDOUT_FILENO) < 0) { //fd[1] is the writing end
            perror("dup2 failed");
            close(fd[1]);
            exit(1);
        }
        close(fd[1]);

        const char* commandToExecute = strdup(command1.c_str());
        smash.executeCommand(commandToExecute);
        free(const_cast<char*>(commandToExecute));
        exit(0);
    }

    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("fork failed");
        close(fd[0]);
        close(fd[1]);
        return;
    }
    // Second child process (command2)
    if (pid2 == 0) {
        cout << "Second child process" << endl;
        close(fd[1]);
        int inputChannel = STDIN_FILENO;
        if (c == '&')
            inputChannel = STDERR_FILENO;
        if (dup2(fd[0], inputChannel) < 0) { // fd[0] is the reading end
            perror("dup2 failed");
            close(fd[0]);
            exit(1);
        }
        close(fd[0]);
//        char *buffer[100];
//        read(fd[0], buffer, 100);
//        const char* commandToExecute = strdup(command2.c_str()+" "+buffer);
        const char* commandToExecute = strdup(command2.c_str());
        smash.executeCommand(commandToExecute);
        free(const_cast<char*>(commandToExecute));
        exit(0);
    }

    // Parent process
    close(fd[0]);
    close(fd[1]);
    int status;
    waitpid(pid1, &status, 0);
    waitpid(pid2, &status, 0);
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
    // Remove all finshed background jobs.
    removeFinishedJobs();

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

        // The child process of this job has terminated
        if (result == it->getProcessID()) { 
            // Update the status of the job to stopped & erase it from the list.
            it->setStopped(true);
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
        if (!job.isStopped()) {
            cout << "[" << job.getJobID() << "] " << job.getCommand()->getOriginalCommand() << endl;
        }
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
        if (!job.isStopped()) {
            cout << job.getProcessID() << ": " << job.getCommand()->getOriginalCommand() << endl;
        }
    }
}

bool JobsList::isEmpty(){
    return m_jobEntries->empty();
}



