#define NVHLS_VERIFY_BLOCKS (MLP)
#include "MLP_vanilla.h"
#include <nvhls_verify.h>
//#include <mc_scverify.h>
#include "nvhls_connections.h"
#include "ac_sysc_trace.h"
#include <random>
//#include <ac_channel.h>
#include <systemc.h>
#include <nvhls_module.h>
#include <mc_connections.h>
#include "mlp_test.h"

#define SAMPLE_NUM 192

class Top : public sc_module {
public:
    sc_clock clk;
    sc_signal<bool> rst;

    Connections::Combinational<MemReq> memreq;
    Connections::Combinational<MLP_In_Type> MLPInput;
    Connections::Combinational<MLP_Out_Type> MLPOutput;

    NVHLS_DESIGN(MLP) dut;
//    CCS_DESIGN(SDAcc) CCS_INIT_S1(dut);

    SC_CTOR(Top) : clk("clk", 1, SC_NS, 0.5, 0, SC_NS, true),
                   rst("rst"),
                   memreq("memreq"),
                   MLPInput("MLPInput"),
                   MLPOutput("MLPOutput"),
                   dut("dut") {

        sc_object_tracer<sc_clock> trace_clk(clk);

        dut.clk(clk);
        dut.rst(rst);
        dut.memreq(memreq);
        dut.MLPInput(MLPInput);
        dut.MLPOutput(MLPOutput);

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
        memreq.ResetWrite();
        MLPInput.ResetWrite();
        wait(10);

        // Write to layer0 weight memory 256x256 
        cout << "Weight memory (256x256): " << endl;
        for (int i = 0; i < MLP0_OUT_DIM; i++) {
            MemReq req1;
            for (int j = 0; j < MLP0_IN_DIM; j++) {
                req1.index[0] = i;
                req1.index[1] = j;
                req1.data = MLP_Weight_Type(mlp0_weight[i][j]);
                req1.forMLP0 = true;
                req1.isBias = false;
                memreq.Push(req1);
            }
            req1.index[0] = i;
#ifdef USE_FLOAT
            req1.data = MLP_Weight_Type(mlp0_bias[i]*128); // scale up
#else
            req1.data = MLP_Weight_Type(mlp0_bias[i]);
#endif
            req1.forMLP0 = true;
            req1.isBias = true;
            memreq.Push(req1);
        }
        cout << "Finish writing to layer0 @ " << sc_time_stamp() << endl;

        // Write to layer1 weight memory 4x256 
        cout << "Weight memory (4x256): " << endl;
        for (int i = 0; i < MLP1_OUT_DIM; i++) {
            MemReq req1;
            for (int j = 0; j < MLP1_IN_DIM; j++) {
                req1.index[0] = i;
                req1.index[1] = j;
                req1.data = MLP_Weight_Type(mlp1_weight[i][j]);
                req1.forMLP0 = false;
                req1.isBias = false;
                memreq.Push(req1);
            }
            req1.index[0] = i;
#ifdef USE_FLOAT
            req1.data = MLP_Weight_Type(mlp1_bias[i]*128*128); // scale up
#else
            req1.data = MLP_Weight_Type(mlp1_bias[i]);
#endif
            req1.forMLP0 = false;
            req1.isBias = true;
            memreq.Push(req1);
        }
        cout << "Finish writing to layer1 @ " << sc_time_stamp() << endl;


        wait(10);

        // Random inputs
        for (int i = 0; i < SAMPLE_NUM; i++) {
            MLP_In_Type vec;
            for (int j = 0; j < MLP0_IN_DIM; j++) {
                vec.X[j] = MLP_In_Elem_Type(test_input[i][j]);
            }
            vec.isLastSample = (i == SAMPLE_NUM-1);
            MLPInput.Push(vec);
        }
    }

    void collect() {
        MLPOutput.ResetRead();
        while (1) {
            wait(); // 1 cc

            float acc = 0;
            for (int i = 0; i < SAMPLE_NUM; i++) {
                MLP_Out_Type tmp;
                tmp = MLPOutput.Pop();
                cout << "MLPOutput: @ timestep: " << sc_time_stamp() << endl;
                cout << "Sample " << i << ": ";
                for (uint j = 0; j < MLP1_OUT_DIM; j++) {
#ifdef USE_FLOAT
                    MLP_Out_Elem_Type _ = tmp.X[j]/(MLP_Out_Elem_Type(128*128*128)); // scale down
                    cout << _ << " ";
                    acc += _.to_float() - test_output[i][j];
#else
                    MLP_Out_Elem_Type _ = tmp.X[j]; // scale down
                    cout << float(_.to_int())/128 << " ";
                    acc += float(_.to_int())/128 - test_output[i][j];
#endif
                }
                cout << endl;
            }
            cout << "Avg Error: " << acc / (SAMPLE_NUM*MLP1_OUT_DIM) << endl;

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
