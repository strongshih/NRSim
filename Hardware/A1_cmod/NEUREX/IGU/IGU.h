#ifndef NEUREX_IGU_H
#define NEUREX_IGU_H

#include "NEUREXPackDef.h"
#include <nvhls_int.h>
#include <nvhls_connections.h>
#include <ac_std_float.h>
#include <ac_math/ac_abs.h>

// position scaling, hash index computation, and interpolation weight computation.
class IGU : public match::Module {
    SC_HAS_PROCESS(IGU);
public:

    Connections::In<IGU_Grid_Res> sugbrid_res;
    Connections::In<IGU_In_Type> pos;
    Connections::Out<IGU_Grid_Res> grid_id;
    Connections::Out<Hashed_addr> hashed_addr;
    Connections::Out<IGU_Weight> weight;

    IGU(sc_module_name name) : match::Module(name),
                               sugbrid_res("sugbrid_res"),
                               pos("pos"),
                               grid_id("grid_id"),
                               hashed_addr("hashed_addr"),
                               weight("weight") {
        SC_THREAD(start);
        sensitive << clk.pos();
        async_reset_signal_is(rst, false);
    }

    IGU_Grid_Res to_add[8][3] = {{0,0,0},
                                 {0,0,1},
                                 {0,1,0},
                                 {0,1,1},
                                 {1,0,0},
                                 {1,0,1},
                                 {1,1,0},
                                 {1,1,1}};

    void start() {
        sugbrid_res.Reset();
        pos.Reset();
        grid_id.Reset();
        hashed_addr.Reset();
        weight.Reset();
        wait();

        Hashed_addr ret_addr;
        IGU_Weight w_addr;

        #pragma hls_pipeline_init_interval 1
        while (1) {
            wait();

            IGU_In_Type pos_tmp;
            IGU_Grid_Res sugbrid_res_tmp;
            sugbrid_res.PopNB(sugbrid_res_tmp); // get resolution

            IGU_In_Elem_Type pos_after_mul[3];
            IGU_In_Elem_Type pos_lower_int[3];
            IGU_In_Elem_Type pos_fraction[3];

            int to_hash[8][3];
            if (pos.PopNB(pos_tmp)) {
                #pragma hls_unroll
                for (int i = 0; i < 3; i++) {
                     pos_after_mul[i] = pos_tmp.x[i] * IGU_In_Elem_Type(sugbrid_res_tmp);
                     pos_lower_int[i] = pos_after_mul[i];
                     pos_fraction[i] = pos_after_mul[i] - pos_lower_int[i];
                }
                // Get Grid ID
                IGU_In_Elem_Type mul_tmp = pos_lower_int[0] + (pos_lower_int[1] * IGU_In_Elem_Type(sugbrid_res_tmp)) + (pos_lower_int[2] * IGU_In_Elem_Type(sugbrid_res_tmp+sugbrid_res_tmp));
                IGU_Grid_Res tmp = IGU_Grid_Res(mul_tmp.to_int()); 

                // Get pos
                #pragma hls_unroll
                for (int idx = 0; idx < 8; idx ++) {
                    #pragma hls_unroll
                    for (int i = 0; i < 3; i++) {
                        to_hash[idx][i] = pos_lower_int[i].to_int() + to_add[idx][i];
                    }
                }

                // Start hashing
                #pragma hls_unroll
                for (int i = 0; i < 8; i++) {
                    ret_addr.x[i] = (to_hash[i][0] ^ (to_hash[i][1] * P1) ^ (to_hash[i][2] * P2)) & (IGU_Grid_Res((1<<19) - 1)); 
                }
                 

                // Generate Weight
                #pragma hls_unroll
                for (int i = 0; i < 8; i++) {
                    IGU_In_Elem_Type abs_val0 = pos_fraction[0]-IGU_In_Elem_Type(1); 
                    IGU_In_Elem_Type abs_val1 = pos_fraction[1]-IGU_In_Elem_Type(1);
                    IGU_In_Elem_Type abs_val2 = pos_fraction[2]-IGU_In_Elem_Type(1);
                    if (abs_val0 < IGU_In_Elem_Type(0)) abs_val0 = -abs_val0;
                    if (abs_val1 < IGU_In_Elem_Type(0)) abs_val1 = -abs_val1;
                    if (abs_val2 < IGU_In_Elem_Type(0)) abs_val2 = -abs_val2;
                    w_addr.x[i] = (IGU_In_Elem_Type(1) - abs_val0) * 
                                  (IGU_In_Elem_Type(1) - abs_val1) * 
                                  (IGU_In_Elem_Type(1) - abs_val2);
                }

                hashed_addr.Push(ret_addr);
                weight.Push(w_addr);
            }
        }
    }

    // Hash : (xv · 1) ⊕ (yv · P1) ⊕ (zv · P2) mod T
};

#endif //NEUREX_IGU_H
