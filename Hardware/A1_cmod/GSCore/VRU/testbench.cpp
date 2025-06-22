#define NVHLS_VERIFY_BLOCKS (VRU)
#include "VRU.h"
#include <nvhls_verify.h>
#include "nvhls_connections.h"
#include "ac_sysc_trace.h"
#include <random>
#include <systemc.h>
#include <nvhls_module.h>
#include <mc_connections.h>
#include <cmath>
#include <iomanip>

#pragma hls_design top
class testbench : public sc_module {
public:
    sc_clock clk;
    sc_signal<bool> rst;

    Connections::Combinational<VRU_IN_TYPE> VRUInput;
    Connections::Combinational<VRU_OUT_TYPE> VRUOutput;

    NVHLS_DESIGN(VRU) dut;

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

    // Helper function to compute expected alpha value (for verification)
    double compute_expected_alpha(double pixel_x, double pixel_y, 
                                double mean_x, double mean_y,
                                double conx, double cony, double conz,
                                double opacity) {
        // Compute the exponent
        double diff_x = pixel_x - mean_x;
        double diff_y = pixel_y - mean_y;
        
        double exponent = -0.5 * (
            diff_x * (conx * diff_x + cony * diff_y) +
            diff_y * (cony * diff_x + conz * diff_y)
        );
        
        // Compute alpha
        double alpha = opacity * exp(exponent);
        return alpha;
    }
    
    // Helper function to compute expected color contributions
    void compute_expected_color(double transmittance, double alpha, 
                               double color_r, double color_g, double color_b,
                               double &accum_r, double &accum_g, double &accum_b) {
        accum_r += transmittance * alpha * color_r;
        accum_g += transmittance * alpha * color_g;
        accum_b += transmittance * alpha * color_b;
    }

    // Helper function to create a Gaussian test case
    void createGaussian(VRU_IN_TYPE &gaussian, 
                       double x_val, double y_val,      // Pixel position
                       double mean_x_val, double mean_y_val, // Mean
                       double conx_val, double cony_val, double conz_val, // Inverse covariance
                       double color_r, double color_g, double color_b,
                       double opacity_val,
                       bool last_gaussian) {
        
        // Set pixel position
        gaussian.pixel_pos_x = FP16_TYPE(x_val);
        gaussian.pixel_pos_y = FP16_TYPE(y_val);
        
        // Set mean
        gaussian.mean_x = FP16_TYPE(mean_x_val);
        gaussian.mean_y = FP16_TYPE(mean_y_val);
        
        // Set inverse covariance components
        gaussian.conx = FP16_TYPE(conx_val);
        gaussian.cony = FP16_TYPE(cony_val);
        gaussian.conz = FP16_TYPE(conz_val);
        
        // Set color and opacity
        gaussian.color.r = FP16_TYPE(color_r);
        gaussian.color.g = FP16_TYPE(color_g);
        gaussian.color.b = FP16_TYPE(color_b);
        gaussian.opacity = FP16_TYPE(opacity_val);
        
        // Set last_gaussian flag
        gaussian.last_gaussian = last_gaussian;
        gaussian.rotate_idx = 0;
    }

