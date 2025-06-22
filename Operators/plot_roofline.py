# subplot_roofline.py

from utils.system import System
from utils.analysis_model import analysis_model
from utils.unit import Unit
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import ScalarFormatter
import colorsys

# Import all operator types
from operators.computation_operator import ComputationOperator, MLPOperator, SphericalHarmonicsOperator
from operators.encoding_operator import EncodingOperator, HashEncodingOperator, RFFEncodingOperator
from operators.sampling_operator import SamplingOperator, UniformSamplerOperator, FrustrumCullingOperator, ProjectionOperator
from operators.blending_operator import BlendingOperator, GaussianAlphaBlendOperator, RGBRendererOperator, DensityRendererOperator, SortingOperator

def plot_subplot_roofline():
    # Create A100 GPU System instance
    A100_GPU = System(offchip_mem_bw=1935, flops=314, frequency=1095,
                      compute_efficiency=0.75, memory_efficiency=0.7)
    print("System Information:")
    print(A100_GPU)
    
    # Define dimensions (B, N): for instance, 4 rays with 128 samples each
    dim = (4, 128)
    
    # Create lists to hold operators by category
    computation_operators = []
    encoding_operators = []
    sampling_operators = []
    blending_operators = []
    
    # Initialize computation operators
    computation_operators.append(MLPOperator(dim, in_dim=32, num_layers=4, layer_width=128, out_dim=3))
    computation_operators.append(SphericalHarmonicsOperator(dim, degree=4))
    
    # Initialize encoding operators
    encoding_operators.append(HashEncodingOperator(dim, num_levels=16, features_per_level=2))
    encoding_operators.append(RFFEncodingOperator(dim, input_dim=3, num_features=128))
    
    # Initialize sampling operators
    sampling_operators.append(UniformSamplerOperator(dim, sampler_type="uniform"))
    sampling_operators.append(FrustrumCullingOperator(dim, fov=60.0))
    sampling_operators.append(ProjectionOperator(dim, width=800, height=600))
    
    # Initialize blending operators
    blending_operators.append(RGBRendererOperator(dim, background_color="random"))
    blending_operators.append(DensityRendererOperator(dim, method="median"))
    blending_operators.append(SortingOperator(dim, sort_by="depth"))
    blending_operators.append(GaussianAlphaBlendOperator(dim))
    
    # Create dataframes for each operator type
    computation_df = create_operator_dataframe(computation_operators, A100_GPU)
    encoding_df = create_operator_dataframe(encoding_operators, A100_GPU)
    sampling_df = create_operator_dataframe(sampling_operators, A100_GPU)
    blending_df = create_operator_dataframe(blending_operators, A100_GPU)
    
    # For debugging: print column names and some sample data
    print("\nComputation DF Columns:", computation_df.columns.tolist())
    if not computation_df.empty:
        print("\nSample Computation DF row:")
        print(computation_df.iloc[0])
    
    # Same for Encoding DF
    if not encoding_df.empty:
        print("\nSample Encoding DF row:")
        print(encoding_df.iloc[0])
    
    # Plot all operators in a two-subplot figure
    plot_subplots(
        computation_df=computation_df,
        encoding_df=encoding_df,
        sampling_df=sampling_df,
        blending_df=blending_df,
        system=A100_GPU,
        save_path="subplot_roofline.png"
    )
    
    # Save analysis to CSV files
    computation_df.to_csv("computation_roofline.csv", index=False)
    encoding_df.to_csv("encoding_roofline.csv", index=False)
    sampling_df.to_csv("sampling_roofline.csv", index=False)
    blending_df.to_csv("blending_roofline.csv", index=False)
    
    print("Subplot roofline analysis completed and saved to CSV and PNG files.")

