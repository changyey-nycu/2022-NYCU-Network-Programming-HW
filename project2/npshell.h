#ifndef _npshell_h
#define _npshell_h
#include <string>
using namespace std;

// find the index of sep in massage
// return the number of sep + 1, found initial to npos, last found is the last index + 1 of massage
int find_position(string massage, size_t found[], string sep);

// delete continuous space and begin and end space in massage
string standardize(string massage);

void sig_handler(int sig);

int npshell(int sd);

#endif