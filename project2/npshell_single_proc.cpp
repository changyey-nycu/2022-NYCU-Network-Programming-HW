#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <vector>

#include "npshell_single_proc.h"
using namespace std;

vector<int> blockList[31];

int execCommand(int sock, string oriMsg, vector<struct USERINFO> &userinfo, struct NUMBERPIPE *numberPipeList[], vector<struct USERPIPE> &userPipeList, struct ENV envList[])
{
    pipeFlag readFlag = standard;
    pipeFlag writeFlag = standard;

    if (oriMsg.back() == '\n' || oriMsg.back() == '\r')
        oriMsg.pop_back();
    if (oriMsg.back() == '\r' || oriMsg.back() == '\n')
        oriMsg.pop_back();

    string line = standardize(oriMsg);

    if (line.compare("exit") == 0)
        return 1;

    if (line == "")
    {
        send(sock, "% ", strlen("% "), 0);
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

    // get user info
    int userIndex = getUserIndexBySock(sock, userinfo);
    if (userIndex == -1)
    {
        cerr << "no user";
        exit(1);
    }
    int userUid = userinfo[userIndex].uid;

    // update number pipe counter
    for (struct NUMBERPIPE *i = numberPipeList[userUid]; i != NULL; i = i->next)
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
        who(sock, userinfo);
        send(sock, "% ", strlen("% "), 0);
        return 0;
    }
    else if (str_prog == "tell")
    {
        tell(sock, stoi(arg[1]), oriMsg, userinfo);
        send(sock, "% ", strlen("% "), 0);
        return 0;
    }
    else if (str_prog == "yell")
    {
        yell(sock, oriMsg, userinfo);
        send(sock, "% ", strlen("% "), 0);
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
            {
                int existEnv = 0;
                for (int i = 0; i < envList[userUid].name.size(); i++)
                {
                    if (envList[userUid].name[i] == arg[1])
                    {
                        existEnv = 1;
                        envList[userUid].value[i] = arg[2];
                        // cout << "setenv:" << envList[userUid].name[i] << " " << envList[userUid].value[i] << endl;
                        break;
                    }
                }
                if (existEnv == 0)
                {
                    envList[userUid].name.push_back(arg[1]);
                    envList[userUid].value.push_back(arg[2]);
                    // cout << "setenv:" << envList[userUid].name.back() << " " << envList[userUid].value.back() << endl;
                }
            }
            send(sock, "% ", strlen("% "), 0);
            for (int i = 0; i < arg.size(); i++)
                delete[] argv[i];
            delete[] argv;
            return 0;
        }
        else if (str_prog == "printenv")
        {
            if (arg.size() > 1)
            {
                int existEnv = 0;
                for (int i = 0; i < envList[userUid].name.size(); i++)
                {
                    if (envList[userUid].name[i] == arg[1])
                    {
                        existEnv = 1;
                        string str = envList[userUid].value[i] + "\n";
                        const char *temp = str.c_str();
                        if (temp != NULL)
                            send(sock, temp, strlen(temp), 0);
                    }
                }
                if (existEnv == 0)
                {
                    char *temp = getenv(argv[1]);
                    if (temp != NULL)
                    {
                        string str = temp;
                        str = str + "\n";
                        const char *buf = str.c_str();
                        send(sock, buf, strlen(buf), 0);
                    }
                }
            }
            send(sock, "% ", strlen("% "), 0);
            for (int i = 0; i < arg.size(); i++)
                delete[] argv[i];
            delete[] argv;
            return 0;
        }
        else if (str_prog == "name")
        {
            name(sock, argv[1], userinfo);
            send(sock, "% ", strlen("% "), 0);
            for (int i = 0; i < arg.size(); i++)
                delete[] argv[i];
            delete[] argv;
            return 0;
        }
        else if (str_prog == "block")
        {
            int opUid = stoi(argv[1]);
            block(sock, opUid, userinfo);
            send(sock, "% ", strlen("% "), 0);
            for (int i = 0; i < arg.size(); i++)
                delete[] argv[i];
            delete[] argv;
            return 0;
        }

        char *prog = argv[0];

        // user pipe check "pipe already exists, pipe does not exist, sender or receiver does not exist"
        if (readFlag == user)
        {
            // sender does not exist -> do nothing
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
                for (size_t i = 0; i < userPipeList.size(); i++)
                {
                    if (userPipeReadId == userPipeList[i].formId)
                    {
                        pipeExist = 1;
                        // get the read socket item index
                        readUserPipeIndex = i;
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
                // pipe already exist
                int pipeExist = 0;
                for (size_t i = 0; i < userPipeList.size(); i++)
                {
                    if (userUid == userPipeList[i].formId && userPipeWriteId == userPipeList[i].toId)
                    {
                        pipeExist = 1;
                        break;
                    }
                }
                if (pipeExist)
                {
                    userPipeWriteError = 1;
                    string str = "*** Error: the pipe #" + to_string(userUid) + "->#" + to_string(userPipeWriteId) + " already exists. ***\n";
                    const char *buf = str.c_str();
                    send(sock, buf, strlen(buf), 0);
                }
                else
                {
                    // create a object in vector
                    struct USERPIPE temp = {.numberPipeRead = 0, .numberPipeWrite = 0, .formId = userUid, .toId = userPipeWriteId};
                    userPipeList.push_back(temp);
                }
            }
        }

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
            for (struct NUMBERPIPE *i = numberPipeList[userUid]; i != NULL; i = i->next)
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
                writeNumberPipe->next = numberPipeList[userUid];
                numberPipeList[userUid] = writeNumberPipe;
            }
            // cout << "write" << writeNumberPipe->numberPipeRead << writeNumberPipe->numberPipeWrite << endl;
            break;
        }
        case user:
            if (userPipeWriteError != 1)
            {
                if (pipe(newPipe) < 0)
                {
                    cerr << "can not create new pipe" << endl;
                    exit(1);
                }
                userPipeList.back().numberPipeRead = newPipe[0];
                userPipeList.back().numberPipeWrite = newPipe[1];
            }
            break;
        default:
            break;
        }

        // broadcast user pipe
        // receive user pipe
        if (readFlag == user && userPipeReadError == 0)
        {
            string from = to_string(userPipeList[readUserPipeIndex].formId);
            string to = to_string(userPipeList[readUserPipeIndex].toId);
            int tindex = userIndex;
            int findex = getUserIndexById(userPipeList[readUserPipeIndex].formId, userinfo);
            string str = "*** " + userinfo[tindex].name + " (#" + to + ") just received from " + userinfo[findex].name + " (#" + from + ") by '" + oriMsg + "' ***\n";
            broadcast(str, userinfo);
        }
        // send user pipe
        if (writeFlag == user && userPipeWriteError == 0)
        {
            string from = to_string(userPipeList.back().formId);
            string to = to_string(userPipeList.back().toId);
            int findex = userIndex;
            int tindex = getUserIndexById(userPipeList.back().toId, userinfo);
            string str = "*** " + userinfo[findex].name + " (#" + from + ") just piped '" + oriMsg + "' to " + userinfo[tindex].name + " (#" + to + ") ***\n";
            broadcast(str, userinfo);
        }

        // cout << "flag state read:" << readFlag << " write :" << writeFlag << endl;
        pid_t pid;
        while ((pid = fork()) < 0)
            wait(NULL);

        if (pid == 0) // child process
        {
            // setenv
            for (int i = 0; i < envList[userUid].name.size(); i++)
            {
                string str1 = envList[userUid].name[i];
                const char *envName = str1.c_str();
                string str2 = envList[userUid].value[i];
                const char *envValue = str2.c_str();
                setenv(envName, envValue, 1);
            }
            dup2(sock, STDOUT_FILENO);
            dup2(sock, STDERR_FILENO);
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
                    dup2(userPipeList[readUserPipeIndex].numberPipeRead, STDIN_FILENO);
                    close(userPipeList[readUserPipeIndex].numberPipeWrite);
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
                        dup2(userPipeList.back().numberPipeWrite, STDOUT_FILENO);
                        close(userPipeList.back().numberPipeRead);
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
            if (numberPipeList[userUid] == temp)
            {
                numberPipeList[userUid] = numberPipeList[userUid]->next;
            }
            else
            {
                for (struct NUMBERPIPE *i = numberPipeList[userUid]; i != NULL; i = i->next)
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
                close(userPipeList[readUserPipeIndex].numberPipeRead);
                close(userPipeList[readUserPipeIndex].numberPipeWrite);
            }
            break;
        default:
            break;
        }
        // delete user pipe from list
        if (userPipeReadError == 0 && readFlag == user)
        {
            for (vector<struct USERPIPE>::iterator it = userPipeList.begin(); it != userPipeList.end(); it++)
            {
                if (it->formId == userPipeReadId && it->toId == userUid)
                {
                    // cout << "del user pipe " << it->formId << " to " << it->toId << endl;
                    userPipeList.erase(it);
                    break;
                }
            }
        }

        // update number pipe counter
        readFlag = standard;
        if (writeFlag == number || writeFlag == error)
        {
            if (arg_iter + 1 < arg.size()) // not the tail of command
            {
                for (struct NUMBERPIPE *i = numberPipeList[userUid]; i != NULL; i = i->next)
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
        // for (struct NUMBERPIPE *i = numberPipeList[userUid]; i != NULL; i = i->next)
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
    send(sock, "% ", strlen("% "), 0);
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
    wait(NULL);
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

void broadcast(string msg, vector<struct USERINFO> userinfo)
{
    // cout << "broadcast:" << msg;
    const char *buff = msg.c_str();
    for (vector<struct USERINFO>::iterator it = userinfo.begin(); it != userinfo.end(); it++)
    {
        send(it->sockid, buff, msg.length(), 0);
    }
}

int getUserIndexBySock(int sock, vector<struct USERINFO> userinfo)
{
    for (int i = 0; i < userinfo.size(); i++)
    {
        if (userinfo[i].sockid == sock)
            return i;
    }
    return -1;
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

void who(int sock, vector<struct USERINFO> userinfo)
{
    string str = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
    for (vector<struct USERINFO>::iterator it = userinfo.begin(); it != userinfo.end(); it++)
    {
        string userStr;
        if (it->sockid == sock)
            userStr = to_string(it->uid) + "\t" + it->name + "\t" + inet_ntoa(it->sin.sin_addr) + ":" + to_string(ntohs(it->sin.sin_port)) + "\t" + "<-me\n";
        else
            userStr = to_string(it->uid) + "\t" + it->name + "\t" + inet_ntoa(it->sin.sin_addr) + ":" + to_string(ntohs(it->sin.sin_port)) + "\n";
        str = str + userStr;
    }
    const char *buf = str.c_str();
    send(sock, buf, strlen(buf), 0);
}

void tell(int sock, int opId, string msg, vector<struct USERINFO> userinfo)
{
    // check user exist
    int exist = 0;
    int opSock = 0;
    for (vector<struct USERINFO>::iterator it = userinfo.begin(); it != userinfo.end(); it++)
    {
        if (opId == it->uid)
        {
            exist = 1;
            opSock = it->sockid;
            break;
        }
    }
    if (!exist)
    {
        string str;
        str = "*** Error: user #" + to_string(opId) + " does not exist yet. ***\n";
        const char *buf = str.c_str();
        send(sock, buf, strlen(buf), 0);
        return;
    }
    else
    {
        int index = getUserIndexBySock(sock, userinfo);
        if (!blockList[index].empty())
        {
            for (int i = 0; i < blockList[index].size(); i++)
            {
                if (blockList[index][i] == opSock)
                    return;
            }
        }
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
        // cout << str;
        const char *buf = str.c_str();
        send(opSock, buf, strlen(buf), 0);
    }
}

void yell(int sock, string msg, vector<struct USERINFO> userinfo)
{
    int index = getUserIndexBySock(sock, userinfo);
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
    const char *buff = str.c_str();
    for (vector<struct USERINFO>::iterator it = userinfo.begin(); it != userinfo.end(); it++)
    {
        int block = 0;

        if (!blockList[index].empty())
        {
            for (int i = 0; i < blockList[index].size(); i++)
            {
                if (blockList[index][i] == it->sockid)
                    block = 1;
            }
        }
        if (block == 0)
            send(it->sockid, buff, strlen(buff), 0);
    }
}

void name(int sock, string name, vector<struct USERINFO> &userinfo)
{
    for (vector<struct USERINFO>::iterator it = userinfo.begin(); it != userinfo.end(); it++)
    {
        if (it->name == name)
        {
            string str = "*** User '" + name + "' already exists. ***\n";
            const char *buf = str.c_str();
            send(sock, buf, strlen(buf), 0);
            return;
        }
    }

    int index = getUserIndexBySock(sock, userinfo);
    userinfo[index].name = name;
    string str = "*** User from ";
    str = str + inet_ntoa(userinfo[index].sin.sin_addr) + ":" + to_string(ntohs(userinfo[index].sin.sin_port)) + " is named '" + name + "'. ***\n";
    const char *buf = str.c_str();
    broadcast(str, userinfo);
    return;
}

void block(int sock, int opUid, vector<struct USERINFO> &userinfo)
{
    int index = getUserIndexBySock(sock, userinfo);
    int opIndex = getUserIndexById(opUid, userinfo);
    blockList[index].push_back(userinfo[opIndex].sockid);
    blockList[opIndex].push_back(sock);
}