def create_operator_dataframe(operators, system):
    """Create a dataframe from a list of operators for roofline analysis."""
    results = []
    for operator in operators:
        # Get the roofline analysis for this operator
        result = operator.get_roofline(system)
        # Annotate whether this operator is measured in FLOPs or generic OPs
        # Any subclass of ComputationOperator is counted in floating‑point FLOPs; others are generic ops.
        from operators.computation_operator import ComputationOperator
        from operators.encoding_operator import HashEncodingOperator, RFFEncodingOperator
        from operators.blending_operator import (
            GaussianAlphaBlendOperator,
            DensityRendererOperator,
            RGBRendererOperator,
        )

        flop_classes = (
            ComputationOperator,
            HashEncodingOperator,
            RFFEncodingOperator,
            GaussianAlphaBlendOperator,
            DensityRendererOperator,
            RGBRendererOperator,
        )

        metric = 'FLOPs' if isinstance(operator, flop_classes) else 'OPs'
        result['Metric'] = metric
        results.append(result)
    
    return pd.DataFrame(results)

def find_throughput_column(df):
    """Find the throughput column in a dataframe."""
    for column in df.columns:
        if 'Throughput' in column:
            return column
    return None

def plot_subplots(computation_df, encoding_df, sampling_df, blending_df, system, save_path):
    """Plot all operators in two separate subplots."""
    unit = Unit()
    
    # Set global font size and weight for better readability with adjusted values for narrower plot
    plt.rcParams.update({
        'font.size': 24,  # Slightly reduced for narrower plot
        'font.weight': 'bold',
        'axes.labelweight': 'bold',
        'axes.titleweight': 'bold',
        'axes.linewidth': 3.5,  # Slightly reduced for better proportion
        'lines.linewidth': 5.0,  # Slightly reduced for better proportion
        'xtick.labelsize': 22,  # Reduced for narrower plot
        'ytick.labelsize': 22,  # Reduced for narrower plot
        'legend.fontsize': 20,  # Reduced for narrower plot
        'figure.titleweight': 'bold'
    })
    
    # Create a more compact but still large figure with a more balanced aspect ratio
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 20))  # independent x‑axes
    
    # Roofline parameters
    op_intensity = system.flops / system.offchip_mem_bw
    
    # Find max_x for proper scaling of the x-axis
    all_op_intensities = []
    for df in [computation_df, encoding_df, sampling_df, blending_df]:
        all_op_intensities.extend(df['Op Intensity'].tolist())
    max_x = max(all_op_intensities)
    min_x = min(all_op_intensities)
    
    # Choose left limit (10x smaller than smallest op intensity) but keep above 1e-3
    left_lim = max(min_x * 0.1, 1e-3)
    
    # Make sure right limits cover the critical intensity so divider is visible
    right_lim_comp  = max(max_x * 2, op_intensity * 2)
    right_lim_other = right_lim_comp
    
    # Find throughput columns for each dataframe
    comp_thrpt_col = find_throughput_column(computation_df)
    enc_thrpt_col = find_throughput_column(encoding_df)
    samp_thrpt_col = find_throughput_column(sampling_df)
    blend_thrpt_col = find_throughput_column(blending_df)
    
    # Print throughput columns for debugging
    print(f"Computation throughput column: {comp_thrpt_col}")
    print(f"Encoding throughput column: {enc_thrpt_col}")
    print(f"Sampling throughput column: {samp_thrpt_col}")
    print(f"Blending throughput column: {blend_thrpt_col}")
    
    # ====== SUBPLOT 1: OPERATORS MEASURED IN FLOPs ======
    # Calculate peak throughput for FLOPs
    flops = unit.raw_to_unit(system.op_per_sec, type='C')
    print(f"Peak FLOPs throughput: {flops}")
    
    # Draw the FLOPs roofline without zero values (log‑scale safe)
    mem_bound_y = (flops / op_intensity) * left_lim  # y value at left limit
    turning_points_flops = np.array([[left_lim, mem_bound_y], [op_intensity, flops], [max(max_x, 1.5*op_intensity), flops]])
    ax1.plot(turning_points_flops[:, 0], turning_points_flops[:, 1], c='blue', linestyle='-', linewidth=5.0, label='FLOPs Roofline')
    
    # Function to generate slightly different colors based on a base color
    def generate_color_variations(base_color, num_variations):
        # Convert hex to RGB
        if base_color.startswith('#'):
            r = int(base_color[1:3], 16) / 255.0
            g = int(base_color[3:5], 16) / 255.0
            b = int(base_color[5:7], 16) / 255.0
        else:  # If it's a named color like 'blue'
            r, g, b = colorsys.rgb_to_hsv(*plt.cm.colors.to_rgb(base_color))
        
        # Convert RGB to HSV for easier modification
        h, s, v = colorsys.rgb_to_hsv(r, g, b)
        
        # Generate variations by adjusting saturation and value
        colors = []
        for i in range(num_variations):
            # Adjust saturation and value slightly for each variation
            new_s = max(0.3, min(1.0, s + (i - num_variations/2) * 0.15))
            new_v = max(0.7, min(1.0, v + (i - num_variations/2) * 0.10))
            # Convert back to RGB
            new_r, new_g, new_b = colorsys.hsv_to_rgb(h, new_s, new_v)
            # Convert to hex
            hex_color = "#{:02x}{:02x}{:02x}".format(int(new_r*255), int(new_g*255), int(new_b*255))
            colors.append(hex_color)
        return colors
    
    # Collect all dataframes for unified processing
    category_dfs = {
        'Computation': {'df': computation_df, 'thrpt_col': comp_thrpt_col, 'base_color': 'blue', 'marker': 'o'},
        'Encoding':    {'df': encoding_df,    'thrpt_col': enc_thrpt_col,  'base_color': '#FF1E1E', 'marker': 's'},
        'Sampling':    {'df': sampling_df,    'thrpt_col': samp_thrpt_col, 'base_color': '#00CC00', 'marker': '^'},
        'Blending':    {'df': blending_df,    'thrpt_col': blend_thrpt_col,'base_color': '#8A00E6', 'marker': 'd'},
    }

    # --- Plot FLOP‑based operators on ax1 ---
    for cat, meta in category_dfs.items():
        df = meta['df']
        thrpt_col = meta['thrpt_col']
        if df.empty or thrpt_col is None:
            continue
        flop_rows = df[df['Metric'] == 'FLOPs']
        if flop_rows.empty:
            continue
        colors = generate_color_variations(meta['base_color'], len(flop_rows))
        for idx, (_, row) in enumerate(flop_rows.iterrows()):
            ax1.scatter(row['Op Intensity'], row[thrpt_col],
                        label=f'{cat}: {row["Op Type"]}',
                        marker=meta['marker'],
                        color=colors[idx],
                        s=700, alpha=1.0, edgecolors='black', linewidth=3.5)
    
    # Set subplot 1 labels and title with larger text
    ax1.set_xlabel('Op Intensity (FLOPs/Byte)', fontsize=26, fontweight='bold')
    ax1.set_ylabel(f'Throughput ({unit.unit_compute.upper()})', fontsize=26, fontweight='bold')
    ax1.set_title("Operators Measured in FLOPs", fontsize=28, fontweight='bold')
    ax1.grid(True, linestyle='--', alpha=0.7, linewidth=2.0)
    
    # Position legend at the bottom right with single column
    ax1.legend(
        loc='lower right',
        fontsize=18,
        frameon=True,
        framealpha=0.95,
        fancybox=True,
        shadow=True,
        ncol=1  # Single column format
    )
    
    # Set log scales for better visualization
    ax1.set_xscale('log')
    ax1.set_yscale('log')
    
    # Format the ticks
    ax1.yaxis.set_major_formatter(ScalarFormatter())
    
    # Make tick labels darker and larger
    for tick in ax1.xaxis.get_major_ticks() + ax1.yaxis.get_major_ticks():
        tick.label1.set_fontsize(22)
        tick.label1.set_fontweight('bold')

    # ====== SUBPLOT 2: OPERATORS MEASURED IN GENERIC OPs ======
    # Calculate peak throughput for OPs
    ops = unit.raw_to_unit(system.op_per_sec, type='O')
    print(f"Peak OPs throughput: {ops}")
    
    # Collect all non-computation throughput values to determine appropriate y-axis scale
    all_throughputs = []
    for df, col in [(encoding_df, enc_thrpt_col), (sampling_df, samp_thrpt_col), (blending_df, blend_thrpt_col)]:
        if not df.empty and col is not None:
            all_throughputs.extend(df[col].tolist())
    
    if all_throughputs:
        max_thrpt = max(all_throughputs)
        min_thrpt = min(all_throughputs)
        print(f"Min OPs throughput in data: {min_thrpt}")
        print(f"Max OPs throughput in data: {max_thrpt}")
        
        # Make sure ops value is within the scale of the actual data
        if ops > max_thrpt * 1000:  # If peak is more than 1000x the max data
            adjusted_ops = max_thrpt * 50  # Adjust to show a reasonable ceiling
            print(f"Adjusting peak OPs from {ops} to {adjusted_ops} for better visibility")
            ops = adjusted_ops
    
    # Draw the OPs roofline avoiding zeros
    mem_bound_y_ops = (ops / op_intensity) * left_lim
    turning_points_ops = np.array([[left_lim, mem_bound_y_ops], [op_intensity, ops], [right_lim_other, ops]])
    ax2.plot(turning_points_ops[:, 0], turning_points_ops[:, 1], c='red', linestyle='-', linewidth=5.0, label='OPs Roofline')
    
    # Plot OP‑based operators on ax2
    for cat, meta in category_dfs.items():
        df = meta['df']
        thrpt_col = meta['thrpt_col']
        if df.empty or thrpt_col is None:
            continue
        op_rows = df[df['Metric'] == 'OPs']
        if op_rows.empty:
            continue
        colors = generate_color_variations(meta['base_color'], len(op_rows))
        for idx, (_, row) in enumerate(op_rows.iterrows()):
            print(f"{cat} {row['Op Type']} - Intensity: {row['Op Intensity']}, Throughput: {row[thrpt_col]}")
            ax2.scatter(row['Op Intensity'], row[thrpt_col],
                        label=f'{cat}: {row["Op Type"]}',
                        marker=meta['marker'],
                        color=colors[idx],
                        s=700, alpha=1.0, edgecolors='black', linewidth=3.5)
    
    # Set subplot 2 labels and title with larger text
    ax2.set_xlabel('Op Intensity (OPs/Byte)', fontsize=26, fontweight='bold')
    # Use OPs unit (replace FLOP string)
    ops_unit_label = unit.unit_flop.upper().replace('FLOP', 'OP')
    ax2.set_ylabel(f'Throughput ({ops_unit_label})', fontsize=26, fontweight='bold')
    ax2.set_title("Operators Measured in Generic OPs", fontsize=28, fontweight='bold')
    ax2.grid(True, linestyle='--', alpha=0.7, linewidth=2.0)
    
    # Position legend at the bottom right with single column
    ax2.legend(
        loc='lower right',
        fontsize=18,
        frameon=True,
        framealpha=0.95,
        fancybox=True,
        shadow=True,
        ncol=1  # Single column format
    )
    
    # Set log scales for better visualization
    ax2.set_yscale('log')
    ax2.set_xscale('log')  # log scale on x-axis for OPs plot
    
    # Format the ticks
    ax2.yaxis.set_major_formatter(ScalarFormatter())
    ax2.xaxis.set_major_formatter(ScalarFormatter())
    
    # Make tick labels darker and larger
    for tick in ax2.xaxis.get_major_ticks() + ax2.yaxis.get_major_ticks():
        tick.label1.set_fontsize(22)
        tick.label1.set_fontweight('bold')
    
    # Add thicker, darker spines
    for spine in ax1.spines.values():
        spine.set_linewidth(3.5)
        spine.set_color('black')
    
    for spine in ax2.spines.values():
        spine.set_linewidth(3.5)
        spine.set_color('black')
    
    # Adjust layout for denser appearance in narrower plot
    plt.tight_layout()
    plt.subplots_adjust(hspace=0.20, left=0.2, right=0.9)  # Adjusted for narrower plot
    
    # Reduce the margins around the plot while ensuring all points are visible
    for ax in [ax1, ax2]:
        ax.margins(x=0.05, y=0.1)  # Increased margins slightly to keep points visible in narrower plot
    
    # Extend x‑axis leftwards so the horizontal roofline segment appears at the far left
    ax1.set_xlim(left=left_lim)
    ax1.set_xlim(right=right_lim_comp)
    ax2.set_xlim(left=left_lim)
    ax2.set_xlim(right=right_lim_other)
    
    # Save the plot with higher DPI for better quality
    plt.savefig(save_path, bbox_inches='tight', dpi=300)
    print(f"Subplot plot saved to {save_path}")
    
    # Show the plot
    plt.show()

if __name__ == "__main__":
    plot_subplot_roofline() 