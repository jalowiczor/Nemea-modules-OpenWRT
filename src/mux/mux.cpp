/**
 * \file mux.cpp
 * \brief Unite traffic incoming on multiple interface into one output stream (input for demux module).
 * \author Dominik Soukup <soukudom@fit.cvut.cz>
 * \date 2018
 */
/*
 * Copyright (C) 2018 CESNET
 *
 * LICENSE TERMS
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */


#include "config.h"
#include <iostream>

#include <unirec/unirec.h>
#include <libtrap/trap.h>
#include <getopt.h>
#include <omp.h>
#include <signal.h>
#include "fields.h"

using namespace std;
int exit_value=0;
static ur_template_t **in_templates = NULL; // UniRec input templates
static int n_inputs = 0;                    // number of input interface
trap_ctx_t *ctx = NULL;                     // Trap context interface
int stop = 0;                               // Tmp local variable 
int ret = 2;                                // Tmp local variable
int verbose = 0;                            // Vebose level
trap_module_info_t *module_info = NULL;     // Trap module info structure

// Data packet fromat between Mux and Demux
typedef struct meta_info_s {
    uint16_t messageID; // 1 - normal data, 2 - hello message (unirec fmt changed)
    uint16_t interfaceID;
    uint8_t data_fmt;
    char payload[0];
} meta_info_t;

#define MODULE_BASIC_INFO(BASIC) \
  BASIC("mux","This module unite more input interfaces into one output interface",-1,1)
#define MODULE_PARAMS(PARAM) \
PARAM('n', "link_count", "Sets count of input links. Must correspond to parameter -i (trap).", required_argument, "int32")

void capture_thread(int index){

const void *data_nemea_input = NULL; // Received data 
uint16_t memory_received = 0;        // Tmp local variable
uint8_t data_fmt = TRAP_FMT_UNKNOWN; // Tmp local variable
const char *spec = NULL;             // tmp local variable


    // Set output interface format
    trap_ctx_set_data_fmt(ctx, 0, TRAP_FMT_RAW);

    // Allocate structure with new header and data payload
    meta_info_t *meta_data;
    char *buffer[65535];
    meta_data = (meta_info_t *) buffer;
    // Main loop
    while (!stop){
        ret = trap_ctx_recv(ctx,index,&data_nemea_input,&memory_received);
        // === Process received data ===
        if (ret == TRAP_E_OK | ret == TRAP_E_FORMAT_CHANGED){
            if (ret == TRAP_E_FORMAT_CHANGED){
                // Get input interface format
                if (trap_ctx_get_data_fmt(ctx, TRAPIFC_INPUT, index, &data_fmt, &spec) != TRAP_E_OK){
                    cerr << "ERROR: Data format was not loaded." << endl;
                    return;
                }

                if (verbose >= 0){
                    cout << "Data format has been changed. Sending hello message" << endl;
                }
            
                // Fill in hello message
                meta_data->messageID = 2;
                meta_data->interfaceID = index;
                meta_data->data_fmt = data_fmt;
                memcpy(meta_data->payload, spec,strlen(spec)+1);
                #pragma omp critical
                {
                    ret = trap_ctx_send(ctx,0,buffer,sizeof(*meta_data)+strlen(spec)+1);
                }

            } // Endif ret == TRAP_E_FORMAT_CHANGED

                // Forward received payload data
                meta_data->messageID = 1;
                meta_data->interfaceID = index;
                meta_data->data_fmt = data_fmt;
                memcpy(meta_data->payload, data_nemea_input,memory_received);
        
        // Endif ret == TRAP_E_OK | ret == TRAP_E_FORMAT_CHANGED
        }  else{
            cerr << "ERROR: Undefined option on input interface" << endl;
            meta_data->messageID = -1;
            meta_data->interfaceID = index;
        }
        
        // Send data out   
        #pragma omp critical
        {
            ret = trap_ctx_send(ctx,0,buffer,sizeof(*meta_data)+memory_received);
            if (verbose >= 0){
                cout << "Iterface with index " << index << " sent data out" << endl;
            }
        }
    }// End while (!stop){
}

