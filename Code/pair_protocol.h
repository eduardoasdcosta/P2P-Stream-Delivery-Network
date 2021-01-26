#ifndef PAIR_PROTOCOL_H
#define PAIR_PROTOCOL_H

#include "iamroot.h"

void handle_downstream_requests(NetCon* upcon, char* buffer, NetId* hostid, int tcpsessionsleft, NetCon* tcp_down_cons, int connections, NetId* rsid, char* streamID, NetId* treerootid, int isroot, int detailed_info, int *flow, int show_stream_data, int ascii, NetId* streamFont);
void handle_upstream_requests(int* dwfd ,NetId* down_id ,char* buffer, int isroot, NetCon* upcon, AccessPointTable* aux, AccessPointTable* table, uint16_t pop_id, int bestpops, NetCon* tcp_down_cons, int connections, int tcpsessions, int detailed_info, int *flow, char* streamID, NetId* treerootid, NetId* rsid, NetId* hostid, NetId* streamFont);

#endif
