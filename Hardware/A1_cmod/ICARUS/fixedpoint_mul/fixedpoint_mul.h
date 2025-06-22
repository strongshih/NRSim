#ifndef ICARUS_FIXEDPOINT_MUL_H
#define ICARUS_FIXEDPOINT_MUL_H

#include "ICARUSPackDef.h"
#include <ac_channel.h>
#include <nvhls_connections.h>
#include <ac_math/ac_sincos_cordic.h>
#include <ac_std_float.h>
#include <boost/preprocessor/repetition/repeat.hpp>
#define MULTIPLIER 3

class pcm : public match::Module {
    SC_HAS_PROCESS(pcm);
public:
    Connections::In<PCM_In_Type> PCMInput;
    Connections::Out<PCM_Out_Type> PCMOutput;

    pcm(sc_module_name name) : match::Module(name),
                               PCMInput("PCMInput"),
                               PCMOutput("PCMOutput") {
        SC_THREAD(run);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);
    }
    
    void run () {
        PCMInput.Reset();
        PCMOutput.Reset();
        wait();
        
        while (1) {
            wait();
 
            PCM_In_Type tmp = PCMInput.Pop();
            PCM_Out_Type ret;
            if (tmp == 0) {
                ret.is_zero = true; 
            } else {
                ret.is_zero = false; 
                ac_int<3*BIT_PER_PART+1, true> tmp_int = tmp.slc<3*BIT_PER_PART+1>(0); // sign extension (so shift works)
                ret.X[0] = tmp_int;                  //  1x
                ret.X[1] = (tmp_int<<1) + tmp_int;   //  3x
                ret.X[2] = (tmp_int<<2) + tmp_int;   //  5x
                ret.X[3] = (tmp_int<<3) - tmp_int;   //  7x
            }
            PCMOutput.Push(ret);
        }
    }
};

class ssa : public match::Module {
    SC_HAS_PROCESS(ssa);
public:
    Connections::In<SSA_In_Type> SSAInput;
    Connections::Out<SSA_Out_Type> SSAOutput;
    Connections::In<MUL_In_Type> MULWeight;
    Connections::Out<MUL_In_Type> MULWeight_prop;

    ssa(sc_module_name name) : match::Module(name),
                               SSAInput("SSAInput"),
                               SSAOutput("SSAOutput"),
                               MULWeight("MULWeight"),
                               MULWeight_prop("MULWeight_prop") {
        SC_THREAD(run);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);
    }

    MUL_In_Type w;
    
    void run () {
        SSAInput.Reset();
        SSAOutput.Reset();
        MULWeight.Reset();
        MULWeight_prop.Reset();
        wait();
        
        while (1) {
            wait();
            bool has_weight = MULWeight.PopNB(w);
            

            SSA_In_Type tmp = SSAInput.Pop();
            SSA_Out_Type ret;
            if (tmp.is_zero) {
                ret = 0;
            } else {
                /*** Select and shift-add part (MUL) ***/
                bool is_neg_w = false;
                if (w[BIT_PER_PART*MUL_PART] == 1) { // 2's to 1's
                    is_neg_w = true;
                    w = -w;
                }
                ac_int<2, false> sel[2], sft[2];
                for (int i = 0; i < 2; i++) { // Kmap to get selectors and shift numbers (only 4 bits)
                    ac_int<BIT_PER_PART, false> bits = w.slc<BIT_PER_PART>(i*BIT_PER_PART);
                    sel[i][0] = (bits[3] & bits[2]) | (bits[1] & bits[2]) | ((!bits[3]) & bits[1] & bits[0]);
                    sel[i][1] = (bits[3] & bits[1]) | ((!bits[3]) & bits[2] & bits[0]);
                    sft[i][0] = (bits[1] & (!bits[0])) | (bits[3] & (!bits[2])) | (bits[3] & bits[1]); 
                    sft[i][1] = (bits[3] & (!bits[1])) | (bits[2] & (!bits[1]) & (!bits[0]));
                }
                ac_int<3*BIT_PER_PART+1, true> lower = (tmp.X[sel[0]]*(1<<sft[0]));
                ac_int<4*BIT_PER_PART+1, true> upper = (tmp.X[sel[1]]*(1<<sft[1])); // sign extended
                ac_int<4*BIT_PER_PART+1, true> shifted = lower + (upper<<BIT_PER_PART);
                ac_fixed<4*BIT_PER_PART+1, 2*INTEGER_WIDTH+1, true> shifted_fixed;
                shifted_fixed.set_slc(0, shifted.slc<4*BIT_PER_PART+1>(0));                    // uint to ufixed
                ret = MUL_Out_Type(shifted_fixed >> nvhls::log2_ceil<SCALE>::val);             // quantized back
                if (is_neg_w) {                                                                // 1's to 2's
                    ret = -ret;
                }
            //cout << "test: " << w << " "  << tmp.X[0] << " " << tmp.X[1] << " " << tmp.X[2] << " " << tmp.X[3] << " " << lower << upper << endl;
            }
            //cout << ret << endl;
            SSAOutput.Push(ret);

            if (has_weight) {
                MULWeight_prop.PushNB(w);
            }
        }
    }
};


