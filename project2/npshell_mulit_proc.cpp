#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <semaphore.h>
#include <vector>

#include "npshell_multi_proc.h"
using namespace std;
void *infoAddr;
void *broAddr;
struct READUSERPIPE *readUserPipeList = NULL;
struct NUMBERPIPE *numberpipe = NULL;
sem_t *semW = sem_open("sem1", O_CREAT, 0600, 1);
sem_t *semR = sem_open("sem2", O_CREAT, 0600, 1);

int npshell(int uid, int sock)
{
    // initial envirnment variable
    setenv("PATH", "bin:.", 1);

    // int v;
    // cout << "sem value:" << sem_getvalue(semChild, &v) << " " << v << endl;
    // sem_post(semChild);

    // user pipe read signal
    struct sigaction act;
    act.sa_sigaction = getUserPipeSig;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    if (sigaction(SIGUSR2, &act, NULL) == -1)
    {
        cerr << "signal error";
        exit(1);
    }

    send(sock, "% ", strlen("% "), 0);
    char buffer[15000];
    while (recv(sock, buffer, sizeof(buffer), 0))
    {
        string line = buffer;
        execCommand(uid, sock, line);
        memset(buffer, 0, sizeof(buffer));
        if (!line.empty())
            send(sock, "% ", strlen("% "), 0);
    }
    return 0;
}

