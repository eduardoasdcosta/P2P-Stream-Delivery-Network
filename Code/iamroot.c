#include "pair_protocol.h"
//variables used in main loop select()
fd_set rfds;
int maxfd = 0, counter = 0;
//flag to identify if application is root of a tree or not
int isroot = 0;
//command line argument variables
int tcpsessions = 1, bestpops = 1, tsecs = 5;
int show_stream_data = 1, detailed_info = 0, ascii = 1;
//counter used to keep track of seconds elapsed
unsigned long long alarm_counter = 0;
//variables used to create sockets and to check for errors of system calls
size_t n;
socklen_t addrlen;
struct addrinfo hints;
struct sockaddr_in addr;
//character buffers used to store strings
char buffer[BUFF_SIZE], data_buf[BUFF_SIZE], hex[BUFF_SIZE];
//stack of commands to be processed by the application
CommStack cmdstack;
//table of available access points (used by udp access server)
AccessPointTable table, aux;
// POP QUERY id counter
uint16_t pq_id = 0;
//variable used to access error string after a failed system call
extern errno;
//flag that identifies if stream is flowing (1) or broken (0)
int flow = 0;

int main(int argc, char** argv)
{
    char streamID[BUFF_SIZE];
    NetCon accesscon;
	NetId hostid;
	sprintf(hostid.uport, "%s", "58000");
	sprintf(hostid.tport, "%s", "58000");
	accesscon.netid = &hostid;
    NetId rsid; //root server ip and ports
    sprintf(rsid.ip, "%s", "194.210.157.214");
    sprintf(rsid.uport, "%s", "59000");
    NetId treerootid; // ip and ports of root par 
    NetCon upcon; // file descriptor and id of upstream par
    NetId upid;   // ip and ports of upstream par
    NetId streamFont; // ip and ports of the stream source 
    sprintf(streamFont.ip, "%s", "194.210.157.214");
    sprintf(streamFont.tport, "%s", "59000");
    sprintf(upid.ip, "%s", "194.210.157.214");//192.168.1.2 193.136.138.142
    sprintf(upid.tport, "%s", "59000"); 
    upcon.netid = &upid;
    NetCon downcon; // file descriptor and id of downstream server
    downcon.netid = &hostid;
    int i = 1, running = 1, j = 0;
    struct sigaction act1;
    struct timeval t;
    t.tv_sec = 1;
    t.tv_usec = 0;
    
    cmdstack.sp = 0;
    //ignore SIGPIPE signal so that application does not crash when an internet connection is lost to another par 
    memset(&act1, 0, sizeof(act1));
    act1.sa_handler = SIG_IGN;
    
    if(sigaction(SIGPIPE, &act1, NULL) == -1)
    {
        printf("Error: Could not ignore SIGPIPE\n");
        fflush(stdout);
        exit(1);
    }
    //get command line arguments
    if(argc < 2)
        exit(1);
    printf("------------Inputs---------------------------\n");
    sprintf(streamID, "%s", argv[i]);
    i++;
    printf("streamID:%s\n", streamID);

    while(i < argc)
    {
        if(strcmp(argv[i], "-i") == 0)
        {
            i++;
            sprintf(hostid.ip, "%s", argv[i]);
            i++;
            printf("ipaddr:%s\n", hostid.ip);
        }
        else if(strcmp(argv[i], "-t") == 0)
        {
            i++;
            sprintf(hostid.tport, "%s", argv[i]);
            i++;
            printf("tport:%s\n", hostid.tport);
        }
        else if(strcmp(argv[i], "-u") == 0)
        {
            i++;
            sprintf(hostid.uport, "%s", argv[i]);
            i++;
            printf("uport:%s\n", hostid.uport);
        }
        else if(strcmp(argv[i], "-s") == 0)
        {
            i++;
            sscanf( argv[i], "%[^:]:%s", rsid.ip, rsid.uport);
            i++;
            printf("rsaddr:%s rsport:%s\n", rsid.ip, rsid.uport);
        }
        else if(strcmp(argv[i], "-p") == 0)
        {
            i++;
            tcpsessions = atoi(argv[i]);
            i++;
            printf("tcpsessions:%d\n", tcpsessions);
        }
        else if(strcmp(argv[i], "-n") == 0)
        {
            i++;
            bestpops = atoi(argv[i]);
            i++;
            printf("bestpops:%d\n", bestpops);
        }
        else if(strcmp(argv[i], "-x") == 0)
        {
            i++;
            tsecs = atoi(argv[i]);
            i++;
            printf("tsecs:%d\n", tsecs);
        }
        else if(strcmp(argv[i], "-b") == 0)
        {
            i++;
            show_stream_data = 0;
            printf("show_stream_data:%d\n", show_stream_data);
        }
        else if(strcmp(argv[i], "-d") == 0)
        {
            i++;
            detailed_info = 1;
            printf("detailed_info:%d\n", detailed_info);
        }
        else if(strcmp(argv[i], "-h") == 0)
        {
            i++;
            printf("./iamroot [streamID] -i [ipaddr] -t [tcp port] -u [udp port] -s [rs addr:rs udp port] -p [tcpsessions] -x [tsecs] -b [display on/off] -d [debug] -h [help]\n");
            exit(0);
        }
        else
        {
            printf("Invalid arguments\n");
            exit(1);
        }
    }
    printf("---------------------------------------------\n");
    //query the root server to see who is root of stream with id streamID and update the variable isroot accordingly
    if(who_is_root(streamID, strlen(streamID), &treerootid, &rsid, &hostid) == -1)
    {    
        printf("Error connecting to root server: %s\n", strerror(errno));
        fflush(stdout);
        exit(1);
    }
    
    //tcp downstream variables
    NetCon tcp_down_cons[tcpsessions]; // array of downstream connections that stores all fd's and ip/ports needed
    for(i = 0; i < tcpsessions; i++)
    {
        tcp_down_cons[i].netid = (NetId*)malloc(sizeof(NetId));
    }
    int connections = 0; // number of currently connected pars downstream to the application
    
    //SETUP ACCESS UDP SERVER
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
    
    n = getaddrinfo(NULL, hostid.uport, &hints, &accesscon.con_addr);
    if(n != 0)
    {
        printf("Error getting access server info: %s\n", strerror(errno));
        fflush(stdout);
        exit(1);
    }
    accesscon.fd = socket(accesscon.con_addr->ai_family, accesscon.con_addr->ai_socktype, accesscon.con_addr->ai_protocol);
    if(accesscon.fd == -1)
    {
        printf("Error creating access server socket: %s\n", strerror(errno));
        fflush(stdout);
        exit(1);
    }

    n = bind(accesscon.fd, accesscon.con_addr->ai_addr, accesscon.con_addr->ai_addrlen);
    if(n == -1)
    {
        printf("Error binding access server: %s\n", strerror(errno));
        fflush(stdout);
        exit(1);
    }

    addrlen = sizeof(addr);
    //initialize the access point table and the auxiliary table used to update the primary table
    table.iterator = 0;
    table.total_acs = 0;
    aux.total_acs = bestpops;

    //Get Access Point if not root, else register yourself as a valid access point in the access point table
    if(!isroot)
    {
        if(get_access_point(&treerootid, &upid) == -1)
        {
            printf("Error getting access point: %s\n", strerror(errno));
            fflush(stdout);
            exit(1);
        }
    }
    else
    {
        table.acs[table.total_acs].id = hostid;
        table.acs[table.total_acs++].avails = tcpsessions;
    }
    
    upcon.netid = &upid;
    //try and connect to upstream par. if the connection fails 3 times, exit the application
    for(i = 0; i < 3; i++)
    {
        n = connect_upstream_tcp(&upcon, &rsid, streamID);
        if(n == 0)
        {
            i = 3;
        }
        else
        {
            sleep(1);
        }
    }
    
    if(n != 0)
    {
        exit(1);
    }

    //Create downstream tcp server
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;

    
    n = getaddrinfo(NULL, hostid.tport, &hints, &downcon.con_addr);
    if(n != 0)
    {
        printf("Error getting downstream server info: %s\n", strerror(errno));
        fflush(stdout);
        exit(1);
    }
    downcon.fd = socket(downcon.con_addr->ai_family, downcon.con_addr->ai_socktype, downcon.con_addr->ai_protocol);
    if(downcon.fd == -1)
    {
        printf("Error creating socket for downstream server: %s\n", strerror(errno));
        fflush(stdout);
        exit(1);
    }

    n = bind(downcon.fd, downcon.con_addr->ai_addr, downcon.con_addr->ai_addrlen);
    if(n == -1) 
    {
        printf("Error binding downstream server: %s\n", strerror(errno));
        fflush(stdout);
        exit(1);
    }

    if(listen(downcon.fd, tcpsessions) == -1)
    {
        printf("Error listening from downstream server: %s\n", strerror(errno));
        fflush(stdout);
        exit(1);
    }
    
    //Main loop
    while(running)
    {
    	//Set all read file descriptors 
        FD_ZERO(&rfds);
        FD_SET(0, &rfds);
        FD_SET(upcon.fd, &rfds); maxfd = upcon.fd;
        FD_SET(accesscon.fd, &rfds); maxfd = max(maxfd, accesscon.fd);
        FD_SET(downcon.fd, &rfds); maxfd = max(maxfd, downcon.fd);
        
        for(i = 0; i < connections; i++)
        {
            FD_SET(tcp_down_cons[i].fd, &rfds); maxfd = max(maxfd, tcp_down_cons[i].fd);
        }
        
        //if application is root use the timer (1 second timer), else do not use it
        if(isroot)
            counter = select(maxfd+1, &rfds, (fd_set*) NULL, (fd_set*)NULL, &t);
        else
            counter = select(maxfd+1, &rfds, (fd_set*) NULL, (fd_set*)NULL, NULL);
        //if select system call fails exit application
        if(counter < 0)
        {
            printf("Error on select: %s\n", strerror(errno));
            fflush(stdout);
            exit(1);
        }
        //if timeout
        if(counter == 0)
        {
            alarm_counter++; //increment seconds elapsed

            if(alarm_counter%PQ_ALARM_TIME == 0) //if PQ_ALARM_TIME (=3) seconds have elapsed send POP QUERY
            {
            	//if last POP QUERY did not receive bestpop resposes then update the table with the values that it received after 3 seconds 
                if(aux.total_acs < bestpops)
                {
                    for(i = 0; i < aux.total_acs; i++)
                        table.acs[i] = aux.acs[i];
                    
                    table.total_acs = aux.total_acs;
                    table.iterator = 0;
                }

                aux.total_acs = 0;
                int pops = bestpops; // 1 best pop corresponds to one application, regardless of the number of connections it can accept

                if(tcpsessions - connections > 0) //if there is still space for connections downstream register yourself as a valid access point in the access point table
                {

                    aux.acs[aux.total_acs].id = hostid;
                    aux.acs[aux.total_acs++].avails = tcpsessions - connections;
                    pops--;

                    if(pops > 0)
                    {
                        sprintf(buffer, "PQ %.4X %d\n", pq_id, pops);
                        for(i = 0; i < connections; i++)
                        { 
                            if(write(tcp_down_cons[i].fd, buffer, strlen(buffer)) <= 0)
                            {
                                if(detailed_info)
                                {
                                    printf("Connection ended downstream. Reorganizing down pairs: %s\n", strerror(errno));
                                    fflush(stdout);
                                }
                                close(tcp_down_cons[i].fd);
                                if(i != --connections)
                                {
                                    if(detailed_info)
                                    {
                                        printf("gonna delete: %s:%s\n", tcp_down_cons[i].netid->ip, tcp_down_cons[i].netid->tport);
                                        printf("and replace the array pos with: %s:%s\n", tcp_down_cons[connections].netid->ip, tcp_down_cons[connections].netid->tport);
                                    } 
                                    tcp_down_cons[i].fd = tcp_down_cons[connections].fd;
                                    memcpy(tcp_down_cons[i].netid, tcp_down_cons[connections].netid, sizeof(NetId));

                                    tcp_down_cons[connections].fd = 0;
                                    memset(tcp_down_cons[connections].netid, 0, sizeof(NetId));
                                    
                                    i--;
                                }
                            }
                        }
                    }
                    else
                    {
                        for(i = 0; i < aux.total_acs; i++)
                            table.acs[i] = aux.acs[i];
                    
                        table.total_acs = aux.total_acs;
                        table.iterator = 0;
                    }
                }
                else //if there is no space left for connections downstream send only the POP QUERY down
                {
                    sprintf(buffer, "PQ %.4X %d\n", pq_id, pops);
                    for(i = 0; i < connections; i++)
                    { 
                        if(write(tcp_down_cons[i].fd, buffer, strlen(buffer)) <= 0)
                        {
                            if(detailed_info)
                            {
                                printf("Connection ended downstream. Reorganizing down pairs: %s\n", strerror(errno));
                                fflush(stdout);
                            }
                            close(tcp_down_cons[i].fd);
                            if(i != --connections)
                            {
                                if(detailed_info)
                                {
                                    printf("gonna delete: %s:%s\n", tcp_down_cons[i].netid->ip, tcp_down_cons[i].netid->tport);
                                    printf("and replace the array pos with: %s:%s\n", tcp_down_cons[connections].netid->ip, tcp_down_cons[connections].netid->tport);
                                } 
                                tcp_down_cons[i].fd = tcp_down_cons[connections].fd;
                                memcpy(tcp_down_cons[i].netid, tcp_down_cons[connections].netid, sizeof(NetId));

                                tcp_down_cons[connections].fd = 0;
                                memset(tcp_down_cons[connections].netid, 0, sizeof(NetId));
                                
                                i--;
                            }
                        }
                    }
                }
                pq_id++; // increment the POP QUERY id counter after it was sent
            }

            if(alarm_counter%tsecs == 0) // if tsecs have elapsed then refresh yourself as root in the root server
            {
                if(who_is_root(streamID, strlen(streamID), &treerootid, &rsid, &hostid) == -1)
                {    
                    printf("Error connecting to root server: %s\n", strerror(errno));
                    fflush(stdout);
                    exit(1);
                }

                if(!isroot) //in the case that the application is not root anymore get an access point to connect to an existing tree
                {
                    if(get_access_point(&treerootid, &upid) == -1)
                    {
                        printf("Error getting access point: %s\n", strerror(errno));
                        fflush(stdout);
                        exit(1);
                    }

                    upcon.netid = &upid;
                    for(i = 0; i < 3; i++)
                    {
                        n = connect_upstream_tcp(&upcon, &rsid, streamID);
                        if(n == 0)
                        {
                            i = 3;
                        }
                        else
                        {
                            sleep(1);
                        }
                    }
                    
                    if(n != 0)
                    {
                        exit(1);
                    }
                }
            }

            t.tv_sec = 1; //reset the alarm time to 1 for select() call
        }
        
        if(FD_ISSET(upcon.fd, &rfds)) //if upstream file descriptor is ready to be read
        {
            n = read(upcon.fd, buffer, BUFF_SIZE-1); //read data from upstream
            if(n <= 0 && isroot)
            {
                printf("lost connection to source: %s\n", strerror(errno));
                fflush(stdout);
                //if connection to stream source failed send BROKEN STREAM downstream and exit the application 
                flow = 0;
                if(detailed_info)
                {
                    printf("Sending BS downstream.\n");
                    fflush(stdout);
                }

                for(i = 0; i < connections; i++)
                {
                    if(write(tcp_down_cons[i].fd, "BS\n", strlen("BS\n")) <= 0)
                    {
                        printf("Connection ended abruptely downstream: %s\n", strerror(errno));
                        fflush(stdout);
                    }
                }

                sprintf(buffer, "REMOVE %s\n", streamID);
                root_send(buffer, &rsid);
                exit(1);
            }
            else if(n <= 0 && !isroot)
            {
                if(detailed_info)
                {
                    printf("Connection ended upstream. Redirecting: %s\n", strerror(errno));
                    fflush(stdout);
                }
                //if connection upstream failed send BROKEN STREAM downstream and query the root server/get new access point if needed 
                flow = 0;
                for(i = 0; i < connections; i++)
                {
                    if(write(tcp_down_cons[i].fd, "BS\n", strlen("BS\n")) <= 0)
                    {
                        printf("Connection ended abruptely downstream: %s\n", strerror(errno));
                        fflush(stdout);
                    }
                }
                
                close(upcon.fd); // close the previous connection
                
                if(who_is_root(streamID, strlen(streamID), &treerootid, &rsid, &hostid) == -1)
                {    
                    printf("Error connecting to root server: %s\n", strerror(errno));
                    fflush(stdout);
                    exit(1);
                }
                
                if(!isroot)
                {
                    sleep(3);
                    if(get_access_point(&treerootid, &upid) == -1)
                    {
                        printf("Error getting access point: %s\n", strerror(errno));
                        fflush(stdout);
                        exit(1);
                    }
                    
                    upcon.netid = &upid;
                    for(i = 0; i < 3; i++)
                    {
                        n = connect_upstream_tcp(&upcon, &rsid, streamID);
                        if(n == 0)
                        {
                            i = 3;
                        }
                        else
                        {
                            sleep(1);
                        }
                    }
                    
                    if(n != 0)
                    {
                        exit(1);
                    }
                    
                }
                else
                {
                    upcon.netid = &streamFont;
                    for(i = 0; i < 3; i++)
                    {
                        n = connect_upstream_tcp(&upcon, &rsid, streamID);
                        if(n == 0)
                        {
                            i = 3;
                        }
                        else
                        {
                            sleep(1);
                        }
                    }
                    
                    if(n != 0)
                    {
                        exit(1);
                    }

                    flow = 1;
                    for(i = 0; i < connections; i++)
                    { 
                        if(write(tcp_down_cons[i].fd, "SF\n", strlen("SF\n")) <= 0)
                        {
                            if(detailed_info)
                            {
                                printf("Connection ended downstream. Reorganizing down pairs: %s\n", strerror(errno));
                                fflush(stdout);
                            }
                            close(tcp_down_cons[i].fd);
                            if(i != --connections)
                            {
                                if(detailed_info)
                                {
                                    printf("gonna delete: %s:%s\n", tcp_down_cons[i].netid->ip, tcp_down_cons[i].netid->tport);
                                    printf("and replace the array pos with: %s:%s\n", tcp_down_cons[connections].netid->ip, tcp_down_cons[connections].netid->tport);
                                } 
                                tcp_down_cons[i].fd = tcp_down_cons[connections].fd;
                                memcpy(tcp_down_cons[i].netid, tcp_down_cons[connections].netid, sizeof(NetId));

                                tcp_down_cons[connections].fd = 0;
                                memset(tcp_down_cons[connections].netid, 0, sizeof(NetId));
                                
                                i--;
                            }
                        }
                    }

                    table.total_acs = 0;
                    table.acs[table.total_acs].id = hostid;
                    table.acs[table.total_acs++].avails = tcpsessions;
                }
            }

            buffer[n] = '\0';

            if(show_stream_data && isroot)
            {
                if(!ascii)
                {
                    for(i = 0; i < n; i++)
                    {
                        sprintf(hex + i*2, "%.2X", buffer[i]);
                    }
                    write(1, hex, 2*n);
                    write(1, "\n", 1);
                }
                else
                {
                    write(1, buffer, n);
                }
            }
            //if not root try to read commands from upstream buffer and if commands were identified process them
            if(!isroot)
            {
                readCmdsFromBuffer(buffer, &cmdstack);
                i = 0;
                while(cmdstack.sp > 0)
                {
                    sprintf(buffer, "%s", cmdstack.commands[i]);
                    cmdstack.commands[i][0] = '\0';
                    handle_downstream_requests(&upcon, buffer, &hostid, tcpsessions - connections, tcp_down_cons, connections, &rsid, streamID, &treerootid, isroot, detailed_info, &flow, show_stream_data, ascii, &streamFont);
                    cmdstack.sp--;
                    i++;
                } 
            }
            else //if root replicate stream data downstream(send DA)
            {
                
                sprintf(data_buf, "DA %.4X\n%s", (uint16_t)(strlen(buffer)), buffer);
                if(detailed_info)
                {
                    printf("Sending DA.\n");
                    fflush(stdout);
                }
                for(i = 0; i < connections; i++)
                { 
                    if(write(tcp_down_cons[i].fd, data_buf, strlen(data_buf)) <= 0)
                    {
                        if(detailed_info)
                        {
                            printf("Connection ended downstream. Reorganizing down pairs: %s\n", strerror(errno));
                            fflush(stdout);
                        }
                        close(tcp_down_cons[i].fd);
                        if(i != --connections)
                        {
                            if(detailed_info)
                            {
                                printf("gonna delete: %s:%s\n", tcp_down_cons[i].netid->ip, tcp_down_cons[i].netid->tport);
                                printf("and replace the array pos with: %s:%s\n", tcp_down_cons[connections].netid->ip, tcp_down_cons[connections].netid->tport);
                            } 
                            tcp_down_cons[i].fd = tcp_down_cons[connections].fd;
                            memcpy(tcp_down_cons[i].netid, tcp_down_cons[connections].netid, sizeof(NetId));

                            tcp_down_cons[connections].fd = 0;
                            memset(tcp_down_cons[connections].netid, 0, sizeof(NetId));
                            
                            i--;
                        }
                    }
                }
            }
        }
        if(FD_ISSET(downcon.fd, &rfds)) //if downstream server file descriptor is ready to be read
        {
            if(connections < tcpsessions) //if space for connections downstream left accept the new par 
            {
                if((tcp_down_cons[connections++].fd = accept(downcon.fd, (struct sockaddr*) &addr, &addrlen)) == -1)
                {
                    printf("Error accepting connection: %s\n", strerror(errno));
                    fflush(stdout);
                }
                else 
                {
                    if(detailed_info)
                    {
                        printf("Sending WE and SF\n");
                    }
                    sprintf(buffer, "WE %s\nSF\n", streamID); //send WELCOME and STREAM FLOWING to the new par
                    if(write(tcp_down_cons[connections-1].fd, buffer, strlen(buffer)) <= 0)
                    {
                        if(detailed_info)
                        {
                            printf("Connection ended abruptely downstream WE/SF: %s\n", strerror(errno));
                            fflush(stdout);
                        }
                        close(tcp_down_cons[i].fd);
                        connections--;
                    }
                }
            }
            else // else redirect it to your newest connection downstream
            {
            	NetCon temp;
                
                if((temp.fd = accept(downcon.fd, (struct sockaddr*) &addr, &addrlen)) == -1)
                {
                    printf("Error accepting connection: %s\n", strerror(errno));
                    fflush(stdout);
                }
                else
                {
                    if(detailed_info)
                    {
                        printf("Sending RE to %s:%s.\n", tcp_down_cons[connections-1].netid->ip, tcp_down_cons[connections-1].netid->tport);
                        fflush(stdout);
                    }
                    sprintf(buffer, "RE %s:%s\n", tcp_down_cons[connections-1].netid->ip, tcp_down_cons[connections-1].netid->tport);
                    if(write(temp.fd, buffer, strlen(buffer)) <= 0)
                    {
                        if(detailed_info)
                        {
                            printf("Connection ended abruptely downstream RE: %s\n", strerror(errno));
                            fflush(stdout);
                        }
                    }
                    close(temp.fd);
                }
            }
        }
        
        for(i = 0; i < connections; i++) // for all current downstream connections check if file descriptor is ready to be read
        {
            if(FD_ISSET(tcp_down_cons[i].fd, &rfds))
            {
                n = read(tcp_down_cons[i].fd, buffer, BUFF_SIZE-1); // read data from downstream
                if(n <= 0) //if read call failed, connection was lost so close the current connection fd and delete the par from the connection array
                {
                    if(detailed_info)
                    {
                        printf("Connection ended downstream. Reorganizing down pairs: %s\n", strerror(errno));
                        fflush(stdout);
                    }
                    close(tcp_down_cons[i].fd);
                    if(i != --connections)
                    {
                        if(detailed_info)
                        {
                            printf("gonna delete: %s:%s\n", tcp_down_cons[i].netid->ip, tcp_down_cons[i].netid->tport);
                            printf("and replace the array pos with: %s:%s\n", tcp_down_cons[connections].netid->ip, tcp_down_cons[connections].netid->tport);
                        } 
                        tcp_down_cons[i].fd = tcp_down_cons[connections].fd;
                        memcpy(tcp_down_cons[i].netid, tcp_down_cons[connections].netid, sizeof(NetId));

                        tcp_down_cons[connections].fd = 0;
                        memset(tcp_down_cons[connections].netid, 0, sizeof(NetId));
                        
                        i--;
                    }

                    if(detailed_info)
                    {
                        for(int j = 0; j < tcpsessions; j++)
                        {
                            printf("ip:port[%d]: %s:%s\n", j, tcp_down_cons[j].netid->ip, tcp_down_cons[j].netid->tport);
                        }
                    }
                }
                buffer[n] = '\0';
                //try to read commands from downstream buffer
                readCmdsFromBuffer(buffer, &cmdstack);

                j = 0;
                while(cmdstack.sp > 0) //if commands were detected process them 
                {
                    sprintf(buffer, "%s", cmdstack.commands[j]);
                    cmdstack.commands[j][0] = '\0';
                    handle_upstream_requests(&tcp_down_cons[i].fd, tcp_down_cons[i].netid, buffer, isroot, &upcon, &aux, &table, pq_id, bestpops, tcp_down_cons, connections, tcpsessions, detailed_info, &flow, streamID, &treerootid, &rsid, &hostid, &streamFont);
                    cmdstack.sp--;
                    j++;
                }     
            }
        }
        
        if(FD_ISSET(accesscon.fd, &rfds)) //if access server file descriptor is ready to be read
        {
            if(handle_access_commands(accesscon.fd, streamID) == -1) //process commands and send appropriate resposes
            {
                printf("Received unknown command in access server!");
            }
        }

        if(FD_ISSET(0, &rfds)) //if stdin is ready to be read process user input accordingly
            user_input(&rsid, &running, tcp_down_cons, connections, &hostid, &upid, streamID);      
    }
	
	if(isroot) // if application is root, removes itself from the root server as root
    {
        sprintf(buffer, "REMOVE %s\n", streamID);
        root_send(buffer, &rsid);
    }
    
    for(i = 0; i < connections; i++) // close all downstream connections
        close(tcp_down_cons[i].fd);

    for(i = 0; i < tcpsessions; i++) //free downstream connection array
        free(tcp_down_cons[i].netid);

    printf("EXITING SUCCESSFULLY\n");
    //close and free all socket variables
	freeaddrinfo(downcon.con_addr);
    freeaddrinfo(upcon.con_addr);
    freeaddrinfo(accesscon.con_addr);
    close(downcon.fd);
    close(upcon.fd);
    close(accesscon.fd);

    return 0;
}

