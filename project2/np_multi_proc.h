#ifndef _np_multi_proc_h
#define _np_multi_proc_h

#include <netdb.h>

using namespace std;

int initConnect(int port, struct sockaddr_in *sin);

void sig_exit(int sig);

// return uid of new user
int newUserIn(int sock, struct sockaddr_in sin, pid_t pid);

// broadcast the login msg
void loginMsg(string name, string addrPort);

#endif