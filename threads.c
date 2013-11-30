/* 
 * File:   threads.c
 * Author: tobias
 *
 * Created on 26. November 2013, 10:16
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

// GRAPES
#include <grapes_msg_types.h>
#include <chunk.h>
#include <chunkbuffer.h>
#include <net_helper.h>
#include <peer.h>
#include <peersampler.h>
#include <chunkidset.h>
#include <trade_sig_ha.h>

#include "threads.h"
#include "streamer.h"
#include "network.h"

#define BUFFSIZE 64 * 1024

pthread_mutex_t chunkBufferMutex;
pthread_mutex_t topologyMutex;
pthread_mutex_t peerChunkMutex;

static int stopThreads;
static int transId = 1;

static void *threads_receive_data(void *mut) {
#ifdef DEBUG
    fprintf(stderr, "DEBUG: Thread started receive_data\n");
#endif

    while (stopThreads != 1) {
        int numberOfReceivedBytes;
        struct nodeID *remote;
        static uint8_t buffer[BUFFSIZE];

        numberOfReceivedBytes = recv_from_peer(localSocket, &remote, buffer, BUFFSIZE);
        switch (buffer[0] /* Message Type */) {
            case MSG_TYPE_TOPOLOGY:
            {
#ifdef DEBUG
                char remoteAddress[256];
                node_addr(remote, remoteAddress, 256);
                fprintf(stderr, "DEBUG: Received topology message from peer %s!\n", remoteAddress);
#endif
                pthread_mutex_lock(&topologyMutex);
                psample_parse_data(peersampleContext, buffer, numberOfReceivedBytes);
                pthread_mutex_unlock(&topologyMutex);
                break;
            }
            case MSG_TYPE_CHUNK:
            {
#ifdef DEBUG
                char remoteAddress[256];
                node_addr(remote, remoteAddress, 256);
                fprintf(stderr, "DEBUG: Received a chunk from %s!\n", remoteAddress);
#endif
                pthread_mutex_lock(&chunkBufferMutex);
                pthread_mutex_lock(&topologyMutex);

                int res;
                static struct chunk c;
                struct peer *p;
                static int bcast_cnt;
                uint16_t transid;

                res = parseChunkMsg(buff + 1, len - 1, &c, &transid);
                if (res > 0) {
                    chunk_attributes_update_received(&c);
                    chunk_unlock(c.id);
                    dprintf("Received chunk %d from peer: %s\n", c.id, node_addr_tr(from));
                    if (chunk_log) {
                        fprintf(stderr, "TEO: Received chunk %d from peer: %s at: %"PRIu64" hopcount: %i Size: %d bytes\n", c.id, node_addr_tr(from), gettimeofday_in_us(), chunk_get_hopcount(&c), c.size);
                    }
                    output_deliver(&c);
                    res = cb_add_chunk(cb, &c);
                    reg_chunk_receive(c.id, c.timestamp, chunk_get_hopcount(&c), res == E_CB_OLD, res == E_CB_DUPLICATE);
                    cb_print();
                    if (res < 0) {
                        dprintf("\tchunk too old, buffer full with newer chunks\n");
                        if (chunk_log) {
                            fprintf(stderr, "TEO: Received chunk: %d too old (buffer full with newer chunks) from peer: %s at: %"PRIu64"\n", c.id, node_addr_tr(from), gettimeofday_in_us());
                        }
                        free(c.data);
                        free(c.attributes);
                    }
                    p = nodeid_to_peer(from, neigh_on_chunk_recv);
                    if (p) { //now we have it almost sure
                        chunkID_set_add_chunk(p->bmap, c.id); //don't send it back
                        gettimeofday(&p->bmap_timestamp, NULL);
                    }
                    ack_chunk(&c, from, transid); //send explicit ack
                    if (bcast_after_receive_every && bcast_cnt % bcast_after_receive_every == 0) {
                        bcast_bmap();
                    }
                } else {
                    fprintf(stderr, "\tError: can't decode chunk!\n");
                }

                ////////////////////////////////////////////////////////////////
                ////////////////////////////////////////////////////////////////
                // OLD
                ////////////////////////////////////////////////////////////////
                ////////////////////////////////////////////////////////////////
                /*
                // decode chunk from buffer...
                struct chunk *chunkReceived;
                chunkReceived = (struct chunk*) malloc(sizeof (struct chunk));
                parseChunkMsg(buffer, numberOfReceivedBytes, chunkReceived, transId++);

                // add chunk to chunkbuffer
                cb_add_chunk((struct chunk_buffer*) chunkBuffer, chunkReceived);

                // add chunk to chunkid set
                chunkID_set_add_chunk((struct chunkID_set*) chunkIDSet, chunkReceived->id);
#ifdef DEBUG
                fprintf(stderr, "DEBUG: Chunks in buffer:\n");
                network_print_chunkBuffer();
#endif
                */
                ////////////////////////////////////////////////////////////////
                ////////////////////////////////////////////////////////////////
                // OLD
                ////////////////////////////////////////////////////////////////
                ////////////////////////////////////////////////////////////////

                pthread_mutex_unlock(&topologyMutex);
                pthread_mutex_unlock(&chunkBufferMutex);
                break;
            }
            case MSG_TYPE_SIGNALLING:
            {
                struct nodeID *owner;
                int maxDeliver;
                uint16_t transId;
                enum signaling_type signalingType;
                int result;
                ChunkIDSet *chunkIDSetReceived;

                result = parseSignaling(buffer + 1, numberOfReceivedBytes - 1, &owner, &chunkIDSetReceived, &maxDeliver, &transId, &signalingType);
                if (owner)
                    nodeid_free(owner);

                switch (signalingType) {
                    case sig_accept:
                    {
#ifdef DEBUG
                        fprintf(stderr, "DEBUG: 1) Message ACCEPT: peer accepted %d chunks\n", chunkID_set_size(chunkIDSetReceived));
#endif
                        pthread_mutex_lock(&peerChunkMutex);
                        int i, chunkID;
                        for (i = 0; i < chunkID_set_size(chunkIDSetReceived); ++i) {
                            chunkID = chunkID_set_get_chunk(chunkIDSetReceived, i);

                            // add to PeerChunk
                            network_add_to_peerChunk(remote, chunkID);
                        }
                        pthread_mutex_unlock(&peerChunkMutex);

                        break;
                    }
                    case sig_deliver:
                    {
#ifdef DEBUG
                        fprintf(stderr, "DEBUG: 1) Message DELIVER: peer wants me to deliver %d chunks\n", chunkID_set_size(chunkIDSetReceived));
#endif
                        pthread_mutex_lock(&peerChunkMutex);
                        int i, chunkID;
                        for (i = 0; i < chunkID_set_size(chunkIDSetReceived); ++i) {
                            chunkID = chunkID_set_get_chunk(chunkIDSetReceived, i);

                            // add to PeerChunk
                            network_add_to_peerChunk(remote, chunkID);
                        }
                        pthread_mutex_unlock(&peerChunkMutex);
                        break;
                    }
                    case sig_ack:
                        break;
                    case sig_offer:
                    {
#ifdef DEBUG
                        fprintf(stderr, "DEBUG: 1) Message OFFER: peer offers %d chunks\n", chunkID_set_size(chunkIDSetReceived));
#endif
                        int i, count = 0;
                        ChunkIDSet *chunkIDSetForMe = chunkID_set_init("size=0");

                        // run through all chunks transmitted chunkIDSet and accept 'maxDeliver' chunks not in own chunkbuffer
                        for (i = 0; i < chunkID_set_size((struct chunkID_set*) chunkIDSetReceived); ++i) {
                            int chunkId = chunkID_set_get_chunk((struct chunkID_set*) chunkIDSetReceived, i);
                            const struct chunk *chunk;
                            chunk = cb_get_chunk((struct chunk_buffer*) chunkBuffer, chunkId);
                            if (chunk == NULL && count < maxDeliver) {
                                chunkID_set_add_chunk((struct chunkID_set*) chunkIDSetForMe, chunkId);
                                ++count;
                            }
                        }
#ifdef DEBUG
                        fprintf(stderr, "DEBUG: 2) Accepting %d chunks\n", chunkID_set_size((struct chunkID_set*) chunkIDSetForMe));
#endif
                        // accept the chunks in chunkset
                        acceptChunks(remote, (struct chunkID_set*) chunkIDSetForMe, transId++);
                        break;
                    }
                    case sig_request: // peer requests x chunks
                    {
#ifdef DEBUG
                        fprintf(stderr, "DEBUG: 1) Message REQUEST: peer requests %d chunks\n", chunkID_set_size(chunkIDSetReceived));
#endif
                        int chunkToSend;
                        chunkToSend = chunkID_set_get_earliest((struct chunkID_set*) chunkIDSet);

                        pthread_mutex_lock(&chunkBufferMutex);
                        pthread_mutex_lock(&peerChunkMutex);
                        int res;
                        res = network_add_to_peerChunk(remote, chunkToSend); // add peer/chunk to a list, for sending in sendChunk thread

                        if (res == 0) {
                            // we have the chunk, we could send it to the remote peer
                            struct ChunkIDSet *chunkIDSetForDeliverToRemote = (struct ChunkIDSet*) chunkID_set_init("size=1");
#ifdef DEBUG
                            fprintf(stderr, "DEBUG: 2) Deliver only earliest chunk #%d\n", chunkToSend);
#endif
                            chunkID_set_add_chunk((struct chunkID_set*) chunkIDSetForDeliverToRemote, chunkToSend);

                            // tell remote that this chunk could be delivered
                            deliverChunks(remote, (struct chunkID_set*) chunkIDSetForDeliverToRemote, transId++);
                        } else {
                            // the requested chunk is too old, send current buffermap
                            struct ChunkIDSet *chunkIDSetRemote = network_chunkBuffer_to_buffermap();
                            sendBufferMap(remote, localSocket, (struct chunkID_set*) chunkIDSetRemote, chunkBufferSize, transId++);
                        }
                        pthread_mutex_unlock(&peerChunkMutex);
                        pthread_mutex_unlock(&chunkBufferMutex);

                        break;
                    }
                    case sig_send_buffermap: // peer sent a buffer of chunks
                    {
                        fprintf(stdout, "1) Message SEND_BMAP: I received a buffer of %d chunks\n", chunkID_set_size(chunkIDSetReceived));
                        //printChunkID_set(Streamer::chunkIDSet);
                        //chunkIDSetRemote = chunkID_set_init("size=15,type=bitmap");
                        //if (!Streamer::chunkIDSetRemote) {
                        //    fprintf(stderr, "Unable to allocate memory for Streamer::chunkIDSetRemote\n");
                        //    return -1;
                        //}
                        //fillChunkID_set(Streamer::chunkIDSetRemote, Streamer::random_bmap);
                        //fprintf(stdout, "2) Message SEND_BMAP: I send my buffer of %d chunks\n", chunkID_set_size(Streamer::chunkIDSetRemote));
                        //printChunkID_set(rcset);
                        //sendBufferMap(remote, localSocket, chunkIDSetRemote, 0, transId++);

                        // parse buffermap and check what chunks are needed

                        break;
                    }
                    case sig_request_buffermap: // peer requests my buffer map
                    {
#ifdef DEBUG
                        char remoteAddress[256];
                        node_addr(remote, remoteAddress, 256);
                        fprintf(stdout, "1) Message REQUEST_BMAP: %s requested my buffer map\n", remoteAddress);
#endif
                        sendBufferMap(remote, localSocket, (struct chunkID_set*) chunkIDSet, chunkID_set_size((struct chunkID_set*) chunkIDSet), transId++);
                        break;
                    }
                }
                break;
            }
            default:
            {
                fprintf(stderr, "Unknown Message Type %x\n", buffer[0]);
            }
        }
        nodeid_free(remote);
    }

    return NULL;
}