int root_send(char* command, NetId* rsid)
{
	/*
		function used to send a command to the root server.
		It creates a temporary UDP connection to the server and sends the desired command.
		It then waits for a response for 3 seconds if a respose is needed.
	*/
    char buf[BUFF_SIZE], temp[BUFF_SIZE];
    struct addrinfo *root_addr;
    int root_server_fd;
    int success = 0, scounter = 0;
    fd_set srfds;
    struct timeval t;
    t.tv_sec = 1;
    t.tv_usec = 0;

    buf[0] = '\0';
    temp[0] = '\0';

    sprintf(buf, "%s", command);
    sscanf(buf, "%s", temp);

    //GET ROOT_SERVER ADDR
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_NUMERICSERV;

    n = getaddrinfo(rsid->ip, rsid->uport, &hints, &root_addr);
    if(n != 0) return -1;

    root_server_fd = socket(root_addr->ai_family, root_addr->ai_socktype, root_addr->ai_protocol);
    if(root_server_fd == -1) return -1;

    n = sendto(root_server_fd, buf, strlen(buf), 0, root_addr->ai_addr, root_addr->ai_addrlen);
    if(n == -1) return -1;

    if(strcmp(temp, "REMOVE") != 0)
    {
        FD_ZERO(&srfds);
        FD_SET(root_server_fd, &srfds); maxfd = max(maxfd, root_server_fd);
        
        for(int j = 0; j < 3 && success != 1; j++)
        {
            scounter = select(maxfd+1, &srfds, NULL, NULL, &t);   

            if(scounter < 0)
                return -1;
            else if(scounter == 0)
            {
                t.tv_sec = 1;
                t.tv_usec = 0;
            }
            else
            {
                success = 1;
            }
        }
        if(!success)
        {   
            return -1;
        }

        n = recvfrom(root_server_fd, buf, BUFF_SIZE-1, 0, (struct sockaddr*) &addr, &addrlen);
        if(n == -1) return -1;
    }
    buf[n] = '\0';

    if(detailed_info)
    	write(1, buf, n);
    

    freeaddrinfo(root_addr);
    close(root_server_fd);

    return 0;
}