/*
 * Input: Signed fixed point weight and input
 * Output: Result
 */
class fixedpoint_mul : public match::Module {
    SC_HAS_PROCESS(fixedpoint_mul);
public:

    Connections::In<MUL_In_Type> MULInput;
    Connections::In<MUL_In_Type> MULWeight;
    Connections::Out<MUL_Out_Type> MULOutput;

#if MULTIPLIER == 3
    // This divides MULTIPLIER=2 to two modules pcm and ssa.
    //
    // input        wire              module       wire       module
    // MULInput  -> MULInput_wire  -> pcm_block -> PCM_out -> ssa_block
    // input        wire              module
    // MULWeight -> MULWeight_wire -> ssa_block
    // module       wire              output
    // ssa_block -> MULOutput_wire -> MULOutput

    Connections::Combinational<PCM_Out_Type> PCM_out;
    Connections::Combinational<MUL_In_Type> MULInput_wire;
    Connections::Combinational<MUL_In_Type> MULWeight_wire;
    Connections::Combinational<MUL_In_Type> MULWeight_prop;
    Connections::Combinational<MUL_Out_Type> MULOutput_wire;
    pcm *pcm_block;
    ssa *ssa_block;

    fixedpoint_mul(sc_module_name name) : match::Module(name),
                                          MULInput("MULInput"),
                                          MULWeight("MULWeight"),
                                          MULOutput("MULOutput"),
                                          PCM_out("PCM_out"),
                                          MULInput_wire("MULInput_wire"),
                                          MULWeight_wire("MULWeight_wire"),
                                          MULWeight_prop("MULWeight_prop"),
                                          MULOutput_wire("MULOutput_wire") {

        pcm_block = new pcm(sc_gen_unique_name("pcm"));
        pcm_block->clk(clk);
        pcm_block->rst(rst);
        pcm_block->PCMInput(MULInput_wire);
        pcm_block->PCMOutput(PCM_out);

        ssa_block = new ssa(sc_gen_unique_name("ssa"));
        ssa_block->clk(clk);
        ssa_block->rst(rst);
        ssa_block->SSAInput(PCM_out);
        ssa_block->MULWeight(MULWeight_wire);
        ssa_block->MULWeight_prop(MULWeight_prop);
        ssa_block->SSAOutput(MULOutput_wire);

        SC_THREAD(run);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);
    }
#else
    mul(sc_module_name name) : match::Module(name),
                               MULInput("MULInput"),
                               MULWeight("MULWeight"),
                               MULOutput("MULOutput") {
        SC_THREAD(run);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);
    }
#endif

#if MULTIPLIER == 0
    void run() {
        MULInput.Reset();
        MULWeight.Reset();
        MULOutput.Reset();
        wait();

        while (1) {
            wait();

            MUL_In_Type tmp = MULInput.Pop();
            MUL_In_Type w = MULWeight.Pop();
            MUL_Out_Type ret = tmp*w >> nvhls::log2_ceil<SCALE>::val;              // Vanilla multiplier and truncation
            MULOutput.Push(ret);
        }
    }
