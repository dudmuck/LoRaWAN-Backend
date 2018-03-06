/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "libserver.h"

#ifdef MIC_DEBUG
    #define DEBUG_MIC_BUF(x,y,z)    print_buf(x,y,z)
    #define DEBUG_MIC(...)        printf(__VA_ARGS__)
#else
    #define DEBUG_MIC_BUF(x,y,z)    
    #define DEBUG_MIC(...)        
#endif


extern uint64_t myJoinEui64;
extern char myJoinEuiStr[];
extern MYSQL *sql_conn_lora_join;
extern key_envelope_t key_envelope_nwk;

const char* parse_rf_join_req(const char* mt, json_object* inJobj, uint32_t network_id, json_object** answer);
void GenerateJoinKey(uint8_t token, const uint8_t* root_key, const uint8_t* dev_eui, uint8_t* output);

