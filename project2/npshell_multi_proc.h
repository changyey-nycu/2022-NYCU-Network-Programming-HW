#ifndef _npshell_multi_proc_h
#define _npshell_multi_proc_h

#include <string>
#include <vector>
#include <netdb.h>

using namespace std;

struct USERINFO
{
    int uid;
    string name;
    string addrPort;
    pid_t pid;
};

struct NUMBERPIPE
{
    int numberPipeRead, numberPipeWrite;
    int numberPipeCounter;
    struct NUMBERPIPE *next;
};

struct READUSERPIPE
{
    int fd;
    int formId, toId;
    struct READUSERPIPE *next;
};

enum pipeFlag
{
    standard,
    normal,
    number,
    error, // only use in write
    user
};

int npshell(int uid, int sock);

// find the index of sep in massage
// return the number of sep + 1, found initial to npos, last found is the last index + 1 of massage
int find_position(string massage, size_t found[], string sep);

// delete continuous space and begin and end space in massage
string standardize(string massage);

void sig_handler(int sig);

// broadcast msg to all users ### remember to "\n"
void broadcast(string msg, sem_t *semWrite, sem_t *semRead);

int getUserIndexById(int uid, vector<struct USERINFO> userinfo);

int execCommand(int uid, int sock, string oriMsg);

void argSep(vector<string> &arg, string line);

void who(int uid);

void tell(int uid, int opId, string msg);

void yell(int uid, string msg);

void name(int uid, string name);

// -----part 3 add function-----
void sepMsg(vector<string> &arg, string line, string sep);

string vecToStr(vector<struct USERINFO> userinfo);

vector<struct USERINFO> getInfoList(void *addr);

int exitChild(int uid);

void storeBack(vector<struct USERINFO> userinfo);

void getBroadcast(int signo, siginfo_t *info, void *ctx);

// when int < 0 : someone exit, int > 0 : someone transfer user pipe
void getUserPipeSig(int signo, siginfo_t *info, void *ctx);

void openReadFIFO(int writeUid, int myUid);

#endif