#elif MULTIPLIER == 1
    // RMCM
    void run() {
        MULInput.Reset();
        MULWeight.Reset();
        MULOutput.Reset();
        ac_int<3*BIT_PER_PART+1, true> precompute_input[1<<(BIT_PER_PART-1)];      // Pre-compute result
        wait();

        while (1) {
            wait();

            /*** Pre-Compute Module (PCM) ***/
            MUL_In_Type tmp = MULInput.Pop();                     // Get input
            MUL_In_Type w = MULWeight.Pop();                      // Get weight
            MUL_Out_Type ret;
            if (tmp == 0) {                                       // Gated ?
                ret = 0;
            } else {
                ac_int<3*BIT_PER_PART+1, true> tmp_int = tmp.slc<3*BIT_PER_PART+1>(0); // sign extension (so shift works)
                precompute_input[0] = tmp_int;                        //  1x
                precompute_input[1] = (tmp_int<<1) + tmp_int;         //  3x
                precompute_input[2] = (tmp_int<<2) + tmp_int;         //  5x
                precompute_input[3] = (tmp_int<<3) - tmp_int;         //  7x
                precompute_input[4] = (tmp_int<<3);                   // ~9x
                precompute_input[5] = ((tmp_int<<2) + tmp_int) << 1;  // ~11x
                precompute_input[6] = ((tmp_int<<1) + tmp_int) << 2;  // ~13x
                precompute_input[7] = ((tmp_int<<3) - tmp_int) << 1;  // ~15x

                /*** Select and shift-add part (MUL) ***/
                bool is_neg_w = false;                                // For 1's sign-bit
                if (w[BIT_PER_PART*MUL_PART] == 1) {                  // 2's to 1's
                    is_neg_w = true;
                    w = -w;
                }

                ac_int<BIT_PER_PART-1, false>  low_sel = w.slc<BIT_PER_PART-1>(0*BIT_PER_PART+1), // selectors
                                              high_sel = w.slc<BIT_PER_PART-1>(1*BIT_PER_PART+1);
                ac_int<3*BIT_PER_PART+1, true> lower = precompute_input[low_sel];
                ac_int<4*BIT_PER_PART+1, true> upper = precompute_input[high_sel]; // sign extension
                ac_int<4*BIT_PER_PART+1, true> shifted = lower + (upper<<BIT_PER_PART);
                ac_fixed<4*BIT_PER_PART+1, 2*INTEGER_WIDTH+1, true> shifted_fixed;
                shifted_fixed.set_slc(0, shifted.slc<4*BIT_PER_PART+1>(0));        // uint to ufixed
                ret = MUL_Out_Type(shifted_fixed >> nvhls::log2_ceil<SCALE>::val); // quantized back
                if (is_neg_w) {                                                    // 1's to 2's
                    ret = -ret;
                }
            }
            MULOutput.Push(ret);
        }
    }
