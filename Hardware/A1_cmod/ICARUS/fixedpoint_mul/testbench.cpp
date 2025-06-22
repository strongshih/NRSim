#define NVHLS_VERIFY_BLOCKS (fixedpoint_mul)
#include "fixedpoint_mul.h"
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

    Connections::Combinational<MUL_In_Type> MULInput;
    Connections::Combinational<MUL_In_Type> MULWeight;
    Connections::Combinational<MUL_In_Type> MULOutput;

    NVHLS_DESIGN(fixedpoint_mul) dut;
//    CCS_DESIGN(SDAcc) CCS_INIT_S1(dut);

    SC_CTOR(Top) : clk("clk", 1, SC_NS, 0.5, 0, SC_NS, true),
                   rst("rst"),
                   MULInput("MULInput"),
                   MULWeight("MULWeight"),
                   MULOutput("MULOutput"),
                   dut("dut") {

        sc_object_tracer<sc_clock> trace_clk(clk);

        dut.clk(clk);
        dut.rst(rst);
        dut.MULInput(MULInput);
        dut.MULWeight(MULWeight);
        dut.MULOutput(MULOutput);

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
        MULInput.ResetWrite();
        MULWeight.ResetWrite();
        wait(10);

// Test fixed point
#if SCALE == 1
        // Test all cases
        for (ac_int<total_bit+1, false> i = 0; i < (1<<total_bit); i++) {     // +1 since range end 
            for (ac_int<total_bit+1, false> j = 0; j < (1<<total_bit); j++) { // cannot be covered
                MUL_In_Type a, b;
                a.set_slc(0, i.slc<total_bit>(0)); // bit copy
                b.set_slc(0, j.slc<total_bit>(0)); // bit copy
                MULInput.Push(a);
                MULWeight.Push(b);
            }
        }
// Test integer
#else
        // Test all cases
        for (int i = -(SCALE); i < (SCALE); i++) {     // +1 since range end 
            for (int j = -(SCALE); j < (SCALE); j++) { // cannot be covered
                MUL_In_Type a = MUL_In_Type(i); 
                MUL_In_Type b = MUL_In_Type(j);
                MULInput.Push(a);
                MULWeight.Push(b);
            }
        }
#endif
    }

    void collect() {
        MULOutput.ResetRead();
        while (1) {
            wait(); // 1 cc
            
            ac_std_float<32, 8> diff;
            float acc = 0;
// Test fixed point + floating point
#if SCALE == 1
            for (ac_int<total_bit+1, false> i = 0; i < (1<<total_bit); i++) {
                for (ac_int<total_bit+1, false> j = 0; j < (1<<total_bit); j++) {
                    MUL_In_Type a, b;
                    a.set_slc(0, i.slc<total_bit>(0));            // Bit assignment
                    b.set_slc(0, j.slc<total_bit>(0));            // Bit assignment
                    ac_std_float<32, 8> tmp;
                    if (INTEGER_WIDTH == BIT_PER_PART*MUL_PART) { // Use integer, so should deal with scale
                        tmp = ac_std_float<32, 8>(a)* \
                              ac_std_float<32, 8>(b)/ac_std_float<32, 8>(SCALE*SCALE);
                    } else {                                      // Use fixed-point, handled in the datatype
                        MUL_Out_Type tmp2 = a*b;
                        tmp = ac_std_float<32, 8>(tmp2);
                    }
                    MUL_Out_Type ret;
                    ret = MULOutput.Pop();
                    diff = tmp - ac_std_float<32, 8>(ret)/ac_std_float<32, 8>(SCALE); // difference between mul and shiftadd
                    acc += diff.abs().to_float();
                    //if (diff.abs() > ac_std_float<32, 8>(0.5)) {
                    //    cout << "G: " << i << " " << j << " " << a << " " << b << " "
                    //    << ac_std_float<32, 8>(ret)/ac_std_float<32, 8>(SCALE) << " " << tmp << " " << diff.abs() << endl;
                    //}
                }
            }
            cout << acc/((1<<total_bit)*(1<<total_bit)) << endl; // Print out average difference (compare this to range)
// Test integer
#else
            for (int i = -(SCALE); i < (SCALE); i++) {     // +1 since range end 
                for (int j = -(SCALE); j < (SCALE); j++) { // cannot be covered
                    ac_std_float<32, 8> tmp = ac_std_float<32, 8>(i)* \
                                              ac_std_float<32, 8>(j)/ac_std_float<32, 8>(SCALE*SCALE);
                    MUL_Out_Type ret;
                    ret = MULOutput.Pop();
                    diff = tmp - ac_std_float<32, 8>(ret)/ac_std_float<32, 8>(SCALE); // difference between mul and shiftadd
                    acc += diff.abs().to_float();
                }
            }
            cout << acc/((2*SCALE)*(2*SCALE)) << endl; // Print out average difference (compare this to range)
#endif

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
