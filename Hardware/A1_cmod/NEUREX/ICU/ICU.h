#ifndef NEUREX_ICU_H
#define NEUREX_ICU_H

#include "NEUREXPackDef.h"
#include <nvhls_int.h>
#include <nvhls_connections.h>
#include <ac_std_float.h>

/*
 * Input: Read one memreq
 * Output: output 8 cycles for each Vidx 
 */
class ICU : public match::Module {
    SC_HAS_PROCESS(ICU);
public:

    Connections::In<ICU_In_Type> data_in;
    Connections::In<ICU_In_Type> w_in;
    Connections::Out<ICU_Out_Type> data_out;

    ICU(sc_module_name name) : match::Module(name),
                              data_in("data_in"),
                              w_in("w_in"),
                              data_out("data_out") {
        SC_THREAD(start);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);
    }

    void start() {
        data_in.Reset();
        w_in.Reset();
        data_out.Reset();
        wait();

        #pragma hls_pipeline_init_interval 1
        while (1) {
            wait();

            ICU_In_Type w_tmp;
            ICU_In_Type data_tmp;
            if (data_in.PopNB(data_tmp)) {
                w_in.PopNB(w_tmp);
                ICU_Out_Type tmp[8];
                #pragma hls_unroll
                for (int i = 0; i < 8; i++) {
                    tmp[i] = w_tmp.x[i] * data_tmp.x[i];
                }
                ICU_Out_Type out = ICU_Out_Type(0);
                wait();
                ICU_Out_Type tmp2[4];
                #pragma hls_unroll
                for (int i = 0; i < 4; i++) {
                    tmp2[i] = tmp[i] + tmp[i+4];
                }
                wait();
                ICU_Out_Type tmp3[2];
                #pragma hls_unroll
                for (int i = 0; i < 2; i++) {
                    tmp3[i] = tmp2[i] + tmp2[i+2];
                }
                wait();
                ICU_Out_Type result = tmp3[0] + tmp3[1];
                data_out.Push(result);
            }
        }
    }
};

#endif //NEUREX_ICU_H