int execCommand(int uid, int sock, string oriMsg)
{
    pipeFlag readFlag = standard;
    pipeFlag writeFlag = standard;

    if (oriMsg.back() == '\n' || oriMsg.back() == '\r')
        oriMsg.pop_back();
    if (oriMsg.back() == '\r' || oriMsg.back() == '\n')
        oriMsg.pop_back();

    string line = standardize(oriMsg);

    if (line.compare("exit") == 0)
    {
        exitChild(uid);
        exit(0);
    }

    if (line == "")
    {
        // send(sock, "% ", strlen("% "), 0);
        return 0;
    }

    vector<string> arg;
    argSep(arg, line);

    // normal pipe
    int newPipe[2];
    vector<int> normalPipeRead, normalPipeWrite;
    int curPipeIndex = 0;
    int prePipeIndex = 0;
    vector<pid_t> pidList;

    // number pipe
    struct NUMBERPIPE *writeNumberPipe; // indicate the number pipe should write
    struct NUMBERPIPE *readNumberPipe;  // indicate the number pipe should read

    // user pipe
    int userPipeWriteId = 0;
    int userPipeReadId = 0;
    int readUserPipeIndex = 0;
    struct READUSERPIPE *readUserPipe = NULL;
    // update number pipe counter
    for (struct NUMBERPIPE *i = numberpipe; i != NULL; i = i->next)
    {
        i->numberPipeCounter = i->numberPipeCounter - 1;
        if (i->numberPipeCounter == 0)
        {
            readFlag = number;
            readNumberPipe = i;
            // cout << "read" << readNumberPipe->numberPipeRead << readNumberPipe->numberPipeWrite << endl;
        }
    }
    // built in function
    string str_prog = arg[0];
    if (str_prog == "who")
    {
        who(uid);
        return 0;
    }
    else if (str_prog == "tell")
    {
        tell(uid, stoi(arg[1]), oriMsg);
        return 0;
    }
    else if (str_prog == "yell")
    {
        yell(uid, oriMsg);
        return 0;
    }

    int arg_iter = 0;
    while (arg_iter < arg.size())
    {
        // user pipe error handle
        int userPipeReadError = 0;
        int userPipeWriteError = 0;
        // file
        int file_redirection = 0;
        string file_name = "";
        // save arg to argv
        char **argv;
        argv = new char *[arg.size() + 1];
        str_prog = arg[arg_iter];

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
            else if (arg[arg_iter].find("|") != std::string::npos || arg[arg_iter].find("!") != std::string::npos) // write normal pipe or number pipe
            {
                if (arg[arg_iter].compare("|") == 0) // normal pipe
                    writeFlag = normal;
                else // number pipe
                {
                    writeFlag = number;
                    string temp = arg[arg_iter].substr(1, arg[arg_iter].size() - 1);
                    int t = stoi(temp);

                    writeNumberPipe = new struct NUMBERPIPE;
                    writeNumberPipe->numberPipeCounter = t;
                    writeFlag = number;
                    if (arg[arg_iter].find("!") != std::string::npos) // Error Pipe
                        writeFlag = error;
                }
                break;
            }
            else if (arg[arg_iter].find(">") != std::string::npos) // user pipe write
            {
                writeFlag = user;
                string temp = arg[arg_iter].substr(1, arg[arg_iter].size() - 1);
                int t = stoi(temp);
                userPipeWriteId = t;
                arg[arg_iter] = "";
            }

            if (arg[arg_iter].find("<") != std::string::npos) // user pipe write
            {
                readFlag = user;
                string temp = arg[arg_iter].substr(1, arg[arg_iter].size() - 1);
                int t = stoi(temp);
                userPipeReadId = t;
                arg[arg_iter] = "";
            }
            int len = arg[arg_iter].size();
            argv[arg_num] = new char[len + 1];
            if (arg[arg_iter] == "")
            {
                argv[arg_num++] = NULL;
            }
            else
            {
                const char *temp = arg[arg_iter].c_str();
                strcpy(argv[arg_num++], temp);
            }
            arg_iter++;
        }
        argv[arg_num] = NULL;

        // built in command
        if (str_prog == "setenv")
        {
            if (arg.size() > 2)
                setenv(argv[1], argv[2], 1);
            for (int i = 0; i < arg.size(); i++)
                delete[] argv[i];
            delete[] argv;
            break;
        }
        else if (str_prog == "printenv")
        {
            if (arg.size() > 1)
            {
                char *temp = getenv(argv[1]);
                if (temp != NULL)
                    cout << temp << endl;
            }
            for (int i = 0; i < arg.size(); i++)
                delete[] argv[i];
            delete[] argv;
            break;
        }
        else if (str_prog == "name")
        {
            name(uid, argv[1]);
            for (int i = 0; i < arg.size(); i++)
                delete[] argv[i];
            delete[] argv;
            return 0;
        }

        char *prog = argv[0];

        // get user info
        vector<struct USERINFO> userinfo = getInfoList(infoAddr);
        int userIndex = getUserIndexById(uid, userinfo);
        if (userIndex == -1)
        {
            cerr << "no user";
            exit(1);
        }
        int userUid = uid;

        // user pipe file name
        int userReadFd = -1;
        int userWriteFd = -1;
        // cout << "uid:" << uid << " write to:" << userPipeWriteId << endl;
        string readFIFO = "./user_pipe/FIFO" + to_string(userPipeReadId) + to_string(uid);
        string writeFIFO = "./user_pipe/FIFO" + to_string(uid) + to_string(userPipeWriteId);
        // cout << writeFIFO.c_str() << endl;

        // user pipe check "pipe already exists, pipe does not exist, sender or receiver does not exist"
        if (readFlag == user)
        {
            // sender does not exist
            int exist = 0;
            for (size_t i = 0; i < userinfo.size(); i++)
            {
                if (userPipeReadId == userinfo[i].uid)
                {
                    exist = 1;
                    break;
                }
            }
            if (!exist)
            {
                userPipeReadError = 1;
                string str = "*** Error: user #" + to_string(userPipeReadId) + " does not exist yet. ***\n";
                const char *buf = str.c_str();
                send(sock, buf, strlen(buf), 0);
            }
            else
            {
                // pipe not exist
                int pipeExist = 0;
                for (struct READUSERPIPE *i = readUserPipeList; i != NULL; i = i->next)
                {
                    if (userPipeReadId == i->formId)
                    {
                        pipeExist = 1;
                        // get the read socket item index
                        readUserPipe = i;
                        break;
                    }
                }
                if (!pipeExist)
                {
                    userPipeReadError = 1;
                    string str = "*** Error: the pipe #" + to_string(userPipeReadId) + "->#" + to_string(userinfo[userIndex].uid) + " does not exist yet. ***\n";
                    const char *buf = str.c_str();
                    send(sock, buf, strlen(buf), 0);
                }
            }
        }
        // receive user pipe broadcast
        if (readFlag == user && userPipeReadError == 0)
        {
            string from = to_string(readUserPipe->formId);
            string to = to_string(readUserPipe->toId);
            int tindex = userIndex;
            int findex = getUserIndexById(readUserPipe->formId, userinfo);
            string str = "*** " + userinfo[tindex].name + " (#" + to + ") just received from " + userinfo[findex].name + " (#" + from + ") by '" + oriMsg + "' ***\n";
            broadcast(str, semW, semR);
        }

        // cout << "test";
        // create pipe
        prePipeIndex = curPipeIndex;

        switch (writeFlag)
        {
        case standard:
            break;
        case normal: // normal pipe
            if (pipe(newPipe) < 0)
            {
                cerr << "can not create new pipe" << endl;
                exit(1);
            }
            normalPipeRead.push_back(newPipe[0]);
            normalPipeWrite.push_back(newPipe[1]);
            curPipeIndex = normalPipeRead.size() - 1;
            break;
        case number:
        case error: // number and error pipe
        {
            int counter = writeNumberPipe->numberPipeCounter;
            int same = 0;
            for (struct NUMBERPIPE *i = numberpipe; i != NULL; i = i->next)
            {
                if (counter == i->numberPipeCounter)
                {
                    same = 1;
                    writeNumberPipe = i;
                    break;
                }
            }
            if (!same)
            {
                if (pipe(newPipe) < 0)
                {
                    cerr << "can not create new pipe" << endl;
                    exit(1);
                }
                writeNumberPipe->numberPipeRead = newPipe[0];
                writeNumberPipe->numberPipeWrite = newPipe[1];
                writeNumberPipe->next = numberpipe;
                numberpipe = writeNumberPipe;
            }
            // cout << "write" << writeNumberPipe->numberPipeRead << writeNumberPipe->numberPipeWrite << endl;
            break;
        }
        case user:
            break;
        default:
            break;
        }

        // write user pipe
        if (writeFlag == user)
        {
            // receiver does not exist
            int exist = 0;
            for (size_t i = 0; i < userinfo.size(); i++)
            {
                if (userPipeWriteId == userinfo[i].uid)
                {
                    exist = 1;
                    break;
                }
            }
            if (!exist)
            {
                userPipeWriteError = 1;
                string str = "*** Error: user #" + to_string(userPipeWriteId) + " does not exist yet. ***\n";
                const char *buf = str.c_str();
                send(sock, buf, strlen(buf), 0);
            }
            else
            {
                // check pipe already exist, 0:success, -1:fail
                int pipeExist = mkfifo(writeFIFO.c_str(), 0666);
                // cout << "open FIFO" << pipeExist << endl;
                if (pipeExist == -1)
                {
                    if (errno == EEXIST)
                    {
                        userPipeWriteError = 1;
                        string str = "*** Error: the pipe #" + to_string(userUid) + "->#" + to_string(userPipeWriteId) + " already exists. ***\n";
                        const char *buf = str.c_str();
                        send(sock, buf, strlen(buf), 0);
                    }
                    else
                    {
                        cerr << "mkfifo error" << endl;
                        exit(1);
                    }
                }
            }
        }

        // broadcast write user pipe
        if (writeFlag == user && userPipeWriteError == 0)
        {
            // send signal to other process
            int sendInt = uid * 100 + userPipeWriteId;
            union sigval mysigval;
            mysigval.sival_int = sendInt;
            int opIndex = getUserIndexById(userPipeWriteId, userinfo);
            // cout << "send signal" << sendInt << "to" << userinfo[opIndex].pid << endl;
            sigqueue(userinfo[opIndex].pid, SIGUSR2, mysigval);

            // open the fifo file
            userWriteFd = open(writeFIFO.c_str(), O_WRONLY);
            // cout << " success fd:" << userWriteFd << endl;

            string from = to_string(uid);
            string to = to_string(userPipeWriteId);
            int findex = userIndex;
            int tindex = getUserIndexById(userPipeWriteId, userinfo);
            string str = "*** " + userinfo[findex].name + " (#" + from + ") just piped '" + oriMsg + "' to " + userinfo[tindex].name + " (#" + to + ") ***\n";
            broadcast(str, semW, semR);
        }

        // cout << "flag state read:" << readFlag << " write :" << writeFlag << endl;
        pid_t pid;
        while ((pid = fork()) < 0)
            wait(NULL);

        if (pid == 0) // child process
        {
            // dup2(sock, STDOUT_FILENO);
            // dup2(sock, STDERR_FILENO);
            // read direction
            switch (readFlag)
            {
            case standard:
                break;
            case normal: // normal pipe
                dup2(normalPipeRead[prePipeIndex], STDIN_FILENO);
                close(normalPipeWrite[prePipeIndex]);
                break;
            case number: // number pipe
                dup2(readNumberPipe->numberPipeRead, STDIN_FILENO);
                close(readNumberPipe->numberPipeWrite);
                break;
            case user:
                if (userPipeReadError == 1) // error
                {
                    int devNULLr = open("/dev/null", 0);
                    dup2(devNULLr, STDIN_FILENO);
                    close(devNULLr);
                }
                else
                {
                    dup2(readUserPipe->fd, STDIN_FILENO);
                    close(readUserPipe->fd);
                }
                break;
            default:
                break;
            }

            // write direction
            if (file_redirection)
                freopen(file_name.c_str(), "w", stdout);
            else
                switch (writeFlag)
                {
                case standard:
                    break;
                case normal: // normal pipe
                    dup2(normalPipeWrite.back(), STDOUT_FILENO);
                    close(normalPipeRead.back());
                    break;
                case number: // number pipe
                    dup2(writeNumberPipe->numberPipeWrite, STDOUT_FILENO);
                    close(writeNumberPipe->numberPipeRead);
                    break;
                case error:
                    dup2(writeNumberPipe->numberPipeWrite, STDOUT_FILENO);
                    dup2(writeNumberPipe->numberPipeWrite, STDERR_FILENO);
                    close(writeNumberPipe->numberPipeRead);
                    break;
                case user:
                    if (userPipeWriteError == 1) // error
                    {
                        int devNULLw = open("/dev/null", O_WRONLY);
                        dup2(devNULLw, STDOUT_FILENO);
                        close(devNULLw);
                    }
                    else
                    {
                        dup2(userWriteFd, STDOUT_FILENO);
                        close(userWriteFd);
                    }
                    break;
                default:
                    break;
                }

            if (execvp(prog, argv) == -1)
                cerr << "Unknown command: [" << prog << "]." << endl;
            exit(0);
        }

        // parent process
        if (writeFlag == user && userPipeWriteError != 1)
        {
            close(userWriteFd);
        }

        if (writeFlag == standard || writeFlag == normal)
            pidList.push_back(pid);
        if (writeFlag == user && userPipeWriteError == 1)
            pidList.push_back(pid);

        // close pipe
        switch (readFlag)
        {
        case standard:
            break;
        case normal: // normal pipe
            close(normalPipeRead[prePipeIndex]);
            close(normalPipeWrite[prePipeIndex]);
            break;
        case number: // number pipe
        {
            // delete number pipe struct
            struct NUMBERPIPE *temp = readNumberPipe;
            if (numberpipe == temp)
            {
                numberpipe = numberpipe->next;
            }
            else
            {
                for (struct NUMBERPIPE *i = numberpipe; i != NULL; i = i->next)
                {
                    if (temp == i->next)
                    {
                        i->next = temp->next;
                        break;
                    }
                }
            }

            close(readNumberPipe->numberPipeWrite);
            close(readNumberPipe->numberPipeRead);
            delete readNumberPipe;
            // cout << "delete over" << endl;
            break;
        }
        case user:
            if (userPipeReadError == 0)
            {
                close(readUserPipe->fd);
            }
            break;
        default:
            break;
        }

        // delete user pipe from list
        if (userPipeReadError == 0 && readFlag == user)
        {
            if (readUserPipeList == readUserPipe)
                readUserPipeList = readUserPipeList->next;
            else
            {
                for (struct READUSERPIPE *i = readUserPipeList; i != NULL; i = i->next)
                {
                    if (i->next == readUserPipe)
                    {
                        // cout << "del user pipe " << it->formId << " to " << it->toId << endl;
                        i->next = readUserPipe->next;
                        break;
                    }
                }
            }
            unlink(readFIFO.c_str());
            delete readUserPipe;
        }

        // update number pipe counter
        readFlag = standard;
        if (writeFlag == number || writeFlag == error)
        {
            if (arg_iter + 1 < arg.size()) // not the tail of command
            {
                // cout << "not tail" << endl;
                for (struct NUMBERPIPE *i = numberpipe; i != NULL; i = i->next)
                {
                    i->numberPipeCounter = i->numberPipeCounter - 1;
                    if (i->numberPipeCounter == 0)
                    {
                        readFlag = number;
                        readNumberPipe = i;
                    }
                }
            }
        }

        // reset flag
        if (readFlag != number)
            if (writeFlag == normal)
                readFlag = normal;
            else
                readFlag = standard;
        writeFlag = standard;
        // cout << "counter :";
        // for (struct NUMBERPIPE *i = numberpipe; i != NULL; i = i->next)
        // {
        //     cout << i->numberPipeCounter << " ";
        // }
        // cout << endl;
        for (int i = 0; i < arg_num; i++)
            delete[] argv[i];
        delete[] argv;
        arg_iter++;
    }
    if (!pidList.empty())
    {
        waitpid(pidList.back(), 0, 0);
        // pid_t pid = waitpid(pidList.back(), 0, 0);
        // cout << "wait " << pid << " complete\n";
    }
    signal(SIGCHLD, sig_handler);
    // send(sock, "% ", strlen("% "), 0);
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
    if (sig == SIGCHLD)
    {
        wait(NULL);
    }
    // pid_t pid = wait(NULL);
    // cout << "sig : wait " << pid << " complete\n";
}

