# operators/blending_operator.py

from operators.base_operator import Operator
from typing import Literal
import numpy as np

class BlendingOperator(Operator):
    """Base class for all blending operators."""
    def __init__(self, dim, bitwidth: int = 16, graph=None):
        super().__init__(dim, bitwidth, graph)
        
    def get_effective_dim_len(self):
        return 2

    def get_input_tensor_shapes(self):
        B, N = self.dim[:2]
        return [(B, N, 3), (B, N, 1)]

    def get_output_tensor_shape(self):
        B, _ = self.dim[:2]
        return (B, 3)

class RGBRendererOperator(BlendingOperator):
    def __init__(self, dim, background_color="random", bitwidth: int = 16, graph=None):
        self.background_color = background_color
        super().__init__(dim, bitwidth, graph)
        self.op_type = "RGBRenderer"

    def get_tensors(self):
        B, N = self.dim[:2]
        input_a = B * N * 3
        input_b = B * N * 1
        output = B * 3
        return input_a, input_b, output

    def get_num_ops(self):
        B, N = self.dim[:2]
        return B * N * 3 * 2

class DensityRendererOperator(BlendingOperator):
    def __init__(self, dim, method: Literal["median", "expected"] = "median", bitwidth: int = 16, graph=None):
        self.method = method
        super().__init__(dim, bitwidth, graph)
        self.op_type = "DensityRenderer"

    def get_tensors(self):
        B, N = self.dim[:2]
        input_a = B * N * 1
        input_b = B * N * 1
        output = B * 1
        return input_a, input_b, output

    def get_num_ops(self):
        B, N = self.dim[:2]
        return B * N

    def get_input_tensor_shapes(self):
        B, N = self.dim[:2]
        return [(B, N, 1), (B, N, 1)]

    def get_output_tensor_shape(self):
        B, _ = self.dim[:2]
        return (B, 1)

class SortingOperator(BlendingOperator):
    """Operator that sorts samples by depth or other criteria before blending."""
    def __init__(self, dim, sort_by: Literal["depth", "opacity", "contribution"] = "depth", 
                 order: Literal["ascending", "descending"] = "ascending", bitwidth: int = 16, graph=None):
        self.sort_by = sort_by
        self.order = order
        super().__init__(dim, bitwidth, graph)
        self.op_type = "Sorting"
        
    def get_tensors(self):
        B, N = self.dim[:2]
        input_a = B * N * 1  # Values to sort by (e.g., depth values)
        input_b = B * N * 3  # Values to be sorted (e.g., RGB values)
        output = B * N * 3   # Sorted values
        return input_a, input_b, output
        
    def get_num_ops(self):
        B, N = self.dim[:2]
        # Approximate complexity for sorting is O(N log N)
        return B * N * np.log2(N)

    def get_input_tensor_shapes(self):
        B, N = self.dim[:2]
        return [(B, N, 1), (B, N, 3)]

    def get_output_tensor_shape(self):
        B, N = self.dim[:2]
        return (B, N, 3)

class GaussianAlphaBlendOperator(BlendingOperator):
    def __init__(self, dim, bitwidth: int = 16, graph=None):
        super().__init__(dim, bitwidth, graph)
        self.op_type = "GaussianAlphaBlend"

    def get_tensors(self):
        B, N = self.dim[:2]
        input_a = B * N * 3   # per‑gaussian RGB
        input_b = B * N * 1   # per‑gaussian α  (already premultiplied by transmittance)
        output  = B * 3       # final pixel colour
        return input_a, input_b, output

    def get_num_ops(self):
        B, N = self.dim[:2]
        return B * N * 3 * 2  # multiply‑add per channel

    def get_input_tensor_shapes(self):
        B, N = self.dim[:2]
        return [(B, N, 3), (B, N, 1)]

    def get_output_tensor_shape(self):
        B, _ = self.dim[:2]
        return (B, 3)