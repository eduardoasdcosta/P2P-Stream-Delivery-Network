#ifndef IAMROOT_H
#define IAMROOT_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>

#define BUFF_SIZE 1024
#define IP_SIZE 16
#define PORT_SIZE 8
#define MAX_ACS 50 // maximum number of access points stored
#define PQ_ALARM_TIME 3 //time interval between POP QUERIES
#define max(A,B)  ((A)>=(B)?(A):(B))

//structure used to identify a par, stores the ip and UDP/TCP ports.
typedef struct _net_id
{
    char ip[IP_SIZE];
    char uport[PORT_SIZE];
    char tport[PORT_SIZE];
}NetId;
//structure used to identify a connection, stores the file descriptor and a par id
typedef struct _net_con
{
    int fd;
    NetId* netid;
    struct addrinfo* con_addr;
}NetCon;
//structure used to store an access point, stores a par id and the number of connections available
typedef struct _access_point
{
	NetId id;
	int avails;
}AccessPoint;
//table used to store all access points, the iterator variable is used to index the table and the total_acs variable keeps track of how many access points are in the table
typedef struct _access_point_table
{
	AccessPoint acs[MAX_ACS];
	int iterator;
	int total_acs;
}AccessPointTable;
//structure used to store commands to be processed by the application
typedef struct _command_stack
{
    char commands[50][1024];
    int sp;
}CommStack;

int root_send(char* command, NetId* rsid);
int who_is_root(char* streamID, int streamID_size, NetId* treerootid, NetId* rsid, NetId* hostid);
int get_access_point(NetId* treerootid, NetId* upid);
int handle_access_commands(int fd, char* streamID);
int connect_upstream_tcp(NetCon* upcon, NetId* rsid, char* streamID);
void readCmdsFromBuffer(char* buffer, CommStack* stack);
void user_input(NetId* rsid, int* running, NetCon* tcp_down_cons, int connections, NetId* hostid, NetId* upid, char* streamID);

#endif
