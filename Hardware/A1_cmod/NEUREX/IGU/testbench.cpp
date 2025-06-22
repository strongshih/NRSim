#define NVHLS_VERIFY_BLOCKS (IGU)
#include "IGU.h"
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

    Connections::Combinational<IGU_Grid_Res> sugbrid_res;
    Connections::Combinational<IGU_In_Type> pos;
    Connections::Combinational<IGU_Grid_Res> grid_id;
    Connections::Combinational<Hashed_addr> hashed_addr;
    Connections::Combinational<IGU_Weight> weight;

    NVHLS_DESIGN(IGU) dut;

    SC_CTOR(Top) : clk("clk", 1, SC_NS, 0.5, 0, SC_NS, true),
                   rst("rst"),
                   sugbrid_res("sugbrid_res"),
                   pos("pos"),
                   grid_id("grid_id"),
                   hashed_addr("hashed_addr"),
                   dut("dut") {

        sc_object_tracer<sc_clock> trace_clk(clk);

        dut.clk(clk);
        dut.rst(rst);
        dut.sugbrid_res(sugbrid_res);
        dut.pos(pos);
        dut.grid_id(grid_id);
        dut.hashed_addr(hashed_addr);

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
        sugbrid_res.ResetWrite();
        pos.ResetWrite();
        wait(10);

        // Random inputs
        for (int i = 0; i < 10; i++) {
            wait();
            IGU_Grid_Res res = IGU_Grid_Res(4);
            IGU_In_Type in;
            for (int i = 0; i < 3; i++) in.x[i] = IGU_In_Elem_Type(0.1*i+0.2);
            pos.Push(in);
            sugbrid_res.Push(res);
        }
    }

    void collect() {
        grid_id.ResetRead();
        hashed_addr.ResetRead();
        weight.ResetRead();
        while (1) {
            wait(); // 1 cc

            for (int i = 0; i < 10; i++) {
                cout << "address Generation Output: @ timestep: " << sc_time_stamp() << endl;

                IGU_Grid_Res tmp_grid_id;
                Hashed_addr tmp_hashed_addr;
                IGU_Weight tmp_weight;
                tmp_grid_id = grid_id.Pop();
                tmp_hashed_addr = hashed_addr.Pop();
                tmp_weight = weight.Pop();
                cout << "Output " << tmp_grid_id << sc_time_stamp() << endl;
                for (int i = 0; i < 8; i++) {
                    cout << "(" << tmp_hashed_addr.x[i] << ", " << tmp_weight.x[i] << ") " << endl;
                }
                cout << endl;
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