int who_is_root(char* streamID, int streamID_size, NetId* treerootid, NetId* rsid, NetId* hostid)
{
	/*
		This function creates a temporary UDP connection to the root server and queries to know who is the root of
		the stream with id = streamID. If no one is root the application that sends the query becomes root, else 
		the ip and port of the root application is obtained from the root server.
		The function waits for a response from the root server for 3 seconds.
	*/
    char buf[BUFF_SIZE];
    memset(buf, 0, BUFF_SIZE);
    char command[BUFF_SIZE];
    struct addrinfo *root_addr;
    int root_server_fd;
    int success = 0, scounter = 0;
    fd_set srfds;
    struct timeval t;
    t.tv_sec = 1;
    t.tv_usec = 0;


    sprintf(buf, "WHOISROOT %s %s:%s\n", streamID, hostid->ip, hostid->uport);

    //GET ROOT_SERVER ADDR
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_NUMERICSERV;
    
    n = getaddrinfo(rsid->ip, rsid->uport, &hints, &root_addr);
    if(n != 0) return -1;
    
    root_server_fd = socket(root_addr->ai_family, root_addr->ai_socktype, root_addr->ai_protocol);
    if(root_server_fd == -1) 
    {
        freeaddrinfo(root_addr);
        return -1;
    }
    
    n = sendto(root_server_fd, buf, strlen(buf), 0, root_addr->ai_addr, root_addr->ai_addrlen);
    if(n == -1) 
    {
        freeaddrinfo(root_addr);
        close(root_server_fd);
        return -1;
    }   

    FD_ZERO(&srfds);
    FD_SET(root_server_fd, &srfds); maxfd = max(maxfd, root_server_fd);
    
    for(int j = 0; j < 3 && success != 1; j++)
    {
        scounter = select(maxfd+1, &srfds, NULL, NULL, &t);   

        if(scounter < 0)
            return -1;
        else if(scounter == 0)
        {
            t.tv_sec = 1;
            t.tv_usec = 0;
        }
        else
        {
            success = 1;
        }
    }
    if(!success)
    {   
        return -1;
    }
    
    n = recvfrom(root_server_fd, buf, BUFF_SIZE-1, 0, (struct sockaddr*) &addr, &addrlen);
    if (n <= 0)
    {
        freeaddrinfo(root_addr);
        close(root_server_fd);
        return n;
    }

    buf[n] = '\0';

    if(detailed_info)
    {
        write(1, "Who_Is_Root receive: ", strlen("Who_Is_Root receive: "));
        write(1, buf, n);
    }

    sscanf(buf, "%s ", command);

    if(strcmp(command, "URROOT") == 0)
    {
        sprintf(treerootid->ip, "%s", hostid->ip);
        sprintf(treerootid->uport, "%s", hostid->uport);
        isroot = 1;
    }
    else if(strcmp(command, "ROOTIS") == 0)
    {
        sscanf(buf, "%*s %*s %s", buf);

        sscanf(buf, "%[^:]:%s", treerootid->ip, treerootid->uport);
        isroot = 0;
    }
    else
    {
        freeaddrinfo(root_addr);
        close(root_server_fd);
        return -1;
    }

    freeaddrinfo(root_addr);
    close(root_server_fd);
    return 1;
}

