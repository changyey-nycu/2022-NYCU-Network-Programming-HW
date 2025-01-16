#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "./npshell.h"

int main(int argc, char *argv[])
{
    int port;
    if (argc == 2)
        port = stoi(argv[1]);
    else
        port = 8888;
    int msock;               // master server socket
    int ssock;               // slave server socket
    struct sockaddr_in sin;  // an Internet endpoint address
    struct sockaddr_in fsin; // the address of a client
    int alen;                // length if client's address
    struct servent *pse;     // pointer to service information entry
    struct protoent *ppe;    // pointer to protocol information

    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);
    ppe = getprotobyname("tcp");

    // create socket
    if ((msock = socket(PF_INET, SOCK_STREAM, ppe->p_proto)) < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    int flag = 1;
    if (setsockopt(msock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)))
    {
        perror("socket setting failed");
        exit(EXIT_FAILURE);
    }
    // bind socket to sin_port 8080
    if (bind(msock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(msock, 30) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    signal(SIGCHLD, sig_handler);
    // cout << "msock:" << msock << endl;
    while (1)
    {
        alen = sizeof(fsin);
        if ((ssock = accept(msock, (struct sockaddr *)&fsin, (socklen_t *)&alen)) < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        // cout << "accept success, ssock:" << ssock << endl;
        // cout << "fsin addr:" << inet_ntoa(fsin.sin_addr) << endl
        //      << "fsin port:" << ntohs(fsin.sin_port) << endl;

        switch (fork())
        {
        case 0: /*child*/
            close(msock);
            dup2(ssock, STDOUT_FILENO);
            dup2(ssock, STDERR_FILENO);
            npshell(ssock);
            exit(0);
            break;
        default: /*parent*/
            close(ssock);
            break;
        case -1:
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}