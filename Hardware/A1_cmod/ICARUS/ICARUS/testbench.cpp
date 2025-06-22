#define NVHLS_VERIFY_BLOCKS (ICARUS)
#include "ICARUS.h"
#include <nvhls_verify.h>
//#include <mc_scverify.h>
#include "nvhls_connections.h"
#include "ac_sysc_trace.h"
#include <random>
//#include <ac_channel.h>
#include <systemc.h>
#include <nvhls_module.h>
#include <mc_connections.h>

#define SAMPLE_NUM 192

class Top : public sc_module {
public:
    sc_clock clk;
    sc_signal<bool> rst;

    Connections::Combinational<ICARUS_Op_In_Type> ICARUS_Op;
    Connections::Combinational<VRU_Out_Type> VRU_out;                // TODO: should connect to buffer, then write to memory

    Connections::Buffer<PEU_In_Type, SAMPLE_NUM> pos_in;             // TODO: should be offchip memory access
    Connections::Combinational<PEU_In_Type> pos_in_enq;
    Connections::Combinational<PEU_In_Type> pos_in_deq;

    Connections::Buffer<MemReq, 1024> memory_req_in;  // TODO: should be offchip memory access
    // There's a limit in terms of buffer depth, just put 1024 here, will latter test for correctness

    Connections::Combinational<MemReq> memory_req_in_enq;
    Connections::Combinational<MemReq> memory_req_in_deq;

    NVHLS_DESIGN(ICARUS) dut;
//    CCS_DESIGN(SDAcc) CCS_INIT_S1(dut);

    SC_CTOR(Top) : clk("clk", 1, SC_NS, 0.5, 0, SC_NS, true),
                   rst("rst"),
                   ICARUS_Op("ICARUS_Op"),
                   VRU_out("VRU_out"),
                   pos_in_enq("pos_in_enq"),
                   pos_in_deq("pos_in_deq"),
                   memory_req_in_enq("memory_req_in_enq"),
                   memory_req_in_deq("memory_req_in_deq"),
                   dut("dut") {

        sc_object_tracer<sc_clock> trace_clk(clk);

        dut.clk(clk);
        dut.rst(rst);
        dut.ICARUS_Op(ICARUS_Op);
        dut.pos_in(pos_in_deq);
        dut.memory_req_in(memory_req_in_deq);
        dut.VRU_out(VRU_out);

        pos_in.clk(clk);
        pos_in.rst(rst);
        pos_in.enq(pos_in_enq);
        pos_in.deq(pos_in_deq);

        memory_req_in.clk(clk);
        memory_req_in.rst(rst);
        memory_req_in.enq(memory_req_in_enq);
        memory_req_in.deq(memory_req_in_deq);

        SC_THREAD(reset);
        sensitive << clk.posedge_event();

        SC_THREAD(run);
        sensitive << clk.posedge_event();
        async_reset_signal_is(rst, false);

        SC_THREAD(collect);
        sensitive << clk.posedge_event();
        async_reset_signal_is(rst, false);
    }

    void reset() {
        rst.write(false);
        wait(10);
        rst.write(true);
    }

    void run() {
        ICARUS_Op.ResetWrite();
        pos_in_enq.ResetWrite();
        memory_req_in_enq.ResetWrite();
        wait(10);
/*
        // Write to matrix A memory 128x3
        cout << "Matrix A (128x3): " << endl;
        for (int i = 0; i < PEU_CORDIC_IN_DIM; i++) {
            for (int j = 0; j < PEU_INPUT_DIM; j++) {
                MemReq req1;
                req1.index[0] = i;
                req1.index[1] = j;
                int power = i / 3;
                if (j == i % 3) {
                    // not quite the same as mentioned in paper, here hust test matrix-vector and cordic functionalities
                    req1.data = PEU_Matrix_A_Type((1 << power)/3.141592653589793);
                } else {
                    req1.data = PEU_Matrix_A_Type(0);
                }
                req1.forMLP0 = false;
                req1.forPEU = true;
                memory_req_in_enq.Push(req1);
            }
            cout << endl;
        }

        // Write to layer0 weight memory 256x256
        cout << "Weight memory (256x256): " << endl;
        for (int i = 0; i < MLP0_OUT_DIM; i++) {
            for (int j = 0; j < MLP0_IN_DIM; j++) {
                MemReq req1;
                req1.index[0] = i;
                req1.index[1] = j;
                req1.data = MLP_Weight_Type(0.001*(i*MLP0_IN_DIM+j));
                req1.forMLP0 = true;
                req1.forPEU = false;
                memory_req_in_enq.Push(req1);
            }
        }
        cout << "Finish writing to layer0 @ " << sc_time_stamp() << endl;
*/
        // Write to layer1 weight memory 4x256
        cout << "Weight memory (4x256): " << endl;
        for (int i = 0; i < MLP1_OUT_DIM; i++) {
            for (int j = 0; j < MLP1_IN_DIM; j++) {
                MemReq req1;
                req1.index[0] = i;
                req1.index[1] = j;
                req1.data = MLP_Weight_Type(0.001*(i*MLP1_IN_DIM+j));
                req1.forMLP0 = false;
                req1.forPEU = false;
                memory_req_in_enq.Push(req1);
            }
        }
        cout << "Finish writing to layer1 @ " << sc_time_stamp() << endl;

        // Random input Poly input
        for (int i = 0; i < SAMPLE_NUM; i++) {
            PEU_In_Type pos;
            pos.X[0] = PEU_Position_Type(i*0.1);
            pos.X[1] = PEU_Position_Type(i*0.1);
            pos.X[2] = PEU_Position_Type(i*0.1);
            pos.isLastSample = (i == SAMPLE_NUM-1);
            pos_in_enq.Push(pos);
        }
        cout << "Finish writing to pos @ " << sc_time_stamp() << endl;

        wait(10);

        // Start testing 
        ICARUS_Op_In_Type op_init;
        op_init.mode = inst_type::WEIGHT_INIT;
        op_init.num  = 1024;
        ICARUS_Op.Push(op_init);

        ICARUS_Op_In_Type op_run;
        op_run.mode = inst_type::READ_POS;
        op_run.num  = 192;
        ICARUS_Op.Push(op_run);
    }

    void collect() {
        VRU_out.ResetRead();
        while (1) {
            wait(); // 1 cc

            VRU_Out_Type tmp;
            tmp = VRU_out.Pop();
            cout << "ICARUSOutput: @ timestep: " << sc_time_stamp() << endl;
            for (uint i = 0; i < 3; i++) {
                cout << tmp.c[i] << " ";
            }

            sc_stop();
        }
    }
};

int sc_main(int argc, char *argv[]) {
    Top tb("tb");
    sc_start();
    return 0;
}

//#include <>
