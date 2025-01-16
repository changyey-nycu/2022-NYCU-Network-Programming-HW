#include <iostream>
#include <fcntl.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
using namespace std;

// find the index of sep in massage
// return the number of sep + 1, found initial to npos, last found is the last index + 1 of massage
int find_position(string massage, size_t found[], string sep);
// delete continuous space and begin and end space in massage
string standardize(string massage);
void sig_handler(int sig);

int main()
{
    // set initial env variable
    setenv("PATH", "bin:.", 1);

    //  number pipe
    int numberPipeWriteFlag = 0;
    int numberPipeReadFlag = 0;
    vector<int> numberPipeRead, numberPipeWrite;
    vector<int> numberPipeCounter;
    int newNumberPipe[2];
    int numberPipeReadIndex = 0;
    int numberPipeWriteIndex = 0;

    //  number error pipe
    int numberErrorPipeWriteFlag = 0;
    string line;
    cout << "% ";
    while (getline(cin, line))
    {
        if (line.compare("exit") == 0)
            exit(0);
        line = standardize(line);
        if (line == "")
        {
            cout << "% ";
            continue;
        }

        size_t found[15000];
        int count = find_position(line, found, " ");
        vector<string> arg;
        for (int i = 0; i < count; i++)
            arg.push_back(line.substr(found[i] + 1, found[i + 1] - found[i] - 1));
        //  count for number pipe and set numberPipeFlag
        numberPipeReadFlag = 0;
        numberPipeReadIndex = 0;
        for (int i = 0; i < numberPipeCounter.size(); i++)
        {
            if (!numberPipeWriteFlag)
                numberPipeCounter[i] = numberPipeCounter[i] - 1;
            if (numberPipeCounter[i] == 0)
            {
                numberPipeReadFlag = 1;
                numberPipeReadIndex = i;
            }
        }

        //  normal pipe
        int readPipeFlag = 0;
        int writePipeFlag = 0;
        int newPipe[2];
        vector<int> normalPipeRead, normalPipeWrite;
        int curPipeIndex = 0;
        int prePipeIndex = 0;

        vector<pid_t> pidList;
        vector<pid_t> numberPidList;
        int arg_iter = 0;
        while (arg_iter < arg.size())
        {
            // init file and Pipe Write Flag
            int file_redirection = 0;
            string file_name = "";
            writePipeFlag = 0;
            numberPipeWriteFlag = 0;
            numberErrorPipeWriteFlag = 0;

            // save arg to argv
            char **argv;
            argv = new char *[arg.size() + 1];
            string str_prog = arg[arg_iter];

            int arg_num = 0;
            while (arg_iter < arg.size())
            {
                // file redirection
                if (arg[arg_iter] == ">")
                {
                    file_redirection = 1;
                    file_name = arg[arg_iter + 1];
                    arg_iter += 2;
                    continue; // break;
                }
                // normal pipe or number pipe
                if (arg[arg_iter].find("|") != std::string::npos || arg[arg_iter].find("!") != std::string::npos)
                {
                    if (arg[arg_iter].compare("|") == 0) // normal pipe
                        writePipeFlag = 1;
                    else // number pipe
                    {
                        string temp = arg[arg_iter].substr(1, arg[arg_iter].size() - 1);
                        int t = stoi(temp);
                        numberPipeCounter.push_back(t);
                        numberPipeWriteFlag = 1;
                        if (arg[arg_iter].find("!") != std::string::npos) // Error Pipe
                            numberErrorPipeWriteFlag = 1;
                    }

                    break;
                }
                int len = arg[arg_iter].size();
                argv[arg_num] = new char[len + 1];
                const char *temp = arg[arg_iter].c_str();
                strcpy(argv[arg_num++], temp);
                arg_iter++;
            }
            argv[arg_num] = NULL;

            char *prog = argv[0];
            // built in command
            if (str_prog == "setenv")
            {
                if (arg.size() > 1)
                    setenv(argv[1], argv[2], 1);
                for (int i = 0; i < arg.size(); i++)
                    delete[] argv[i];
                delete[] argv;
                break;
            }
            else if (str_prog == "printenv")
            {

                char *temp = getenv(argv[1]);
                if (temp != NULL)
                    cout << temp << endl;
                
                for (int i = 0; i < arg.size(); i++)
                    delete[] argv[i];
                delete[] argv;
                break;
            }

            // create pipe
            prePipeIndex = curPipeIndex;
            if (writePipeFlag) // normal pipe
            {
                pipe(newPipe);
                normalPipeRead.push_back(newPipe[0]);
                normalPipeWrite.push_back(newPipe[1]);
                curPipeIndex = normalPipeRead.size() - 1;
            }
            else if (numberPipeWriteFlag) // number pipe
            {
                int counter = numberPipeCounter.back();
                int same = 0;
                for (int i = 0; i < numberPipeCounter.size() - 1; i++)
                {
                    if (counter == numberPipeCounter[i])
                    {
                        same = 1;
                        numberPipeWriteIndex = i;
                        numberPipeCounter.pop_back();
                        break;
                    }
                }
                if (!same)
                {
                    pipe(newNumberPipe);
                    numberPipeRead.push_back(newNumberPipe[0]);
                    numberPipeWrite.push_back(newNumberPipe[1]);
                    numberPipeWriteIndex = numberPipeRead.size() - 1;
                }
            }
            // cout << "--------before fork-------" << endl;
            // cout << "normal pipe: read " << readPipeFlag << " write " << writePipeFlag << endl;
            // cout << "number pipe: read " << numberPipeReadFlag << " write " << numberPipeWriteFlag << " index " << numberPipeReadIndex << endl;
            // cout << "--------before fork-------" << endl;
            // fork child
            pid_t pid;
            while ((pid = fork()) < 0)
                wait(NULL);

            if (pid == 0) // child process
            {
                if (readPipeFlag)
                {
                    close(STDIN_FILENO);
                    dup(normalPipeRead[prePipeIndex]);
                    close(normalPipeWrite[prePipeIndex]);
                }
                else if (numberPipeReadFlag)
                {
                    close(STDIN_FILENO);
                    dup(numberPipeRead[numberPipeReadIndex]);
                    close(numberPipeWrite[numberPipeReadIndex]);
                }

                if (file_redirection)
                    freopen(file_name.c_str(), "w", stdout);
                else if (writePipeFlag)
                {
                    close(STDOUT_FILENO);
                    dup(normalPipeWrite.back());
                    close(normalPipeRead.back());
                }
                else if (numberPipeWriteFlag)
                {
                    close(STDOUT_FILENO);
                    dup(numberPipeWrite[numberPipeWriteIndex]);
                    if (numberErrorPipeWriteFlag)
                    {
                        close(STDERR_FILENO);
                        dup(numberPipeWrite[numberPipeWriteIndex]);
                    }
                    close(numberPipeRead[numberPipeWriteIndex]);
                }
                if (execvp(prog, argv) == -1)
                    cerr << "Unknown command: [" << prog << "]." << endl;
                exit(0);
            }
            // parent process
            if (numberPipeWriteFlag)
                numberPidList.push_back(pid);
            else
                pidList.push_back(pid);

            if (readPipeFlag)
            {
                close(normalPipeRead[prePipeIndex]);
                close(normalPipeWrite[prePipeIndex]);
            }
            else if (numberPipeReadFlag)
            {
                close(numberPipeRead[numberPipeReadIndex]);
                close(numberPipeWrite[numberPipeReadIndex]);
            }

            if (writePipeFlag)
                readPipeFlag = 1;
            else
                readPipeFlag = 0;

            //  count for number pipe and set numberPipeFlag number error pipe
            numberPipeReadFlag = 0;
            numberPipeReadIndex = 0;
            if (numberPipeWriteFlag)
            {
                for (int i = 0; i < numberPipeCounter.size(); i++)
                {
                    numberPipeCounter[i] = numberPipeCounter[i] - 1;
                    if (numberPipeCounter[i] == 0)
                    {
                        numberPipeReadFlag = 1;
                        numberPipeReadIndex = i;
                    }
                }
            }

            for (int i = 0; i < arg_num; i++)
                delete[] argv[i];
            delete[] argv;
            arg_iter++;
        }

        if (!pidList.empty())
        {
            pid_t pid = waitpid(pidList.back(), 0, 0);
            // cout << "wait " << pid << " complete\n";
        }

        signal(SIGCHLD, sig_handler);

        cout << "% ";
    }

    return 0;
}

int find_position(string message, size_t found[], string sep)
{
    int count = 0;
    found[0] = -1;
    while ((found[count + 1] = message.find(sep, found[count] + 1)) != string::npos)
        count++;
    found[++count] = message.length();
    found[count + 1] = string::npos;
    return count;
}

string standardize(string message)
{
    size_t found;
    while ((found = message.find("  ")) != string::npos)
    {
        message = message.replace(found, 2, " ");
        // cout << message << endl;
    }

    if (!message.empty() && message[0] == ' ')
        message.erase(message.begin());
    if (!message.empty() && message[message.size() - 1] == ' ')
        message.erase(message.end() - 1);
    return message;
}

void sig_handler(int sig)
{
    pid_t pid = wait(NULL);
    // cout << "sig : wait " << pid << " complete\n";
}