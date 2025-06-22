# operators/gscore_pipeline.py

"""GS‑Core pipeline composed of
    1. Frustum‑Culling‑Projection (composite sampling op)
    2. Spherical‑Harmonics shading
    3. Per‑ray Sorting
    4. Gaussian Alpha Blending

This mirrors the dependency wiring style of the other pipeline helpers.
"""

from typing import Tuple

from operators.sampling_operator import FrustumCullingProjectionOperator
from operators.computation_operator import SphericalHarmonicsOperator
from operators.blending_operator import SortingOperator, GaussianAlphaBlendOperator

from utils.operator_graph import OperatorGraph


def build_gaurast_pipeline(dim: Tuple[int, int]):
    """Build and return the GauRast operator graph.

    Args:
        dim: Tuple ``(B, N)`` giving batch size (B) and number of samples (N)
             per ray.  These are used for the SH, sorting, and blending stages.
             The initial Frustum‑Culling‑Projection stage receives only the
             *total* number of Gaussians; for simplicity we assume ``G = B · N``.
    """

    B, N = dim
    G = B * N  # simple heuristic; real usage may differ

    g = OperatorGraph()

    # Stage 1: composite sampling (culling → projection → intersection) -------
    frust_cull = FrustumCullingProjectionOperator((G,), image_wh=(1280, 720), graph=g)

    # Stage 2: SH evaluation --------------------------------------------------
    shading = SphericalHarmonicsOperator(dim, degree=4, graph=g)

    # Stage 3: depth/opacity sorting -----------------------------------------
    sorter = SortingOperator(dim, sort_by="depth", order="ascending", graph=g)

    # Stage 4: final alpha blending ------------------------------------------
    blend = GaussianAlphaBlendOperator(dim, graph=g)

    # Wire dependencies -------------------------------------------------------
    frust_cull.add_child(shading)
    shading.add_child(blend)
    frust_cull.add_child(sorter)
    sorter.add_child(blend)

    return g 