int get_access_point(NetId* treerootid, NetId* upid)
{
	/*
		This function creates a temporary UDP connection to the root application access server
		and queries it about a valid access point on the tree.
		It then waits for 3 seconds for a reply and if a reply is received, the upid variable is
		updated with the ip and port of the access point obtained.
	*/
    char buf[BUFF_SIZE];
    memset(buf, 0, BUFF_SIZE);
    char command[BUFF_SIZE], streamID[BUFF_SIZE];
    struct addrinfo *aaddr;
    int fd;
    int success = 0, scounter = 0;
    fd_set srfds;
    struct timeval t;
    t.tv_sec = 1;
    t.tv_usec = 0;

    sprintf(buf, "POPREQ\n");
    
    //GET ACCESS_SERVER ADDR
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_NUMERICSERV;

    n = getaddrinfo(treerootid->ip, treerootid->uport, &hints, &aaddr);
    if(n != 0) return -1;

    fd = socket(aaddr->ai_family, aaddr->ai_socktype, aaddr->ai_protocol);
    if(fd == -1)
    {
        freeaddrinfo(aaddr);
        return -1;
    }

    n = sendto(fd, buf, strlen(buf), 0, aaddr->ai_addr, aaddr->ai_addrlen);
    if(n == -1)
    {
        freeaddrinfo(aaddr);
        close(fd);
        return -1;
    }

    FD_ZERO(&srfds);
    FD_SET(fd, &srfds); maxfd = max(maxfd, fd);
    
    for(int j = 0; j < 3 && success != 1; j++)
    {
        scounter = select(maxfd+1, &srfds, NULL, NULL, &t);   

        if(scounter < 0)
            return -1;
        else if(scounter == 0)
        {
            t.tv_sec = 1;
            t.tv_usec = 0;
        }
        else
        {
            success = 1;
        }
    }
    if(!success)
    {   
        return -1;
    }

    n = recvfrom(fd, buf, BUFF_SIZE-1, 0, (struct sockaddr*) &addr, &addrlen);
    if(n <= 0)
    {
        freeaddrinfo(aaddr);
        close(fd);
        return n;
    }
    
    addrlen = sizeof(addr);
    
    buf[n] = '\0';

    if(detailed_info)
    {
        write(1, "Access_Point receive: ", strlen("Access_Point receive: "));
        write(1, buf, n);
    }

    sscanf(buf, "%s ", command);

    if(strcmp(command, "POPRESP") == 0)
    {
        sscanf(buf, "%*s %s %s", streamID, buf);
        sscanf( buf, "%[^:]:%s", upid->ip, upid->tport);
    }
    else
    {
        freeaddrinfo(aaddr);
        close(fd);
        return -1;
    }

    freeaddrinfo(aaddr);
    close(fd);
    return 1;
}

