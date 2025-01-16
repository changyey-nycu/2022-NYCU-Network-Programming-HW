#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <semaphore.h>
#include "./np_multi_proc.h"
#include "./npshell_multi_proc.h"

int infoMem;
int broMem;
extern void *infoAddr;
extern void *broAddr;

sem_t *sem1 = sem_open("sem1", O_CREAT, 0600, 1); // w
sem_t *sem2 = sem_open("sem2", O_CREAT, 0600, 1); // r

int main(int argc, char *argv[])
{
    int port;
    if (argc == 2)
        port = stoi(argv[1]);
    else
        port = 8888;
    int sock;                // master server socket
    int newSock;             // slave server socket
    struct sockaddr_in sin;  // an Internet endpoint address
    struct sockaddr_in fsin; // the address of a client
    int alen;                // length if client's address

    /* create share memory */
    // user info memory
    const long page_size = sysconf(_SC_PAGESIZE);
    key_t key1 = (key_t)703;

    infoMem = shmget(key1, page_size, IPC_CREAT | 0666);
    if (infoMem == -1)
    {
        perror("shmget failed\n");
        exit(EXIT_FAILURE);
    }

    infoAddr = (void *)shmat(infoMem, NULL, 0);
    if (infoAddr == (void *)-1)
    {
        perror("shmat failed\n");
        exit(EXIT_FAILURE);
    }

    memset(infoAddr, 0, page_size);

    // broadcast memory
    key_t key2 = (key_t)704;

    broMem = shmget(key2, page_size, IPC_CREAT | 0666);
    if (broMem == -1)
    {
        perror("shmget failed\n");
        exit(EXIT_FAILURE);
    }

    broAddr = (void *)shmat(broMem, NULL, 0);
    if (broAddr == (void *)-1)
    {
        perror("shmat failed\n");
        exit(EXIT_FAILURE);
    }

    memset(broAddr, 0, page_size);

    // initial connection
    sock = initConnect(port, &sin);

    signal(SIGCHLD, sig_handler);
    signal(SIGINT, sig_exit);
    // cout << "msock:" << msock << endl;
    while (1)
    {
        alen = sizeof(fsin);
        if ((newSock = accept(sock, (struct sockaddr *)&fsin, (socklen_t *)&alen)) < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        // cout << "accept success, ssock:" << newSock << endl;
        // cout << "fsin addr:" << inet_ntoa(fsin.sin_addr) << endl
        //      << "fsin port:" << ntohs(fsin.sin_port) << endl;

        pid_t pid = fork();

        // child
        if (pid == 0)
        {
            // int r, w;
            // sem_getvalue(sem1, &w);
            // sem_getvalue(sem2, &r);
            // cout << "sem r:" << r << " w:" << w << endl;
            dup2(newSock, STDOUT_FILENO);
            dup2(newSock, STDERR_FILENO);
            close(sock);
            const char *wellComeMsg = "****************************************\n** Welcome to the information server. **\n****************************************\n";
            send(newSock, wellComeMsg, strlen(wellComeMsg), 0);

            // broadcast signal
            struct sigaction act;
            act.sa_sigaction = getBroadcast;
            sigemptyset(&act.sa_mask);
            act.sa_flags = SA_SIGINFO;
            if (sigaction(SIGUSR1, &act, NULL) == -1)
            {
                cerr << "signal error";
                exit(1);
            }

            int childPid = getpid();
            int uid = newUserIn(newSock, fsin, childPid);

            npshell(uid, newSock);
            exit(0);
        }

        // parent
        close(newSock);
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

void sig_exit(int sig)
{
    /*clear share memory*/
    // cout << "get signal" << endl;
    if (sig == SIGINT)
    {
        shmdt(infoAddr);
        shmdt(broAddr);

        if (shmctl(infoMem, IPC_RMID, 0) == -1)
        {
            perror("shmctl delete shared memory1 failed\n");
            exit(EXIT_FAILURE);
        }

        if (shmctl(broMem, IPC_RMID, 0) == -1)
        {
            perror("shmctl delete shared memory2 failed\n");
            exit(EXIT_FAILURE);
        }
        memset(infoAddr, 0, sysconf(_SC_PAGESIZE));
        memset(broAddr, 0, sysconf(_SC_PAGESIZE));

        // destroy semaphore
        sem_close(sem1);
        sem_destroy(sem1);
        sem_unlink("sem1");

        sem_close(sem2);
        sem_destroy(sem2);
        sem_unlink("sem2");
        exit(0);
    }
}

int newUserIn(int sock, struct sockaddr_in sin, pid_t pid)
{
    string nowUser = (char *)infoAddr;
    struct USERINFO user;
    // cout << "nowUser:" << nowUser << endl;

    string str_uid = "1";
    string str_name = "(no name)";
    string str_addr = inet_ntoa(sin.sin_addr);
    string str_port = to_string(ntohs(sin.sin_port));
    string str_pid = to_string((int)pid);

    if (nowUser.empty())
    {
        string info = str_uid + "," + str_name + "," + str_addr + ":" + str_port + "," + str_pid + "\n";
        struct USERINFO newUser = {.uid = 1, .name = "(no name)", .addrPort = str_addr + ":" + str_port, .pid = pid};
        const char *buf = info.c_str();
        // cout << "if new user:" << buf << endl;
        memset(infoAddr, 0, sysconf(_SC_PAGESIZE));
        memcpy(infoAddr, buf, strlen(buf));
        user = newUser;
    }
    else
    {
        string info = str_uid + "," + str_name + "," + str_addr + ":" + str_port + "," + str_pid + "\n";
        vector<struct USERINFO> userinfo = getInfoList(infoAddr);
        struct USERINFO newUser = {.uid = 1, .name = "(no name)", .addrPort = str_addr + ":" + str_port, .pid = pid};
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
        storeBack(userinfo);
        user = newUser;
    }
    loginMsg(user.name, user.addrPort);
    return user.uid;
}

void loginMsg(string name, string addrPort)
{
    string str;
    str = "*** User '" + name + "' entered from " + addrPort + ". ***\n";
    // cout << str;
    broadcast(str, sem1, sem2);
}