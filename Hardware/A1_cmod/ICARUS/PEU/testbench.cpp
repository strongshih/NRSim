#define NVHLS_VERIFY_BLOCKS (PEU)
#include "PEU.h"
#include <nvhls_verify.h>
//#include <mc_scverify.h>
#include "nvhls_connections.h"
#include "ac_sysc_trace.h"
#include <random>
//#include <ac_channel.h>
#include <systemc.h>
#include <nvhls_module.h>
#include <mc_connections.h>

#define SAMPLE_NUM 5

class Top : public sc_module {
public:
    sc_clock clk;
    sc_signal<bool> rst;

    Connections::Combinational<MemReq> memreq;
    Connections::Combinational<PEU_In_Type> PEUInput;
    Connections::Combinational<PEU_Out_Type> PEUOutput;

    NVHLS_DESIGN(PEU) dut;
//    CCS_DESIGN(SDAcc) CCS_INIT_S1(dut);

    SC_CTOR(Top) : clk("clk", 1, SC_NS, 0.5, 0, SC_NS, true),
                   rst("rst"),
                   //memreq("memreq"),
                   PEUInput("PEUInput"),
                   PEUOutput("PEUOutput"),
                   dut("dut") {

        sc_object_tracer<sc_clock> trace_clk(clk);

        dut.clk(clk);
        dut.rst(rst);
        //dut.memreq(memreq);
        dut.PEUInput(PEUInput);
        dut.PEUOutput(PEUOutput);

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
        //memreq.ResetWrite();
        PEUInput.ResetWrite();
        wait(10);

        // Write to matrix A memory 128x3
        /*cout << "Matrix A (128x3): " << endl;
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
                cout << req1.data << " ";
                memreq.Push(req1);
            }
            cout << endl;
        }*/

        wait(10);

        // Random input Poly input (5 inputs)
        for (int i = 0; i < SAMPLE_NUM; i++) {
            PEU_In_Type pos;
            pos.X[0] = PEU_Position_Type(i);
            pos.X[1] = PEU_Position_Type(i);
            pos.X[2] = PEU_Position_Type(i);
            pos.isLastSample = (i == SAMPLE_NUM-1);
            PEUInput.Push(pos);
        }
    }

    void collect() {
        PEUOutput.ResetRead();
        while (1) {
            wait(); // 1 cc

            for (int i = 0; i < SAMPLE_NUM; i++) {
                PEU_Out_Type tmp;
                tmp = PEUOutput.Pop();
                cout << "PEUOutput: @ timestep: " << sc_time_stamp() << endl;
                // Rearrange outputs and compare with test_output in peu_test.h
                for (uint j = 0; j < PEU_CORDIC_IN_DIM/3; j++) {
                    for (uint k = 0; k < 3; k++)
                        cout << tmp.X[6*j+2*k] << " ";
                    for (uint k = 0; k < 3; k++)
                        cout << tmp.X[6*j+2*k+1] << " ";
                }
                cout << endl;
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
