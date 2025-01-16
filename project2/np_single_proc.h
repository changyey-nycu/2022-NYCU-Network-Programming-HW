#ifndef _np_single_proc_h
#define _np_single_proc_h

#include <string>
#include <vector>
#include <netdb.h>

using namespace std;

int initConnect(int port, struct sockaddr_in *sin);

// insert new user to vector
void newConnection(int newSock, sockaddr_in fsin, vector<struct USERINFO> &userinfo, struct NUMBERPIPE *numberPipeList[]);

int getMaximumSock(int sock, vector<struct USERINFO> userinfo);

void setFDS(fd_set *fds, vector<struct USERINFO> userinfo);

// generate login msg for newUser
string loginMsg(struct USERINFO newUser);

void shell(int sock, string line, vector<struct USERINFO> &userinfo, struct NUMBERPIPE *numberPipeList[], vector<struct USERPIPE> &userPipeList, struct ENV envList[]);

// clear number pipe, user pipe, close socket, send exit msg
void logout(int sock, vector<struct USERINFO> &userinfo, struct NUMBERPIPE *numberPipeList[], vector<struct USERPIPE> &userPipeList, struct ENV envList[]);

#endif