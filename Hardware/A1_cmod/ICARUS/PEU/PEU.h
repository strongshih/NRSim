#ifndef ICARUS_PEU_H
#define ICARUS_PEU_H

#include "ICARUSPackDef.h"
#include <ac_channel.h>
#include <nvhls_connections.h>
#include <ac_math/ac_sincos_cordic.h>
#include <ac_std_float.h>

/*
 * Input: (x,y,z)
 * Output: [--256 dimensional vector--]
 * Perform: [cos(A * (x,y,z)), sin(A * (x,y,z))], where A \in R^{128x3}
 */
class PEU : public match::Module {
    SC_HAS_PROCESS(PEU);
public:

    //Connections::In<MemReq> memreq; // For write request to Matrix A
    Connections::In<PEU_In_Type> PEUInput;
    Connections::Out<PEU_Out_Type> PEUOutput;

    Connections::Combinational<PEU_CORDIC_In_Type> PEUMatMulResult;
    
    PEU(sc_module_name name) : match::Module(name),
                               ////memreq("memreq"),
                               PEUInput("PEUInput"),
                               PEUOutput("PEUOutput"),
                               PEUMatMulResult("PEUMatMulResult") {
        /*SC_THREAD(InitializeMatrixA);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);*/

        SC_THREAD(PEU_MatMul);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);

        SC_THREAD(PEU_CORDIC);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);
    }

    // Frequency memory
    //PEU_Matrix_A_Type MatrixA[PEU_CORDIC_IN_DIM][PEU_INPUT_DIM];
    /*
     * Dummy memory to write to (may be replaced with technology dependent memory)
     */
    /*void InitializeMatrixA() {
        memreq.Reset();
        wait();

        while (1) {
            wait();

            MemReq q;
            if (memreq.PopNB(q)) {
                // assert((q.index[0] < PEU_CORDIC_IN_DIM) && (q.index[1] < PEU_INPUT_DIM));
                MatrixA[q.index[0]][q.index[1]] = q.data;
            }
        }
    }*/

    /*
     * Input: (x,y,z)
     * Output: 128-dimensional data
     * Requirement: 1 sample finished in 128 cycles --> 1 mul
     */
    void PEU_MatMul() {
        PEUInput.Reset();
        PEUMatMulResult.ResetWrite();
        wait();

        #pragma hls_pipeline_init_interval 1
        while (1) {
            wait();

            PEU_In_Type pos;
            if (PEUInput.PopNB(pos)) {
                PEU_CORDIC_In_Type vec;
                 #pragma hls_pipeline_init_interval 1
                for (uint i = 0; i < PEU_CORDIC_IN_DIM; i++) {
                    PEU_CORDIC_In_Elem_Type tmp = PEU_CORDIC_In_Elem_Type(0);
                    // 3-stage MAC
                 #pragma hls_pipeline_init_interval 1
                    for (uint j = 0; j < PEU_INPUT_DIM; j++) {
                        tmp += (i+j) * pos.X[j];
                    }
                    vec.X[i] = tmp;
                }
                PEUMatMulResult.Push(vec);
            }
        }
    }

    /*
     * Input: 128-dimensional data
     * Output: 256-dimensional data (cos, sin)
     * Requirement: 1 sample finished in 128 cycles --> 2 cordics (sin, cos)
     */
    void PEU_CORDIC() {
        PEUMatMulResult.ResetRead();
        PEUOutput.Reset();

        wait();
        #pragma hls_pipeline_init_interval 1
        while (1) {
            wait();

            PEU_CORDIC_In_Type vec;
            if (PEUMatMulResult.PopNB(vec)) {
                PEU_Out_Type tmp;
                #pragma hls_pipeline_init_interval 1
                for (uint i = 0; i < PEU_CORDIC_IN_DIM; i++) {
                    // Original work use cordic, input here is over pi, e.g. pi/4 --> 1/4
                    ac_math::ac_sin_cordic(vec.X[i], tmp.X[2*i+0]);
                    ac_math::ac_cos_cordic(vec.X[i], tmp.X[2*i+1]);
                }
                tmp.isLastSample = vec.isLastSample;
                PEUOutput.Push(tmp);
            }
        }
    }

};

#endif //ICARUS_PEU_H
