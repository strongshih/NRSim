#ifndef CICERO_reducer_H
#define CICERO_reducer_H

#include "CICEROPackDef.h"
#include <nvhls_int.h>
#include <nvhls_connections.h>
#include <ac_std_float.h>

/*
 * Input: Read one memreq
 * Output: output 8 cycles for each Vidx 
 */
#pragma hls_design top
class reducer : public match::Module {
    SC_HAS_PROCESS(reducer);
public:

    Connections::In<reducer_W> w;
    Connections::In<reducer_feature> f_in;
    Connections::Out<reducer_feature> f_out;

    reducer(sc_module_name name) : match::Module(name),
                              w("w"),
                              f_in("f_in"),
                              f_out("f_out") {
        SC_THREAD(start);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);
    }

    reducer_feature acc;

    void start() {
        w.Reset();
        f_in.Reset();
        f_out.Reset();
        acc = reducer_feature(0);
        wait();

        #pragma hls_pipeline_init_interval 1
        while (1) {
            wait();

            #pragma hls_pipeline_init_interval 1
            for (int i = 0; i < 8; i++) { 
                reducer_W       w_tmp = w.Pop();
                reducer_feature f_in_tmp = f_in.Pop();
                acc += w_tmp*f_in_tmp;
                if (i == 7) {
                    f_out.Push(acc);
                    acc = 0;
                }
            }
        }
    }
};

#endif //CICERO_reducer_H
