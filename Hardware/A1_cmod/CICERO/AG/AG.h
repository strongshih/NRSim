#ifndef CICERO_AG_H
#define CICERO_AG_H

#include "CICEROPackDef.h"
#include <nvhls_int.h>
#include <nvhls_connections.h>
#include <ac_std_float.h>

/*
 * Input: Read one memreq
 * Output: output 8 cycles for each Vidx 
 */
class AG : public match::Module {
    SC_HAS_PROCESS(AG);
public:

    Connections::In<AG_VIDsWs> memreq;
    Connections::Out<AG_VID> vid_out;
    Connections::Out<AG_W> w_out;

    AG(sc_module_name name) : match::Module(name),
                              memreq("memreq"),
                              vid_out("vid_out"),
                              w_out("w_out") {
        SC_THREAD(start);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);
    }

    void start() {
        memreq.Reset();
        vid_out.Reset();
        w_out.Reset();
        wait();

        #pragma hls_pipeline_init_interval 1
        while (1) {
            wait();

            AG_VIDsWs q;
            if (memreq.PopNB(q)) {
                #pragma hls_pipeline_init_interval 1
                for (int i = 0; i < 8; i++) {
                    vid_out.Push(q.v[i]);
                    w_out.Push(q.w[i]);
                }
            }
        }
    }
};

#endif //CICERO_AG_H
