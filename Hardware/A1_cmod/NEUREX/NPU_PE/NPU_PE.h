#ifndef NEUREX_NPU_PE_H
#define NEUREX_NPU_PE_H

#include "NEUREXPackDef.h"
#include <nvhls_int.h>
#include <nvhls_connections.h>
#include <ac_math/ac_sincos_cordic.h>
#include <ac_std_float.h>

/*
  A weight-stationary PE
 */

class NPU_PE : public match::Module {
    SC_HAS_PROCESS(NPU_PE);
public:
    // Input/Output ports
    Connections::In<NPU_W_Elem_Type>   w_in;
    Connections::In<NPU_In_Elem_Type>  act_in;
    Connections::In<NPU_Out_Elem_Type> psum_in;  // Partial sum input (from top
  
    Connections::Out<NPU_W_Elem_Type>   w_out;
    Connections::Out<NPU_In_Elem_Type>  act_out;
    Connections::Out<NPU_Out_Elem_Type> psum_out; // Partial sum output (to bottom)

    // Constructor
    NPU_PE(sc_module_name name) : match::Module(name),
                                 w_in("w_in"),
                                 act_in("act_in"),
                                 psum_in("psum_in"),
                                 w_out("w_out"),
                                 act_out("act_out"),
                                 psum_out("psum_out") {
        SC_THREAD(run);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);
    }

    NPU_W_Elem_Type w_reg;           // Fixed weight for this PE
    NPU_W_Elem_Type w_out_reg;
    NPU_In_Elem_Type act_reg;        // Current activation

    #pragma hls_pipeline_init_interval 1
    void run() {
        // Reset all connections
        w_in.Reset();
        act_in.Reset();
        psum_in.Reset();
        w_out.Reset();
        act_out.Reset();
        psum_out.Reset();

        // Initialize registers
        // w_reg = NPU_W_Elem_Type(0);
        // w_out_reg = NPU_W_Elem_Type(0);
        // act_reg = NPU_In_Elem_Type(0);
        wait(); // Wait for the first clock edge after reset

        #pragma hls_pipeline_init_interval 1
        while (1) {
            wait();

            NPU_W_Elem_Type tmp_weight;
            if (w_in.PopNB(tmp_weight)) {
                w_out_reg = w_reg;
                w_reg = tmp_weight;
                w_out.PushNB(w_out_reg);
            }

            // Handle activation streaming (left to right)
            NPU_In_Elem_Type tmp_act;
            if (act_in.PopNB(tmp_act)) {
                act_reg = tmp_act;
                act_out.PushNB (act_reg);
                
                // Get partial sum from above (or zero if not available)
                NPU_Out_Elem_Type psum;
                psum_in.PopNB(psum);
                
                // Compute new partial sum
                NPU_Out_Elem_Type new_psum = (act_reg * w_reg) + psum;
                
                // Send partial sum down
                psum_out.PushNB(new_psum);
            }
        }
    }
};

#endif //NEUREX_NPU_PE_H
