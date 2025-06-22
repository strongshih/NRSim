# operators/sampling_operator.py

from operators.base_operator import Operator
from typing import Literal, Optional, Tuple
import numpy as np

class SamplingOperator(Operator):
    """Base class for all sampling operators."""
    def __init__(self, dim, bitwidth: int = 16, graph=None):
        super().__init__(dim, bitwidth, graph)
        
    def get_effective_dim_len(self):
        return 2  # assume dim is (B, N)

class UniformSamplerOperator(SamplingOperator):
    def __init__(self, dim, sampler_type: Literal["uniform", "pdf"] = "uniform", bitwidth: int = 16, graph=None):
        self.sampler_type = sampler_type
        super().__init__(dim, bitwidth, graph)
        self.op_type = f"{sampler_type.capitalize()}Sampler"

    def get_tensors(self):
        B, N = self.dim[:2]
        input_a = B          # one activation per ray (e.g., ray origin/ID)
        input_b = 0
        output = B * N       # generated sample positions/IDs
        return input_a, input_b, output

    def get_num_ops(self):
        B, N = self.dim[:2]
        return B * N

    # shape helpers ------------------------------------------------------
    def get_input_tensor_shapes(self):
        B, _ = self.dim[:2]
        return [(B,)]  # one value per ray

    def get_output_tensor_shape(self):
        B, N = self.dim[:2]
        return (B, N)

class FrustrumCullingOperator(SamplingOperator):
    """Culls invisible Gaussians based on camera frustum.

    Args:
        dim (Tuple[int,int]): (B, G) where *G* is the total number of Gaussians before culling.
        fov, near, far: Camera frustum parameters.
        conservative: Whether to keep Gaussians close to frustum border.
        cull_ratio: Expected keep ratio 0‒1 (after near‑plane test).
    """
    def __init__(self, dim: Tuple[int], *,              # dim[0] == G
                 fov=60.0, near=0.1, far=100.0,
                 conservative=False, cull_ratio=0.5):
        """
        Args:
            dim: The dimensions of the input/output tensors
            fov: Field of view of the camera in degrees
            near: Near plane distance
            far: Far plane distance
            conservative: Whether to use conservative culling (keeps points near frustum boundaries)
            cull_ratio: Expected keep ratio 0‒1 (after near‑plane test)
        """
        self.fov = fov
        self.near = near
        self.far = far
        self.conservative = conservative
        self.cull_ratio = cull_ratio
        
        super().__init__(dim)          # now dim has length‑1
        self.op_type = "FrustumCulling"
    
    def get_tensors(self):
        G = self.dim[0]
        G_keep = int(np.ceil(self.cull_ratio * G))
        input_a = G * 3
        input_b = 16
        output  = G_keep
        return input_a, input_b, output
    
    def get_num_ops(self):
        G = self.dim[0]
        # Matrix multiplication for transforming points + frustum plane checks
        transform_ops = G * 4 * 4  # Transform each Gaussian mean
        frustum_test_ops = G * 1   # Near‑plane depth test (one compare)
        return transform_ops + frustum_test_ops

    # shape helpers ------------------------------------------------------
    def get_input_tensor_shapes(self):
        G = self.dim[0]
        return [(G, 3), (4, 4)]

    def get_output_tensor_shape(self):
        G = self.dim[0]
        G_keep = int(np.ceil(self.cull_ratio * G))
        return (G_keep, 1)