void argSep(vector<string> &arg, string line)
{
    size_t found[15000];
    int count = find_position(line, found, " ");

    for (int i = 0; i < count; i++)
    {
        string str = line.substr(found[i] + 1, found[i + 1] - found[i] - 1);
        arg.push_back(str);
    }
}

void broadcast(string msg, sem_t *semWrite, sem_t *semRead)
{
    // cout << "broadcast:" << msg;
    vector<struct USERINFO> userinfo = getInfoList(infoAddr);

    const char *buf = msg.c_str();
    sem_wait(semRead);
    sem_wait(semWrite);
    memset(broAddr, 0, sysconf(_SC_PAGESIZE));
    memcpy(broAddr, buf, strlen(buf));
    sem_post(semWrite);
    string broMsg = (char *)broAddr;
    union sigval mysigval;
    mysigval.sival_int = 0;

    if (userinfo.empty())
    {
        sem_post(semRead);
        return;
    }
    else if (userinfo.size() == 1)
    {
        // cout << "size 1" << endl;
        mysigval.sival_int = 1;
        sigqueue(userinfo[userinfo.size() - 1].pid, SIGUSR1, mysigval);
        return;
    }

    for (int i = 0; i < userinfo.size() - 1; i++)
    {
        sigqueue(userinfo[i].pid, SIGUSR1, mysigval);
    }

    // cout << "is empty: " << userinfo.empty() << endl;
    mysigval.sival_int = 1;

    sigqueue(userinfo[userinfo.size() - 1].pid, SIGUSR1, mysigval);

    // for (vector<struct USERINFO>::iterator it = userinfo.begin(); it != userinfo.end(); it++)
    // {
    //     // send signal to all process to read shared memory
    //     // cout << "send signal " << v << endl;
    //     sigqueue(it->pid, SIGUSR1, mysigval);
    // }
}

