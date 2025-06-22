from __future__ import annotations
import math, itertools, argparse, sys
from dataclasses import dataclass, asdict
import pandas as pd
import matplotlib as mpl
import matplotlib.pyplot as plt
from matplotlib.ticker import LogLocator, LogFormatterExponent, FuncFormatter
import numpy as np
from typing import Optional

# ─────────────────────────── 0. Config ───────────────────────────
BUF_BYTES = 34 * 1024  # capacity of one "Buf" bank (bytes)

CONFIG = {
    # HW unit area & dyn‑power (from Table 7 of the GSCore paper)
    "unit_area_mm2": {"CCU":0.82/4, "QSU":0.01/8, "BSU":0.06/4,
                       "VRCore":1.81/64, "Buf":1.25/8},
    "unit_power_W" : {"CCU":0.52/4, "QSU":0.01/8, "BSU":0.05/4,
                       "VRCore":0.25/64, "Buf":0.04/8},

    # Core timing constants
    "clock_hz":   1_000_000_000,
    "FEATURE_BYTES": 32,
    "BSU_WIDTH":  16,
    "ccu_cycle_per_gauss": 1.0,
    "qsort_cmp_cyc":       1.0,
    "bsu_cmp_cyc":         0.5,
    "dram_load_cyc":       1.0,
    "PIX_CYCLES":          4,
    "GAUSS_HITS_PER_PIXEL":5,

    # Memory‑system energy numbers (bandwidth is per‑design)
    "E_SRAM_BYTE":   0.02e-12,   # 0.02 pJ  per on‑chip byte access
    "E_DRAM_BYTE":   15e-12,     # 15 pJ   per DRAM byte
    "P_DRAM_STATIC": 0.20        # 200 mW idle/refresh
}

# ────────────────────────── 1. Data classes ─────────────────────────
@dataclass(frozen=True)
class Scene:
    name:str; gaussians:int; all_points:int; width:int; height:int
    @property
    def pixels(self): return self.width * self.height
    @property
    def tiles(self):  return math.ceil(self.pixels / 256)
    @property
    def gauss_per_tile(self): return self.all_points / self.tiles

@dataclass(frozen=True)
class Hardware:
    CCU:int; QSU:int; BSU:int; VRCore:int; Buf:int; BW_GBps:float
    # area & *core* dynamic‑power (SRAM/DRAM dyn energy handled separately)
    def area_mm2(self):
        ua = CONFIG["unit_area_mm2"]
        return (self.CCU*ua["CCU"] + self.QSU*ua["QSU"] + self.BSU*ua["BSU"]
              + self.VRCore*ua["VRCore"] + self.Buf*ua["Buf"])
    def power_W(self):
        up = CONFIG["unit_power_W"]
        return (self.CCU*up["CCU"] + self.QSU*up["QSU"] + self.BSU*up["BSU"]
              + self.VRCore*up["VRCore"] + self.Buf*up["Buf"])
    @property
    def dram_bw_Bps(self):
        return self.BW_GBps * 1e9   # convert GB/s → bytes/s

# ─────────────────────── 2. Core latency model ─────────────────────
FB   = CONFIG["FEATURE_BYTES"]
BSUW = CONFIG["BSU_WIDTH"]

def buf_cap(hw):
    return hw.Buf * BUF_BYTES // FB

# --- helpers that understand hw parallelism -------------------------

def qsort_cyc(n:int, qsus:int):
    """Cycles to quick‑sort *n* keys with *qsus* compare units."""
    cyc = 0
    while n > BSUW:
        comps  = n
        lanes  = max(1, qsus)
        steps  = math.ceil(comps / lanes)
        cyc   += steps * CONFIG["qsort_cmp_cyc"]
        n      = math.ceil(n / 2)
    return cyc


def bsu_cyc(n:int, bsus:int):
    """Cycles to bitonic‑merge *n* keys using *bsus* sorters."""
    if n <= 1:
        return 0
    comps = n * int(math.log2(BSUW))
    lanes = max(1, bsus)
    steps = math.ceil(comps / lanes)
    return steps * CONFIG["bsu_cmp_cyc"]


def tile_lat(scene: Scene, hw: Hardware):
    gpt    = scene.gauss_per_tile
    cap    = buf_cap(hw)
    chunks = max(1, math.ceil(gpt / cap))
    g      = gpt / chunks

    L = g * CONFIG["dram_load_cyc"]
    S = qsort_cyc(int(g), hw.QSU) + bsu_cyc(int(g), hw.BSU)  # Now using hw.BSU instead of hw.QSU//2
    R = 256 * CONFIG["GAUSS_HITS_PER_PIXEL"] * CONFIG["PIX_CYCLES"] / hw.VRCore
    
    return chunks * max(L, S, R)   # fully overlapped pipeline


def frame_cyc(scene: Scene, hw: Hardware):
    prep = CONFIG["ccu_cycle_per_gauss"] * scene.gaussians / hw.CCU
    
    return prep + scene.tiles * tile_lat(scene, hw)

