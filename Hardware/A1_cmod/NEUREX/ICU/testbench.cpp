#define NVHLS_VERIFY_BLOCKS (ICU)
#include "ICU.h"
#include <nvhls_verify.h>
#include "nvhls_connections.h"
#include "ac_sysc_trace.h"
#include <random>
#include <systemc.h>
#include <nvhls_module.h>
#include <mc_connections.h>


class Top : public sc_module {
public:
    sc_clock clk;
    sc_signal<bool> rst;

    Connections::Combinational<ICU_In_Type> data_in;
    Connections::Combinational<ICU_In_Type> w_in;
    Connections::Combinational<ICU_Out_Type> data_out;

    NVHLS_DESIGN(ICU) dut;

    SC_CTOR(Top) : clk("clk", 1, SC_NS, 0.5, 0, SC_NS, true),
                   rst("rst"),
                   data_in("data_in"),
                   w_in("w_in"),
                   data_out("data_out"),
                   dut("dut") {

        sc_object_tracer<sc_clock> trace_clk(clk);

        dut.clk(clk);
        dut.rst(rst);
        dut.data_in(data_in);
        dut.w_in(w_in);
        dut.data_out(data_out);

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
        data_in.ResetWrite();
        w_in.ResetWrite();
        wait(10);

        // Random inputs
        for (int i = 0; i < 4; i++) {
            wait();
            ICU_In_Type data_in_tmp;
            ICU_In_Type w_in_tmp;
            for (int j = 0; j < 8; j++) {
                data_in_tmp.x[j] = ICU_In_Elem(j + 8*i);
                w_in_tmp.x[j] = ICU_In_Elem(10*(j + 8*i));
            }
            data_in.Push(data_in_tmp);
            w_in.Push(w_in_tmp);
        }
    }

    void collect() {
        data_out.ResetRead();
        while (1) {
            wait(); // 1 cc

            for (int i = 0; i < 4; i++) {
                ICU_Out_Type data;
                data = data_out.Pop();
                cout << "Output " << data << ", " << sc_time_stamp() << endl;
            }

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