int handle_access_commands(int fd, char* streamID)
{
	/*
		This function processes the commands received by the access server socket.
	*/
    char buf[BUFF_SIZE];
    memset(buf, 0, BUFF_SIZE);
    
    n = recvfrom(fd, buf, BUFF_SIZE-1, 0, (struct sockaddr*) &addr, &addrlen);
    if(n == -1)
    {
        printf("Error receiving in access server: %s\n", strerror(errno));
        fflush(stdout);
        exit(1);
    }
    
    addrlen = sizeof(addr);
    
    buf[n] = '\0';
    
    if(strcmp(buf, "POPREQ\n") == 0)
    {
        if(table.total_acs > 0)
        {
            sprintf(buf, "POPRESP %s %s:%s\n", streamID, table.acs[table.iterator].id.ip, table.acs[table.iterator].id.tport);
            table.iterator = (table.iterator + 1)%(table.total_acs);
        }
        
        n = sendto(fd, buf, strlen(buf), 0, (struct sockaddr*)&addr, addrlen);
        if(n == -1) 
        {
            printf("Error sending from access server: %s\n", strerror(errno));
            fflush(stdout);
            exit(1);
        }
    }
    else
    {
        printf("Unknown command: %s\n", buf);
        return -1;
    }
    return 0;
}