static void *threads_send_topology(void *mut) {
#ifdef DEBUG
    fprintf(stderr, "DEBUG: Thread started send_topology\n");
#endif
    //    mutexes *m = (mutexes *) mut;
    //    Threads *t = m->t;
    char addr[256];

    while (stopThreads != 1) {
        const struct nodeID * const *neighbours;
        int numberOfNeighbours, i;

        pthread_mutex_lock(&topologyMutex);
        psample_parse_data(peersampleContext, NULL, 0);
        neighbours = psample_get_cache(peersampleContext, &numberOfNeighbours);
#ifdef DEBUG
        fprintf(stderr, "DEBUG: I have %d neighbours:\n", numberOfNeighbours);
        for (i = 0; i < numberOfNeighbours; i++) {
            node_addr(neighbours[i], addr, 256);
            fprintf(stderr, "DEBUG: \t%d: %s\n", i, addr);
        }
#endif
        if (numberOfNeighbours == 0) {
            fprintf(stderr, "Lost all connections, even to server.\n");
            stopThreads = 1;
        }
        pthread_mutex_unlock(&topologyMutex);
        usleep(1 * 1000 * 1000); // 1 sec
    }

    return NULL;
}

static void *threads_trade_chunk(void *mut) {
#ifdef DEBUG
    fprintf(stderr, "DEBUG: Thread started trade_chunk\n");
#endif
    while (stopThreads != 1) {
        pthread_mutex_lock(&topologyMutex);
        pthread_mutex_lock(&chunkBufferMutex);

        int maximumNeighboursToSendOffersTo = 10;
        const struct nodeID * const *neighbours;
        int numberOfNeighbours, i;
        neighbours = psample_get_cache(peersampleContext, &numberOfNeighbours);

        for (i = 0; i < maximumNeighboursToSendOffersTo && i < numberOfNeighbours; ++i) {
#ifdef DEBUG
            char addr[256];
            node_addr(neighbours[i], addr, 256);
            fprintf(stderr, "DEBUG: trade chunk: requesting buffer map from %s\n", addr);
#endif
            requestBufferMap(neighbours[i], NULL, transId++);
            //sendBufferMap(peers[i]->id, localSocket, network_chunkBuffer_to_buffermap(), chunkBufferSize, transId++);
        }

        pthread_mutex_unlock(&chunkBufferMutex);
        pthread_mutex_unlock(&topologyMutex);
        usleep(5 * 1000 * 100); // 0.5 sec
    }

    return NULL;
}

