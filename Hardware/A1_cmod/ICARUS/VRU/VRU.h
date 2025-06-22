#ifndef ICARUS_VRU_H
#define ICARUS_VRU_H

#include "ICARUSPackDef.h"
#include <ac_channel.h>
#include <nvhls_connections.h>
#include <ac_math/ac_hcordic.h>
#include <ac_math/ac_sigmoid_pwl.h>
#include <ac_std_float.h>

#pragma hls_design block
class VRU : public match::Module {
    SC_HAS_PROCESS(VRU);
public:
    
    Connections::In<VRU_In_Type> VRUInput;
    Connections::Out<VRU_Out_Type> VRUOutput;

    VRU(sc_module_name name) : match::Module(name),
                               VRUInput("VRUInput"),
                               VRUOutput("VRUOutput") {
        SC_THREAD(VRU_CALC);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);
    }

    /*
     * Input: (cx,cy,cz,sigma,delta)
     * Output: (r,g,b)
     * Perform: C(r) = \Sigma_{i=0}^{N-1} T_i * (1 - exp(-\sigma_i*\delta_i)) * c_i
     */
    #pragma hls_pipeline_init_interval 1
    void VRU_CALC() {
        VRU_Color_Type T = VRU_Color_Type(1); // registers
        VRU_Color_Type color[3] = {VRU_Color_Type(0),VRU_Color_Type(0),VRU_Color_Type(0)}; // registers
        VRUInput.Reset();
        VRUOutput.Reset();
        wait();

        while (1) {
            wait();
            
            VRU_In_Type vru_input;
            if (VRUInput.PopNB(vru_input)) {
                // Perform C(r) += (T_i - T_{i+1})*sigmoid(emitted_c)
                //         T_{i+1} = T_i * exp(-\sigma_i*\delta_i)
                // Where T_0 = 1, initial C(r) = 0
#ifdef USE_FLOAT
                VRU_Color_Type exp_result;
                VRU_Color_Type sigmoid_result;
#else
                ac_fixed<32, 16, false, AC_TRN, AC_SAT> exp_result; // exp result > 0, so false
                ac_fixed<32, 16, false, AC_TRN, AC_SAT> sigmoid_result;
#endif
                ac_math::ac_exp_cordic(-vru_input.sigma * vru_input.delta, exp_result);
                VRU_Color_Type tmp_T = T * exp_result;
                #pragma hls_pipeline_init_interval 1
                for (int i = 0; i < 3; i++) {
                    ac_math::ac_sigmoid_pwl(vru_input.emitted_c[i], sigmoid_result);
                    color[i] += sigmoid_result*(T - tmp_T);
                }
                T = tmp_T;

                if (vru_input.isLastSample) { // Last sample
                    // after accumulating last sample, push out the result
                    VRU_Out_Type vru_output;
                    #pragma hls_unroll
                    for (int i = 0; i < 3; i++) {
                        vru_output.c[i] = color[i];
                    }
                    VRUOutput.Push(vru_output);

                    // reset accumulators and T
                    #pragma hls_unroll
                    for (int i = 0; i < 3; i++) {
                        color[i] = VRU_Color_Type(0);
                    }
                    T = VRU_Color_Type(1);
                }
            }
        }
    }

};

#endif //ICARUS_VRU_H
