# operators/encoding_operator.py

from operators.base_operator import Operator
from typing import Optional, Literal
import numpy as np

class EncodingOperator(Operator):
    """Base class for all encoding operators."""
    def __init__(self, dim, bitwidth: int = 16, graph=None):
        super().__init__(dim, bitwidth, graph)
        
    def get_effective_dim_len(self):
        return 2

class HashEncodingOperator(EncodingOperator):
    def __init__(self, dim,
                 input_dim: int = 3,
                 num_levels: int = 16,
                 min_res: int = 16,
                 max_res: int = 1024,
                 log2_hashmap_size: int = 19,
                 features_per_level: int = 2,
                 hash_init_scale: float = 0.001,
                 implementation: Literal["tcnn", "torch"] = "tcnn",
                 interpolation: Optional[Literal["Nearest", "Linear", "Smoothstep"]] = None,
                 bitwidth: int = 16,
                 graph=None):
        self.input_dim = input_dim
        self.num_levels = num_levels
        self.min_res = min_res
        self.max_res = max_res
        self.log2_hashmap_size = log2_hashmap_size
        self.features_per_level = features_per_level
        self.hash_init_scale = hash_init_scale
        self.implementation = implementation
        self.interpolation = interpolation
        
        # Cache placeholder for lazily‑constructed stage operators
        self.sub_ops = None  # populated on first call to _build_sub_ops()

        super().__init__(dim, bitwidth, graph)
        self.op_type = "HashEncoding"

    def get_tensors(self):
        B, N = self.dim[:2]
        input_a = B * N * self.input_dim
        input_b = 0
        output = B * N * (self.num_levels * self.features_per_level)
        return input_a, input_b, output

    # ------------------------------------------------------------------
    #  Internal helper mirroring SamplingOperator design
    # ------------------------------------------------------------------
    def _build_sub_ops(self):
        """Lazily construct and cache the three stage operators used to
        implement the hash‑grid encoding (index generation → hash lookup →
        interpolation).  This mirrors the design used in
        *FrustumCullingProjectionOperator* for sampling.
        """

        if getattr(self, "sub_ops", None) is not None:
            return self.sub_ops

        self.sub_ops = [
            IndexGenerationOperator(self.dim,
                                     input_dim=self.input_dim,
                                     num_levels=self.num_levels,
                                     min_res=self.min_res,
                                     max_res=self.max_res,
                                     log2_hashmap_size=self.log2_hashmap_size,
                                     features_per_level=self.features_per_level,
                                     hash_init_scale=self.hash_init_scale,
                                     implementation=self.implementation,
                                     interpolation=self.interpolation),
            HashLookupOperator(self.dim,
                               input_dim=self.input_dim,
                               num_levels=self.num_levels,
                               min_res=self.min_res,
                               max_res=self.max_res,
                               log2_hashmap_size=self.log2_hashmap_size,
                               features_per_level=self.features_per_level,
                               hash_init_scale=self.hash_init_scale,
                               implementation=self.implementation,
                               interpolation=self.interpolation),
            InterpolationOperator(self.dim,
                                  input_dim=self.input_dim,
                                  num_levels=self.num_levels,
                                  min_res=self.min_res,
                                  max_res=self.max_res,
                                  log2_hashmap_size=self.log2_hashmap_size,
                                  features_per_level=self.features_per_level,
                                  hash_init_scale=self.hash_init_scale,
                                  implementation=self.implementation,
                                  interpolation=self.interpolation),
        ]

        # Wire internal dependencies: IndexGeneration → HashLookup → Interpolation
        for a, b in zip(self.sub_ops, self.sub_ops[1:]):
            a.add_child(b)

        return self.sub_ops

    def get_num_ops(self):
        """Aggregate FLOP counts from the three modular stages."""
        return sum(op.get_num_ops() for op in self._build_sub_ops())

    # shape helpers
    def get_input_tensor_shapes(self):
        B, N = self.dim[:2]
        return [(B, N, self.input_dim)]

    def get_output_tensor_shape(self):
        B, N = self.dim[:2]
        return (B, N, self.num_levels * self.features_per_level)

class IndexGenerationOperator(HashEncodingOperator):
    """Generate integer hash keys for each level."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.op_type = "IndexGeneration"

    def get_num_ops(self):
        B, N = self.dim[:2]
        # For each sample & level we compute 8 corner indices + 8 weights.
        # Rough cost estimate: ~6 FLOPs per corner (scale, floor, hash, etc.)
        # winterp = (1 − |xs − xv |) · (1 − |ys − yv |) · (1 − |zs − zv |)
        generate_weights = 5 * (B * N * self.num_levels * 8)
        # hash = (xv · 1) ⊕ (yv · P1) ⊕ (zv · P2) mod T
        generate_address = 2 * (B * N * self.num_levels * 8)
        return generate_weights + generate_address

    def get_tensors(self):
        """Coordinates in → hashed indices (8 per level) + weights (8 per level)."""
        B, N = self.dim[:2]
        input_a = B * N * self.input_dim  # xyz per sample
        input_b = 0
        # 8 indices  + 8 weights  per level per sample
        output  = B * N * self.num_levels * 8 * 2
        return input_a, input_b, output

    # shape helpers
    def get_input_tensor_shapes(self):
        B, N = self.dim[:2]
        # One logical activation input: 3‑D coordinates
        return [(B, N, self.input_dim)]

    def get_output_tensor_shape(self):
        B, N = self.dim[:2]
        return (B, N, self.num_levels * 8 * 2)

class HashLookupOperator(HashEncodingOperator):
    """Hash‑table lookup stage."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.op_type = "HashLookup"

    def get_num_ops(self):
        B, N = self.dim[:2]
        # One memory read per corner-feature fetch; negligible arithmetic.
        return 0 # 1 * (B * N * self.num_levels * 8 * self.features_per_level)

    def get_tensors(self):
        """Hashed corner indices in → corner features out."""
        B, N = self.dim[:2]
        # 8 indices per level (no features yet)
        input_a = B * N * self.num_levels * 8
        # Static hash‑table parameters
        hash_table_size = (1 << self.log2_hashmap_size) * self.features_per_level * self.num_levels
        input_b = hash_table_size
        # 8 corners × features per level
        output  = B * N * self.num_levels * 8 * self.features_per_level
        return input_a, input_b, output

    # shape helpers
    def get_input_tensor_shapes(self):
        B, N = self.dim[:2]
        indices_shape      = (B, N, self.num_levels * 8)
        hash_table_size    = (1 << self.log2_hashmap_size) * self.features_per_level * self.num_levels
        hash_table_shape   = (hash_table_size,)  # 1‑D flattened parameter tensor
        return [indices_shape, hash_table_shape]

    def get_output_tensor_shape(self):
        B, N = self.dim[:2]
        return (B, N, self.num_levels * 8 * self.features_per_level)