int connect_upstream_tcp(NetCon* upcon, NetId* rsid, char* streamID)
{
	/*
		This function creates a TCP connection to a par upstream using the ip and port located in the variable upcon.
	*/

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV;
    
    n = getaddrinfo(upcon->netid->ip, upcon->netid->tport, &hints, &upcon->con_addr);
    if(n != 0)
    {
        printf("Error getting upstream par info: %s\n", strerror(errno));
        fflush(stdout);
        if(isroot)
        {
            sprintf(buffer, "REMOVE %s\n", streamID);
            root_send(buffer, rsid);
        }
        return -1;
    }

    
    upcon->fd = socket(upcon->con_addr->ai_family, upcon->con_addr->ai_socktype, upcon->con_addr->ai_protocol);
    if(upcon->fd == -1)
    {
        printf("Error creating socket for upstream par: %s\n", strerror(errno));
        fflush(stdout);
        if(isroot)
        {
            sprintf(buffer, "REMOVE %s\n", streamID);
            root_send(buffer, rsid);
        }
        return -1;
    }

    n = connect(upcon->fd, upcon->con_addr->ai_addr, upcon->con_addr->ai_addrlen);
    if(n == -1)
    {
        printf("Error connecting with upstream par: %s\n", strerror(errno));
        fflush(stdout);
        if(isroot)
        {
            sprintf(buffer, "REMOVE %s\n", streamID);
            root_send(buffer, rsid);
        }
        return -1;
    }
    if(isroot)
    {
        flow = 1;
    }

    return 0;
}

