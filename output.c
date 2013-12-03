/* 
 * File:   output.c
 * Author: tobias
 *
 * Created on 01. December 2013, 11:06
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// GRAPES
#include <chunk.h>
#include <chunkbuffer.h>
#include <chunkiser.h>

#include "output.h"

struct output_buffer {
    struct chunk c;
};

int reorderChunks = 1; // 0 disables reorder of received chunks
                       // 1 enables reorder of received chunks

int nextChunk = -1;
int lastChunk = -1;
int outputBufferSize;
struct output_buffer *outputBuffer;
struct output_stream *outputStream;
char sflag = 0;
char eflag = 0;

int startId = -1;
int endId = -1;

int output_init(int bufferSize, const char *config) {
    char *c;

    c = strchr(config, ',');
    if (c) {
        *(c++) = 0;
    }
    outputStream = out_stream_init(config, c);
    if (outputStream == NULL) {
        fprintf(stderr, "Error: can't initialize output module\n");
        exit(1);
    }

    if (!outputBuffer) {
        int i;

        outputBufferSize = bufferSize;
        outputBuffer = malloc(sizeof (struct output_buffer) * outputBufferSize);
        if (!outputBuffer) {
            fprintf(stderr, "Error: can't allocate output buffer\n");
            return -1;
        }
        for (i = 0; i < outputBufferSize; ++i) {
            outputBuffer[i].c.data = NULL;
        }
    } else {
        fprintf(stderr, "Error: output already initialized!\n");
        return -1;
    }

    return 0;
}

static void buffer_print() {
#ifdef DEBUG
    int i;

    if (nextChunk < 0) {
        return;
    }

    fprintf(stderr, "\toutputBuffer: %d-> ", nextChunk);
    for (i = nextChunk; i < nextChunk + outputBufferSize; i++) {
        if (outputBuffer[i % outputBufferSize].c.data) {
            fprintf(stderr, "%d", i % 10);
        } else {
            fprintf(stderr, ".");
        }
    }
    fprintf(stderr, "\n");
#endif
}

static void buffer_free(int i) {
#ifdef DEBUG
    fprintf(stderr, "DEBUG: \t\tFlush outputBuffer %d: %s\n", i, outputBuffer[i].c.data);
#endif
    if (startId == -1 || outputBuffer[i].c.id >= startId) {
        if (endId == -1 || outputBuffer[i].c.id <= endId) {
            if (sflag == 0) {
#ifdef DEBUG
                fprintf(stderr, "DEBUG: First chunk id played out: %d\n\n", outputBuffer[i].c.id);
#endif
                sflag = 1;
            }
            if (reorderChunks) chunk_write(outputStream, &outputBuffer[i].c);
            lastChunk = outputBuffer[i].c.id;
        } else if (eflag == 0 && lastChunk != -1) {
#ifdef DEBUG
            fprintf(stderr, "DEBUG: Last chunk id played out: %d\n\n", lastChunk);
#endif
            eflag = 1;
        }
    }

    free(outputBuffer[i].c.data);
    outputBuffer[i].c.data = NULL;
#ifdef DEBUG
    fprintf(stderr, "DEBUG: Next Chunk: %d -> %d\n", nextChunk, outputBuffer[i].c.id + 1);
#endif
    //reg_chunk_playout(outputBuffer[i].c.id, true, outputBuffer[i].c.timestamp);
    nextChunk = outputBuffer[i].c.id + 1;
}

static void buffer_flush(int id) {
    int i = id % outputBufferSize;

    while (outputBuffer[i].c.data) {
        buffer_free(i);
        i = (i + 1) % outputBufferSize;
        if (i == id % outputBufferSize) {
            break;
        }
    }
}

int output_deliver(struct chunk *c) {
    if (!outputBuffer) {
        fprintf(stderr, "Warning: code should use output_init!!! Setting output buffer to 50\n");
        output_init(50, NULL);
    }

    if (!reorderChunks) chunk_write(outputStream, c);
#ifdef DEBUG
    fprintf(stderr, "DEBUG: Chunk %d delivered\n", c->id);
#endif
    //buffer_print(); // debugging output: printing complete output buffer
    if (c->id < nextChunk) {
        return 0; // chunk already written...
    }

    /* Initialize buffer with first chunk */
    if (nextChunk == -1) {
        nextChunk = c->id; // FIXME: could be anything between c->id and (c->id - buff_size + 1 > 0) ? c->id - buff_size + 1 : 0
#ifdef DEBUG
        fprintf(stderr, "DEBUG: First RX Chunk ID: %d\n", c->id);
#endif
    }

    if (c->id >= nextChunk + outputBufferSize) {
        int i;

        /* We might need some space for storing this chunk,
         * or the stored chunks are too old
         */
        for (i = nextChunk; i <= c->id - outputBufferSize; i++) {
            if (outputBuffer[i % outputBufferSize].c.data) {
                buffer_free(i % outputBufferSize);
            } else {
                nextChunk++;
            }
        }
        buffer_flush(nextChunk);
#ifdef DEBUG
        fprintf(stderr, "DEBUG: Next is now %d, chunk is %d\n", nextChunk, c->id);
#endif
    }

#ifdef DEBUG
    fprintf(stderr, "DEBUG: received chunk [%d] == next chunk [%d]?\n", c->id, nextChunk);
#endif
    if (c->id == nextChunk) {
#ifdef DEBUG
        //fprintf(stderr, "\tOut Chunk[%d] - %d: %s\n", c->id, c->id % outputBufferSize, c->data);
        fprintf(stderr, "DEBUG: \tOut Chunk[%d] - %d\n", c->id, c->id % outputBufferSize);
#endif

        if (startId == -1 || c->id >= startId) {
            if (endId == -1 || c->id <= endId) {
                if (sflag == 0) {
#ifdef DEBUG
                    fprintf(stderr, "DEBUG: First chunk id played out: %d\n\n", c->id);
#endif
                    sflag = 1;
                }
                if (reorderChunks) chunk_write(outputStream, c);
                lastChunk = c->id;
            } else if (eflag == 0 && lastChunk != -1) {
#ifdef DEBUG
                fprintf(stderr, "DEBUG: Last chunk id played out: %d\n\n", lastChunk);
#endif
                eflag = 1;
            }
        }
        //reg_chunk_playout(c->id, true, c->timestamp);
        ++nextChunk;
        buffer_flush(nextChunk);
    } else {
#ifdef DEBUG
        fprintf(stderr, "DEBUG: Storing %d (in %d)\n", c->id, c->id % outputBufferSize);
#endif
        if (outputBuffer[c->id % outputBufferSize].c.data) {
            if (outputBuffer[c->id % outputBufferSize].c.id == c->id) {
                // Duplicate of a stored chunk
#ifdef DEBUG
                fprintf(stderr, "DEBUG: Duplicate! chunkID: %d\n", c->id);
#endif
                //reg_chunk_duplicate();
                return;
            }
            fprintf(stderr, "Crap!, chunkid:%d, storedid: %d\n", c->id, outputBuffer[c->id % outputBufferSize].c.id);
            exit(-1);
        }
        /* We previously flushed, so we know that c->id is free */
        memcpy(&outputBuffer[c->id % outputBufferSize].c, c, sizeof (struct chunk));
        outputBuffer[c->id % outputBufferSize].c.data = malloc(c->size);
        memcpy(outputBuffer[c->id % outputBufferSize].c.data, c->data, c->size);
    }
}
