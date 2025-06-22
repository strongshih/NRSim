#ifndef ICARUS_MLP_MONB_H
#define ICARUS_MLP_MONB_H

#include "ICARUSPackDef.h"
#include "fixedpoint_mul.h"
#include <ac_channel.h>
#include <nvhls_connections.h>
#include <ac_math/ac_sincos_cordic.h>
#include <ac_std_float.h>

// ulimit -s unlimited (for large data structure on stack)

/*
 * Input: [--256 dimensional vector--]
 * Output: [--4 dimensional vector--]
 * Perform: MLP0 (256x256)
 */
class MLP_monb : public match::Module {
    SC_HAS_PROCESS(MLP_monb);
public:

    Connections::In<MemReq> memreq; // For write request to memory
    Connections::In<MLP_In_Type> MLPInput;
    Connections::Out<MLP_Out_Type> MLPOutput;

    pcm *pcm_block[BLOCK_SZ];
    ssa *ssa_block[BLOCK_SZ][BLOCK_SZ];
    Connections::Combinational<PCM_In_Type>  MULInput_wire[BLOCK_SZ];
    Connections::Combinational<PCM_Out_Type> PCM_out[BLOCK_SZ];
    Connections::Combinational<PCM_Out_Type> PCM_out_fanout[BLOCK_SZ][BLOCK_SZ];
    Connections::Combinational<SSA_Out_Type> MULOutput_wire[BLOCK_SZ][BLOCK_SZ];
    Connections::Combinational<MUL_In_Type>  MULWeight_in[BLOCK_SZ][BLOCK_SZ];
    Connections::Combinational<MUL_In_Type>  w_in[BLOCK_SZ];

    MLP(sc_module_name name) : match::Module(name),
                               memreq("memreq"),
                               MLPInput("MLPInput"),
                               MLPOutput("MLPOutput") {
        // wire        wire             module       wire       reg           wire      module    wire
        // vec_in.X -> MULInput_wire -> pcm_block -> PCM_out -> pcm_buffer -> SSA_in -> ssa    -> MULOutput_wire
        for (int i = 0; i < BLOCK_SZ; i++) {
            pcm_block[i] = new pcm(sc_gen_unique_name("pcm"));
            pcm_block[i]->clk(clk);
            pcm_block[i]->rst(rst);
            pcm_block[i]->PCMInput(MULInput_wire[i]);
            pcm_block[i]->PCMOutput(PCM_out[i]);
        }
        for (int i = 0; i < BLOCK_SZ; i++) {
            for (int j = 0; j < BLOCK_SZ; j++) {
                ssa_block[i][j] = new ssa(sc_gen_unique_name("ssa"));
                ssa_block[i][j]->clk(clk);
                ssa_block[i][j]->rst(rst);
                ssa_block[i][j]->SSAInput(PCM_out_fanout[i][j]);
                ssa_block[i][j]->SSAOutput(MULOutput_wire[i][j]);
                if (j == 0)
                    ssa_block[i][j]->MULWeight(w_in[i]);              // weight in
                else
                    ssa_block[i][j]->MULWeight(MULWeight_in[i][j-1]); // weight in
                ssa_block[i][j]->MULWeight_prop(MULWeight_in[i][j]);  // weight out
            }
        }

        SC_THREAD(InitializeMLP);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);

        SC_THREAD(Monb);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);

        SC_THREAD(fanout_pcm);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);
    }


    // Layer0 weight memory
    MLP_Weight_Type mlp0[MLP0_OUT_DIM][MLP0_IN_DIM];
    MLP_Weight_Type mlp0_bias[MLP0_OUT_DIM];
    /*
     * Dummy memory to write to (may be replaced with technology dependent memory)
     */
    void InitializeMLP() {
        memreq.Reset();
        wait();

        while (1) {
            wait();

            MemReq q;
            if (memreq.PopNB(q)){
                if (q.forMLP0) {
                    if (q.isBias) mlp0_bias[q.index[0]]        = q.data;
                    else          mlp0[q.index[0]][q.index[1]] = q.data;
                }
            }
        }
    }

    // PCM to several SSAs
    void fanout_pcm () {
        #pragma unroll
        for (int i = 0; i < BLOCK_SZ; i++) {
            PCM_out[i].ResetRead();
            #pragma unroll
            for (int j = 0; j < BLOCK_SZ; j++) {
                PCM_out_fanout[i][j].ResetWrite();
            }
        }
        wait();

        while (1) {
            wait();
            #pragma unroll
            for (int j = 0; j < BLOCK_SZ; j++) {
                PCM_Out_Type tmp = PCM_out[j].Pop();
                #pragma unroll
                for (int i = 0; i < BLOCK_SZ; i++) {
                    PCM_out_fanout[i][j].Push(tmp);
                }
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
        #pragma unroll
        for (int i = 0; i < BLOCK_SZ; i++) {
            MULInput_wire[i].ResetWrite();
            #pragma unroll
            for (int j = 0; j < BLOCK_SZ; j++) {
                MULOutput_wire[i][j].ResetRead();
            }
        }
        #pragma unroll
        for (int i = 0; i < BLOCK_SZ; i++) {
            w_in[i].ResetWrite();
            MULWeight_in[i][BLOCK_SZ-1].ResetRead();
        }
        wait();

        // #pragma hls_pipeline_init_interval 1
        while (1) {
            wait();

            MLP_In_Type vec_in = MLPInput.Pop();

            typedef MUL_Out_Type::rt_unary::set<MLP0_IN_DIM>::sum SUM_TYPE;
            SUM_TYPE acc[BLOCK_SZ]; // accumulator (reg);

            if (cnt == 0) { // first sample, fill weight first
               for (int jj = BLOCK_SZ-1; jj >= 0; jj--) { // 64 cc
                    #pragma unroll
                    for (int ii = 0; ii < BLOCK_SZ; ii++) { 
                        w_in[ii].Push(mlp0[i+ii][j+jj]); 
                    }
                    #pragma unroll
                    for (int ii = 0; ii < BLOCK_SZ; ii++) {
                        MULWeight_in[ii][BLOCK_SZ-1].Pop(); // pop here to not get stuck
                    }
                }
            }
            #pragma unroll
            for (uint jj = 0; jj < BLOCK_SZ; jj++) {
                MULInput_wire[jj].Push(vec_in.X[j+jj]);
            }
            #pragma unroll
            for (uint ii = 0; ii < BLOCK_SZ; ii++) {                            // submatrix row
                acc[ii] = (j == 0) ? SUM_TYPE(mlp0_bias[i+ii] << nvhls::log2_ceil<SCALE>::val) : 
                                     SUM_TYPE(act_mem[i+ii][cnt] << nvhls::log2_ceil<SCALE>::val); // load bias or psum
                #pragma unroll
                for (uint jj = 0; jj < BLOCK_SZ; jj++) {                        // submatrix col (adder tree)
                    MUL_Out_Type m = MULOutput_wire[ii][jj].Pop();
                    acc[ii] += m;
                }
                if (j == (MLP0_IN_DIM-BLOCK_SZ) && acc[ii] < MLP1_In_Elem_Type(0)) { // ReLU
                    acc[ii] = MLP1_In_Elem_Type(0);
                }
                act_mem[i+ii][cnt] = acc[ii] >> nvhls::log2_ceil<SCALE>::val;
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

};

#endif //ICARUS_MLP_MONB_H
