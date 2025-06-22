"""Neurex pipeline: baseline hash‑grid rendering path.

This was previously called *grid_pipeline*; it has been renamed across the
code‑base to avoid ambiguity with other variants.
"""

from operators.sampling_operator import UniformSamplerOperator
from operators.encoding_operator import HashEncodingOperator
from operators.computation_operator import MLPOperator
from operators.blending_operator import RGBRendererOperator, DensityRendererOperator

from utils.operator_graph import OperatorGraph


def build_neurex_pipeline(dim):
    """Return list of operators forming the Neurex (hash‑grid baseline) pipeline.

    The operators are automatically registered into an internal graph upon
    construction; dependencies are expressed via `add_child`, so we never
    manually append to a Python list.
    """

    g = OperatorGraph()

    # Create operator nodes ------------------------------------------------
    sampler = UniformSamplerOperator(dim, sampler_type="uniform", bitwidth=16, graph=g)

    hash_encoding = HashEncodingOperator(dim, num_levels=16, bitwidth=16, graph=g)

    mlp_in_dim = hash_encoding.num_levels * hash_encoding.features_per_level
    mlp = MLPOperator(dim, in_dim=mlp_in_dim, num_layers=4, layer_width=64,
                      use_bias=True, bitwidth=16, graph=g)

    rgb_renderer = RGBRendererOperator(dim, background_color="random", bitwidth=16, graph=g)
    density_renderer = DensityRendererOperator(dim, method="expected", bitwidth=16, graph=g)

    # Wire dependencies ----------------------------------------------------
    sampler.add_child(hash_encoding)
    hash_encoding.add_child(mlp)
    mlp.add_child(rgb_renderer)
    mlp.add_child(density_renderer)

    # Return operators in construction order ------------------------------
    return g 