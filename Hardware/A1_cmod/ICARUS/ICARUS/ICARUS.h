#ifndef ICARUS_H
#define ICARUS_H

#include "ICARUSPackDef.h"
#include "PEU.h"
#include "MLP_vanilla.h" // current use this version. TODO: allocate a scratchpad mem for PEUOutput, and include TriggerMLP
#include "VRU.h"
#include <ac_channel.h>
#include <nvhls_connections.h>
#include <ac_math/ac_sincos_cordic.h>
#include <ac_std_float.h>

/*
 * Input: (x,y,z)
 * Output: (r,g,b)
 * 45.75 (s) / (800*800(pixels)*1/(400MHz)) = 28593 cycles for batch 192 samples
 *     - PEU: 128 * 192 (if 1 mul and 2 cordics only)
 *     - MLP:  32 * 192 
 *     - VRU:   1 * 192
 */
class ICARUS : public match::Module {
    SC_HAS_PROCESS(ICARUS);
public:

    // Host, Off-chip mem access
    // DMA: https://github.com/hlslibs/matchlib_toolkit/tree/main/examples/08_dma

    Connections::In<ICARUS_Op_In_Type> ICARUS_Op;
    Connections::In<PEU_In_Type> pos_in;    // TODO: should be offchip-memory access
    Connections::In<MemReq> memory_req_in;  // TODO: should be offchip-memory access
    Connections::Out<VRU_Out_Type> VRU_out; // TODO: should connect to buffer, then write to memory

    PEU *peu;
    Connections::Combinational<MemReq> peu_memreq; 
    Connections::Combinational<PEU_In_Type> PEUInput;
    Connections::Combinational<PEU_Out_Type> PEUOutput;

    MLP *mlp;
    Connections::Combinational<MemReq> mlp_memreq;
    Connections::Combinational<MLP_In_Type> MLPInput;
    Connections::Combinational<MLP_Out_Type> MLPOutput;

    VRU *vru;
    Connections::Combinational<VRU_In_Type> VRUInput;
    Connections::Combinational<VRU_Out_Type> VRUOutput;

    Connections::Buffer<PEU_Out_Type, PEU_MLP_TO_DEPTH> peu_mlp;
    Connections::Buffer<MemReq, MEMREQ_DEPTH> memreq_fifo;
    Connections::Buffer<VRU_Out_Type, VRUOUT_DEPTH> vruout_fifo;

    Connections::Combinational<MemReq> memory_req_out; 
    Connections::Combinational<MemReq> memory_fifo_in; 

    ICARUS(sc_module_name name) : match::Module(name),
                                  ICARUS_Op     ("ICARUS_Op"),
                                  pos_in        ("pos_in"),
                                  memory_req_in ("memory_req_in"),
                                  VRU_out       ("VRU_out"),
                                  peu_memreq    ("peu_memreq"),
                                  PEUInput      ("PEUInput"),
                                  PEUOutput     ("PEUOutput"),
                                  mlp_memreq    ("mlp_memreq"),
                                  MLPInput      ("MLPInput"),
                                  MLPOutput     ("MLPOutput"),
                                  VRUInput      ("VRUInput"),
                                  VRUOutput     ("VRUOutput"),
                                  memory_req_out("memory_req_out"),
                                  memory_fifo_in("memory_fifo_in") {

        peu = new PEU(sc_gen_unique_name("PEU"));
        peu->clk(clk);
        peu->rst(rst);
        peu->memreq(peu_memreq);
        peu->PEUInput(PEUInput);
        peu->PEUOutput(PEUOutput);

        peu_mlp.clk(clk);
        peu_mlp.rst(rst);
        peu_mlp.enq(PEUOutput);
        peu_mlp.deq(MLPInput);

        mlp = new MLP(sc_gen_unique_name("MLP"));
        mlp->clk(clk);
        mlp->rst(rst);
        mlp->memreq(mlp_memreq);
        mlp->MLPInput(MLPInput);
        mlp->MLPOutput(MLPOutput);

        vru = new VRU(sc_gen_unique_name("VRU"));
        vru->clk(clk);
        vru->rst(rst);
        vru->VRUInput(VRUInput);
        vru->VRUOutput(VRUOutput);

        vruout_fifo.clk(clk);
        vruout_fifo.rst(rst);
        vruout_fifo.enq(VRUOutput);
        vruout_fifo.deq(VRU_out);

        memreq_fifo.clk(clk);
        memreq_fifo.rst(rst);
        memreq_fifo.enq(memory_req_out);
        memreq_fifo.deq(memory_fifo_in);

        SC_THREAD(RouteMemReq);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);

        SC_THREAD(Cfg);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);

        SC_THREAD(MLP_to_VRU);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);
    }

    void RouteMemReq() {
        memory_fifo_in.ResetRead();
        peu_memreq.ResetWrite();
        mlp_memreq.ResetWrite();
        wait();

        while (1) {
            wait();

            MemReq q;
            if (memory_fifo_in.PopNB(q)) {
                if (q.forPEU)
                    peu_memreq.Push(q);
                else
                    mlp_memreq.Push(q);
            }
        }
    }

    void Cfg() {
        ICARUS_Op.Reset();
        pos_in.Reset();
        memory_req_in.Reset();
        memory_req_out.ResetWrite();
        PEUInput.ResetWrite();
        wait();

        while (1) {
            wait();

            ICARUS_Op_In_Type op;
            if (ICARUS_Op.PopNB(op)) {
                switch (op.mode) {
                    case (inst_type::WEIGHT_INIT): {
                        for (uint i = 0; i < op.num; i++) {
                            MemReq q = memory_req_in.Pop(); // should be poppable
                            memory_req_out.Push(q);
                        }
                    }
                    case (inst_type::READ_POS): {
                        for (uint i = 0; i < op.num; i++) {
                            PEU_In_Type x = pos_in.Pop(); // should be poppable
                            PEUInput.Push(x);
                        }
                    }
                    default:
                        break;
                }
            }
        }
    }

    void MLP_to_VRU() {
        MLPOutput.ResetRead();
        VRUInput.ResetWrite();
        wait();

        while (1) {
            wait();
            MLP_Out_Type m;
            VRU_In_Type v;
            if (MLPOutput.PopNB(m)) {
                for (int i = 0; i < 3; i++)
                    v.emitted_c[i] = m.X[i];
                v.sigma = m.X[3];
                v.delta = VRU_Delta_Type(0.1); // Placeholder. TODO: change this
                VRUInput.Push(v);
            }
        }
    }



};

#endif //ICARUS_H
