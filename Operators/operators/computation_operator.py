# operators/computation_operator.py

from operators.base_operator import Operator
from typing import Optional, Tuple, Literal
import torch.nn as nn
import numpy as np

class ComputationOperator(Operator):
    """Base class for all computation operators."""
    def __init__(self, dim, bitwidth: int = 16, graph=None):
        super().__init__(dim, bitwidth, graph)
        
    def get_effective_dim_len(self):
        return 2

class MLPOperator(ComputationOperator):
    def __init__(self, dim, in_dim: int, num_layers: int, layer_width: int, out_dim: Optional[int] = None,
                 skip_connections: Optional[Tuple[int]] = None,
                 activation: Optional[nn.Module] = nn.ReLU(), out_activation: Optional[nn.Module] = None,
                 implementation: Literal["tcnn", "torch"] = "torch",
                 use_bias: bool = True,
                 bitwidth: int = 16, graph=None):
        self.in_dim = in_dim
        self.num_layers = num_layers
        self.layer_width = layer_width
        self.out_dim = out_dim if out_dim is not None else layer_width
        self.skip_connections = skip_connections
        self.activation = activation
        self.out_activation = out_activation
        self.implementation = implementation
        self.use_bias = use_bias
        
        super().__init__(dim, bitwidth, graph)
        self.op_type = "MLP"

    def get_tensors(self):
        B, N = self.dim[:2]

        # (1) Runtime activations – always stream in
        input_a = B * N * self.in_dim      # element count

        # (2) Weight / bias parameters – stream or keep, decision is up to the scheduler
        # Determine parameter counts based on number of layers
        if self.num_layers == 1:
            # Single linear layer: in_dim -> out_dim
            weight_elems = self.in_dim * self.out_dim
        else:
            # First layer
            weight_elems = self.in_dim * self.layer_width
            # Hidden layers (only if there are > 2 layers total)
            if self.num_layers > 2:
                weight_elems += (self.num_layers - 2) * self.layer_width ** 2
            # Last layer
            weight_elems += self.layer_width * self.out_dim
        bias_elems = 0
        if self.use_bias:
            if self.num_layers == 1:
                bias_elems = self.out_dim
            else:
                bias_elems = self.layer_width * (self.num_layers - 1) + self.out_dim
        input_b = weight_elems + bias_elems

        # (3) Results
        output  = B * N * self.out_dim

        return input_a, input_b, output

    def get_num_ops(self):
        B, N = self.dim[:2]
        # FLOP count (multiply+add per weight element)
        if self.num_layers == 1:
            ops = B * N * 2 * self.in_dim * self.out_dim
        else:
            ops = B * N * 2 * self.in_dim * self.layer_width
            if self.num_layers > 2:
                ops += B * N * 2 * self.layer_width * self.layer_width * (self.num_layers - 2)
            ops += B * N * 2 * self.layer_width * self.out_dim
        return ops

    def get_input_tensor_shapes(self):
        """Return shapes for activation input and flattened parameter tensor."""
        B, N = self.dim[:2]
        activ_shape = (B, N, self.in_dim)
        # Re‑use same logic as in get_tensors to count params
        if self.num_layers == 1:
            weight_elems = self.in_dim * self.out_dim
        else:
            weight_elems = self.in_dim * self.layer_width
            if self.num_layers > 2:
                weight_elems += (self.num_layers - 2) * self.layer_width ** 2
            weight_elems += self.layer_width * self.out_dim
        bias_elems = 0
        if self.use_bias:
            if self.num_layers == 1:
                bias_elems = self.out_dim
            else:
                bias_elems = self.layer_width * (self.num_layers - 1) + self.out_dim
        param_shape = (weight_elems + bias_elems,)
        return [activ_shape, param_shape]

    def get_output_tensor_shape(self):
        B, N = self.dim[:2]
        return (B, N, self.out_dim)

class SphericalHarmonicsOperator(ComputationOperator):
    """Operator that evaluates SH basis and applies learned RGB weights."""

    def __init__(self, dim, degree: int = 4, implementation: Literal["tcnn", "torch"] = "torch", bitwidth: int = 16, graph=None):
        """Create a SH operator.

        Args:
            dim: Tuple containing (B, N)
            degree: Spherical‑harmonics degree L.  Total basis functions = (L+1)^2.
            implementation: Placeholder backend hint.
        """
        self.degree = degree
        self.implementation = implementation

        # number of basis functions (per colour channel)
        self.num_basis = (degree + 1) ** 2  # (L+1)^2

        super().__init__(dim, bitwidth, graph)
        self.op_type = "SphericalHarmonics"

    def get_tensors(self):
        B, N = self.dim[:2]

        # Input directions (x,y,z) per sample
        input_a = B * N * 3

        # Learned SH coefficients  —  one RGB triplet per basis function
        # These are model parameters analogous to MLP weights;
        # loaded once per invocation, independent of (B,N).
        input_b = self.num_basis * 3

        # Output RGB per sample
        output = B * N * 3

        return input_a, input_b, output

    def get_num_ops(self):
        B, N = self.dim[:2]

        # For each sample we:
        #   (1) compute SH basis values  -> cost ~ 4 mult/add per basis (empirical)
        #   (2) multiply each basis by a weight and accumulate into RGB  (MAC)
        ops_basis = 4 * self.num_basis              # basis evaluation (x*x, x*y, x*z, y*y, y*z, z*z, etc)
        ops_mac   = 2 * self.num_basis * 3          # multiply+add for RGB accumulation
        ops_per_sample = ops_basis + ops_mac

        return B * N * ops_per_sample

    def get_output_tensor_shape(self):
        B, N = self.dim[:2]
        return (B, N, 3)

    def get_input_tensor_shapes(self):
        B, N = self.dim[:2]
        return [(B, N, 3), (self.num_basis * 3,)]
