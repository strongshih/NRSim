#define NVHLS_VERIFY_BLOCKS (VRU)
#include "VRU.h"
#include <nvhls_verify.h>
//#include <mc_scverify.h>
#include "nvhls_connections.h"
#include "ac_sysc_trace.h"
#include <random>
//#include <ac_channel.h>
#include <systemc.h>
#include <nvhls_module.h>
#include <mc_connections.h>
//#include "vru_test.h"

#define SAMPLE_NUM 192

#pragma hls_design top
class testbench : public sc_module {
public:
    sc_clock clk;
    sc_signal<bool> rst;

    Connections::Combinational<VRU_In_Type> VRUInput;
    Connections::Combinational<VRU_Out_Type> VRUOutput;

    NVHLS_DESIGN(VRU) dut;
//    CCS_DESIGN(SDAcc) CCS_INIT_S1(dut);

    SC_CTOR(testbench) : clk("clk", 1, SC_NS, 0.5, 0, SC_NS, true),
                   rst("rst"),
                   VRUInput("VRUInput"),
                   VRUOutput("VRUOutput"),
                   dut("dut") {

        sc_object_tracer<sc_clock> trace_clk(clk);

        dut.clk(clk);
        dut.rst(rst);
        dut.VRUInput(VRUInput);
        dut.VRUOutput(VRUOutput);

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
        VRUInput.ResetWrite();
        wait(10);

        // Test vru accumulation (5 times)
        for (int t = 0; t < 5; t++) {
            for (int i = 0; i < SAMPLE_NUM; i++) {
                VRU_In_Type vru_in;
                vru_in.emitted_c[0] = VRU_C_Type(1);
                vru_in.emitted_c[1] = VRU_C_Type(2);
                vru_in.emitted_c[2] = VRU_C_Type(3);
                vru_in.sigma        = VRU_Sigma_Type(4);
                vru_in.delta        = VRU_Delta_Type(5);
                vru_in.isLastSample = ((i%SAMPLE_NUM) == SAMPLE_NUM-1);
                VRUInput.Push(vru_in);
            }
        }
    }

    void collect() {
        VRUOutput.ResetRead();
        while (1) {
            wait(); // 1 cc

            for (int i = 0; i < 5; i++) {
                VRU_Out_Type tmp;
                tmp = VRUOutput.Pop();
                // compare with sample_color in vru_test.h
                cout << "VRUOutput: @ timestep: " << sc_time_stamp() << endl;
                for (uint j = 0; j < 3; j++) {
                    cout << tmp.c[j] << " ";
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
