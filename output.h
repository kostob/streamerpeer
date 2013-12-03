/* 
 * File:   output.h
 * Author: tobias
 *
 * Created on 26. November 2013, 08:17
 */

#ifndef OUTPUT_H
#define	OUTPUT_H

extern int nextChunk;

int output_init(int bufsize, const char *config);
int output_deliver(struct chunk *c);

#endif	/* OUTPUT_H */