#elif MULTIPLIER == 2
    // Approximated RMCM
    void run() {
        MULInput.Reset();
        MULWeight.Reset();
        MULOutput.Reset();
        ac_int<3*BIT_PER_PART+1, true> precompute_input[4]; // Pre-compute result
        wait();

        while (1) {
            wait();

            MUL_In_Type tmp = MULInput.Pop();                   // Get input
            MUL_In_Type w = MULWeight.Pop();                    // Get weight
            MUL_Out_Type ret;
            if (tmp == 0) {                                     // Gated ?
                ret = 0;
            } else {
                /*** Pre-Compute Module (PCM) ***/
                ac_int<3*BIT_PER_PART+1, true> tmp_int = tmp.slc<3*BIT_PER_PART+1>(0); // sign extension (so shift works)
                precompute_input[0] = tmp_int;                  //  1x
                precompute_input[1] = (tmp_int<<1) + tmp_int;   //  3x
                precompute_input[2] = (tmp_int<<2) + tmp_int;   //  5x
                precompute_input[3] = (tmp_int<<3) - tmp_int;   //  7x

                /* dec |bin  |sel   |shift (Can use Kmap do this)
                 * 00  |0000 |gated |
                 * 01  |0001 |0     |<<0
                 * 02  |0010 |0     |<<1
                 * 03  |0011 |1     |<<0
                 * 04  |0100 |0     |<<2
                 * 05  |0101 |2     |<<0
                 * 06  |0110 |1     |<<1
                 * 07  |0111 |3     |<<0
                 * 08  |1000 |0     |<<3
                 * 09  |1001 |0     |<<3
                 * 10  |1010 |2     |<<1
                 * 11  |1011 |2     |<<1
                 * 12  |1100 |1     |<<2
                 * 13  |1101 |1     |<<2
                 * 14  |1110 |3     |<<1
                 * 15  |1111 |3     |<<1
                 */
                /*** Select and shift-add part (MUL) ***/
                bool is_neg_w = false;
                if (w[BIT_PER_PART*MUL_PART] == 1) { // 2's to 1's
                    is_neg_w = true;
                    w = -w;
                }
                ac_int<2, false> sel[2], sft[2];
                for (int i = 0; i < 2; i++) { // Kmap to get selectors and shift numbers (only 4 bits)
                    ac_int<BIT_PER_PART, false> bits = w.slc<BIT_PER_PART>(i*BIT_PER_PART);
                    sel[i][0] = (bits[3] & bits[2]) | (bits[1] & bits[2]) | ((!bits[3]) & bits[1] & bits[0]);
                    sel[i][1] = (bits[3] & bits[1]) | ((!bits[3]) & bits[2] & bits[0]);
                    sft[i][0] = (bits[1] & (!bits[0])) | (bits[3] & (!bits[2])) | (bits[3] & bits[1]); 
                    sft[i][1] = (bits[3] & (!bits[1])) | (bits[2] & (!bits[1]) & (!bits[0]));
                }
                ac_int<3*BIT_PER_PART+1, true> lower = (precompute_input[sel[0]]*(1<<sft[0]));
                ac_int<4*BIT_PER_PART+1, true> upper = (precompute_input[sel[1]]*(1<<sft[1])); // sign extended
                ac_int<4*BIT_PER_PART+1, true> shifted = lower + (upper<<BIT_PER_PART);
                ac_fixed<4*BIT_PER_PART+1, 2*INTEGER_WIDTH+1, true> shifted_fixed;
                shifted_fixed.set_slc(0, shifted.slc<4*BIT_PER_PART+1>(0));                    // uint to ufixed
                ret = MUL_Out_Type(shifted_fixed >> nvhls::log2_ceil<SCALE>::val);             // quantized back
                if (is_neg_w) {                                                                // 1's to 2's
                    ret = -ret;
                }
            }
            MULOutput.Push(ret);
        }
    }
#elif MULTIPLIER == 3
    #pragma hls_pipeline_init_interval 1
    // Approximated RMCM
    void run() { // make II=1
        MULInput.Reset();
        MULWeight.Reset();
        MULOutput.Reset();
        MULInput_wire.ResetWrite();
        MULWeight_wire.ResetWrite();
        MULOutput_wire.ResetRead();
        MULWeight_prop.ResetRead();
        wait();

        #pragma hls_pipeline_init_interval 1
        while (1) {
            wait();

            MUL_In_Type tmp2, ttt; 
            if (MULWeight.PopNB(tmp2)) {
                MULWeight_wire.Push(tmp2);                // 1cc
                MULWeight_prop.PopNB(ttt);                     // 1cc
            }
            MUL_In_Type tmp;
            if (MULInput.PopNB(tmp)) {
                MULInput_wire.Push(tmp);                  // 1cc
            }
            MUL_Out_Type tmp3;
            if (MULOutput_wire.PopNB(tmp3)) {
                MULOutput.Push(tmp3);                     // 1cc
            }
        }
    }

#endif

};

#endif //ICARUS_FIXEDPOINT_MUL_H
