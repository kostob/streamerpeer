/* 
 * File:   output_interface.h
 * Author: tobias
 *
 * Created on 10. Dezember 2013, 14:07
 */

#ifndef OUTPUT_INTERFACE_H
#define	OUTPUT_INTERFACE_H

#include <stdio.h>
#include <stdint.h>

// GRAPES
#include <chunk.h>

struct output_context;

struct output_interface {
    struct output_context *(*init)(const char *config); // init the output module and open output stream
    int (*deliver)(struct output_context *context, struct chunk *c); // handle incomming chunk
    void (*close)(struct output_context *context); // close output stream
    int (*deliver_secured_data_chunk)(struct output_context *context, struct chunk *securedData); // handle incomming secured data for chunk
    int (*deliver_secured_data_login)(struct output_context *context, struct chunk *securedData); // handle inclomming secured data for beginning of transaction
    int (*secure_data_enabled_chunk)(struct output_context *context); // secured data for chunk enabled? 1: yes, 0: false
    int (*secure_data_enabled_login)(struct output_context *context); // secure data for beginning of transaction enabled? 1: yes, 0: false
};

#endif	/* OUTPUT_INTERFACE_H */

