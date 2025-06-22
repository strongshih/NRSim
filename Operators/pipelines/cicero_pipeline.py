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
#   Pipeline construction
# ---------------------------------------------------------------------------

def build_cicero_pipeline(dim: Tuple[int, int]):
    """Return staged‑render pipeline as an operator list built via graph."""

    g = OperatorGraph()

    # --- Low‑precision path (4‑bit) --------------------------------------
    sampler = UniformSamplerOperator(dim, bitwidth=4, graph=g)

    reorder = PointArrangeOperator(dim, bitwidth=16, graph=g)
    sampler.add_child(reorder)

    hash_enc_low = HashEncodingOperator(dim, bitwidth=4, graph=g)
    reorder.add_child(hash_enc_low)

    mlp_low = MLPOperator(dim, in_dim=hash_enc_low.num_levels * hash_enc_low.features_per_level,
                          num_layers=4, layer_width=64, bitwidth=4, graph=g)
    hash_enc_low.add_child(mlp_low)

    blend_low = RGBRendererOperator(dim, bitwidth=4, graph=g)
    mlp_low.add_child(blend_low)

    return g 