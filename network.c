/* 
 * File:   network.c
 * Author: tobias
 *
 * Created on 26. November 2013, 15:43
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>

// GRAPES
#include <net_helper.h>
#include <chunk.h>
#include <chunkbuffer.h>
#include <chunkidset.h>
#include <peer.h>
#include <peerset.h>

#include "network.h"
#include "streamer.h"

char *network_create_interface(char *interface) {
#ifdef DEBUG
    fprintf(stdout, "Called network_create_interface with parameter interface=%s\n", interface);
#endif

    int socketHandle;
    int res;
    struct ifreq iface_request;
    struct sockaddr_in *sin;
    char buff[512];

    socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (socketHandle < 0) {
        fprintf(stderr, "Could not create socket\n");
        exit(EXIT_FAILURE);
    }

    memset(&iface_request, 0, sizeof (struct ifreq));
    sin = (struct sockaddr_in *) & iface_request.ifr_addr;
    strcpy(iface_request.ifr_name, interface);
    /* sin->sin_family = AF_INET); */
    res = ioctl(socketHandle, SIOCGIFADDR, &iface_request);
    if (res < 0) {
        perror("ioctl(SIOCGIFADDR)");
        close(socketHandle);

        exit(EXIT_FAILURE);
    }
    close(socketHandle);

    inet_ntop(AF_INET, &sin->sin_addr, buff, sizeof (buff));

    return strdup(buff);
}

int network_add_to_peerChunk(struct nodeID *remote, int chunkId) {
    // check if chunk is not too old and is still in the chunk buffer
    if (cb_get_chunk((struct chunk_buffer*) chunkBuffer, chunkId) != NULL) {
        peerChunks = (struct PeerChunk*) realloc(peerChunks, sizeof (struct PeerChunk));
        peerChunks[peerChunksSize].peer = peerset_get_peer((struct peerset*) peerSet, remote);
        peerChunks[peerChunksSize++].chunk = chunkId;
    } else {
        return -1;
    }
    return 0;
}

struct ChunkIDSet *network_chunkBuffer_to_buffermap() {
    struct chunk *chunks;
    int num_chunks, i;
    struct ChunkIDSet *my_bmap = (struct ChunkIDSet*) chunkID_set_init(0);
    chunks = cb_get_chunks((struct chunk_buffer*) chunkBuffer, &num_chunks);

    for (i = 0; i < num_chunks; i++) {
        chunkID_set_add_chunk((struct chunkID_set*) my_bmap, chunks[i].id);
    }
    return my_bmap;
}

void network_send_chunks_to_peers() {
    char addressRemote[256];
    int i;

    for (i = 0; i < peerChunksSize; ++i) {
        node_addr(peerChunks[i].peer->id, addressRemote, 256);
        fprintf(stderr, "Sending chunk %d to peer %s", peerChunks[i].chunk, addressRemote);
        sendChunk(peerChunks[i].peer->id, cb_get_chunk((struct chunk_buffer*) chunkBuffer, (int) peerChunks[i].chunk), 0);
    }

    // reset peerChunks
    free(peerChunks);
    peerChunksSize = 0;
}

void network_print_chunkBuffer() {
    struct chunk *chunks;
    int num_chunks, i;
    chunks = cb_get_chunks((struct chunk_buffer*) chunkBuffer, &num_chunks);

    for (i = 0; i < num_chunks; i++) {
        fprintf(stderr, "DEBUG: \t%3d: ChunkID %d\n", i, chunks[i].id);
    }
}
