#define NVHLS_VERIFY_BLOCKS (NPU)
#include "NPU.h"
#include <nvhls_verify.h>
//#include <mc_scverify.h>
#include "nvhls_connections.h"
#include "ac_sysc_trace.h"
#include <random>
//#include <ac_channel.h>
#include <systemc.h>
#include <nvhls_module.h>
#include <mc_connections.h>


class Top : public sc_module {
public:
    sc_clock clk;
    sc_signal<bool> rst;

    Connections::Combinational<NPU_W_Type>   w_in;
    Connections::Combinational<NPU_In_Type>  act_in;
    Connections::Combinational<NPU_Out_Type> psum_out;

    NVHLS_DESIGN(NPU) dut;

    SC_CTOR(Top) : clk("clk", 1, SC_NS, 0.5, 0, SC_NS, true),
                   rst("rst"),
                   w_in("w_in"),
                   act_in("act_in"),
                   psum_out("psum_out"),
                   dut("dut") {

        sc_object_tracer<sc_clock> trace_clk(clk);

        dut.clk(clk);
        dut.rst(rst);
        dut.w_in(w_in);
        dut.act_in(act_in);
        dut.psum_out(psum_out);

        SC_THREAD(stream);
        sensitive << clk.posedge_event();

        SC_THREAD(run);
        sensitive << clk.posedge_event();
        async_reset_signal_is(rst, false);

        SC_THREAD(collect);
        sensitive << clk.posedge_event();
        async_reset_signal_is(rst, false);
    }

    void stream() {
        rst.write(false);
        wait(10);
        rst.write(true);
    }

    void run() {
        w_in.ResetWrite();
        act_in.ResetWrite();
        wait(10);

        // Phase 1: Load weights by streaming from last row to first row
        cout << "======= Phase 1: Loading Weights - Streaming from Last Row to First Row =======" << endl;
        
        // In the new approach, we stream weights from bottom (row NPU_SIZE-1) to top (row 0)
        // This way, each PE will automatically store its appropriate weight
        for (int row = NPU_SIZE - 1; row >= 0; row--) {
            // Create weight row - each value represents a weight for a specific column
            NPU_W_Type w_data;
            for (int j = 0; j < NPU_SIZE; j++) {
                // Each column j will get the value (row + 1)
                w_data.X[j] = NPU_W_Elem_Type(row*NPU_SIZE + j + 1);
            }
            
            // Push weights
            w_in.Push(w_data);
            
            cout << "Streaming weights @ " << sc_time_stamp() << " for row " << row << ", weight values: ";
            for (int j = 0; j < NPU_SIZE; j++) {
                // For proper floating-point display, convert to double
                double display_val = w_data.X[j].to_double();
                cout << display_val << " ";
            }
            cout << endl;
            
            wait();
        }
        
        // Add a small wait after all weights are loaded
        cout << "All weights loaded, waiting for stabilization..." << endl;
        wait(NPU_SIZE+1);
        
        // Phase 2: Stream activations for matrix multiplication
        cout << "======= Phase 2: Streaming Activations =======" << endl;
        for (int t = 0; t < NPU_SIZE * 2; t++) {
            // Create activation row - shift pattern to have systolic behavior
            NPU_In_Type act_data;
            for (int i = 0; i < NPU_SIZE; i++) {
                if (t - i >= 0 && t - i < NPU_SIZE) {
                    act_data.X[i] = NPU_In_Elem_Type(t - i + 1); // Value depends on timestep
                } else {
                    act_data.X[i] = NPU_In_Elem_Type(0); // Zero padding
                }
            }
            
            // Push activations
            act_in.Push(act_data);
            
            cout << "Activations at time " << t << ": ";
            for (int i = 0; i < NPU_SIZE; i++) {
                // For proper floating-point display, convert to double
                double display_val = act_data.X[i].to_double();
                cout << display_val << " ";
            }
            cout << endl;
            
            wait();
        }
        
        // Let computation complete
        wait(NPU_SIZE * 2);
    }

    void collect() {
        psum_out.ResetRead();
        wait(10);  // Wait for reset

        int count = 0;
        while (1) {
            NPU_Out_Type result;
            if (psum_out.PopNB(result)) {
                cout << "NPU Output @ " << sc_time_stamp() << " : ";
                for (int i = 0; i < NPU_SIZE; i++) {
                    cout << result.X[i] << " ";
                }
                cout << endl; 
            }
            count++;
            if (count > NPU_SIZE * 10) {
                break;
            }
            wait();
        }
        sc_stop();
    }
};

int sc_main(int argc, char *argv[]) {
    Top tb("tb");
    sc_start();
    return 0;
}

//#include <>