    void run() {
        VRUInput.ResetWrite();
        wait(10);

        // Test Case 1: Simple Gaussian - pixel at center
        cout << "\n=== Test Case 1: Simple Gaussian at Center ===" << endl;
        
        // Test parameters
        double pixel_x = 10.0, pixel_y = 10.0;         // Pixel at center of Gaussian
        double mean_x = 10.0, mean_y = 10.0;           // Mean at the same position
        double conx = 0.5, cony = 0.0, conz = 0.5;     // Simple diagonal inverse covariance
        double color_r = 1.0, color_g = 0.0, color_b = 0.0;  // Red color
        double opacity = 0.8;                          // High opacity
        
        // Create and send Gaussian
        VRU_IN_TYPE gaussian1;
        createGaussian(gaussian1, 
                     pixel_x, pixel_y,
                     mean_x, mean_y,
                     conx, cony, conz,
                     color_r, color_g, color_b,
                     opacity,
                     true);          // Last Gaussian
        
        // Calculate expected alpha and color
        double expected_alpha = compute_expected_alpha(
            pixel_x, pixel_y, mean_x, mean_y, conx, cony, conz, opacity
        );
        double expected_r = 0.0, expected_g = 0.0, expected_b = 0.0;
        compute_expected_color(1.0, expected_alpha, color_r, color_g, color_b, 
                             expected_r, expected_g, expected_b);
        
        cout << "Expected Alpha: " << expected_alpha << endl;
        cout << "Expected Color: (" << expected_r << ", " << expected_g << ", " << expected_b << ")" << endl;
        
        VRUInput.Push(gaussian1);
        wait(5);
        
        
        // Test Case 2: Multiple Gaussians (front-to-back)
        cout << "\n=== Test Case 2: Multiple Gaussians ===" << endl;
        
        // First Gaussian (red, closest to camera)
        VRU_IN_TYPE gaussian2_1;
        double px2 = 20.0, py2 = 20.0;
        double mx2_1 = 20.0, my2_1 = 20.0;
        double conx2_1 = 0.5, cony2_1 = 0.0, conz2_1 = 0.5;
        double r2_1 = 1.0, g2_1 = 0.0, b2_1 = 0.0;  // Red
        double o2_1 = 0.4;
        
        createGaussian(gaussian2_1, 
                      px2, py2, mx2_1, my2_1,
                      conx2_1, cony2_1, conz2_1,
                      r2_1, g2_1, b2_1, o2_1,
                      false);         // Not Last Gaussian
        
        // Second Gaussian (green)
        VRU_IN_TYPE gaussian2_2;
        double mx2_2 = 20.0, my2_2 = 20.0;
        double conx2_2 = 0.5, cony2_2 = 0.0, conz2_2 = 0.5;
        double r2_2 = 0.0, g2_2 = 1.0, b2_2 = 0.0;  // Green
        double o2_2 = 0.3;
        
        createGaussian(gaussian2_2, 
                      px2, py2, mx2_2, my2_2,
                      conx2_2, cony2_2, conz2_2,
                      r2_2, g2_2, b2_2, o2_2,
                      false);         // Not Last Gaussian
        
        // Third Gaussian (blue)
        VRU_IN_TYPE gaussian2_3;
        double mx2_3 = 20.0, my2_3 = 20.0;
        double conx2_3 = 0.5, cony2_3 = 0.0, conz2_3 = 0.5;
        double r2_3 = 0.0, g2_3 = 0.0, b2_3 = 1.0;  // Blue
        double o2_3 = 0.2;
        
        createGaussian(gaussian2_3, 
                      px2, py2, mx2_3, my2_3,
                      conx2_3, cony2_3, conz2_3,
                      r2_3, g2_3, b2_3, o2_3,
                      true);          // Last Gaussian
        
        // Calculate expected results for multiple Gaussians
        double alpha2_1 = compute_expected_alpha(
            px2, py2, mx2_1, my2_1, conx2_1, cony2_1, conz2_1, o2_1
        );
        double transmittance2_1 = 1.0;
        double transmittance2_2 = transmittance2_1 * (1.0 - alpha2_1);
        
        double alpha2_2 = compute_expected_alpha(
            px2, py2, mx2_2, my2_2, conx2_2, cony2_2, conz2_2, o2_2
        );
        double transmittance2_3 = transmittance2_2 * (1.0 - alpha2_2);
        
        double alpha2_3 = compute_expected_alpha(
            px2, py2, mx2_3, my2_3, conx2_3, cony2_3, conz2_3, o2_3
        );
        
        double expected_r2 = 0.0, expected_g2 = 0.0, expected_b2 = 0.0;
        compute_expected_color(transmittance2_1, alpha2_1, r2_1, g2_1, b2_1, 
                             expected_r2, expected_g2, expected_b2);
        compute_expected_color(transmittance2_2, alpha2_2, r2_2, g2_2, b2_2, 
                             expected_r2, expected_g2, expected_b2);
        compute_expected_color(transmittance2_3, alpha2_3, r2_3, g2_3, b2_3, 
                             expected_r2, expected_g2, expected_b2);
        
        cout << "Expected Alphas: " << alpha2_1 << ", " << alpha2_2 << ", " << alpha2_3 << endl;
        cout << "Expected Transmittances: " << transmittance2_1 << ", " 
                                           << transmittance2_2 << ", " 
                                           << transmittance2_3 << endl;
        cout << "Expected Color: (" << expected_r2 << ", " << expected_g2 << ", " << expected_b2 << ")" << endl;
        
        VRUInput.Push(gaussian2_1);
        wait(1);
        VRUInput.Push(gaussian2_2);
        wait(1);
        VRUInput.Push(gaussian2_3);
        wait(5);
        

        // Test Case 3: Early Termination
        cout << "\n=== Test Case 3: Early Termination ===" << endl;
        
        // First Gaussian with very high opacity (should trigger early termination)
        VRU_IN_TYPE gaussian3_1;
        double px3 = 30.0, py3 = 30.0;
        double mx3_1 = 30.0, my3_1 = 30.0;
        double conx3_1 = 0.5, cony3_1 = 0.0, conz3_1 = 0.5;
        double r3_1 = 1.0, g3_1 = 1.0, b3_1 = 0.0;  // Yellow
        double o3_1 = 0.95;
        
        createGaussian(gaussian3_1, 
                      px3, py3, mx3_1, my3_1,
                      conx3_1, cony3_1, conz3_1,
                      r3_1, g3_1, b3_1, o3_1,
                      false);         // Not Last Gaussian
        
        // Second Gaussian should have minimal contribution due to early termination
        VRU_IN_TYPE gaussian3_2;
        double mx3_2 = 30.0, my3_2 = 30.0;
        double conx3_2 = 0.5, cony3_2 = 0.0, conz3_2 = 0.5;
        double r3_2 = 0.0, g3_2 = 1.0, b3_2 = 1.0;  // Cyan
        double o3_2 = 0.8;
        
        createGaussian(gaussian3_2, 
                      px3, py3, mx3_2, my3_2,
                      conx3_2, cony3_2, conz3_2,
                      r3_2, g3_2, b3_2, o3_2,
                      true);          // Last Gaussian
        
        // Calculate expected results for early termination
        double alpha3_1 = compute_expected_alpha(
            px3, py3, mx3_1, my3_1, conx3_1, cony3_1, conz3_1, o3_1
        );
        double transmittance3_1 = 1.0;
        double transmittance3_2 = transmittance3_1 * (1.0 - alpha3_1);
        
        // We'll calculate the second Gaussian's contribution, though 
        // it might get dropped due to early termination
        double alpha3_2 = compute_expected_alpha(
            px3, py3, mx3_2, my3_2, conx3_2, cony3_2, conz3_2, o3_2
        );
        
        double expected_r3 = 0.0, expected_g3 = 0.0, expected_b3 = 0.0;
        compute_expected_color(transmittance3_1, alpha3_1, r3_1, g3_1, b3_1, 
                             expected_r3, expected_g3, expected_b3);
        
        // Check if early termination should occur
        bool early_term = (transmittance3_2 < 0.0001);
        cout << "Transmittance after first Gaussian: " << transmittance3_2 << endl;
        cout << "Should early terminate? " << (early_term ? "Yes" : "No") << endl;
        
        if (!early_term) {
            compute_expected_color(transmittance3_2, alpha3_2, r3_2, g3_2, b3_2, 
                                 expected_r3, expected_g3, expected_b3);
        }
        
        cout << "Expected Color: (" << expected_r3 << ", " << expected_g3 << ", " << expected_b3 << ")" << endl;
        
        VRUInput.Push(gaussian3_1);
        wait(1);
        VRUInput.Push(gaussian3_2);
        wait(5);
    }

    void collect() {
        VRUOutput.ResetRead();
        wait(10);  // Wait for reset and initialization

        int count = 0;
        while (1) {
            VRU_OUT_TYPE result;
            if (VRUOutput.PopNB(result)) {
                cout << "VRU Output @ " << sc_time_stamp() << " : ";
                cout << "  Actual Color: (" << std::setprecision(6) << result.color.r.to_double() << ", " 
                                        << std::setprecision(6) << result.color.g.to_double() << ", " 
                                        << std::setprecision(6) << result.color.b.to_double() << ")" << endl;
                count++;
            }
            
            if (count >= 3) {
                break;
            }
            wait();
        }
        sc_stop();
    }
};

int sc_main(int argc, char *argv[]) {
    testbench tb("tb");
    sc_start();
    return 0;
}