int getUserIndexById(int uid, vector<struct USERINFO> userinfo)
{
    for (int i = 0; i < userinfo.size(); i++)
    {
        if (userinfo[i].uid == uid)
            return i;
    }
    return -1;
}

void who(int uid)
{
    string str = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
    vector<struct USERINFO> userinfo = getInfoList(infoAddr);
    for (vector<struct USERINFO>::iterator it = userinfo.begin(); it != userinfo.end(); it++)
    {
        string userStr;
        if (it->uid == uid)
            userStr = to_string(it->uid) + "\t" + it->name + "\t" + it->addrPort + "\t" + "<-me\n";
        else
            userStr = to_string(it->uid) + "\t" + it->name + "\t" + it->addrPort + "\n";
        str = str + userStr;
    }
    const char *buf = str.c_str();
    write(STDOUT_FILENO, buf, strlen(buf));
}

void tell(int uid, int opId, string msg)
{
    // check user exist
    vector<struct USERINFO> userinfo = getInfoList(infoAddr);
    int exist = 0;
    for (vector<struct USERINFO>::iterator it = userinfo.begin(); it != userinfo.end(); it++)
    {
        if (opId == it->uid)
        {
            exist = 1;
            break;
        }
    }
    if (!exist)
    {
        string str;
        str = "*** Error: user #" + to_string(opId) + " does not exist yet. ***\n";
        const char *buf = str.c_str();
        write(STDOUT_FILENO, buf, strlen(buf));
        return;
    }
    else
    {
        int index = getUserIndexById(uid, userinfo);

        size_t start = msg.find(to_string(opId));
        start++;
        for (size_t i = start; i < msg.length(); i++)
        {
            char t = msg[i];
            if (t == ' ')
                continue;
            else
            {
                start = i;
                break;
            }
        }

        size_t msgLen = msg.length() - start;
        string sendingMsg = msg.substr(start, msgLen);

        string str = "*** " + userinfo[index].name + " told you ***: " + sendingMsg + "\n";
        int opIndex = getUserIndexById(opId, userinfo);
        // cout << str;
        const char *buf = str.c_str();
        memset(broAddr, 0, sysconf(_SC_PAGESIZE));
        memcpy(broAddr, buf, strlen(buf));
        string broMsg = (char *)broAddr;
        union sigval mysigval;
        mysigval.sival_int = 0;
        mysigval.sival_ptr = broAddr;
        sigqueue(userinfo[opIndex].pid, SIGUSR1, mysigval);
    }
}

