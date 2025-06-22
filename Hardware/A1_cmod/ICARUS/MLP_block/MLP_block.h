#ifndef ICARUS_MLP_BLOCK_H
#define ICARUS_MLP_BLOCK_H

#include "ICARUSPackDef.h"
#include <ac_channel.h>
#include <nvhls_connections.h>
#include <ac_math/ac_sincos_cordic.h>
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

    Connections::Combinational<sample_cnt> sample_num;
    
    MLP(sc_module_name name) : match::Module(name),
                               memreq("memreq"),
                               MLPInput("MLPInput"),
                               MLPOutput("MLPOutput"),
                               sample_num("sample_num") {
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

            MemReq q = memreq.Pop();
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

    /* 
     * Multi-Output Network Block
     * Input: 256-dimensional data
     * Output: 256-dimensional data
     * Processing order: 1. inner loops for 64x64 submatrix            (ii, jj)
     *                   2. pipelined samples (weight stationary)      (cnt)
     *                   3. outer loops for which row/col of submatrix (i, j)
     * cycles: 16*192
     */
    // temporary memory
    MLP1_In_Elem_Type act_mem[MLP0_OUT_DIM][MAX_SAMPLE_NUM];
    void Monb() {
        sample_cnt cnt = 0; // which sample (pipelined in)
        uint i=0, j=0;      // which submatrices
        MLPInput.Reset();
        sample_num.ResetWrite();
        wait();

// #pragma hls_pipeline_init_interval 1
        while (1) {
            wait();

            MLP_In_Type vec_in = MLPInput.Pop();

#ifdef USE_FLOAT
            MLP1_In_Elem_Type acc[BLOCK_SZ]; // accumulator (reg);
#else
            typedef MLP_In_Elem_Type::rt_T<MLP_Weight_Type>::mult PROD_TYPE;
            typedef PROD_TYPE::rt_unary::set<MLP0_IN_DIM>::sum SUM_TYPE;
            SUM_TYPE acc[BLOCK_SZ]; // accumulator (reg);
#endif

#pragma unroll
            for (uint ii = 0; ii < BLOCK_SZ; ii++) {                            // submatrix row
#ifdef USE_FLOAT
                acc[ii] = (j == 0) ? mlp0_bias[i+ii] : act_mem[i+ii][cnt];      // load from temporary memory
#pragma unroll
                for (uint jj = 0; jj < BLOCK_SZ; jj++) {                        // submatrix col
                    acc[ii] += mlp0[i+ii][j+jj] * vec_in.X[j+jj];
                }
                MLP1_In_Elem_Type tmp = acc[ii];
                if (j == (MLP0_IN_DIM-BLOCK_SZ) && (acc[ii] < MLP1_In_Elem_Type(0))) { // ReLU
                    tmp = MLP1_In_Elem_Type(0);
                }
                act_mem[i+ii][cnt] = tmp;
#else
                acc[ii] = (j == 0) ? SUM_TYPE(mlp0_bias[i+ii] << nvhls::log2_ceil<SCALE>::val) : 
                                     SUM_TYPE(act_mem[i+ii][cnt] << nvhls::log2_ceil<SCALE>::val); // load bias or psum
#pragma unroll
                for (uint jj = 0; jj < BLOCK_SZ; jj++) {                        // submatrix col
                    PROD_TYPE m = mlp0[i+ii][j+jj] * vec_in.X[j+jj];
                    acc[ii] += m;
                }
                if (j == (MLP0_IN_DIM-BLOCK_SZ) && acc[ii] < MLP1_In_Elem_Type(0)) { // ReLU
                    acc[ii] = MLP1_In_Elem_Type(0);
                }
                act_mem[i+ii][cnt] = acc[ii] >> nvhls::log2_ceil<SCALE>::val;
#endif
            }
            if ((i == MLP0_OUT_DIM-BLOCK_SZ) && j == (MLP0_IN_DIM-BLOCK_SZ) && vec_in.isLastSample) {
                sample_num.Push(cnt+1); // trigger sonb after processing last submatrix (can be earlier)
            }
            cnt = (vec_in.isLastSample) ? sample_cnt(0) : sample_cnt(cnt+1);
            i = (vec_in.isLastSample) ? 
               ((j == (MLP0_IN_DIM-BLOCK_SZ)) ? (i == (MLP0_OUT_DIM-BLOCK_SZ) ? 0 : i+BLOCK_SZ) : i) : i;
            j = (vec_in.isLastSample) ? ((j == (MLP0_IN_DIM-BLOCK_SZ)) ? 0 : j+BLOCK_SZ) : j;
        }
    }

    /*
     * Single Output Network Block
     * Input: 256-dimensional data
     * Output: 4-dimensional data (r,g,b,\sigma)
     * cycles: 16*192 (can be overlapped with Monb by adjusting the timing of pushing sample_num)
     */
    MLP1_In_Elem_Type out_mem[MLP1_OUT_DIM][MAX_SAMPLE_NUM];
    void Sonb() {
        sample_num.ResetRead();
        MLPOutput.Reset();

        wait();
        while (1) {
            wait();
            
            sample_cnt num = sample_num.Pop(); // wait for Monb to finish

            MLP1_In_Type vec_in;
            MLP_Out_Elem_Type acc; // accumulator (reg);
            for (uint i = 0; i < MLP1_OUT_DIM; i++) { 
                for (uint j = 0; j < MLP1_IN_DIM; j+=BLOCK_SZ) {
// #pragma hls_pipeline_init_interval 1
                    for (uint n = 0; n < num; n++) {
#ifdef USE_FLOAT
                        MLP_Out_Elem_Type acc = (j == 0) ? mlp1_bias[i] : out_mem[i][n];
#pragma unroll
                        for (uint jj = 0; jj < BLOCK_SZ; jj++) {
                            acc += mlp1[i][j+jj] * act_mem[j+jj][n]; // adder tree
                        }
                        out_mem[i][n] = acc;
#else
                        typedef MLP1_In_Elem_Type::rt_T<MLP_Weight_Type>::mult PROD_TYPE;
                        typedef PROD_TYPE::rt_unary::set<MLP1_IN_DIM>::sum SUM_TYPE;
                        SUM_TYPE acc = (j == 0) ? SUM_TYPE(mlp1_bias[i] << nvhls::log2_ceil<SCALE>::val) : 
                                                  SUM_TYPE(out_mem[i][n] << nvhls::log2_ceil<SCALE>::val); // load bias or psum
#pragma unroll
                        for (uint jj = 0; jj < BLOCK_SZ; jj++) {
                            PROD_TYPE m = mlp1[i][j+jj] * act_mem[j+jj][n];
                            acc += m; // adder tree
                        }
                        out_mem[i][n] = acc >> nvhls::log2_ceil<SCALE>::val; // Divided by 128
#endif
                    }
                }
            }

            for (uint n = 0; n < num; n++) {
                MLP_Out_Type vec_out;
#pragma unroll
                for (uint i = 0; i < MLP1_OUT_DIM; i++) {
                    vec_out.X[i] = out_mem[i][n];
                }
                vec_out.isLastSample = (n == num-1);
                MLPOutput.Push(vec_out);
            }
        }
    }

};

#endif //ICARUS_MLP_BLOCK_H
