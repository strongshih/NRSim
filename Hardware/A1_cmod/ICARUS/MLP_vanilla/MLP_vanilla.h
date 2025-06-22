#ifndef ICARUS_MLP_VANILLA_H
#define ICARUS_MLP_VANILLA_H

#include "ICARUSPackDef.h"
#include <ac_channel.h>
#include <nvhls_connections.h>
#include <ac_math/ac_sincos_cordic.h>
#include <ac_math/ac_relu.h>
#include <ac_std_float.h>

/*
 * Input: [--256 dimensional vector--]
 * Output: [--4 dimensional vector--]
 * Perform: MLP0 (256x256), MLP1 (256x4)
 */
class MLP : public match::Module {
    SC_HAS_PROCESS(MLP);
public:

    Connections::In<MemReq> memreq; // For write request to memory
    Connections::In<MLP_In_Type> MLPInput;
    Connections::Out<MLP_Out_Type> MLPOutput;

    Connections::Combinational<MLP1_In_Type> MLP0Result;
    
    MLP(sc_module_name name) : match::Module(name),
                               memreq("memreq"),
                               MLPInput("MLPInput"),
                               MLPOutput("MLPOutput"),
                               MLP0Result("MLP0Result") {
        SC_THREAD(InitializeMLP);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);

        SC_THREAD(Monb);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);

        SC_THREAD(Sonb);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);
    }

    // Layer0 weight memory
    MLP_Weight_Type mlp0[MLP0_OUT_DIM][MLP0_IN_DIM];
    MLP_Weight_Type mlp0_bias[MLP0_OUT_DIM];
    // Layer1 weight memory
    MLP_Weight_Type mlp1[MLP1_OUT_DIM][MLP1_IN_DIM];
    MLP_Weight_Type mlp1_bias[MLP1_OUT_DIM];
    /*
     * Dummy memory to write to (may be replaced with technology dependent memory)
     */
    void InitializeMLP() {
        memreq.Reset();
        wait();

        while (1) {
            wait();

            MemReq q;
            if (memreq.PopNB(q)) {
                // assert((q.index[0] < MLP0_OUT_DIM) && (q.index[1] < MLP0_IN_DIM));
                if (q.forMLP0) {
                    if (q.isBias) mlp0_bias[q.index[0]]        = q.data;
                    else          mlp0[q.index[0]][q.index[1]] = q.data;
                } else {
                    if (q.isBias) mlp1_bias[q.index[0]]        = q.data;
                    else          mlp1[q.index[0]][q.index[1]] = q.data;
                }
            }
        }
    }

    /* 
     * Multi-Output Network Block
     * Input: 256-dimensional data
     * Output: 256-dimensional data
     * 64x64 processing unit
     */
    void Monb() {
        MLPInput.Reset();
        MLP0Result.ResetWrite();
        wait();

        while (1) {
            wait();

            MLP_In_Type vec_in;
            if (MLPInput.PopNB(vec_in)) {
                MLP1_In_Type vec_out;
                for (uint i = 0; i < MLP0_OUT_DIM; i++) {
#ifdef USE_FLOAT
                    MLP1_In_Elem_Type tmp = mlp0_bias[i];
                    for (uint j = 0; j < MLP0_IN_DIM; j++) {
                        tmp += mlp0[i][j] * vec_in.X[j];
                    }
                    if (tmp < MLP1_In_Elem_Type(0)) tmp = MLP1_In_Elem_Type(0);   // ReLU
                    vec_out.X[i] = tmp;
#else
                    typedef MLP_In_Elem_Type::rt_T<MLP_Weight_Type>::mult PROD_TYPE;
                    typedef PROD_TYPE::rt_unary::set<MLP0_IN_DIM>::sum SUM_TYPE;
                    SUM_TYPE tmp = (mlp0_bias[i] << 7);
                    for (uint j = 0; j < MLP0_IN_DIM; j++) {
                        PROD_TYPE m = mlp0[i][j] * vec_in.X[j];
                        tmp += m;
                    }
                    MLP1_In_Elem_Type tmp2;
                    if (tmp < MLP1_In_Elem_Type(0)) tmp2 = MLP1_In_Elem_Type(0);  // ReLU
                    else                            tmp2 = tmp >> 7;              // Divided by 128
                    vec_out.X[i] = tmp2;
#endif
                }
                vec_out.isLastSample = vec_in.isLastSample;

                MLP0Result.Push(vec_out);
            }
        }
    }

    /*
     * Single Output Network Block
     * Input: 256-dimensional data
     * Output: 4-dimensional data (r,g,b,\sigma)
     */
    void Sonb() {
        MLP0Result.ResetRead();
        MLPOutput.Reset();

        wait();
        while (1) {
            wait();

            MLP1_In_Type vec_in;
            if (MLP0Result.PopNB(vec_in)) {
                MLP_Out_Type vec_out;
                for (uint i = 0; i < MLP1_OUT_DIM; i++) {
#ifdef USE_FLOAT
                    MLP_Out_Elem_Type tmp = mlp1_bias[i];
                    for (uint j = 0; j < MLP1_IN_DIM; j++) {
                        tmp += mlp1[i][j] * vec_in.X[j];
                    }
                    vec_out.X[i] = tmp;
#else
                    typedef MLP1_In_Elem_Type::rt_T<MLP_Weight_Type>::mult PROD_TYPE;
                    typedef PROD_TYPE::rt_unary::set<MLP1_IN_DIM>::sum SUM_TYPE;
                    SUM_TYPE tmp = (mlp1_bias[i] << 7);
                    for (uint j = 0; j < MLP1_IN_DIM; j++) {
                        PROD_TYPE m = mlp1[i][j] * vec_in.X[j];
                        tmp += m;
                    }
                    vec_out.X[i] = tmp >> 7; // Divided by 128
#endif
                }
                MLPOutput.Push(vec_out);
            }
        }
    }

};

#endif //ICARUS_MLP_VANILLA_H