void yell(int uid, string msg)
{
    vector<struct USERINFO> userinfo = getInfoList(infoAddr);
    int index = getUserIndexById(uid, userinfo);
    size_t start = msg.find("yell ");
    start = start + 5;
    for (size_t i = start; i < msg.length(); i++)
    {
        char t = msg[i];
        if (t == ' ')
            continue;
        else
        {
            start = i;
            break;
        }
    }
    size_t msgLen = msg.length() - start;
    string sendingMsg = msg.substr(start, msgLen);

    string str = "*** " + userinfo[index].name + " yelled ***: " + sendingMsg + "\n";
    // cout << str;
    broadcast(str, semW, semR);
}

void name(int uid, string name)
{
    vector<struct USERINFO> userinfo = getInfoList(infoAddr);
    for (vector<struct USERINFO>::iterator it = userinfo.begin(); it != userinfo.end(); it++)
    {
        if (it->name == name)
        {
            string str = "*** User '" + name + "' already exists. ***\n";
            const char *buf = str.c_str();
            write(STDOUT_FILENO, buf, strlen(buf));
            return;
        }
    }

    int index = getUserIndexById(uid, userinfo);
    userinfo[index].name = name;
    storeBack(userinfo);
    string str = "*** User from ";
    str = str + userinfo[index].addrPort + " is named '" + name + "'. ***\n";
    const char *buf = str.c_str();
    broadcast(str, semW, semR);
    return;
}

