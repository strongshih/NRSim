#ifndef GSCORE_QSU_H
#define GSCORE_QSU_H

#include "GSCOREPackDef.h"
#include <ac_channel.h>
#include <nvhls_connections.h>
#include <ac_math/ac_hcordic.h>
#include <ac_math/ac_sigmoid_pwl.h>
#include <ac_std_float.h>

#pragma hls_design block
class QSU : public match::Module {
    SC_HAS_PROCESS(QSU);
public:
    // Input/Output channels
    Connections::In<QSU_IN_TYPE> QSUInput;
    Connections::Out<QSU_OUT_TYPE> QSUOutput;

    QSU(sc_module_name name) : match::Module(name),
                               QSUInput("QSUInput"),
                               QSUOutput("QSUOutput") {
        SC_THREAD(QSU_CALC);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);
    }

    // Store the pivot values
    FP16_TYPE pivots[NUM_PIVOTS];

    // Function to set pivot values
    // void setPivots(FP16_TYPE* pivot_values, int num_values) {
    //     for (int i = 0; i < NUM_PIVOTS && i < num_values; i++) {
    //         pivots[i] = pivot_values[i];
    //     }
    // }

    /*
     * Input: (Depth: FP16, GID: UINT16)
     * Output: Subset index for the key
     * Perform: Compare key against pivots to determine subset
     */
    void QSU_CALC() {
        QSUInput.Reset();
        QSUOutput.Reset();
        // Initialize default pivot values
        // These can be overridden by calling setPivots()
        pivots[0] = FP16_TYPE(20.0); // Example: Pivot1
        pivots[1] = FP16_TYPE(40.0); // Example: Pivot2
        // Initialize other pivots as needed
        #pragma hls_unroll
        for (int i = 2; i < NUM_PIVOTS; i++) {
            pivots[i] = pivots[i-1] + FP16_TYPE(20.0); // Example: incrementing by 20
        }
        wait();

        #pragma hls_pipeline_init_interval 1
        while (1) {
            wait();
            
            // Get input key-value pair
            QSU_IN_TYPE qsu_input;
            if(QSUInput.PopNB(qsu_input)) {
                FP16_TYPE depth = qsu_input.depth;
                UINT16_TYPE gid = qsu_input.gid;
                
                // Initialize output
                QSU_OUT_TYPE qsu_output;
                qsu_output.gid = gid;
                
                // Generate a bit vector for all pivot comparisons
                uint8_t comparison_bits = 0;
                
                // Compare depth against each pivot
                // Set the corresponding bit to 1 if depth < pivot
                #pragma hls_unroll
                for (int i = 0; i < NUM_PIVOTS; i++) {
                    if (depth >= pivots[i]) {
                        // Set the corresponding bit
                        comparison_bits |= (1 << i);
                    }
                }
                
                // Perform popcount (count the number of set bits)
                uint8_t popcount = 0;
                #pragma hls_unroll
                for (int i = 0; i < NUM_PIVOTS; i++) {
                    if (comparison_bits & (1 << i)) {
                        popcount++;
                    }
                }
            
                // The popcount determines the subset
                qsu_output.subset = popcount;
            
                // Store the key-value pair in the appropriate subset
                // This logic depends on how you want to handle the storage
                // For now, we're just determining the subset
                
                // Push the result to output
                QSUOutput.Push(qsu_output);
            }
        }
    }

};

#endif //GSCORE_BSU_H
