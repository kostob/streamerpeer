/* 
 * File:   main.c
 * Author: tobias
 *
 * Created on 26. November 2013, 08:08
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GRAPES
#include <net_helper.h>
#include <peersampler.h>
#include <trade_msg_ha.h>

#include "streamer.h"
#include "network.h"
#include "output_factory.h"

static char* configServerAddress = "127.0.0.1";
static int configServerPort = 6666;
static char* configInterface = "lo0";
static int configPort = 5555;
static char *configPeersample = "protocol=cyclon";
static char *configChunkBuffer = "size=100,time=now"; // size must be same value as chunkBufferSizeMax
static char *configChunkIDSet = "size=100"; // size must be same value as chunkBufferSizeMax
static char *configOutput = "buffer=75";
static int configOutputBufferSize = 75;

struct ChunkBuffer *chunkBuffer = NULL;
int chunkBufferSize = 0;
int chunkBufferSizeMax = 100;
struct PeerSet *peerSet = NULL;
struct ChunkIDSet *chunkIDSet = NULL;
struct PeerChunk *peerChunks = NULL;
int peerChunksSize = 0;

struct output_module *output = NULL;

struct nodeID *localSocket;
struct nodeID *serverSocket;
struct psample_context *peersampleContext;

void parseCommandLineArguments(int argc, char* argv[]) {
#ifdef DEBUG
    fprintf(stderr, "DEBUG: called parseCommandLineArguments\n");
#endif
    int i;
    char arg[512];
    for (i = 1; i < argc; ++i) {
        // TODO: check if there is an argument i + 1
        strcpy(arg, argv[i]);

        // server address
        if (strcmp(arg, "-i") == 0) {
            configServerAddress = argv[i + 1];
        }

        // server port
        if (strcmp(arg, "-p") == 0) {
            configServerPort = atoi(argv[i + 1]);
        }

        // interface
        if (strcmp(arg, "-I") == 0) {
            configInterface = argv[i + 1];
        }

        // local port
        if (strcmp(arg, "-P") == 0) {
            configPort = atoi(argv[i + 1]);
        }

        ++i;
    }
}

int init() {

#ifdef DEBUG
    fprintf(stderr, "DEBUG: Called streamer.c init\n");
#endif

    // create the interface for connection
    char *my_addr = network_create_interface(configInterface);

    if (strlen(my_addr) == 0) {
        fprintf(stderr, "Cannot find network interface %s\n", configInterface);
        return -1;
    }

    // initialize net helper
    localSocket = net_helper_init(my_addr, configPort, "");
    if (localSocket == NULL) {
        fprintf(stderr, "Error creating my socket (%s:%d)!\n", my_addr, configPort);
        return -1;
    }

    // initialize PeerSampler
    peersampleContext = psample_init(localSocket, NULL, 0, configPeersample);
    if (peersampleContext == NULL) {
        fprintf(stderr, "Error while initializing the peer sampler\n");
        return -1;
    }

    // initialize ChunkBuffer
    chunkBuffer = (struct ChunkBuffer*) cb_init(configChunkBuffer);
    if (chunkBuffer == NULL) {
        fprintf(stderr, "Error while initializing chunk buffer\n");
        return -1;
    }

    // initialize chunk delivery
    if (chunkDeliveryInit(localSocket) < 0) {
        fprintf(stderr, "Error while initializing chunk delivery\n");
        return -1;
    }

    // init chunk signaling
    if (chunkSignalingInit(localSocket) == 0) {
        fprintf(stderr, "Error while initializing chunk signaling\n");
        return -1;
    }

    // init chunkid set
    chunkIDSet = (struct ChunkIDSet*) chunkID_set_init(configChunkIDSet);
    if (chunkIDSet == NULL) {
        fprintf(stderr, "Error while initializing chunkid set\n");
        return -1;
    }

    // init peerset
    peerSet = (struct PeerSet*) peerset_init(0);
    if (peerSet == NULL) {
        fprintf(stderr, "Error while initializing chunkid set\n");
        return -1;
    }

    // set server
    serverSocket = create_node(configServerAddress, configServerPort);

    // add server node to peersampler
    struct nodeID *s;
    s = create_node(configServerAddress, configServerPort);
    psample_add_peer(peersampleContext, s, NULL, 0);

    // initialize output
    output = output_init(configOutput);
    if (output == NULL) {
        fprintf(stderr, "Error occurred: see message above.\n");
        return -1;
    }

    return 0;
}

/*
 * 
 */
int main(int argc, char* argv[]) {

    // parse command line arguments
    parseCommandLineArguments(argc, argv);

    // initialization
    if (init() != 0) {
        fprintf(stderr, "Error occurred. Please see message above for further details.\n");
        return 0;
    }

    // threads
    threads_start();

    return 0;
}

