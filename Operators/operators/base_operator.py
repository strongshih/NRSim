# operators/base_operator.py

from utils.unit import Unit

class Operator(object):
    def __init__(self, dim, bitwidth: int = 16, graph=None):
        self.dim = dim
        # Numerical precision (bits per element) set at instantiation time.
        # All subclasses and pipelines should now pass the desired precision
        # via this constructor argument instead of assigning to the attribute
        # after creation.
        self.bitwidth = bitwidth

        # --- Data‑flow graph bookkeeping --------------------------------
        # Each Operator can act as a node in a larger dependency graph.
        #   parents  : producers whose outputs feed this op
        #   children : consumers that take this op as input
        self.parents  = []  # type: list["Operator"]
        self.children = []  # type: list["Operator"]

        # Optional graph container; if provided we auto‑register.
        self.graph = graph
        if graph is not None and hasattr(graph, "nodes"):
            graph.nodes.add(self)

        self.input_a, self.input_w, self.output = self.get_tensors()
        self.num_ops = self.get_num_ops()

    def set_tensor(self, input_a=None, input_w=None, output=None):
        if input_a is not None:
            self.input_a = input_a
        if input_w is not None:
            self.input_w = input_w
        if output is not None:
            self.output = output

    def get_op_type(self):
        return self.op_type

    def get_tensors(self):
        # Default implementation derives tensor counts from shape helpers.
        # Subclasses may override for bespoke behaviour.
        try:
            import math
            in_shapes = self.get_input_tensor_shapes()
            out_shape = self.get_output_tensor_shape()

            if not in_shapes or out_shape is None:
                raise NotImplementedError

            # activation input (first), weight input (second if exists)
            input_a = math.prod(in_shapes[0])
            input_b = math.prod(in_shapes[1]) if len(in_shapes) > 1 else 0
            output = math.prod(out_shape)
            return input_a, input_b, output
        except NotImplementedError:
            raise NotImplementedError("Subclasses must implement get_tensors() or the shape helpers.")

    def get_num_ops(self):
        raise NotImplementedError("Subclasses must implement get_num_ops()")

    def get_effective_dim_len(self):
        raise NotImplementedError("Subclasses must implement get_effective_dim_len()")

    def get_ideal_compute_time(self, system):
        number_of_ops = self.get_num_ops()
        return number_of_ops / (system.op_per_sec * system.compute_efficiency)

    def get_ideal_memory_time(self, system):
        input_a, input_b, output = self.get_tensors()
        elem_bytes = system.bytes_per_elem
        input_a_read_time = (elem_bytes * input_a) / (system.offchip_mem_bw * system.memory_efficiency)
        input_b_read_time = (elem_bytes * input_b) / (system.offchip_mem_bw * system.memory_efficiency)
        output_write_time = (elem_bytes * output) / (system.offchip_mem_bw * system.memory_efficiency)
        return input_a_read_time + input_b_read_time + output_write_time

    def get_roofline(self, system):
        unit = Unit() 
        ideal_compute_time = self.get_ideal_compute_time(system)
        ideal_memory_time = self.get_ideal_memory_time(system)
        num_ops = self.get_num_ops()
        input_a_size, input_w_size, output_size = self.get_tensors()
        num_data = input_a_size + input_w_size + output_size
        op_intensity = num_ops / num_data

        exec_time = max(ideal_compute_time, ideal_memory_time)
        thrpt = num_ops / exec_time if exec_time else 0
        com_to_mem_ratio = ideal_compute_time / ideal_memory_time if ideal_memory_time else 0
        boundedness = 'C' if com_to_mem_ratio > 1 else 'M'
    
        return {
            'Op Type': self.get_op_type(),
            'Dimension': self.dim[:self.get_effective_dim_len()],
            'Bound': boundedness,
            'C/M ratio': com_to_mem_ratio,
            'Op Intensity': op_intensity,
            f'Latency ({unit.unit_time})': unit.raw_to_unit(exec_time, type='T'),
            f'Cycles': exec_time * system.frequency,
            f'Num ops ({unit.unit_flop})': unit.raw_to_unit(num_ops, type='O'),
            f'Input_a ({unit.unit_mem})': unit.raw_to_unit(input_a_size, type='M'),
            f'Input_w ({unit.unit_mem})': unit.raw_to_unit(input_w_size, type='M'),
            f'Output ({unit.unit_mem})': unit.raw_to_unit(output_size, type='M'),
            f'Total Data ({unit.unit_mem})': unit.raw_to_unit(num_data, type='M'),
            f'Throughput ({unit.unit_compute})': unit.raw_to_unit(thrpt, type='C'),
            f'Compute Cycles': ideal_compute_time * system.frequency,
            f'Memory Cycles': ideal_memory_time * system.frequency,
        }

    def __str__(self):
        return f"Operator type: {self.get_op_type()}, dim: {self.dim}"

    # ------------------------------------------------------------------
    #  Graph helpers
    # ------------------------------------------------------------------
    def add_child(self, child: "Operator"):
        """Declare *child* as consumer and assert tensor compatibility."""
        # Dimension compatibility check (optional)
        try:
            _, _, parent_out = self.get_tensors()
            child_in, _, _ = child.get_tensors()
            assert parent_out == child_in, (
                f"Tensor mismatch: {self.get_op_type()} outputs {parent_out} elements "
                f"but {child.get_op_type()} expects {child_in}")
        except Exception:
            # If get_tensors not properly initialised, skip
            pass

        # ------------------------------------------------------------------
        #  Helper functions – resolve first/last *leaf* operator
        # ------------------------------------------------------------------

        def _first_sub(op):
            """Return the first leaf operator inside *op* (recursively)."""
            while hasattr(op, "sub_ops") and op.sub_ops:
                op = op.sub_ops[0]
            return op

        def _last_sub(op):
            """Return the last leaf operator inside *op* (recursively)."""
            while hasattr(op, "sub_ops") and op.sub_ops:
                op = op.sub_ops[-1]
            return op

        # ------------------------------------------------------------------
        #  1. Register *high‑level* dependency (always)
        # ------------------------------------------------------------------
        if child not in self.children:
            self.children.append(child)
        if self not in child.parents:
            child.parents.append(self)

        # ------------------------------------------------------------------
        #  2. Also connect the *leaf* boundary nodes to ensure the fine graph
        #     correctly represents data‑flow when composites are expanded.
        # ------------------------------------------------------------------

        link_parent = _last_sub(self)
        link_child  = _first_sub(child)

        if link_child not in link_parent.children:
            link_parent.children.append(link_child)
        if link_parent not in link_child.parents:
            link_child.parents.append(link_parent)

    # New shape helper stubs ------------------------------------------------
    def get_input_tensor_shapes(self):
        """Return list of tuple shapes for each logical input tensor (A, W, ...)."""
        raise NotImplementedError

    def get_output_tensor_shape(self):
        """Return tuple shape for the output tensor."""
        raise NotImplementedError
