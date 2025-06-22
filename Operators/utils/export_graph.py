import json
from typing import List

from operators.base_operator import Operator
from operators.sampling_operator import SamplingOperator
from operators.encoding_operator import EncodingOperator
from operators.computation_operator import ComputationOperator
from operators.blending_operator import BlendingOperator


def get_category(op: Operator) -> str:
    if isinstance(op, SamplingOperator):
        return "Sampling"
    if isinstance(op, EncodingOperator):
        return "Encoding"
    if isinstance(op, ComputationOperator):
        return "FieldCompute"
    if isinstance(op, BlendingOperator):
        return "Blending"
    return "Unknown"


def pipeline_to_json(pipeline: List[Operator], out_path: str):
    """Serialize the python pipeline to a JSON file that can be ingested by the C++ scheduler.

    Each operator is serialized as a dict with:
        id: position index in pipeline
        type: high‑level category (Sampling, Encoding, FieldCompute, Blending)
        sub_type: op.get_op_type() value (e.g. HashEncoding)
        deps: list of integer IDs that must finish before this op starts (sequential chain for now)
    """
    graph = []
    id_map = {op: idx for idx, op in enumerate(pipeline)}
    for idx, op in enumerate(pipeline):
        # Determine deps; ignore parents not present in id_map (may belong to nested sub-ops not exported)
        if getattr(op, "parents", None):
            deps = [id_map[p] for p in getattr(op, "parents") if p in id_map]
        else:
            deps = getattr(op, "deps", [idx - 1] if idx > 0 else [])

        node = {
            "id": idx,
            "type": get_category(op),
            "sub_type": op.get_op_type(),
            "deps": deps,
        }
        # --- Extended metadata -------------------------------------------------
        bw = getattr(op, "bitwidth", 16)
        node["bitwidth"] = bw
        try:
            input_a, input_b, output = op.get_tensors()
            node["bytes_in"] = int((input_a + input_b) * bw // 8)
            node["bytes_out"] = int(output * bw // 8)
        except Exception:
            node["bytes_in"] = node["bytes_out"] = 0
        try:
            node["num_ops"] = int(op.get_num_ops())
        except Exception:
            node["num_ops"] = 0
        # ----------------------------------------------------------------------
        graph.append(node)

    with open(out_path, "w") as f:
        json.dump(graph, f, indent=2)

    print(f"Operator graph exported to {out_path}") 