vector<struct USERINFO> getInfoList(void *addr)
{
    string nowUser = (char *)addr;
    vector<struct USERINFO> userinfo;
    vector<string> infoStr;
    sepMsg(infoStr, nowUser, "\n");
    infoStr.pop_back(); // because end with '\n', have a empty string

    for (vector<string>::iterator i = infoStr.begin(); i != infoStr.end(); i++)
    {
        string str = *i;
        vector<string> item;
        sepMsg(item, str, ","); // uid, name, addr+port, pid
        struct USERINFO info;
        info.uid = stoi(item[0]);
        info.name = item[1];
        info.addrPort = item[2];
        info.pid = stoi(item[3]);
        userinfo.push_back(info);
    }
    infoStr.clear();
    return userinfo;
}

void sepMsg(vector<string> &arg, string msg, string sep)
{
    size_t found[40];
    int count = find_position(msg, found, sep);
    for (int i = 0; i < count; i++)
    {
        string str = msg.substr(found[i] + 1, found[i + 1] - found[i] - 1);
        // cout << str << " ";
        arg.push_back(str);
    }
}

string vecToStr(vector<struct USERINFO> userinfo)
{
    string str = "";
    for (vector<struct USERINFO>::iterator it = userinfo.begin(); it != userinfo.end(); it++)
    {
        string str_uid = to_string(it->uid);
        string str_name = it->name;
        string str_addr_port = it->addrPort;
        string str_pid = to_string((int)it->pid);
        string info = str_uid + "," + str_name + "," + str_addr_port + "," + str_pid + "\n";
        str = str + info;
    }
    return str;
}