void readCmdsFromBuffer(char* buffer, CommStack* stack)
{
	/*
		This function parses the buffer and tries to identify commands known to the application.
		If such commands are found they are moved to a stack to later be processed.
	*/
    char cmd[10] = "\0";
    uint16_t dsize = 0;


    if(detailed_info)
    {
        printf("\n----------------------------\n");
        printf("reading from buffer with contents: \n%s\n", buffer);
    }

    for(int i = 0, aux = 0, istr = 0, isda = 0; i < strlen(buffer); i++)
    {
        if(buffer[i] == '\n')
        {
            sscanf(&buffer[aux], "%s", cmd);
            
            if(strcmp("TR", cmd) == 0) istr = 1;
           
            if(istr && buffer[i+1] != '\n')
                continue;
            else if(istr)
                i++;

            if(strcmp("DA", cmd) == 0) isda = 1;

            if(isda)
            {
                sscanf(&buffer[aux], "%*s %hx", &dsize);
                i += dsize;
            }
 
            memcpy(&(stack->commands[stack->sp]), &buffer[aux], i - aux + 1);
            stack->commands[stack->sp][i - aux + 1] = '\0';

            if(detailed_info)
                printf("pos %d in stack has: \n%s", stack->sp, stack->commands[stack->sp]);

            stack->sp++;
            aux = i+1;
            istr = 0;
        }
    }

    if(detailed_info)
        printf("----------------------------\n\n");
}

