#ifndef _npshell_single_proc_h
#define _npshell_single_proc_h

#include <string>
#include <vector>
#include <netdb.h>

using namespace std;

struct USERINFO
{
    int sockid;
    int uid;
    string name;
    struct sockaddr_in sin;
};

struct NUMBERPIPE
{
    int numberPipeRead, numberPipeWrite;
    int numberPipeCounter;
    struct NUMBERPIPE *next;
};

struct USERPIPE
{
    int numberPipeRead, numberPipeWrite;
    int formId, toId;
};

struct ENV
{
    vector<string> name;
    vector<string> value;
};

enum pipeFlag
{
    standard,
    normal,
    number,
    error, // only use in write
    user
};


// find the index of sep in massage
// return the number of sep + 1, found initial to npos, last found is the last index + 1 of massage
int find_position(string massage, size_t found[], string sep);

// delete continuous space and begin and end space in massage
string standardize(string massage);

void sig_handler(int sig);

// broadcast msg to all users ### remember to "\n"
void broadcast(string msg, vector<struct USERINFO> userinfo);

// return index to the sock in userinfo vector
int getUserIndexBySock(int sock, vector<struct USERINFO> userinfo);

int getUserIndexById(int uid, vector<struct USERINFO> userinfo);

int execCommand(int sock, string oriMsg, vector<struct USERINFO> &userinfo, struct NUMBERPIPE *numberPipeList[], vector<struct USERPIPE> &userPipeList, struct ENV envList[]);

void argSep(vector<string> &arg, string line);

void who(int sock, vector<struct USERINFO> userinfo);

void tell(int sock, int opId, string msg, vector<struct USERINFO> userinfo);

void yell(int sock, string msg, vector<struct USERINFO> userinfo);

void name(int sock, string name, vector<struct USERINFO> &userinfo);

void block(int sock, int opUid, vector<struct USERINFO> &userinfo);

#endif