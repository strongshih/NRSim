from typing import Tuple
import numpy as np

from operators.sampling_operator import FrustumCullingProjectionOperator
from operators.computation_operator import SphericalHarmonicsOperator
from operators.blending_operator import SortingOperator, GaussianAlphaBlendOperator
from operators.optimization_operator import OptimizationOperator
from utils.operator_graph import OperatorGraph


def build_gscore_pipeline(dim: Tuple[int, int]):
    """Build and return the GS‑Core operator graph.

    Args:
        dim: Tuple ``(B, N)`` giving batch size (B) and number of samples (N)
             per ray. These are used for the SH, sorting, and blending stages.
             The initial Frustum‑Culling‑Projection stage receives only the
             *total* number of Gaussians; for simplicity we assume ``G = B · N``.
    """

    B, N = dim
    G = B * N  # heuristic for number of Gaussians

    g = OperatorGraph()

    # 1. Composite sampling -------------------------------------------------
    frust_cull = FrustumCullingProjectionOperator((G,), image_wh=(1280, 720), graph=g)
    
    # 2. Sub‑tile skipping optimisation ------------------------------------
    subtile_skip = SubtileSkippingOperator((G,), image_wh=(1280, 720), tile=16,
                                           tile_keep_ratio=0.1, graph=g)

    # 3. Spherical‑Harmonics shading ---------------------------------------
    shading = SphericalHarmonicsOperator(dim, degree=4, graph=g)
    

    # 4. Depth/opacity sorting --------------------------------------------
    sorter = SortingOperator(dim, sort_by="depth", order="ascending", graph=g)

    # 5. Alpha blending ----------------------------------------------------
    blend = GaussianAlphaBlendOperator(dim, graph=g)

    # Wire dependencies ----------------------------------------------------
    frust_cull.add_child(subtile_skip)
    subtile_skip.add_child(blend)
    frust_cull.add_child(shading)
    shading.add_child(blend)
    frust_cull.add_child(sorter)
    sorter.add_child(blend)

    return g 


# ---------------------------------------------------------------------------
#  Optimisation operator: Sub‑tile Skipping
# ---------------------------------------------------------------------------


class SubtileSkippingOperator(OptimizationOperator):
    """Determine which 2×2 sub‑tiles of each 16×16 tile are impacted by a Gaussian.

    Inputs (conceptual):
        • Gaussian mean position (x, y) in screen space
        • Gaussian covariance / extent parameters (σx, σy)
        • Tile origin (tx, ty)

    Outputs:
        • 4‑bit mask per Gaussian indicating coverage of the four 8×8 sub‑tiles
          composing the parent 16×16 tile (i.e., TL, TR, BL, BR).
    """

    def __init__(self, dim, *, image_wh=(1280,720), tile: int = 16, tile_keep_ratio: float = 0.1,
                 bitwidth: int = 16, graph=None, **kwargs):
        self.image_w, self.image_h = image_wh
        self.tile = tile
        self.tile_keep = tile_keep_ratio
        super().__init__(dim, bitwidth, graph)
        self.op_type = "SubtileSkip"

    # ------------------------------------------------------------------
    #  Tensor sizes (rough approximations)
    # ------------------------------------------------------------------
    def get_tensors(self):
        P = self.dim[0]
        T = (self.image_w // self.tile) * (self.image_h // self.tile)
        gauss_per_tile = int(np.ceil(self.tile_keep * P))
        input_a = P * 4
        input_b = T * 4
        output = T * gauss_per_tile
        return input_a, input_b, output

    def get_num_ops(self):
        P = self.dim[0]
        T = (self.image_w // self.tile) * (self.image_h // self.tile)
        return P * T * 4

    # shape helpers ------------------------------------------------------
    def get_input_tensor_shapes(self):
        P = self.dim[0]
        T = (self.image_w // self.tile) * (self.image_h // self.tile)
        return [(P,4), (T,4)]

    def get_output_tensor_shape(self):
        P = self.dim[0]
        T = (self.image_w // self.tile) * (self.image_h // self.tile)
        gauss_per_tile = int(np.ceil(self.tile_keep * P))
        return (T, gauss_per_tile) 