# ──────────────── 3. SRAM & DRAM traffic / energy ────────────────

def mem_bytes(scene: Scene):
    bytes_gauss = scene.gaussians * CONFIG["FEATURE_BYTES"]
    sram = 2 * bytes_gauss          # write + read
    dram = bytes_gauss              # one DRAM pull
    return sram, dram


def mem_energy_and_bw(scene: Scene, sec: float, hw: Hardware):
    sram_b, dram_b = mem_bytes(scene)
    e  = sram_b * CONFIG["E_SRAM_BYTE"] + dram_b * CONFIG["E_DRAM_BYTE"]
    e += CONFIG["P_DRAM_STATIC"] * sec
    bw_util = (dram_b / sec) / hw.dram_bw_Bps
    return e, bw_util

# ───────────── 4. Eval (one design point) ─────────────

def eval_hw(scene: Scene, hw: Hardware):
    cyc  = frame_cyc(scene, hw)
    sec  = cyc / CONFIG["clock_hz"]
    fps  = 1 / sec
    e_core = hw.power_W() * sec
    e_mem, bw_util = mem_energy_and_bw(scene, sec, hw)
    return fps, (e_core + e_mem) * 1e3, bw_util   # FPS , mJ , BW util

# ───────────── 5. Sweep & Pareto utilities ────────────
SWEEP_GRID = {
    "CCU":    [4, 8, 16, 32],
    "QSU":    [4, 8, 16, 32],
    "VRCore": [32, 64, 128],
    "Buf":    [4, 8, 16],
    "BW_GBps":[51.2]  # 1-chan LPDDR-3200
}


def sweep(scene: Scene) -> pd.DataFrame:
    rows = []
    keys = list(SWEEP_GRID.keys())
    for vals in itertools.product(*SWEEP_GRID.values()):
        # Create a dictionary of parameter values
        params = dict(zip(keys, vals))
        
        # Calculate BSU as QSU//2
        params["BSU"] = params["QSU"] // 2
        
        # Create Hardware instance with all parameters
        hw = Hardware(**params)
        
        fps, energy_mJ, bw_util = eval_hw(scene, hw)
        rows.append({**asdict(hw),
                     "FPS":fps,
                     "Energy_mJ":energy_mJ,
                     "Area_mm2":hw.area_mm2(),
                     "BW_util":bw_util})
    return pd.DataFrame(rows)


def pareto(df: pd.DataFrame):
    df = df.reset_index(drop=True)
    keep = [True]*len(df)
    for i in range(len(df)):
        if not keep[i]:
            continue
        dom = ((df.FPS >= df.FPS[i]) &
               (df.Area_mm2 <= df.Area_mm2[i]) &
               (df.Energy_mJ <= df.Energy_mJ[i]))
        dom.iloc[i] = False
        keep = [k and not d for k, d in zip(keep, dom)]
    return df[keep]


# ────────────────────────── 7. CLI ───────────────────────────

def main(argv: list[str]):
    parser = argparse.ArgumentParser()
    parser.add_argument('--csv',   help='dump full sweep to CSV')
    parser.add_argument('--scene', choices=['lego', 'bicycle'], default='lego',
                        help='which preset scene to sweep')
    args = parser.parse_args(argv)

    # preset scenes (extend as desired)
    if args.scene == 'lego':
        scene = Scene("Lego", 167_894, 1_570_804, 800, 800)
    else:
        scene = Scene("Bicycle", 1_656_176, 10_329_175, 4946, 3286)

    df = sweep(scene)

    # optional CSV dump
    if args.csv:
        df.to_csv(args.csv, index=False)
        print(f"CSV written -> {args.csv}")

    # add GSCore reference design (4/8/4/64/8 @ 51.2 GB/s)
    g_hw = Hardware(CCU=4, QSU=8, BSU=4, VRCore=64, Buf=8, BW_GBps=51.2)
    g_fps, g_e, g_bw = eval_hw(scene, g_hw)
    g_point = {**asdict(g_hw), "FPS":g_fps, "Energy_mJ":g_e,
               "Area_mm2":g_hw.area_mm2(), "BW_util":g_bw}

    # print Pareto
    good  = df[df.FPS >= 30]
    front = pareto(good)
    print(f"\nPareto front (≥30 FPS, {scene.name}):")
    print(front.sort_values(['Area_mm2','Energy_mJ'])
               [["CCU","QSU","BSU","VRCore","Buf","BW_GBps",
                 "Area_mm2","Energy_mJ","FPS","BW_util"]]
               .to_string(index=False,formatters={
                   "Area_mm2":"{:.2f}".format,
                   "Energy_mJ":"{:.1f}".format,
                   "FPS":"{:.0f}".format,
                   "BW_util":"{:.2f}".format}))

# ─────────────────────────── entry ──────────────────────────
if __name__ == "__main__":
    main(sys.argv[1:])
