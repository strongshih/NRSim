#ifndef GSCORE_VRU_H
#define GSCORE_VRU_H

#include "GSCOREPackDef.h"
#include <ac_channel.h>
#include <nvhls_connections.h>
#include <ac_math/ac_hcordic.h>
#include <ac_math/ac_sigmoid_pwl.h>
#include <ac_math.h>
#include <ac_std_float.h>

#pragma hls_design block
class VRU : public match::Module {
    SC_HAS_PROCESS(VRU);
public:
    // Input/Output channels
    Connections::In<VRU_IN_TYPE> VRUInput;
    Connections::Out<VRU_OUT_TYPE> VRUOutput;

    // Stage 1 -> Stage 2
    Connections::Combinational<FP16_TYPE> alpha_out_to_step2;
    Connections::Combinational<RGB_TYPE> gaussian_color_to_step2;
    Connections::Combinational<bool> last_gaussian_to_step2;
    Connections::Combinational<ROTATE_INDEX_TYPE> rotate_idx_to_step2;

    // Stage 2 -> Stage 3
    Connections::Combinational<FP16_TYPE> alpha_out_to_step3;
    Connections::Combinational<FP16_TYPE> transmittance_to_step3;
    Connections::Combinational<RGB_TYPE> gaussian_color_to_step3;
    Connections::Combinational<bool> last_gaussian_to_step3;
    Connections::Combinational<ROTATE_INDEX_TYPE> rotate_idx_to_step3;
    // Constructor
    VRU(sc_module_name name) : match::Module(name),
                              VRUInput("VRUInput"),
                              VRUOutput("VRUOutput"),
                              alpha_out_to_step2("alpha_out_to_step2"),
                              gaussian_color_to_step2("gaussian_color_to_step2"),
                              last_gaussian_to_step2("last_gaussian_to_step2"),
                              rotate_idx_to_step2("rotate_idx_to_step2"),
                              alpha_out_to_step3("alpha_out_to_step3"),
                              transmittance_to_step3("transmittance_to_step3"),
                              gaussian_color_to_step3("gaussian_color_to_step3"),
                              last_gaussian_to_step3("last_gaussian_to_step3"),
                              rotate_idx_to_step3("rotate_idx_to_step3") {
        SC_THREAD(VRU_step1);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);