int main (int argc, char ** argv){
    // Allocate and initialize module_info structure and all its members
    INIT_MODULE_INFO_STRUCT(MODULE_BASIC_INFO, MODULE_PARAMS)
    // Trap parameters processing
    trap_ifc_spec_t ifc_spec;
    ret = trap_parse_params(&argc, argv, &ifc_spec);
    if (ret != TRAP_E_OK) {
        if (ret == TRAP_E_HELP) { // "-h" was found
        trap_print_help(module_info);
        return 0;
        }
        cerr << "ERROR in parsing of parameters for TRAP: " << trap_last_error_msg << endl;
        return 1;
    }

    // Parse remaining parameters and get configuration
    signed char opt;
    while ((opt = TRAP_GETOPT(argc, argv, module_getopt_string, long_options)) != -1) {
        switch (opt) {
        case 'n':
            n_inputs = atoi(optarg);
            break;
        default:
            cerr << "Error: Invalid arguments." << endl;
            FREE_MODULE_INFO_STRUCT(MODULE_BASIC_INFO, MODULE_PARAMS)
            return 1;
        }
    }

    verbose = trap_get_verbose_level();
    if (verbose >= 0) {
        cout << "Verbosity level: " <<  trap_get_verbose_level() << endl;;
    }

    if (verbose >= 0) {
        cerr << "Number of inputs: " <<  n_inputs << endl;
    }
   
    // Check input parameter 
    if (n_inputs < 1) {
        cerr <<  "Error: Number of input interfaces must be positive integer." << endl;
        FREE_MODULE_INFO_STRUCT(MODULE_BASIC_INFO, MODULE_PARAMS)
        return 0;
    }
    if (n_inputs > 32) {
        cerr << "Error: More than 32 interfaces is not allowed by TRAP library." << endl;
        FREE_MODULE_INFO_STRUCT(MODULE_BASIC_INFO, MODULE_PARAMS)
        return 4;
    }

    // Set number of input interfaces
    module_info->num_ifc_in = n_inputs;

    if (verbose >= 0) {
        cout << "Initializing TRAP library ..." << endl;
    }

    ctx = trap_ctx_init(module_info, ifc_spec);

    if (ctx == NULL){
        cerr << "ERROR in TRAP initialization: " << trap_last_error_msg << endl;
        exit_value=1;
        goto cleanup;
    }


    // Check if number of interfaces is correct
    if (strlen(ifc_spec.types) <= 1){
        cerr << "ERROR expected at least 1 input and 1 output interface. Got only 1." << endl;
        return 1;
    }

    // Check if number of input interfaces is correct
    if (strlen(ifc_spec.types)-1 != n_inputs){
        cerr << "ERROR number of input interfaces is incorrect" << endl;
        goto cleanup;
    }

    // Output interface control settings
    if (trap_ctx_ifcctl(ctx, TRAPIFC_OUTPUT,0,TRAPCTL_SETTIMEOUT,TRAP_NO_WAIT) != TRAP_E_OK){
        cerr << "ERROR in output interface initialization" << endl;
        exit_value=2;
        goto cleanup;
    }
    
    
    // Allocace memory for input templates
    in_templates = (ur_template_t**)calloc(n_inputs,sizeof(*in_templates));
    if (in_templates == NULL){
        cerr <<  "Memory allocation error." << endl;
        goto cleanup;
    }

    // Initialize and set templates for UniRec negotiation
    for (int i = 0; i < n_inputs; i++){
        // Input interfaces control settings
        if (trap_ctx_ifcctl(ctx, TRAPIFC_INPUT,i,TRAPCTL_SETTIMEOUT,TRAP_WAIT) != TRAP_E_OK){
            cerr << "ERROR in input interface initialization" << endl;
            exit_value=2;
            goto cleanup;
        }
        // Create empty input template
        in_templates[i] = ur_ctx_create_input_template(ctx,i,NULL,NULL);
        if (in_templates[i] == NULL){
            cerr <<  "Memory allocation error." << endl;
            goto cleanup;
        }
        // Set required incoming format
        trap_ctx_set_required_fmt(ctx,i,TRAP_FMT_UNIREC,NULL);
    }

    if (verbose >= 0){
        cout << "Initialization done" << endl;
    }

    #pragma omp parallel num_threads(n_inputs)
    {
        capture_thread(omp_get_thread_num());
    }
    
cleanup:
    // Cleaning
    trap_ctx_finalize(&ctx);
    return 0;
}
