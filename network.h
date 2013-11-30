/* 
 * File:   network.h
 * Author: tobias
 *
 * Created on 26. November 2013, 08:16
 */

#ifndef NETWORK_H
#define	NETWORK_H

// GRAPES
#include <net_helper.h>

char *network_create_interface(char *interface);
int network_add_to_peerChunk(struct nodeID *remote, int chunkId);
struct ChunkIDSet *network_chunkBuffer_to_buffermap();
void network_send_chunks_to_peers();
void network_print_chunkBuffer();


#endif	/* NETWORK_H */