int exitChild(int uid)
{
    // delete number pipe
    for (struct NUMBERPIPE *i = numberpipe; i != NULL; i = i->next)
    {
        numberpipe = numberpipe->next;
        delete i;
    }

    vector<struct USERINFO> userinfo = getInfoList(infoAddr);
    int index = getUserIndexById(uid, userinfo);
    string username = userinfo[index].name;

    // delete user pipe readUserPipeList
    for (struct READUSERPIPE *i = readUserPipeList; i != NULL; i = i->next)
    {
        string readFIFO = "./user_pipe/FIFO" + to_string(i->formId) + to_string(uid);
        close(i->fd);
        unlink(readFIFO.c_str());
    }
    for (int i = 1; i <= 30; i++)
    {
        string FIFO = "./user_pipe/FIFO" + to_string(uid) + to_string(i);
        if (access(FIFO.c_str(), F_OK) == 0)
            unlink(FIFO.c_str());
    }

    // send signal to other process
    for (vector<struct USERINFO>::iterator it = userinfo.begin(); it != userinfo.end(); it++)
    {
        int sendInt = uid * 100 + it->uid;
        sendInt = -1 * sendInt;
        union sigval mysigval;
        mysigval.sival_int = sendInt;
        // cout << "send signal" << sendInt << "to" << it->pid << endl;
        sigqueue(it->pid, SIGUSR2, mysigval);
    }

    // delete userinfo from vector
    for (vector<struct USERINFO>::iterator it = userinfo.begin(); it != userinfo.end(); it++)
    {
        if (it->uid == userinfo[index].uid)
        {
            // cout << "del" << it->uid << endl;
            userinfo.erase(it);
            break;
        }
    }

    sem_wait(semW);
    // save vector back to shared memory
    storeBack(userinfo);
    sem_post(semW);

    string exitMsg;
    exitMsg = "*** User '" + username + "' left. ***\n";
    broadcast(exitMsg, semW, semR);

    // signal to close user pipe fd
    sem_close(semW);
    sem_close(semR);
    shmdt(infoAddr);
    shmdt(broAddr);
    exit(0);
}

void storeBack(vector<struct USERINFO> userinfo)
{
    string info = vecToStr(userinfo);
    const char *buf = info.c_str();
    memset(infoAddr, 0, sysconf(_SC_PAGESIZE));
    memcpy(infoAddr, buf, strlen(buf));
}

void getBroadcast(int signo, siginfo_t *info, void *ctx)
{
    if (signo == SIGUSR1)
    {
        int temp = info->si_value.sival_int;
        string broMsg = (char *)broAddr;
        const char *buf = broMsg.c_str();
        write(STDOUT_FILENO, buf, strlen(buf));
        if (temp == 1)
            sem_post(semR);
    }
}

void getUserPipeSig(int signo, siginfo_t *info, void *ctx)
{
    if (signo == SIGUSR2)
    {
        // cout << "writeUid " << info->si_value.sival_int << endl;
        if (info->si_value.sival_int < 0)
        {
            if (readUserPipeList == NULL)
                return;
            int t = info->si_value.sival_int;
            t = t * -1;
            int writeUid = t / 100;
            int myUid = t % 100;
            string readFIFO = "./user_pipe/FIFO" + to_string(writeUid) + to_string(myUid);
            struct READUSERPIPE *before = readUserPipeList;
            if (readUserPipeList->formId == writeUid && readUserPipeList->toId == myUid)
            {
                readUserPipeList = readUserPipeList->next;
                delete before;
            }
            else
            {
                for (struct READUSERPIPE *i = readUserPipeList; i != NULL; i = i->next)
                {
                    if (i->formId == writeUid && i->toId == myUid)
                    {
                        close(i->fd);
                        unlink(readFIFO.c_str());
                        before->next = i->next;
                        delete i;
                        break;
                    }
                    before = i;
                }
            }
        }
        else
        {
            int writeUid = info->si_value.sival_int / 100;
            int myUid = info->si_value.sival_int % 100;
            openReadFIFO(writeUid, myUid);
        }
    }
}

void openReadFIFO(int writeUid, int myUid)
{
    string readFIFO = "./user_pipe/FIFO" + to_string(writeUid) + to_string(myUid);

    // cout << "rec sig:"
    //      << "  " << readFIFO << endl;

    int userWriteFd = open(readFIFO.c_str(), O_RDONLY);
    struct READUSERPIPE *temp = new struct READUSERPIPE;
    temp->fd = userWriteFd;
    temp->formId = writeUid;
    temp->toId = myUid;
    temp->next = readUserPipeList;
    readUserPipeList = temp;
    // cout << "open a r fifo fd:" << readUserPipeList->fd << endl;
}
