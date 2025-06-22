"""Staged‑render (s‑render) pipeline that couples a low‑precision front end
with a high‑precision adaptive back end.

Data‑flow:
    Sampler (4‑bit)                     ┐
      → HashGridEncode (4‑bit)          │ low‑precision path
      → MLP (4‑bit)                     │
      → Blending (4‑bit)                ┘
      → SensitivityPrediction (16‑bit)  ┐  optimisation path
      → HashGridEncode + Reorder (16‑bit)
      → MLP (16‑bit)                    ├ high‑precision path
      → Blending (16‑bit)               ┘
      → Recovery (16‑bit)
"""

from operators.sampling_operator import UniformSamplerOperator
from operators.encoding_operator import HashEncodingOperator, PointArrangeOperator
from operators.computation_operator import MLPOperator
from operators.blending_operator import RGBRendererOperator, DensityRendererOperator
from operators.optimization_operator import OptimizationOperator

from typing import Tuple
from utils.operator_graph import OperatorGraph
import numpy as np

# ---------------------------------------------------------------------------
#   Optimisation‑stage operators
# ---------------------------------------------------------------------------


class SensitivityPredictionOperator(OptimizationOperator):
    """Predict importance using per‑sample weights and 3×3 RGB neighbourhood."""

    def __init__(self, dim, coarse_prop: float = 0.4, fine_prop: float = 0.4, patch_size: int = 3,
                 bitwidth: int = 16, graph=None):
        self.coarse_prop = coarse_prop
        self.fine_prop = fine_prop
        self.patch_size = patch_size
        super().__init__(dim, bitwidth, graph)
        self.op_type = "SensitivityPrediction"

    # Tensor sizes
    def get_tensors(self):
        B, N = self.dim[:2]
        input_a = B * N + B * 3  # per‑sample weight + per‑ray RGB
        input_b = 0
        output = B * N + B       # flags per sample + per patch
        return input_a, input_b, output

    # FLOPs (approximation)
    def get_num_ops(self):
        B, N = self.dim[:2]
        fine_ops = B * N  # fine compare per sample
        patch_area = self.patch_size ** 2
        num_patches = int(np.ceil(B / patch_area))
        imp_patches = int(self.coarse_prop * num_patches)
        neighbour_ops = 8 * 8 * imp_patches
        compare_ops = 8 * imp_patches
        return fine_ops + neighbour_ops + compare_ops

    # shape helpers ------------------------------------------------------
    def get_input_tensor_shapes(self):
        B, N = self.dim[:2]
        # Two logical inputs: per‑sample scalar weight & per‑ray RGB (3‑vec)
        return [(B, N), (B, 3)]

    def get_output_tensor_shape(self):
        B, N = self.dim[:2]
        # Flags per sample   +   1 scalar per ray  ≈  N+1 values per ray
        return (B, N + 1)


class RecoveryOperator(OptimizationOperator):
    """Copy / compact RGBD data of important samples."""

    def __init__(self, dim, channels: int = 4, bitwidth: int = 16, graph=None):
        self.channels = channels
        super().__init__(dim, bitwidth, graph)
        self.op_type = "Recovery"

    def get_tensors(self):
        B, N = self.dim[:2]
        flags = B * N
        data = B * N * self.channels
        return flags + data, 0, data

    def get_num_ops(self):
        B, N = self.dim[:2]
        return B * N * self.channels  # copy operations

    # shape helpers ------------------------------------------------------
    def get_input_tensor_shapes(self):
        B, N = self.dim[:2]
        # Concatenate flags (1) and data (channels) along feature dim
        return [(B, N, self.channels), (B, N, 1)]

    def get_output_tensor_shape(self):
        B, N = self.dim[:2]
        return (B, N, self.channels)


# ---------------------------------------------------------------------------
#   Pipeline construction
# ---------------------------------------------------------------------------

def build_srender_pipeline(dim: Tuple[int, int]):
    """Return staged‑render pipeline as an operator list built via graph."""

    g = OperatorGraph()

    # --- Low‑precision path (4‑bit) --------------------------------------
    sampler = UniformSamplerOperator(dim, bitwidth=4, graph=g)

    hash_enc_low = HashEncodingOperator(dim, bitwidth=4, graph=g)
    sampler.add_child(hash_enc_low)

    mlp_low = MLPOperator(dim, in_dim=hash_enc_low.num_levels * hash_enc_low.features_per_level,
                          num_layers=4, layer_width=64, bitwidth=4, graph=g)
    hash_enc_low.add_child(mlp_low)

    blend_low = RGBRendererOperator(dim, bitwidth=4, graph=g)
    mlp_low.add_child(blend_low)

    # --- Optimisation stage ---------------------------------------------
    sens_pred = SensitivityPredictionOperator(dim, bitwidth=16, graph=g)
    blend_low.add_child(sens_pred)

    reorder = PointArrangeOperator(dim, bitwidth=16, graph=g)
    sens_pred.add_child(reorder)

    hash_enc_hi = HashEncodingOperator(dim, bitwidth=16, graph=g)
    reorder.add_child(hash_enc_hi)

    mlp_hi = MLPOperator(dim, in_dim=hash_enc_hi.num_levels * hash_enc_hi.features_per_level,
                         num_layers=4, layer_width=64, bitwidth=16, graph=g)
    hash_enc_hi.add_child(mlp_hi)

    blend_hi = RGBRendererOperator(dim, bitwidth=16, graph=g)
    mlp_hi.add_child(blend_hi)

    recovery = RecoveryOperator(dim, bitwidth=16, graph=g)
    blend_hi.add_child(recovery)
    blend_low.add_child(recovery)

    return g 