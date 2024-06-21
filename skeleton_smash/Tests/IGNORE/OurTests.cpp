#include <functional>
#include <cassert>
#include <sstream>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

/*---------------------------------------------------------------------------------------------------*/
/*------------------------------------------ Declerations -------------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/
using namespace std;

bool compare(string a, string b);
string trimLastFolder(const string& path);

namespace BuiltInCommands {
	bool RunPwdCommand();
	bool RunCdCommand();
	}

function<bool()> testsList[] = {
	BuiltInCommands::RunPwdCommand,
	BuiltInCommands::RunCdCommand
};

const int NUMBER_OF_TESTS = sizeof(testsList)/sizeof(function<bool()>);

/*---------------------------------------------------------------------------------------------------*/
/*--------------------------------------------- Tests -----------------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/
bool BuiltInCommands::RunPwdCommand(){
	// Create Smash
    SmallShell &smash = SmallShell::getInstance();

    // Redirect cout to a stringstream
    stringstream buffer;
    streambuf* oldCoutBuffer = cout.rdbuf(buffer.rdbuf());
    string output;

    // Regular Test
    const char* pwdCmd = "pwd";
    smash.executeCommand(pwdCmd); 

    // Check test resault
    output = buffer.str();
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != nullptr)
        assert(compare(output,string(cwd)));

    // Clear the contents of the stringstream
    buffer.str("");

    // Test an invalid command (edge case)
    const char* invalidCmd = "invalid_command";  // An invalid command
    smash.executeCommand(invalidCmd);

    // Check test resault
    output = buffer.str();
    assert(compare(output,string("Unknown Command")));

    // Clear the contents of the stringstream
    buffer.str("");

    // Restore cout back to its original output stream
    std::cout.rdbuf(oldCoutBuffer);

    return true;
}

bool BuiltInCommands::RunCdCommand(){
	// Create Smash
    SmallShell &smash = SmallShell::getInstance();

    // Redirect cout to a stringstream
    stringstream buffer;
    streambuf* oldCoutBuffer = cerr.rdbuf(buffer.rdbuf());

    // Variable decleration
    string output;
    string oldPath;
    string newPath;
    char cwd[1024];

    // OLDPWD not set Test
    const char* cdCmd = "cd -";
    smash.executeCommand(cdCmd); 

    // Check test resault
    output = buffer.str();
    assert(compare(output,string("smash error: cd: OLDPWD not set")));

    // Clear the contents of the stringstream
    buffer.str("");

    // cd .. Test
    cdCmd = "cd ..";
    if (getcwd(cwd, sizeof(cwd)) != nullptr)
        oldPath = string(cwd);
    newPath = trimLastFolder(oldPath);
    smash.executeCommand(cdCmd); 

    // Check test resault
    output = buffer.str();
    if (getcwd(cwd, sizeof(cwd)) != nullptr)
        assert(compare(newPath,string(cwd)));

    // Clear the contents of the stringstream
    buffer.str("");

    // cd - Test
    cdCmd = "cd -";
    smash.executeCommand(cdCmd); 
    if (getcwd(cwd, sizeof(cwd)) != nullptr)
        newPath = string(cwd);
        
    // Check test resault
    output = buffer.str();
    if (getcwd(cwd, sizeof(cwd)) != nullptr)
        assert(compare(newPath,oldPath));

    // Clear the contents of the stringstream
    buffer.str("");

    // Restore cout back to its original output stream
    std::cerr.rdbuf(oldCoutBuffer);

    return true;
}

void runTest(function<bool()> testFunction, string testName)
{	
	if(!testFunction()){
		cout << testName <<" FAILED." << endl;
	}
	else {
		cout << testName << " SUCCEEDED." << endl;
	}
	cout << endl;
}

/*---------------------------------------------------------------------------------------------------*/
/*--------------------------------------------- Utils -----------------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/
bool compare(string a, string b){
    // Check if the string is not empty and remove the newline character
    if (!a.empty() && a.back() == '\n') 
        a.resize(a.size() - 1); // Remove the newline character from the end

    // Check if the string is not empty and remove the newline character
    if (!b.empty() && b.back() == '\n') 
        b.resize(b.size() - 1); // Remove the newline character from the end
    return a==b;
}

string trimLastFolder(const string& path) {
    size_t lastSlash = path.find_last_of('/');
    
    if (lastSlash != string::npos) {
        return path.substr(0, lastSlash);
    }

    return path; // Return the original path if no last folder exists
}

/*---------------------------------------------------------------------------------------------------*/
/*--------------------------------------------- Main -----------------------------------------------*/
/*---------------------------------------------------------------------------------------------------*/
int main2(){
    if (signal(SIGINT, ctrlCHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }

	for (int i = 0; i < NUMBER_OF_TESTS; ++i) {
		runTest(testsList[i], "Test " + to_string(i + 1));
	}
    return true;
}