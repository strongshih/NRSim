from typing import Tuple

from operators.sampling_operator import UniformSamplerOperator
from operators.encoding_operator import RFFEncodingOperator
from operators.computation_operator import MLPOperator
from operators.blending_operator import RGBRendererOperator
from utils.operator_graph import OperatorGraph


def build_icarus_pipeline(dim: Tuple[int, int]):
    """Construct the *Icarus* rendering pipeline.

    Pipeline stages:
        1. UniformSamplerOperator       – produces 192 points per ray.
        2. RFFEncodingOperator (128 f)  – encodes coordinates to 256‑D.
        3. MLPOperator (depth 8, w 256) – predicts RGB.
        4. RGBRendererOperator          – composite final colour.

    Args:
        dim: Tuple ``(B, N)`` where *N* is expected to be 192.

    Returns:
        OperatorGraph instance populated with the pipeline nodes.
    """

    B, N = dim
    assert N == 192, "Icarus pipeline expects 192 samples per ray"

    g = OperatorGraph()

    # 1) Uniform sampling --------------------------------------------------
    sampler = UniformSamplerOperator(dim, sampler_type="uniform", bitwidth=16, graph=g)

    # 2) RFF encoding ------------------------------------------------------
    rff = RFFEncodingOperator(dim, input_dim=3, num_features=128, scale=1.0,
                              use_sin_cos=True, implementation="torch", graph=g)

    # 3) MLP prediction ----------------------------------------------------
    mlp = MLPOperator(dim, in_dim=256, num_layers=8, layer_width=256,
                      use_bias=True, bitwidth=16, graph=g)

    # 4) Rendering ---------------------------------------------------------
    renderer = RGBRendererOperator(dim, background_color="black", bitwidth=16, graph=g)

    # Wire dependencies ----------------------------------------------------
    sampler.add_child(rff)
    rff.add_child(mlp)
    mlp.add_child(renderer)

    return g 