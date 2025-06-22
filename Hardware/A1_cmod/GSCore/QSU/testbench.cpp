#define NVHLS_VERIFY_BLOCKS (QSU)
#include "QSU.h"
#include <nvhls_verify.h>
#include "nvhls_connections.h"
#include "ac_sysc_trace.h"
#include <random>
#include <systemc.h>
#include <nvhls_module.h>
#include <mc_connections.h>

#pragma hls_design top
class testbench : public sc_module {
public:
    sc_clock clk;
    sc_signal<bool> rst;

    Connections::Combinational<QSU_IN_TYPE> QSUInput;
    Connections::Combinational<QSU_OUT_TYPE> QSUOutput;

    NVHLS_DESIGN(QSU) dut;

    SC_CTOR(testbench) : clk("clk", 1, SC_NS, 0.5, 0, SC_NS, true),
                   rst("rst"),
                   QSUInput("QSUInput"),
                   QSUOutput("QSUOutput"),
                   dut("dut") {

        sc_object_tracer<sc_clock> trace_clk(clk);

        dut.clk(clk);
        dut.rst(rst);
        dut.QSUInput(QSUInput);
        dut.QSUOutput(QSUOutput);

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
        QSUInput.ResetWrite();
        wait(10);

        // Setup test cases with different depth values to test different subsets
        // Test cases will include values below, between, and above the pivot values
        const int NUM_TESTS = 10;
        FP16_TYPE test_depths[NUM_TESTS] = {
            FP16_TYPE(10.0),  // Below Pivot1 (20) -> Subset 0
            FP16_TYPE(20.0),  // Equal to Pivot1 (20) -> Subset 1
            FP16_TYPE(30.0),  // Between Pivot1 (20) and Pivot2 (40) -> Subset 1
            FP16_TYPE(40.0),  // Equal to Pivot2 (40) -> Subset 2
            FP16_TYPE(50.0),  // Above Pivot2 (40) -> Subset 2
            FP16_TYPE(60.0),  // Higher value -> Subset depending on other pivots
            FP16_TYPE(70.0),  // Higher value
            FP16_TYPE(80.0),  // Higher value
            FP16_TYPE(90.0),  // Higher value
            FP16_TYPE(100.0)  // Higher value
        };

        // Send each test case to the QSU
        for (int t = 0; t < NUM_TESTS; t++) {
            QSU_IN_TYPE qsu_in;
            qsu_in.depth = test_depths[t];
            qsu_in.gid = t + 1000;  // GID starting from 1000
            
            cout << "QSUInput @ timestep: " << sc_time_stamp() << ": ";
            cout << "Depth = " << qsu_in.depth << ", GID = " << qsu_in.gid << endl;
            
            QSUInput.Push(qsu_in);
            wait(1);  // Wait one clock cycle between inputs
        }
    }

    void collect() {
        QSUOutput.ResetRead();
        wait(20);  // Wait for reset and initialization
        
        // Define expected pivot values (same as in the QSU implementation)
        FP16_TYPE pivots[NUM_PIVOTS];
        pivots[0] = FP16_TYPE(20.0); // Pivot1
        pivots[1] = FP16_TYPE(40.0); // Pivot2
        // Initialize other pivots
        for (int i = 2; i < NUM_PIVOTS; i++) {
            pivots[i] = FP16_TYPE(20.0 * (i + 1)); // Incrementing by 20
        }
        
        cout << "Expected pivot values: ";
        for (int i = 0; i < NUM_PIVOTS; i++) {
            cout << pivots[i] << " ";
        }
        cout << endl;
        
        const int NUM_TESTS = 10;
        for (int t = 0; t < NUM_TESTS; t++) {
            QSU_OUT_TYPE result;
            result = QSUOutput.Pop();
            
            cout << "QSUOutput @ timestep: " << sc_time_stamp() << ": ";
            cout << "GID = " << result.gid << ", Subset = " << result.subset << endl;
            
            // Calculate expected subset for verification
            // Note: This logic should match the QSU implementation
            FP16_TYPE expected_depth = FP16_TYPE(10.0 + t * 10.0); // Matches the test depths
            uint8_t expected_subset = 0;
            
            for (int i = 0; i < NUM_PIVOTS; i++) {
                if (expected_depth >= pivots[i]) {
                    expected_subset = i + 1;
                }
            }
            
            cout << "  Expected Subset: " << (int)expected_subset;
            if (result.subset == expected_subset) {
                cout << " ✓" << endl;
            } else {
                cout << " ✗ (MISMATCH)" << endl;
            }
        }
        
        sc_stop();
    }
};

int sc_main(int argc, char *argv[]) {
    testbench tb("tb");
    sc_start();
    return 0;
}