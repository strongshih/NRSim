# utils/roofline_plot.py

import matplotlib.pyplot as plt
import numpy as np
from utils.unit import Unit

def plot_roofline_background(system, max_x, unit):
    op_intensity = system.flops / system.offchip_mem_bw
    flops = unit.raw_to_unit(system.op_per_sec, type='C')
    turning_points = np.array([[0, 0], [op_intensity, flops], [max(max_x, 1.5*op_intensity), flops]])
    plt.plot(turning_points[:, 0], turning_points[:, 1], c='grey')
    plt.xlabel('Op Intensity (FLOPs/Byte)')
    plt.ylabel(f'{unit.unit_compute.upper()}')

def dot_roofline(df, system, save_path=None):
    unit = Unit()
    max_x = max(df['Op Intensity'])
    plot_roofline_background(system, max_x, unit)
    markers = [".", ",", "o", "v", "^", "<", ">"]
    for i in range(len(df)):
        op_intensity = df.loc[i, 'Op Intensity']
        thrpt = df.loc[i, 'Throughput (Tflops)']
        plt.scatter(op_intensity, thrpt, label=f'{df.loc[i, "Op Type"]}-{i}', marker=markers[i % len(markers)])
    plt.legend(bbox_to_anchor =(1, 0.9), ncol=1)
    plt.title("Roofline Analysis")
    
    # Save the plot if a path is provided
    if save_path:
        plt.savefig(save_path, bbox_inches='tight', dpi=300)
        print(f"Plot saved to {save_path}")
    
    plt.show()