        SC_THREAD(VRU_step2);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);

        SC_THREAD(VRU_step3);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);  
    }

    /*
     * Input: Gaussian features (mean, covariance, color, opacity)
     * Output: RGB pixel color
     * Perform: Volume rendering based on alpha computation and blending
     */
    void VRU_step1() {
        VRUInput.Reset();
        alpha_out_to_step2.ResetWrite();
        gaussian_color_to_step2.ResetWrite();
        last_gaussian_to_step2.ResetWrite();
        rotate_idx_to_step2.ResetWrite();
        wait();


        #pragma hls_pipeline_init_interval 1
        while (1) {
            wait();
            
            // Get input Gaussian features
            VRU_IN_TYPE vru_input;
            if (VRUInput.PopNB(vru_input)) { 
                // Extract Gaussian features
                FP16_TYPE pixel_x = vru_input.pixel_pos_x;
                FP16_TYPE pixel_y = vru_input.pixel_pos_y;
                FP16_TYPE mean_x = vru_input.mean_x;
                FP16_TYPE mean_y = vru_input.mean_y;
                
                // Get 2x2 covariance matrix
                FP16_TYPE conx = vru_input.conx;
                FP16_TYPE cony = vru_input.cony;
                FP16_TYPE conz = vru_input.conz;
            
                // Get color and opacity
                RGB_TYPE gaussian_color = vru_input.color;
                FP16_TYPE opacity = vru_input.opacity;
                ROTATE_INDEX_TYPE rotate_idx = vru_input.rotate_idx;
                // Bitmap for subtile skipping (optional)
                // UINT8_TYPE bitmap = vru_input.bitmap;
                // UINT8_TYPE subtile_idx = vru_input.subtile_idx;

                // // Check bitmap to see if this subtile should be processed
                // bool skip_computation = ((bitmap & (1 << subtile_idx)) == 0);
                
                // // If subtile should be skipped, skip alpha computation
                // if (!skip_computation) {
                    // Stage 1: Alpha Computation & Pruning
                    // Compute alpha according to equation 2: α_i = o_i * exp(-0.5 * (p' - μ')^T Σ'^(-1) (p' - μ'))
                    
                    // Calculate (p' - μ')
                    FP16_TYPE diff_x = pixel_x - mean_x;
                    FP16_TYPE diff_y = pixel_y - mean_y;
                    
                    // Calculate (p' - μ')^T Σ'^(-1) (p' - μ')
                    FP16_TYPE exponent = FP16_TYPE(FP16_TYPE(-0.5) * (
                        diff_x * (conx * diff_x + cony * diff_y) +
                        diff_y * (cony * diff_x + conz * diff_y)
                    ));
                    
                    // Calculate alpha: α_i = o_i * exp(-0.5 * exponent)
                    FP16_TYPE alpha;
                    // ac_math::ac_exp_cordic(exponent, alpha);
                    ac_math::ac_exp_pwl(exponent, alpha);
                    alpha = opacity * alpha;
                    
                    // Alpha pruning: check if alpha is below threshold (1/255)
                    if (alpha >= FP16_TYPE(1.0/255.0)) {
                        alpha_out_to_step2.PushNB(alpha);
                        gaussian_color_to_step2.PushNB(gaussian_color);
                        last_gaussian_to_step2.PushNB(vru_input.last_gaussian);
                        rotate_idx_to_step2.PushNB(rotate_idx);
                    }
            }
        }
    }

    #pragma hls_pipeline_init_interval 1
    void VRU_step2() {
        alpha_out_to_step2.ResetRead();
        gaussian_color_to_step2.ResetRead();
        last_gaussian_to_step2.ResetRead();
        rotate_idx_to_step2.ResetRead();
        gaussian_color_to_step3.ResetWrite();
        transmittance_to_step3.ResetWrite();
        last_gaussian_to_step3.ResetWrite();
        alpha_out_to_step3.ResetWrite();
        rotate_idx_to_step3.ResetWrite();
        
        // Initialize per-pixel transmittance and color
        FP16_TYPE transmittance[NUM_ROTATE];
        #pragma hls_unroll
        for (int i = 0; i < NUM_ROTATE; i++) {
            transmittance[i] = FP16_TYPE(1.0);
        }

        wait();
        
        #pragma hls_pipeline_init_interval 1
        while (1) {
            wait();

            FP16_TYPE alpha;
            RGB_TYPE gaussian_color;
            bool last_gaussian;
            ROTATE_INDEX_TYPE rotate_idx;

            bool alpha_valid = alpha_out_to_step2.PopNB(alpha);
            bool gaussian_color_valid = gaussian_color_to_step2.PopNB(gaussian_color);
            bool last_gaussian_valid = last_gaussian_to_step2.PopNB(last_gaussian);
            bool rotate_idx_valid = rotate_idx_to_step2.PopNB(rotate_idx);
            if (alpha_valid && gaussian_color_valid && last_gaussian_valid && rotate_idx_valid) {
                // Stage 2: Early Termination
                // Update transmittance according to equation 4: T_i+1 = T_i * (1 - α_i) = T_i - T_i * α_i
                FP16_TYPE temp = transmittance[rotate_idx];
                FP16_TYPE new_transmittance = FP16_TYPE(temp * (FP16_TYPE(1.0) - alpha));
            
               
                // Push to next stage
                gaussian_color_to_step3.PushNB(gaussian_color);
                transmittance_to_step3.PushNB(temp);
                last_gaussian_to_step3.PushNB(last_gaussian);
                alpha_out_to_step3.PushNB(alpha);
                rotate_idx_to_step3.PushNB(rotate_idx);
                 // Update transmittance
                transmittance[rotate_idx] = new_transmittance;

                if (last_gaussian || temp < FP16_TYPE(0.0001)) {
                    transmittance[rotate_idx] = FP16_TYPE(1.0);
                }
            }
        }
    }

    #pragma hls_pipeline_init_interval 1
    void VRU_step3() {
        gaussian_color_to_step3.ResetRead();
        transmittance_to_step3.ResetRead();
        last_gaussian_to_step3.ResetRead();
        alpha_out_to_step3.ResetRead();
        rotate_idx_to_step3.ResetRead();
        VRUOutput.Reset();
        
        RGB_TYPE accumulated_color[NUM_ROTATE];
        #pragma hls_unroll
        for (int i = 0; i < NUM_ROTATE; i++) {
            accumulated_color[i].r = FP16_TYPE(0.0);
            accumulated_color[i].g = FP16_TYPE(0.0);
            accumulated_color[i].b = FP16_TYPE(0.0);
        }
        
        wait();

        #pragma hls_pipeline_init_interval 1
        while (1) {
            wait();

            FP16_TYPE transmittance;
            RGB_TYPE gaussian_color;
            bool last_gaussian;
            FP16_TYPE alpha;
            ROTATE_INDEX_TYPE rotate_idx;
            
            bool gaussian_color_valid = gaussian_color_to_step3.PopNB(gaussian_color);
            bool transmittance_valid = transmittance_to_step3.PopNB(transmittance);
            bool last_gaussian_valid = last_gaussian_to_step3.PopNB(last_gaussian);
            bool alpha_valid = alpha_out_to_step3.PopNB(alpha);
            bool rotate_idx_valid = rotate_idx_to_step3.PopNB(rotate_idx);
            if (gaussian_color_valid && transmittance_valid && last_gaussian_valid && alpha_valid && rotate_idx_valid) {
                // Stage 3: Volume Rendering
                // Accumulate color: C += T_i * α_i * c_i
                FP16_TYPE temp = transmittance * alpha;
                accumulated_color[rotate_idx].r += temp * gaussian_color.r;
                accumulated_color[rotate_idx].g += temp * gaussian_color.g;
                accumulated_color[rotate_idx].b += temp * gaussian_color.b;
                        
                // Early termination or finished processing all Gaussians
                if (last_gaussian || transmittance < FP16_TYPE(0.0001)) {
                     // Initialize output
                    VRU_OUT_TYPE vru_output;
                    
                    // Set the final accumulated color as output
                    vru_output.color = accumulated_color[rotate_idx];
                    
                    // Reset transmittance and accumulated color for next pixel
                    accumulated_color[rotate_idx].r = FP16_TYPE(0.0);
                    accumulated_color[rotate_idx].g = FP16_TYPE(0.0);
                    accumulated_color[rotate_idx].b = FP16_TYPE(0.0);
                    
                    // Push output to channel
                    VRUOutput.PushNB(vru_output);
                }
            }
        }
    }
};

#endif