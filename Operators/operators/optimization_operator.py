from operators.base_operator import Operator


class OptimizationOperator(Operator):
    """Base class for optimisation‑stage operators that can be inserted between major pipeline stages (Encoding, Field Compute, Blending, etc.).

    Concrete optimisation operators (e.g., sensitivity prediction, data recovery) are defined elsewhere so that this
    module remains lightweight and import‑cycle free.
    """

    def __init__(self, dim, bitwidth: int = 16, graph=None):
        super().__init__(dim, bitwidth, graph)

    def get_effective_dim_len(self):
        return 2

    def get_input_tensor_shapes(self):
        raise NotImplementedError

    def get_output_tensor_shape(self):
        raise NotImplementedError