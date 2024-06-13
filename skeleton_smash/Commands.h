#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define CD_COMMAND_ARGS_NUM (2)
#define DEFAULT_JOB_ID (1)
#define CHILD_ID (0)
#define ERROR_VALUE (-1)

using namespace std;

class Command {
// TODO: Add your data members
public:
    Command(const char *cmd_line, bool isBgCmd = false);
    virtual ~Command();

    virtual void execute() = 0;
    virtual Command* clone() const = 0;

    /* Args Methods */
    int getArgCount() const;
    vector<std::string> getArgs() const;
    string getCommand() const;
    bool isBackgroundCommand() const;

    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed
protected:
    string m_cmd_string;
    bool m_bgCmd;    
};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char *cmd_line);
    virtual ~BuiltInCommand() {}

};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char *cmd_line, bool isBgCmd);
    Command* clone() const override;
    virtual ~ExternalCommand() {}

    void execute() override;
    void runSimpleCommand(const string& cmd);
    void runComplexCommand(const string& cmd);
    vector<string> splitCommand(const string& cmd);
};

class PipeCommand : public Command {
    // TODO: Add your data members
public:
    PipeCommand(const char *cmd_line);
    Command* clone() const override;

    virtual ~PipeCommand() {}

    void execute() override;
};

class WatchCommand : public Command {
    // TODO: Add your data members
public:
    WatchCommand(const char *cmd_line);
    Command* clone() const override;

    virtual ~WatchCommand() {}

    void execute() override;
};

class RedirectionCommand : public Command {
    // TODO: Add your data members
public:
    explicit RedirectionCommand(const char *cmd_line);
    Command* clone() const override;

    virtual ~RedirectionCommand() {}

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
    ChangeDirCommand(const char *cmd_line, char **plastPwd);
    Command* clone() const override;

    virtual ~ChangeDirCommand() {}

    void execute() override;

protected:
    char **plastPwd; // Pointer to the previous working directory
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char *cmd_line);
    Command* clone() const override;

    virtual ~GetCurrDirCommand() {}

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char *cmd_line);
    Command* clone() const override;

    virtual ~ShowPidCommand() {}

    void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
// TODO: Add your data members public:
    QuitCommand(const char *cmd_line, JobsList *jobs);
    Command* clone() const override;

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
        bool m_isStopped;
    
    public:
        JobEntry(int id, int pid, Command* cmd, bool stopped);

        /* Setters & Getters */
        void setJobID(int id);
        void setCommand(Command* cmd);
        void setStopped(bool stopped);
        int getJobID() const;
        Command* getCommand() const;
        bool isStopped() const;


    };
    // TODO: Add your data members
public:
    JobsList();

    ~JobsList();

    void addJob(Command *cmd, int jobPid, bool isStopped = false);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry *getLastJob(int *lastJobId);

    JobEntry *getLastStoppedJob(int *jobId);
    // TODO: Add extra methods or modify exisitng ones as needed

    int getNextJobID() const;

protected:
    vector<JobEntry>* m_jobEntries;
    int m_nextJobID;
};

class JobsCommand : public BuiltInCommand {
protected:
    JobsList* m_jobsList;

public:
    JobsCommand(const char *cmd_line, JobsList *jobs);
    Command* clone() const override;
    virtual ~JobsCommand() {}

    void execute() override;
};

class KillCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    KillCommand(const char *cmd_line, JobsList *jobs);
    Command* clone() const override;
    virtual ~KillCommand() {}

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs);
    Command* clone() const override;
    virtual ~ForegroundCommand() {}

    void execute() override;
};

class ListDirCommand : public BuiltInCommand {
public:
    ListDirCommand(const char *cmd_line);

    virtual ~ListDirCommand() {}

    void execute() override;
};

class GetUserCommand : public BuiltInCommand {
public:
    GetUserCommand(const char *cmd_line);

    virtual ~GetUserCommand() {}

    void execute() override;
};

class aliasCommand : public BuiltInCommand {
public:
    aliasCommand(const char *cmd_line);

    virtual ~aliasCommand() {}

    void execute() override;
};

class unaliasCommand : public BuiltInCommand {
public:
    unaliasCommand(const char *cmd_line);

    virtual ~unaliasCommand() {}

    void execute() override;
};

class SmallShell {
private:
    // TODO: Add your data members
    SmallShell();
    char* m_plastPwd;
    JobsList* m_jobList;

public:
    Command *CreateCommand(const char *cmd_line);

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

    char** getPlastPwdPtr();
    JobsList* getJobsList();
};

#endif //SMASH_COMMAND_H_
