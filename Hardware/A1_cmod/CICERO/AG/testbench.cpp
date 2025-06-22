#define NVHLS_VERIFY_BLOCKS (AG)
#include "AG.h"
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

    Connections::Combinational<AG_VIDsWs> memreq;
    Connections::Combinational<AG_VID> vid_out;
    Connections::Combinational<AG_W> w_out;

    NVHLS_DESIGN(AG) dut;

    SC_CTOR(Top) : clk("clk", 1, SC_NS, 0.5, 0, SC_NS, true),
                   rst("rst"),
                   memreq("memreq"),
                   vid_out("vid_out"),
                   w_out("w_out"),
                   dut("dut") {

        sc_object_tracer<sc_clock> trace_clk(clk);

        dut.clk(clk);
        dut.rst(rst);
        dut.memreq(memreq);
        dut.vid_out(vid_out);
        dut.w_out(w_out);

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
        wait(10);

        // Random inputs
        for (int i = 0; i < 4; i++) {
            wait();
            AG_VIDsWs vec;
            for (int j = 0; j < 8; j++) {
                vec.v[j] = AG_VID(j + 8*i);
                vec.w[j] = AG_W(10*(j + 8*i));
            }
            memreq.Push(vec);
        }
    }

    void collect() {
        vid_out.ResetRead();
        w_out.ResetRead();
        while (1) {
            wait(); // 1 cc

            for (int i = 0; i < 4*8; i++) {
                cout << "address Generation Output: @ timestep: " << sc_time_stamp() << endl;
                AG_VID v;
                v = vid_out.Pop();
                AG_W w;
                w = w_out.Pop();
                cout << "Output " << v << ", " << w << ", " << sc_time_stamp() << endl;
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