class ProjectionOperator(SamplingOperator):
    """Projects surviving 3‑D Gaussian centres to 2‑D image space."""

    def __init__(self, dim: Tuple[int], *, width: int = 800, height: int = 600,
                 projection_type: Literal["perspective", "orthographic"] = "perspective"):
        """
        Args:
            dim: (P,) where *P* is the number of input points to project.
            width / height: Target image resolution.
            projection_type: Perspective (default) or orthographic.
        """
        self.width  = width
        self.height = height
        self.projection_type = projection_type

        super().__init__(dim)
        self.op_type = "Projection"

    def get_tensors(self):
        P = self.dim[0]
        input_a = P * 9   # 3‑D mean (3) + covariance (6)
        input_b = 16      # 4×4 projection matrix
        output  = P * 5   # 2‑D mean (2) + covariance (3)
        return input_a, input_b, output

    def get_num_ops(self):
        P = self.dim[0]
        # Mean projection: 4×4 × (x,y,z,1)   ≈ 32 FLOPs
        mean_ops = 32 * P
        # Covariance projection (approx)  : 36 FLOPs per Gaussian (2×3 J · C · J^T)
        cov_ops  = 36 * P
        # Perspective divide + raster mapping: 4 FLOPs per Gaussian
        map_ops  = 4 * P
        return mean_ops + cov_ops + map_ops

    # shape helpers ------------------------------------------------------
    def get_input_tensor_shapes(self):
        P = self.dim[0]
        return [(P, 9), (4, 4)]  # mean+cov , projection matrix

    def get_output_tensor_shape(self):
        P = self.dim[0]
        return (P, 5)

class PDFSamplerOperator(UniformSamplerOperator):
    """Importance / PDF‑based sampler variant (inherits all behaviour)."""
    def __init__(self, dim, bitwidth: int = 16, graph=None):
        super().__init__(dim, sampler_type="pdf", bitwidth=bitwidth, graph=graph)
        # Override op_type explicitly for clarity
        self.op_type = "PDFSampler"