static void *threads_send_chunk(void *mut) {
#ifdef DEBUG
    fprintf(stderr, "DEBUG: Thread started send_chunk\n");
#endif
    //    mutexes *m = (mutexes *) mut;
    //    Threads *t = m->t;
    //
    //    Network *network = Network::getInstance();
    //    int chunk_period = Threads::gossipingPeriod / Threads::chunks_per_period;

    while (stopThreads != 1) {
        pthread_mutex_lock(&topologyMutex);
        pthread_mutex_lock(&chunkBufferMutex);
        pthread_mutex_lock(&peerChunkMutex);
        network_send_chunks_to_peers();
        pthread_mutex_unlock(&peerChunkMutex);
        pthread_mutex_unlock(&chunkBufferMutex);
        pthread_mutex_unlock(&topologyMutex);
        //        usleep(chunk_period);
    }

    return NULL;
}

void threads_start() {
#ifdef DEBUG
    fprintf(stderr, "DEBUG: Called threads_start\n");
#endif

    pthread_t tradeChunkThread, receiveDataThread, sendTopologyThread, sendChunkThread;

    stopThreads = 0;

    //    peerSet = peerset_init(0); // CHECK
    psample_parse_data(peersampleContext, NULL, 0);

    // initialize mutexes
    pthread_mutex_init(&chunkBufferMutex, NULL);
    pthread_mutex_init(&topologyMutex, NULL);
    pthread_mutex_init(&peerChunkMutex, NULL);

    // create threads
    pthread_create(&receiveDataThread, NULL, threads_receive_data, NULL); // Thread for receiving data
    pthread_create(&sendTopologyThread, NULL, threads_send_topology, NULL); // Thread for sharing the topology of the p2p network
    pthread_create(&tradeChunkThread, NULL, threads_trade_chunk, NULL); // Thread for generating chunks
    pthread_create(&sendChunkThread, NULL, threads_send_chunk, NULL); // Thread for sending the chunks

    // join threads
    pthread_join(tradeChunkThread, NULL);
    pthread_join(receiveDataThread, NULL);
    pthread_join(sendTopologyThread, NULL);
    pthread_join(sendChunkThread, NULL);
}
