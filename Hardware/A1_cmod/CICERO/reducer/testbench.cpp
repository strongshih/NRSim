#define NVHLS_VERIFY_BLOCKS (reducer)
#include "reducer.h"
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

    Connections::Combinational<reducer_W> w;
    Connections::Combinational<reducer_feature> f_in;
    Connections::Combinational<reducer_feature> f_out;

    NVHLS_DESIGN(reducer) dut;

    SC_CTOR(Top) : clk("clk", 1, SC_NS, 0.5, 0, SC_NS, true),
                   rst("rst"),
                   w("w"),
                   f_in("f_in"),
                   f_out("f_out"),
                   dut("dut") {

        sc_object_tracer<sc_clock> trace_clk(clk);

        dut.clk(clk);
        dut.rst(rst);
        dut.w(w);
        dut.f_in(f_in);
        dut.f_out(f_out);

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
        w.ResetWrite();
        f_in.ResetWrite();
        wait(10);

        // Random inputs
        for (int i = 0; i < 4; i++) {
            #pragma hls_pipeline_init_interval 1
            for (int j = 0; j < 8; j++) {
                wait();
                reducer_W w_data       = reducer_W(i*8 + j);
                reducer_feature f_data = reducer_feature(i*8 + j);
                w.Push(w_data);
                f_in.Push(f_data);
            }
        }
    }

    void collect() {
        f_out.ResetRead();
        while (1) {
            wait(); // 1 cc

            for (int i = 0; i < 4; i++) {
                reducer_feature v;
                v = f_out.Pop();
                cout << "Output " << v << ", " << sc_time_stamp() << endl;
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
