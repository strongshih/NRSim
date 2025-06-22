#ifndef CICERO_NPU_H
#define CICERO_NPU_H

#include "CICEROPackDef.h"
#include <nvhls_int.h>
#include <nvhls_connections.h>
#include <ac_std_float.h>

#include "../NPU_PE/NPU_PE.h"

class NPU : public match::Module {
    SC_HAS_PROCESS(NPU);
public:
  
    const static int N = NPU_SIZE;
   
    NPU_PE* array[N][N];

    Connections::In<NPU_W_Type>     w_in;
    Connections::In<NPU_In_Type>    act_in;
    Connections::Out<NPU_Out_Type>  psum_out;

    Connections::Combinational<NPU_W_Elem_Type>   w_data[N][N];
    Connections::Combinational<NPU_In_Elem_Type>  act_data[N][N]; 
    Connections::Combinational<NPU_Out_Elem_Type> psum_data[N][N];
  
    Connections::Combinational<NPU_W_Elem_Type>   w_in_vec[N];
    Connections::Combinational<NPU_In_Elem_Type>  act_in_vec[N];
    Connections::Combinational<NPU_Out_Elem_Type> psum_in_vec[N];

    NPU(sc_module_name name) : match::Module(name),
                               w_in("w_in"),
                               act_in("act_in"),
                               psum_out("psum_out") {
        for (int i = 0; i < N; i++) {      // rows
            for (int j = 0; j < N; j++) {  // cols
                array[i][j] = new NPU_PE(sc_gen_unique_name("npu_pe")); // Pass row and column index to PE
                array[i][j]->clk(clk);
                array[i][j]->rst(rst);
                
                // Weight connections (top to bottom)
                if (i == 0) {
                    array[i][j]->w_in(w_in_vec[j]);
                    array[i][j]->w_out(w_data[i][j]);
                } else {
                    array[i][j]->w_in(w_data[i-1][j]);
                    array[i][j]->w_out(w_data[i][j]);
                }

                // Activation connections (left to right)
                if (j == 0) {
                    array[i][j]->act_in(act_in_vec[i]);
                    array[i][j]->act_out(act_data[i][j]);
                } else {
                    array[i][j]->act_in(act_data[i][j-1]);
                    array[i][j]->act_out(act_data[i][j]);
                }

                // Partial sum connections (top to bottom)
                if (i == 0) {
                    array[i][j]->psum_in(psum_in_vec[j]);
                    array[i][j]->psum_out(psum_data[i][j]);
                } else {
                    array[i][j]->psum_in(psum_data[i-1][j]);
                    array[i][j]->psum_out(psum_data[i][j]);
                }
            }
        }
 
        // Initialize threads
        SC_THREAD (CollectPsums);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);

        SC_THREAD (SendInputs);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);

        SC_THREAD (Popout);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);
    }

    // Collect partial sums from the bottom row
    void CollectPsums() {
        #pragma hls_unroll yes
        for (int j = 0; j < N; j++) {
            psum_data[N-1][j].ResetRead();  // Bottom row
        }
        psum_out.Reset();
        wait();

        #pragma hls_pipeline_init_interval 1
        while(1) {
            wait();
            
            NPU_Out_Type out;
            #pragma hls_unroll yes
            for (int j = 0; j < N; j++) {
                NPU_Out_Elem_Type psum_value;
                if (psum_data[N-1][j].PopNB(psum_value)) {  // Bottom row
                    out.X[j] = psum_value;
                } else {
                    out.X[j] = NPU_Out_Elem_Type(0);
                }
            }
            psum_out.Push(out);
        }
    }

    void SendInputs() {
        w_in.Reset();
        act_in.Reset();

        #pragma hls_unroll
        for (int i = 0; i < N; i++) {
            w_in_vec[i].ResetWrite();
            act_in_vec[i].ResetWrite();
        }
        
        // Reset psum_in_vec
        #pragma hls_unroll
        for (int j = 0; j < N; j++) {
            psum_in_vec[j].ResetWrite();
        }
        
        wait();

        #pragma hls_pipeline_init_interval 1
        while (1) {
            wait();

            // Push weight inputs - weights flow top to bottom through columns
            NPU_W_Type w_tmp;
            if (w_in.PopNB(w_tmp)) {
                #pragma hls_unroll
                for (int j = 0; j < N; j++) {
                    w_in_vec[j].Push(w_tmp.X[j]);
                }
            }

            // Push activation inputs
            NPU_In_Type act_tmp;
            if (act_in.PopNB(act_tmp)) {
                #pragma hls_unroll
                for (int i = 0; i < N; i++) {
                    act_in_vec[i].Push(act_tmp.X[i]);
                }
            }
        }
    }

    void Popout() {
        #pragma hls_unroll
        for (int i = 0; i < N; i++) {
            w_data[N-1][i].ResetRead();     // Bottom row
            act_data[i][N-1].ResetRead();   // Rightmost column
        }
        wait();

        #pragma hls_pipeline_init_interval 1
        while (1) {
            wait();
            // Pop out to avoid getting stuck
            #pragma hls_unroll
            for (int i = 0; i < N; i++) {
                NPU_W_Elem_Type w_temp;
                NPU_In_Elem_Type act_temp;
                
                w_data[N-1][i].PopNB(w_temp);          // Bottom row
                act_data[i][N-1].PopNB(act_temp);      // Rightmost column
            }
        }
    }
};

#endif // CICERO_NPU_H
  
