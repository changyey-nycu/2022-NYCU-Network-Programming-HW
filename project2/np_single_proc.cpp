#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <signal.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "./npshell_single_proc.h"
#include "./np_single_proc.h"

using namespace std;

int main(int argc, char *argv[])
{
    setenv("PATH", "bin:.", 1);

    int port;
    if (argc < 2)
        port = 8888;
    else
        port = stoi(argv[1]);

    int sock;                // master server socket
    int newSock;             // slave server socket
    struct sockaddr_in sin;  // an Internet endpoint address
    struct sockaddr_in fsin; // the address of a client
    int alen;                // length if client's address

    // when a user login, add info to userinfo,
    vector<struct USERINFO> userinfo;

    // indicate by user uid, 0 is empty
    struct NUMBERPIPE *numberPipeList[31];
    for (size_t i = 0; i < 31; i++)
        numberPipeList[i] = NULL;

    // user pipe list
    vector<struct USERPIPE> userPipeList;

    // per user environment variable, indicate by user uid, 0 is empty
    struct ENV envList[31];

    sock = initConnect(port, &sin);

    fd_set fds;
    int maxSock = sock;

    while (1)
    {
        FD_ZERO(&fds);
        FD_SET(sock, &fds);

        maxSock = getMaximumSock(sock, userinfo);
        setFDS(&fds, userinfo);

        if (select(maxSock + 1, &fds, NULL, NULL, 0) > 0)
        {
            if (FD_ISSET(sock, &fds))
            {
                if (userinfo.size() <= 30)
                {
                    // cout << "new connection" << endl;
                    alen = sizeof(fsin);
                    if ((newSock = accept(sock, (struct sockaddr *)&fsin, (socklen_t *)&alen)) < 0)
                    {
                        perror("accept");
                        exit(EXIT_FAILURE);
                    }
                    newConnection(newSock, fsin, userinfo, numberPipeList);
                    continue;
                }
            }
            for (int i = 0; i <= maxSock; i++)
            {
                if (FD_ISSET(i, &fds))
                {
                    if (i == sock)
                        continue;
                    // cout << "get msg form " << i << endl;
                    char buffer[15000];
                    recv(i, buffer, sizeof(buffer), 0);
                    string line = buffer;
                    shell(i, line, userinfo, numberPipeList, userPipeList, envList);
                    memset(buffer, 0, sizeof(buffer));
                }
            }
        }
    }

    return 0;
}

int initConnect(int port, struct sockaddr_in *sin)
{
    int sock;
    bzero((char *)sin, sizeof(struct sockaddr_in));
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = INADDR_ANY;
    sin->sin_port = htons(port);

    // create socket
    if ((sock = socket(PF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto)) < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    int flag = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)))
    {
        perror("socket setting failed");
        exit(EXIT_FAILURE);
    }
    // bind socket to sin_port
    if (bind(sock, (struct sockaddr *)sin, sizeof(struct sockaddr_in)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(sock, 30) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    return sock;
}

void newConnection(int newSock, sockaddr_in fsin, vector<struct USERINFO> &userinfo, struct NUMBERPIPE *numberPipeList[])
{
    struct USERINFO newUser = {.sockid = newSock, .uid = 1, .name = "(no name)", .sin = fsin};
    if (userinfo.empty())
    {
        userinfo.push_back(newUser);
    }
    else
    {
        int i = 1;
        bool done = 0;
        for (vector<struct USERINFO>::iterator it = userinfo.begin(); it != userinfo.end(); it++)
        {
            if (i != it->uid)
            {
                newUser.uid = i;
                userinfo.insert(it, newUser);
                done = 1;
                break;
            }
            i++;
        }
        if (!done && i <= 30)
        {
            newUser.uid = i;
            userinfo.push_back(newUser);
        }
    }

    // cout << "accept success, newSock:" << newUser.sockid << endl
    //      << "fsin addr:" << inet_ntoa(newUser.sin.sin_addr) << endl
    //      << "fsin port:" << ntohs(newUser.sin.sin_port) << endl
    //      << "user id:" << newUser.uid << endl;

    const char *wellComeMsg = "****************************************\n** Welcome to the information server. **\n****************************************\n";
    send(newSock, wellComeMsg, strlen(wellComeMsg), 0);
    string login = loginMsg(newUser);
    broadcast(login, userinfo);
    send(newSock, "% ", strlen("% "), 0);
}

int getMaximumSock(int sock, vector<struct USERINFO> userinfo)
{
    if (userinfo.empty() != 1)
        for (vector<struct USERINFO>::iterator it = userinfo.begin(); it != userinfo.end(); it++)
        {
            if (sock < it->sockid)
                sock = it->sockid;
        }
    return sock;
}

void setFDS(fd_set *fds, vector<struct USERINFO> userinfo)
{
    if (!userinfo.empty())
        for (vector<struct USERINFO>::iterator it = userinfo.begin(); it != userinfo.end(); it++)
        {
            FD_SET(it->sockid, fds);
        }
}

string loginMsg(struct USERINFO newUser)
{
    string str;
    string addr = inet_ntoa(newUser.sin.sin_addr);
    string port = to_string(ntohs(newUser.sin.sin_port));
    str = "*** User '" + newUser.name + "' entered from " + addr + ":" + port + ". ***\n";
    // cout << str;
    return str;
}

void shell(int sock, string oriMessage, vector<struct USERINFO> &userinfo, struct NUMBERPIPE *numberPipeList[], vector<struct USERPIPE> &userPipeList, struct ENV envList[])
{
    int status = execCommand(sock, oriMessage, userinfo, numberPipeList, userPipeList, envList);
    if (status == 1)
    {
        // cout << "user exit" << sock << endl;
        logout(sock, userinfo, numberPipeList, userPipeList, envList);
    }
    return;
}

void logout(int sock, vector<struct USERINFO> &userinfo, struct NUMBERPIPE *numberPipeList[], vector<struct USERPIPE> &userPipeList, struct ENV envList[])
{
    int index = getUserIndexBySock(sock, userinfo);
    struct USERINFO info = userinfo[index];
    string username = info.name;
    string exitMsg;
    exitMsg = "*** User '" + username + "' left. ***\n";

    // delete number pipe
    for (struct NUMBERPIPE *i = numberPipeList[info.uid]; i != NULL; i = i->next)
    {
        numberPipeList[info.uid] = numberPipeList[info.uid]->next;
        delete i;
    }

    // clear user pipe
    for (vector<struct USERPIPE>::iterator it = userPipeList.begin(); it != userPipeList.end(); it++)
    {
        if (it->formId == info.uid || it->toId == info.uid)
        {
            close(it->numberPipeRead);
            close(it->numberPipeWrite);
            userPipeList.erase(it);
            it--;
        }
    }

    // clear env vector
    envList[info.uid].name.clear();
    envList[info.uid].value.clear();

    // delete userinfo from vector
    for (vector<struct USERINFO>::iterator it = userinfo.begin(); it != userinfo.end(); it++)
    {
        if (it->uid == userinfo[index].uid)
        {
            userinfo.erase(it);
            break;
        }
    }

    // close socket
    close(sock);

    broadcast(exitMsg, userinfo);
}