void user_input(NetId* rsid, int* running, NetCon* tcp_down_cons, int connections, NetId* hostid, NetId* upid, char* streamID)
{
	/*
		This function is used to process data from stdin and act accordingly.
		All control variables are changed here and its also here that the TREE QUERY command is performed.
	*/
    char input_buffer[BUFF_SIZE];
    char buffer[BUFF_SIZE];
    int i = 0;

    fgets(input_buffer, BUFF_SIZE-1, stdin);
    fflush(stdin);
    input_buffer[strlen(input_buffer) - 1] = '\0';
    
    //convert to all lower case letters (not case sensitive)
    for(i = 0; i < strlen(input_buffer); i++)
    {
        if(input_buffer[i] >= 65 && input_buffer[i] <= 90)
        {
            input_buffer[i] += 32;
        }
    }
    
    //process command
    if(strcmp(input_buffer, "exit") == 0)
    {
        *running = 0;
    }
    else if(strcmp(input_buffer, "streams") == 0)
    {
        root_send("DUMP\n", rsid);
    }
    else if(strcmp(input_buffer, "status") == 0)
    {
        printf("StreamID: %s\n", streamID);
        if(flow)
        {
            printf("Stream flowing.\n");
        }
        else
        {
            printf("Broken stream.\n");
        }
        if(isroot)
        {
            printf("I am root\n");
            printf("Access server address: %s:%s\n", hostid->ip, hostid->uport);
        }
        else
        {
            printf("Root, I'm not\n");
            printf("Connected to access point: %s:%s\n", upid->ip, upid->tport);
        }
        printf("Available access point: %s:%s\n", hostid->ip, hostid->tport);
        printf("Supported TCP sessions: %d\nOccupied TCP sessions: %d\n", tcpsessions, connections);
        if(connections > 0)
        {
	        printf("Access pairs immediately below [IP:TCP PORT]:\n");
	        for(i = 0; i < connections; i++)
	        {
	            printf("%s:%s\n", tcp_down_cons[i].netid->ip, tcp_down_cons[i].netid->tport);
	        }
	    }
	    else
	    	printf("No pairs connected downstream.\n");
    }
    else if(strcmp(input_buffer, "display on") == 0){ show_stream_data = 1; }
    else if(strcmp(input_buffer, "display off") == 0){ show_stream_data = 0; }
    else if(strcmp(input_buffer, "format ascii") == 0){ ascii = 1;}
    else if(strcmp(input_buffer, "format hex") == 0){ ascii = 0;}
    else if(strcmp(input_buffer, "debug on") == 0){ detailed_info = 1;}
    else if(strcmp(input_buffer, "debug off") == 0){ detailed_info = 0;}
    else if(strcmp(input_buffer, "tree") == 0)
    {     
        if(isroot)
        {
            printf("TR %s:%s %d\n", hostid->ip, hostid->tport, tcpsessions);

            for(int i = 0; i < connections; i++)
            {
                printf("%s:%s\n", tcp_down_cons[i].netid->ip, tcp_down_cons[i].netid->tport);
                sprintf(buffer, "TQ %s:%s\n", tcp_down_cons[i].netid->ip, tcp_down_cons[i].netid->tport);
                if(write(tcp_down_cons[i].fd, buffer, strlen(buffer)) <= 0)
                {
                    printf("Error sending data downstream: %s\n", strerror(errno));
                    fflush(stdout);
                    exit(1);
                }
            }
            printf("\n");
            
        }
        else
        {
            printf("Non root pars cant issue tree command, sorry!\n");
            fflush(stdout);
        }
    }
    else if(strcmp(input_buffer, "root send") == 0)
    {
        printf("Message to send: ");
        fgets(input_buffer, BUFF_SIZE-1, stdin);
        fflush(stdin);

        if(root_send(input_buffer, rsid) == -1)
            exit(1);

    }
    else
    {
        printf("Unknown command: %s\n", input_buffer);
    }
}