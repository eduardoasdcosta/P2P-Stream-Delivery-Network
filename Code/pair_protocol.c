#include "pair_protocol.h"

void handle_downstream_requests(NetCon* upcon, char* buffer, NetId* hostid, int tcpsessionsleft, NetCon* tcp_down_cons, int connections, NetId* rsid, char* streamID, NetId* treerootid, int isroot, int detailed_info, int *flow, int show_stream_data, int ascii, NetId* streamFont)
{
    //This functions handles all the downstream messages, this is, messages that are sent from the upstream connection

    // buffers
    char command[50];
    char temp1[BUFF_SIZE];
    char temp2[BUFF_SIZE];
    uint16_t n = 0;
    int i = 0;
    
    //initialize buffers
    command[0] = '\0';
    temp1[0] = '\0';
    temp2[0] = '\0';


    //identify the command
    sscanf(buffer, "%s", command);

    if(detailed_info)
    {
        printf("Downstream: Got %s.\n", command);
        fflush(stdout);
    }
    
    if(strcmp(command, "WE") == 0)
    {
        // replies with a New Pop
        sprintf(temp1, "NP %s:%s\n", hostid->ip, hostid->tport);
        if(detailed_info)
        {
            printf("Sending NP.\n");
            fflush(stdout);
        }
        if(write(upcon->fd, temp1, strlen(temp1)) <= 0)
        {
            if(detailed_info)
            {
                printf("Connection ended upstream: %s\n", strerror(errno));
                fflush(stdout);
            }
            close(upcon->fd);
    
            if(who_is_root(streamID, strlen(streamID), treerootid, rsid, hostid) == -1)
            {    
                printf("Error connecting to root server: %s\n", strerror(errno));
                fflush(stdout);
                exit(1);
            }
            
            if(!isroot)
            {
                sleep(3);
                if(get_access_point(treerootid, upcon->netid) == -1)
                {
                    printf("Error getting access point: %s\n", strerror(errno));
                    fflush(stdout);
                    exit(1);
                }
                
                for(i = 0; i < 3; i++)
                {
                    n = connect_upstream_tcp(upcon, rsid, streamID);
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
                upcon->netid = streamFont;
                for(i = 0; i < 3; i++)
                {
                    n = connect_upstream_tcp(upcon, rsid, streamID);
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

                *flow = 1;
                if(connections > 0 && detailed_info)
                {
                    printf("Sending SF.\n");
                    fflush(stdout);
                }
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
            }
        }
    }
    else if(strcmp(command, "RE") == 0)
    {
        // updates the ip and port of the upstream server
        sscanf(buffer, "%*s %s", temp1);
        sscanf(temp1, "%[^:]:%s", upcon->netid->ip, upcon->netid->tport);
        
        if(close(upcon->fd) == -1)
        {
            printf("Error closing file descriptor in RE\n");
            fflush(stdout);
            exit(1);
        }
        // connects to upstream server
        for(i = 0; i < 3; i++)
        {
            n = connect_upstream_tcp(upcon, rsid, streamID);
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
    else if(strcmp(command, "SF") == 0)
    {
        // sets variable flow to 1 -> stream flowing
        *flow = 1;
        // replicates stream flow message to downstream connections
        if(connections > 0 && detailed_info)
        {
            printf("Sending SF downstream.\n");
            fflush(stdout);
        }
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
    else if(strcmp(command, "BS") == 0)
    {
        // sets variable flow to 0 -> stream broken
        *flow = 0;
        if(connections > 0 && detailed_info)
        {
            printf("Sending BS.\n");
            fflush(stdout);
        }
        // replicates stream broken message to downstream connections
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
    else if(strcmp(command, "DA") == 0)
    {
        // prints DA message on screen - ASCII/HEX
        if(show_stream_data)
        {
            sscanf(buffer, "%*s %hx", &n);
            strncpy(temp1, &buffer[8], n);
            temp1[n] = '\0';
            fflush(stdout);
            if(!ascii)
            {
                for(i = 0; i < n; i++)
                {
                    sprintf(temp2 + i*2, "%02X", temp1[i]);
                }
                write(1, temp2, strlen(temp2));
                write(1, "\n", 1);
            }
            else
            {
                write(1, temp1, strlen(temp1));
            }
        }
        // replicates DA message to downstream connections
        if(connections > 0 && detailed_info)
        {
            printf("Sending DA.\n");
            fflush(stdout);
        }
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
    else if(strcmp(command, "PQ") == 0)
    {
        // if no more connections are available replicates PQ to downstream connections
        if(tcpsessionsleft <= 0)
        {
            if(connections > 0 && detailed_info)
            {
                printf("Sending PQ.\n");
                fflush(stdout);
            }
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
        // if connections are available
        else
        {
            uint16_t queryid;
            int bestpops;

            sscanf(buffer, "%*s %hx %d", &queryid, &bestpops);
            // reply with pop reply
            sprintf(temp1, "PR %.4X %s:%s %d\n", queryid, hostid->ip, hostid->tport, tcpsessionsleft);
            if(detailed_info)
            {
                printf("Sending PR.\n");
                fflush(stdout);
            }
            if(write(upcon->fd, temp1, strlen(temp1)) <= 0)
            {
                if(detailed_info)
                {
                    printf("Connection ended abruptely upstream PQ\n");
                    fflush(stdout);
                }
                close(upcon->fd);
                *flow = 0;
                if(connections > 0 && detailed_info)
                {
                    printf("Sending BS.\n");
                    fflush(stdout);
                }
                for(i = 0; i < connections; i++)
                { 
                    if(write(tcp_down_cons[i].fd, "BS\n", strlen("BS\n")) <= 0)
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

        
                if(who_is_root(streamID, strlen(streamID), treerootid, rsid, hostid) == -1)
                {    
                    printf("Error connecting to root server: %s\n", strerror(errno));
                    fflush(stdout);
                    exit(1);
                }
                
                if(!isroot)
                {
                    sleep(3);
                    if(get_access_point(treerootid, upcon->netid) == -1)
                    {
                        printf("Error getting access point: %s\n", strerror(errno));
                        fflush(stdout);
                        exit(1);
                    }
                    
                    for(i = 0; i < 3; i++)
                    {
                        n = connect_upstream_tcp(upcon, rsid, streamID);
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
                    upcon->netid = streamFont;
                    for(i = 0; i < 3; i++)
                    {
                        n = connect_upstream_tcp(upcon, rsid, streamID);
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

                    *flow = 1;
                    if(connections > 0 && detailed_info)
                    {
                        printf("Sending SF.\n");
                        fflush(stdout);
                    }
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
                }
            }
            else
            {
                // decrements number of bestpops needed
                bestpops--;
                // if there are still more requested bestpops, send pop query downstream with the updated number of bestpops
                if(bestpops > 0)
                {
                    sprintf(buffer, "PQ %.4X %d\n", queryid, bestpops);
                    if(connections > 0 && detailed_info)
                    {
                        printf("Sending PQ.\n");
                        fflush(stdout);
                    }
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
            }         
        }
    }
    else if(strcmp(command, "TQ") == 0)
    {
        // get ip and port of tree query and iamroot app
        sscanf(buffer, "%*s %s\n", temp2);
        sprintf(temp1, "%s:%s", hostid->ip, hostid->tport);
        // if both ips and ports corespond with each other, send tree reply
        if(strcmp(temp1, temp2) == 0)
        {
            // write 1st line of tree reply
            sprintf(buffer, "TR %s:%s %d\n", hostid->ip, hostid->tport, tcpsessionsleft + connections);
            // write a line for every adjacent downstream connection
            for(i = 0; i < connections; i++)
            {
                sprintf(buffer + strlen(buffer), "%s:%s\n", tcp_down_cons[i].netid->ip, tcp_down_cons[i].netid->tport);
            }
            
            sprintf(buffer + strlen(buffer), "\n");
            if(detailed_info)
            {
                printf("Sending TR.\n");
                fflush(stdout);
            }
            if(write(upcon->fd, buffer, strlen(buffer)) <= 0)
            {
                if(detailed_info)
                {
                    printf("Connection ended abruptely upstream TQ\n");
                    fflush(stdout);
                }    
                close(upcon->fd);
                *flow = 0;
                if(connections > 0 && detailed_info)
                {
                    printf("Sending BS.\n");
                    fflush(stdout);
                }

                for(i = 0; i < connections; i++)
                { 
                    if(write(tcp_down_cons[i].fd, "BS\n", strlen("BS\n")) <= 0)
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
            
                if(who_is_root(streamID, strlen(streamID), treerootid, rsid, hostid) == -1)
                {    
                    printf("Error connecting to root server: %s\n", strerror(errno));
                    fflush(stdout);
                    exit(1);
                }
                
                if(!isroot)
                {
                    sleep(3);
                    if(get_access_point(treerootid, upcon->netid) == -1)
                    {
                        printf("Error getting access point: %s\n", strerror(errno));
                        fflush(stdout);
                        exit(1);
                    }
                    
                    for(i = 0; i < 3; i++)
                    {
                        n = connect_upstream_tcp(upcon, rsid, streamID);
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
                    upcon->netid = streamFont;
                    for(i = 0; i < 3; i++)
                    {
                        n = connect_upstream_tcp(upcon, rsid, streamID);
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

                    *flow = 1;
                    if(connections > 0 && detailed_info)
                    {
                        printf("Sending SF.\n");
                        fflush(stdout);
                    }
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
                }
            }
            else if(detailed_info)
                printf("sending up: %s", buffer);
        }
        // if tree query is for other iamroot app, replicate message downstream
        else
        {
            if(connections > 0 && detailed_info)
            {
                printf("Sending TQ.\n");
                fflush(stdout);
            }
            for(i = 0; i < connections; i++)
            { 
                if(write(tcp_down_cons[i].fd, buffer, strlen(buffer)) <= 0)
                {
                    printf("Connection ended abruptely downstream\n");
                    fflush(stdout);
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
    else
    {
        printf("UNKNOWN MESSAGE FROM DOWNSTREAM DETECTED...\n");
    }
}

void handle_upstream_requests(int* dwfd, NetId* down_id, char* buffer, int isroot, NetCon* upcon, AccessPointTable* aux, AccessPointTable* table, uint16_t pop_id, int bestpops, NetCon* tcp_down_cons, int connections, int tcpsessions, int detailed_info, int *flow, char* streamID, NetId* treerootid, NetId* rsid, NetId* hostid, NetId* streamFont)
{
    //This functions handles all the upstream messages, this is, messages that are sent from the downstream connections

    // buffers
    char command[50];
    char temp1[BUFF_SIZE];
    char temp2[BUFF_SIZE];

    int i = 0, j = 0;
    size_t n = 0;

    // initialize buffers
    command[0] = '\0';
    temp1[0] = '\0';
    temp2[0] = '\0';

    // get command
    sscanf(buffer, "%s", command);
    if(detailed_info)
    {
        printf("Upstream: Got %s\n", command);
        fflush(stdout);
    }
        
    if(strcmp(command, "NP") == 0)
    {
        // writes the new connection's ip and port
        sscanf(buffer, "%*s %[^:]:%s", down_id->ip, down_id->tport);
    }
    else if(strcmp(command, "PR") == 0)
    {
        // if iamroot app isnt root, replicates message upstream
        if(!isroot)
        {
            if(detailed_info)
            {
                printf("Sending PR.\n");
                fflush(stdout);
            }
            if(write(upcon->fd, buffer, strlen(buffer)) <= 0)
            {
                if(detailed_info)
                {
                    printf("Connection ended abruptely upstream PR\n");
                    fflush(stdout);
                }
                close(upcon->fd);
            
                if(who_is_root(streamID, strlen(streamID), treerootid, rsid, hostid) == -1)
                {    
                    printf("Error connecting to root server: %s\n", strerror(errno));
                    fflush(stdout);
                    exit(1);
                }
                
                if(!isroot)
                {
                    sleep(3);
                    if(get_access_point(treerootid, upcon->netid) == -1)
                    {
                        printf("Error getting access point: %s\n", strerror(errno));
                        fflush(stdout);
                        exit(1);
                    }
                    
                    for(i = 0; i < 3; i++)
                    {
                        n = connect_upstream_tcp(upcon, rsid, streamID);
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
                    upcon->netid = streamFont;
                    for(i = 0; i < 3; i++)
                    {
                        n = connect_upstream_tcp(upcon, rsid, streamID);
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

                    *flow = 1;
                    if(connections > 0 && detailed_info)
                    {
                        printf("Sending SF.\n");
                        fflush(stdout);
                    }

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
                }
            }
        }
        // if is root, updates the access points table
        else
        {
            uint16_t queryid;
            sscanf(buffer, "%*s %hx", &queryid);
            // if queryID is valid and the requested number of bestpops hasn't been fullfilled yet
            if(queryid+1 == pop_id && aux->total_acs < bestpops)
            {
                // get bestpop's IP, port and available connections, and writes it in an aux table
                sscanf(buffer, "%*s %*x %[^:]:%s %d", aux->acs[aux->total_acs].id.ip, aux->acs[aux->total_acs].id.tport, &aux->acs[aux->total_acs].avails);
                // increments the number of bestpops in aux table
                aux->total_acs++;
                // if the number of bestpops is the one requested, updates bestpops table with the contents of aux table
                if(aux->total_acs == bestpops)
                {
                    for(i = 0; i < aux->total_acs; i++)
                        table->acs[i] = aux->acs[i];
                    
                    table->total_acs = aux->total_acs;
                    table->iterator = 0;
                }
            }
        }

    }
    else if(strcmp(command, "TR") == 0)
    {
        // if iamroot isnt root
        if(!isroot)
        {
            if(detailed_info)
            {
                printf("Sending TR.\n");
                fflush(stdout);
            }
            // replicates tree reply upstream
            if(write(upcon->fd, buffer, strlen(buffer)) <= 0)
            {
                printf("Connection ended abruptely upstream TR\n");
                fflush(stdout);
                
                close(upcon->fd);
            
                if(who_is_root(streamID, strlen(streamID), treerootid, rsid, hostid) == -1)
                {    
                    printf("Error connecting to root server: %s\n", strerror(errno));
                    fflush(stdout);
                    exit(1);
                }
                
                if(!isroot)
                {
                    sleep(3);
                    if(get_access_point(treerootid, upcon->netid) == -1)
                    {
                        printf("Error getting access point: %s\n", strerror(errno));
                        fflush(stdout);
                        exit(1);
                    }
                    
                    for(i = 0; i < 3; i++)
                    {
                        n = connect_upstream_tcp(upcon, rsid, streamID);
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
                    upcon->netid = streamFont;
                    for(i = 0; i < 3; i++)
                    {
                        n = connect_upstream_tcp(upcon, rsid, streamID);
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

                    *flow = 1;
                    if(connections > 0 && detailed_info)
                    {
                        printf("Sending SF.\n");
                        fflush(stdout);
                    }
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
                }
            }
            else if(detailed_info)
            {
                printf("redirecting up: %s", buffer);
            }
        }
        // is iamroot is root
        else
        {
            char* pointer = NULL;
            char* dummy = NULL;
            i = 0;
            j = 0;

            // prints tree reply message
            printf("%s", buffer);

            while(1)
            {
                // if end of tree reply message is reached
                if(buffer[i] == '\0')
                {
                    break;
                }
                // if end of a line in tree reply is reached
                else if(buffer[i] == '\n')
                {
                    // dummy gets position of '\n'
                    dummy = &buffer[i];
                    // if it's not the 1st line of TR message, it means that a Tree Query message must be issued
                    if(pointer != NULL)
                    {
                        // if it's not the end of tree reply -> two '\n' in a row
                        if(dummy - pointer != 0)
                        {
                            // copies line to temp1
                            strncpy(temp1, pointer, dummy - pointer);
                            temp1[dummy - pointer] = '\0';
                            // modifies line in order to follow the TREE QUERY message protocol
                            sprintf(temp2, "TQ %s\n", temp1);
                            
                            if(connections > 0 && detailed_info)
                            {
                                printf("Sending TQ.\n");
                                fflush(stdout);
                            }
                            // sends tree queries downstream to all connections
                            for(j = 0; j < connections; j++)
                            { 
                                if(write(tcp_down_cons[j].fd, temp2, strlen(temp2)) <= 0)
                                {
                                    printf("Connection ended abruptely downstream\n");
                                    fflush(stdout);
                                    
                                    if(detailed_info)
                                    {
                                        printf("Connection ended downstream. Reorganizing down pairs: %s\n", strerror(errno));
                                        fflush(stdout);
                                    }
                                    close(tcp_down_cons[j].fd);
                                    if(j != --connections)
                                    {
                                        if(detailed_info)
                                        {
                                            printf("gonna delete: %s:%s\n", tcp_down_cons[j].netid->ip, tcp_down_cons[j].netid->tport);
                                            printf("and replace the array pos with: %s:%s\n", tcp_down_cons[connections].netid->ip, tcp_down_cons[connections].netid->tport);
                                        } 
                                        tcp_down_cons[j].fd = tcp_down_cons[connections].fd;
                                        memcpy(tcp_down_cons[j].netid, tcp_down_cons[connections].netid, sizeof(NetId));

                                        tcp_down_cons[connections].fd = 0;
                                        memset(tcp_down_cons[connections].netid, 0, sizeof(NetId));
                                        
                                        j--;
                                    }

                                }
                            }
                        }
                    }
                    // updates the position of the pointer to the beginning of a new line
                    pointer = dummy + 1;
                    
                    //if end of tree reply
                    if(*(pointer) == '\0')
                    {
                        break;
                    }
                    
                    i++;
                }
                // if middle of a line
                else
                {
                    i++;
                }  
            }
        }
    }
    else 
    {
        printf("UNKNOWN COMMAND DETECTED UPSTREAM: %s\n\n", buffer);
    }
}
