#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <map>
#include <set>
#include <memory>


#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define CD_COMMAND_ARGS_NUM (2)
#define LIST_DIR_COMMAND_ARGS_NUM (2)
#define DEFAULT_INTERVAL_TIME (2)
#define DEFAULT_JOB_ID (1)
#define DEFAULT_BUFFER_SIZE (256)
#define MAX_SIGNAL_NUMBER (31)
#define DEFAULT_NUM_RUNNING_JOBS (0)
#define CHILD_ID (0)
#define ERROR_VALUE (-1)
#define BIG_NUMBER (1000)

using namespace std;

class Command {
// TODO: Add your data members
public:
    Command(const char *origin_cmd_line, const char *cmd_line, bool isBgCmd = false);
    virtual ~Command();
    virtual void execute() = 0;

    /* Args Methods */
    int getArgCount() const;
    vector<string> getArgs() const;
    string getCommand() const;
    string getOriginalCommand() const;
    void setCommand(string cmd);
    bool isBackgroundCommand() const;
    virtual bool isExternalCommand() const;

    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed
protected:
    string m_origin_cmd_string;
    string m_cmd_string;
    bool m_bgCmd;    
};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char* origin_cmd_line, const char *cmd_line);
    virtual ~BuiltInCommand() {}

};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char* origin_cmd_line, const char *cmd_line, bool isBgCmd);

    virtual ~ExternalCommand() {}

    void execute() override;
    void runSimpleCommand(const string& cmd);
    void runComplexCommand(const string& cmd);
    vector<string> splitCommand(const string& cmd);
    bool isExternalCommand() const override;
};

class PipeCommand : public Command {
protected:
    string m_firstCmd;
    string m_secondCmd;
    bool m_isErr;

public:
    PipeCommand(const char* origin_cmd_line, const char *cmd_line);

    virtual ~PipeCommand() {}

    void execute() override;
};

class WatchCommand : public Command {
private:
    class InvalidInterval : public exception{};
public:
    WatchCommand(const char *origin_cmd_line, const char *cmd_line);
    virtual ~WatchCommand() {}
    
    void execute() override;
    string getWatchCommand(int& interval);
    void extractWatchCommand(string& command, int start, vector<string> args, int argsNum);
    void updateInterval(string value, int& interval);
};

class RedirectionCommand : public Command {
    // TODO: Add your data members
public:
    explicit RedirectionCommand(const char *origin_cmd_line, const char *cmd_line);


    virtual ~RedirectionCommand() {}

    void execute() override;

};

class ChangeDirCommand : public BuiltInCommand {
public:
    ChangeDirCommand(const char* origin_cmd_line, const char *cmd_line, char *plastPwd);

    ~ChangeDirCommand() override;

    void execute() override;

protected:
    char *plastPwd; // Pointer to the previous working directory
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char* origin_cmd_line, const char *cmd_line);


    virtual ~GetCurrDirCommand() {}

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char* origin_cmd_line, const char *cmd_line);


    virtual ~ShowPidCommand() {}

    void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
protected:
    JobsList* m_jobsList;
public:
    QuitCommand(const char* origin_cmd_line, const char *cmd_line, JobsList *jobs);

    virtual ~QuitCommand() {}

    void execute() override;
};

class JobsList {
public:
    class JobEntry {
    protected:
        int m_jobID;
        int m_processID;
        Command* m_command;    
    public:
        JobEntry(int id, int pid, Command* cmd, bool stopped);

        /* Setters & Getters */
        void setJobID(int id);
        void setProcessID(int id);
        void setCommand(Command* cmd);
        int getJobID() const;
        int getProcessID() const;
        Command* getCommand() const;


    };
    // TODO: Add your data members
public:
    JobsList();

    ~JobsList();

    void addJob(Command *cmd, int jobPid, bool isStopped = false);

    void printJobsList();
    void printJobsListWithPid();
    void killAllJobs();
    void removeFinishedJobs();
    void removeJobById(int jobId);
    JobEntry *getJobById(int jobId);
    JobEntry *getJobByPid(int pid);
    JobEntry *getLastJob();
    bool isEmpty();
    int getNextJobID() const;
    int getNumRunningJobs() const;

protected:
    vector<JobEntry>* m_jobEntries;
    int m_nextJobID;
    int m_numRunningJobs;
};

class JobsCommand : public BuiltInCommand {
protected:
    JobsList* m_jobsList;

public:
    JobsCommand(const char* origin_cmd_line, const char *cmd_line, JobsList *jobs);

    virtual ~JobsCommand() {}

    void execute() override;
};

class KillCommand : public BuiltInCommand {
protected:
    JobsList* m_jobsList;
    class InvalidArgument : public exception{};
public:
    KillCommand(const char* origin_cmd_line, const char *cmd_line, JobsList *jobs);

    virtual ~KillCommand() {}

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
protected:
    JobsList* m_jobsList;

public:
    ForegroundCommand(const char* origin_cmd_line, const char *cmd_line, JobsList *jobs);

    virtual ~ForegroundCommand() {}
    void execute() override;
};

class ListDirCommand : public BuiltInCommand {
protected:
    struct linux_dirent {
        long d_ino;
        off_t d_off;
        unsigned short d_reclen;
        char d_name[];
    };
public:
    ListDirCommand(const char* origin_cmd_line, const char *cmd_line);

    virtual ~ListDirCommand() {}

    void execute() override;
    void sortEntreysAlphabetically(vector<string>& dir, int nread, vector<char> buffer);
    void printContent(vector<string> entries, const char* directoryPath, vector<char> buffer, bool files);
};

class GetUserCommand : public BuiltInCommand {
public:
    GetUserCommand(const char *origin_cmd_line, const char *cmd_line);

    virtual ~GetUserCommand() {}

    void execute() override;
    void printUserByPid (pid_t pid);
};

class aliasCommand : public BuiltInCommand {
private:
    string m_name;
    string m_command;
public:
    aliasCommand(const char* origin_cmd_line, const char *cmd_line);
    virtual ~aliasCommand() {}

    void execute() override;
};

class unaliasCommand : public BuiltInCommand {
public:
    unaliasCommand(const char* origin_cmd_line, const char *cmd_line);

    virtual ~unaliasCommand() {}

    void execute() override;
};

class ChangePromptCommand : public BuiltInCommand {
public:
    ChangePromptCommand(const char* origin_cmd_line, const char *cmd_line);

    virtual ~ChangePromptCommand() {}

    void execute() override;
};

class SmallShell {
private:
    SmallShell();

    pid_t m_fg_process;
    string m_prompt;
    char* m_plastPwd;
    JobsList* m_jobList;
    bool m_proceed;
    bool m_stopWatch;
    map<string, string>* m_alias;
    vector<string> m_aliasToPrint;

public:
    const static set<string> COMMANDS;
    bool toProceed () const;
    void quit ();

    void setPrompt(const string str);
    void setStopWatch(bool status);
    void setForegroundProcess(pid_t pid);
    string getPrompt() const;
    bool getStopWatch() const;
    pid_t getForegroundProcess() const;
    void setPlastPwdPtr(char * newPwd);
    char* getPlastPwdPtr();

    void addAlias (string name, string command, string originCommand);
    void removeAlias (vector<string>args);
    void printAlias();

    Command *CreateCommand(const char *cmd_line);
    char* extractCommand(const char* cmd_l,string &firstWord);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }
    ~SmallShell();
    

    void executeCommand(const char *cmd_line);
    void printToTerminal(string line);

    JobsList* getJobsList();
};

#endif //SMASH_COMMAND_H_