class AABBIntersectionOperator(SamplingOperator):
    """Determine which fixed‑size tiles intersect each Gaussian using axis‑aligned bounding boxes (AABB)."""

    def __init__(self, dim: Tuple[int], *, image_wh: Tuple[int, int] = (1280, 720), tile: int = 16,
                 tile_keep_ratio: float = 0.1):
        """
        Args:
            dim: (P,)  number of projected Gaussians.
            image_wh: Resolution (width, height) in pixels.
            tile: Square tile size in pixels (e.g., 16).
            tile_keep_ratio: Expected fraction of tiles a Gaussian overlaps (empirical).
        """
        self.image_w, self.image_h = image_wh
        self.tile      = tile
        self.tile_keep = tile_keep_ratio
        super().__init__(dim)
        self.op_type = "AABBIntersection"

    # ------------------------------------------------------------------
    # Tensor bookkeeping
    # ------------------------------------------------------------------
    def _num_tiles(self):
        return (self.image_w // self.tile) * (self.image_h // self.tile)

    def get_tensors(self):
        P = self.dim[0]
        T = self._num_tiles()
        gauss_per_tile = int(np.ceil(self.tile_keep * P))
        input_a = P * 5          # projected mean+cov (ellipse) per Gaussian
        input_b = T * 4          # xy‑bounds of each tile (static)
        output  = T * gauss_per_tile  # per‑tile list of Gaussian IDs
        return input_a, input_b, output

    def get_num_ops(self):
        P = self.dim[0]
        T = self._num_tiles()
        # AABB test: 4 comparisons per Gaussian‑tile pair
        return P * T * 4

    def get_input_tensor_shapes(self):
        P = self.dim[0]
        T = self._num_tiles()
        return [(P, 5), (T, 4)]

    def get_output_tensor_shape(self):
        P = self.dim[0]
        T = self._num_tiles()
        gauss_per_tile = int(np.ceil(self.tile_keep * P))
        return (T, gauss_per_tile)


class OBBIntersectionOperator(AABBIntersectionOperator):
    """Tile‑intersection using oriented bounding boxes (ellipse‑aware)."""

    def __init__(self, dim: Tuple[int], *, image_wh: Tuple[int, int] = (1280, 720), tile: int = 16,
                 tile_keep_ratio: float = 0.05):
        # OBB intersection is tighter; default keep ratio lower
        super().__init__(dim, image_wh=image_wh, tile=tile, tile_keep_ratio=tile_keep_ratio)
        self.op_type = "OBBIntersection"

    def get_num_ops(self):
        P = self.dim[0]
        T = self._num_tiles()
        # OBB / ellipse test: ~10 FLOPs per Gaussian‑tile pair (more math)
        return P * T * 10

# ---------------------------------------------------------------------------
#   Composite operator (culling + projection + intersection)
# ---------------------------------------------------------------------------
class FrustumCullingProjectionOperator(SamplingOperator):
    """High‑level sampling operator that performs frustum culling → projection → tile intersection.

    Internally instantiates
        1. FrustumCullingOperator
        2. ProjectionOperator
        3. AABB or OBB intersection operator

    and aggregates their FLOPs / tensor counts similar to *HashEncodingOperator* design.
    """

    def __init__(self,
                 dim: Tuple[int],                # (G,) number of gaussians
                 *,
                 cull_ratio: float = 0.5,
                 image_wh: Tuple[int, int] = (1280, 720),
                 tile: int = 16,
                 tile_keep_ratio: float = 0.1,
                 use_obb: bool = False,
                 bitwidth: int = 16,
                 graph=None):
        self.cull_ratio      = cull_ratio
        self.image_wh        = image_wh
        self.tile            = tile
        self.tile_keep_ratio = tile_keep_ratio
        self.use_obb         = use_obb

        super().__init__(dim, bitwidth, graph)
        self.op_type = "FrustumCullProj"

    # ------------------------------------------------------------------
    #  Internal helper to build composite sub‑operators
    # ------------------------------------------------------------------
    def _build_sub_ops(self):
        """Instantiate (and cache) the sequence of sub‑operators that make up
        the composite operation:

            1. FrustumCullingOperator
            2. ProjectionOperator
            3. (A|O)BBIntersectionOperator

        The resulting list is cached to avoid re‑creating objects for every
        query of tensor counts, FLOPs, shapes, etc.
        """

        # Return early if we already built them once
        if hasattr(self, "sub_ops") and self.sub_ops is not None:
            return self.sub_ops

        # ------------------------------------------------------------------
        #  Determine intermediate dimensions
        # ------------------------------------------------------------------
        G = self.dim[0]                          # total Gaussians
        G_keep = int(np.ceil(self.cull_ratio * G))  # after frustum culling

        # 1) Frustum culling --------------------------------------------------
        cull_op = FrustrumCullingOperator(
            (G,),
            cull_ratio=self.cull_ratio,
        )

        # 2) Projection -------------------------------------------------------
        proj_op = ProjectionOperator(
            (G_keep,),
            width=self.image_wh[0],
            height=self.image_wh[1],
        )

        # 3) Tile intersection ------------------------------------------------
        inter_op = AABBIntersectionOperator(
            (G_keep,),
            image_wh=self.image_wh,
            tile=self.tile,
            tile_keep_ratio=self.tile_keep_ratio,
        )
        
        inter_op2 = OBBIntersectionOperator(
            (G_keep,),
            image_wh=self.image_wh,
            tile=self.tile,
            tile_keep_ratio=self.tile_keep_ratio,
        )

        # Cache and return ----------------------------------------------------
        self.sub_ops = [cull_op, proj_op, inter_op, inter_op2]

        # Wire dependencies internally for plotting clarity
        cull_op.add_child(proj_op)
        proj_op.add_child(inter_op)
        inter_op.add_child(inter_op2)

        return self.sub_ops

    # ------------------------------------------------------------------
    # Delegated metrics
    # ------------------------------------------------------------------
    def get_num_ops(self):
        sub_ops = self._build_sub_ops()
        return sum(op.get_num_ops() for op in sub_ops)

    def get_tensors(self):
        sub_ops = self._build_sub_ops()
        # Input counts from first, output from last
        in_a, in_b, _ = sub_ops[0].get_tensors()
        _, _, out_t   = sub_ops[-1].get_tensors()
        total_a = sum(op.get_tensors()[0] for op in sub_ops)
        total_b = sum(op.get_tensors()[1] for op in sub_ops)
        return total_a, total_b, out_t

    # shape helpers
    def get_input_tensor_shapes(self):
        return self._build_sub_ops()[0].get_input_tensor_shapes()

    def get_output_tensor_shape(self):
        return self._build_sub_ops()[-1].get_output_tensor_shape()
