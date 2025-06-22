#define NVHLS_VERIFY_BLOCKS (BSU)
#include "BSU.h"
#include <nvhls_verify.h>
//#include <mc_scverify.h>
#include "nvhls_connections.h"
#include "ac_sysc_trace.h"
#include <random>
//#include <ac_channel.h>
#include <systemc.h>
#include <nvhls_module.h>
#include <mc_connections.h>

#pragma hls_design top
class testbench : public sc_module {
public:
    sc_clock clk;
    sc_signal<bool> rst;

    Connections::Combinational<BSU_IN_OUT_TYPE> BSUInput;
    Connections::Combinational<BSU_IN_OUT_TYPE> BSUOutput;

    NVHLS_DESIGN(BSU) dut;

    SC_CTOR(testbench) : clk("clk", 1, SC_NS, 0.5, 0, SC_NS, true),
                   rst("rst"),
                   BSUInput("BSUInput"),
                   BSUOutput("BSUOutput"),
                   dut("dut") {

        sc_object_tracer<sc_clock> trace_clk(clk);

        dut.clk(clk);
        dut.rst(rst);
        dut.BSUInput(BSUInput);
        dut.BSUOutput(BSUOutput);

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
        BSUInput.ResetWrite();
        wait(10);

        for (int t = 0; t < 5; t++) {
            BSU_IN_OUT_TYPE bsu_in;
            cout << "BSUInput: @ timestep: " << sc_time_stamp() << ": ";
            #pragma unroll
            for (int i = 0; i < SORT_NUM; i++) {
                bsu_in.x[i] = BSU_DATA_TYPE( (t+1)*SORT_NUM - i );   
                cout << (t+1)*SORT_NUM - i << " ";
            }
            cout << endl;
            BSUInput.Push(bsu_in);
        }
    }

    void collect() {
        BSUOutput.ResetRead();
        while (1) {
            wait(); // 1 cc

            for (int t = 0; t < 5; t++) {
                BSU_IN_OUT_TYPE tmp;
                tmp = BSUOutput.Pop();
                // compare with sample_color in vru_test.h
                cout << "BSUOutput: @ timestep: " << sc_time_stamp() << ": ";
                for (uint j = 0; j < SORT_NUM; j++) {
                    cout << tmp.x[j] << " ";
                }
                cout << endl;
            }

            sc_stop();
        }
    }
};

int sc_main(int argc, char *argv[]) {
    testbench tb("tb");
    sc_start();
    return 0;
}

//#include <>