class InterpolationOperator(HashEncodingOperator):
    """Tri‑linear or smoothstep interpolation of the fetched grid values."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.op_type = "Interpolation"

    def get_num_ops(self):
        B, N = self.dim[:2]
        # Combine 8 corners → 1 value (features_per_level) : 8 mul‑adds per feature
        return 8 * 2 * (B * N * self.num_levels * self.features_per_level)  # multiply+add

    def get_tensors(self):
        """Corner features + weights in → interpolated features out."""
        B, N = self.dim[:2]
        # Corner features
        corner_feats = B * N * self.num_levels * 8 * self.features_per_level
        input_a = corner_feats
        # Weights (8 per level per sample)
        input_b = B * N * self.num_levels * 8
        # Final interpolated features (1 per level per feature)
        output  = B * N * self.num_levels * self.features_per_level
        return input_a, input_b, output

    # shape helpers
    def get_input_tensor_shapes(self):
        B, N = self.dim[:2]
        corner_feats_shape = (B, N, self.num_levels * 8 * self.features_per_level)
        weights_shape      = (B, N, self.num_levels * 8)
        return [corner_feats_shape, weights_shape]

    def get_output_tensor_shape(self):
        B, N = self.dim[:2]
        return (B, N, self.num_levels * self.features_per_level)

class PointArrangeOperator(HashEncodingOperator):
    """Optional stage executed *after* interpolation (e.g., feature re‑ordering)."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.op_type = "PointArrange"

    def get_num_ops(self):
        """Estimate cost of scalar‑mapping and bucket sorting.

        1. Scalar conversion per point: y*P2 + z*P3 + x   → 2 multiplies + 2 adds ≈ 4 FLOPs.
        2. Bucket sort: O(M log M) comparisons, where M = N (samples per ray).
        """
        B, N = self.dim[:2]
        # (1) scalar mapping FLOPs
        scalar_ops = 4 * B * N
        # (2) sorting comparisons
        sort_ops = 2 * B * N * np.log2(B * N) # different rays also need to be sorted
        return scalar_ops + sort_ops

    def get_tensors(self):
        """Coordinates in → rearranged coordinates out (same size)."""
        B, N = self.dim[:2]
        input_a = B * N * self.input_dim  # original coordinates
        input_b = 0  # no learnable weights
        output  = input_a                # same data volume, reordered
        return input_a, input_b, output

    # shape helpers
    def get_input_tensor_shapes(self):
        B, N = self.dim[:2]
        return [(B, N, self.input_dim)]

    def get_output_tensor_shape(self):
        B, N = self.dim[:2]
        return (B, N, self.input_dim)

class RFFEncodingOperator(EncodingOperator):
    """Random Fourier Features encoding operator that maps input coordinates to a higher dimension using random projections."""
    def __init__(self, dim, *, input_dim: int = 3, num_features: int = 256, scale: float = 1.0, 
                 use_sin_cos: bool = True, implementation: Literal["tcnn", "torch"] = "torch", 
                 bitwidth: int = 16, graph=None):
        """
        Args:
            dim: The dimensions of the input/output tensors
            input_dim: Dimensionality of input coordinates (typically 3 for xyz)
            num_features: Number of random frequencies to generate
            scale: Scaling factor for the frequency distribution
            use_sin_cos: Whether to use both sin and cos (doubles output dimension)
            implementation: Which implementation to use
        """
        self.input_dim = input_dim
        self.num_features = num_features
        self.scale = scale
        self.use_sin_cos = use_sin_cos
        self.implementation = implementation
        self.output_dim = num_features * 2 if use_sin_cos else num_features
        
        super().__init__(dim, bitwidth, graph)
        self.op_type = "RFFEncoding"
    
    def get_tensors(self):
        B, N = self.dim[:2]
        input_a = B * N * self.input_dim  # Input coordinates
        # Projection matrix (input_dim × num_features) acts like weight parameters
        input_b = self.input_dim * self.num_features
        output = B * N * self.output_dim  # Encoded features
        return input_a, input_b, output
    
    def get_num_ops(self):
        B, N = self.dim[:2]
        # Matrix multiplication for projection + sin/cos operations
        mat_mul_ops = B * N * self.input_dim * self.num_features * 2  # 2 ops per multiply-add
        sin_cos_ops = B * N * self.output_dim  # 1 op per sin/cos function
        return mat_mul_ops + sin_cos_ops

    # shape helpers
    def get_input_tensor_shapes(self):
        B, N = self.dim[:2]
        return [(B, N, self.input_dim), (self.input_dim, self.num_features)]

    def get_output_tensor_shape(self):
        B, N = self.dim[:2]
        return (B, N, self.output_dim)
