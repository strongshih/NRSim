import json
from graphviz import Digraph
import os
from typing import Optional

# colour map by category
COLOR_MAP = {
    "Sampling": "#FFD700",      # gold
    "Encoding": "#FF7F0E",      # orange
    "FieldCompute": "#1f77b4",  # blue
    "Blending": "#2ca02c",      # green
    "Optimization": "#d62728",  # red
    "Unknown": "#87CEEB",       # sky blue
}

# ---------------------------------------------------------------------------
#  Helper to build & plot
# ---------------------------------------------------------------------------
def _get_category(op):
    from operators.sampling_operator import SamplingOperator
    from operators.encoding_operator import EncodingOperator
    from operators.computation_operator import ComputationOperator
    from operators.blending_operator import BlendingOperator
    from operators.optimization_operator import OptimizationOperator

    if isinstance(op, SamplingOperator):
        return "Sampling"
    if isinstance(op, EncodingOperator):
        return "Encoding"
    if isinstance(op, ComputationOperator):
        return "FieldCompute"
    if isinstance(op, BlendingOperator):
        return "Blending"
    if isinstance(op, OptimizationOperator):
        return "Optimization"
    return "Unknown"

def _build_graphviz(nodes):
    """Return Graphviz Digraph constructed from operator nodes.

    If a node has an attribute ``sub_ops`` (list of child operators) these are
    *recursively* expanded so they appear as separate nodes in the fine graph.
    Dashed edges are added from the composite node → first sub‑op and from the
    last sub‑op → composite node's children to preserve visual flow.
    """

    # ------------------------------------------------------------------
    #  Flatten top‑level node list, expanding any .sub_ops recursively
    # ------------------------------------------------------------------
    flat_nodes = []
    composites = []  # list of (composite_op, sub_ops)

    def _dfs(op):
        flat_nodes.append(op)
        subs = getattr(op, "sub_ops", None)
        if subs:
            composites.append((op, subs))
            for s in subs:
                _dfs(s)

    for n in nodes:
        _dfs(n)

    # Deduplicate while preserving order
    seen = set()
    flat_nodes_uni = []
    for n in flat_nodes:
        if n not in seen:
            flat_nodes_uni.append(n)
            seen.add(n)

    g = Digraph(graph_attr={"rankdir": "TB", "nodesep": "0.6", "ranksep": "0.7"})
    idx_map = {op: str(i) for i, op in enumerate(flat_nodes_uni)}

    # add nodes
    for op, idx in idx_map.items():
        label = op.get_op_type()
        # append extra info for certain ops
        from operators.computation_operator import MLPOperator
        from operators.encoding_operator import HashEncodingOperator, RFFEncodingOperator

        if isinstance(op, MLPOperator):
            label += f"\nL{op.num_layers}×{op.layer_width}"
        elif isinstance(op, HashEncodingOperator):
            label += f"\nLvls {op.num_levels}"
        elif 'RFFEncodingOperator' in type(op).__name__:
            try:
                label += f"\nF {op.num_features}"
            except AttributeError:
                pass

        if hasattr(op, "bitwidth"):
            label += f"\n{op.bitwidth}‑bit"
        cat = _get_category(op)
        g.node(
            idx,
            label=label,
            shape="circle",
            style="filled",
            fillcolor=COLOR_MAP.get(cat, COLOR_MAP["Unknown"]),
            fixedsize="true",
            width="1",
            height="1",
            fontsize="10"
        )

    # ------------------------------------------------------------------
    #  Helper lambdas for safe shape extraction
    # ------------------------------------------------------------------
    _shape_to_str = lambda shp: str(shp)

    def _safe_output_shape(node):
        if hasattr(node, "get_output_tensor_shape"):
            try:
                return _shape_to_str(node.get_output_tensor_shape())
            except NotImplementedError:
                pass
        # Fallback: use element count
        try:
            _, _, elems = node.get_tensors()
            return str(elems)
        except Exception:
            return "?"

    def _safe_input_shapes(node):
        if hasattr(node, "get_input_tensor_shapes"):
            try:
                shapes = node.get_input_tensor_shapes()
                if shapes:
                    return ", ".join(_shape_to_str(s) for s in shapes)
            except NotImplementedError:
                pass
        # Fallback: element count of first logical input
        try:
            elems, _, _ = node.get_tensors()
            return str(elems)
        except Exception:
            return "?"

    # ------------------------------------------------------------------
    #  Connect existing parent → child edges with rich labels
    # ------------------------------------------------------------------
    for op, idx in idx_map.items():
        for parent in getattr(op, "parents", []):
            if parent in idx_map:
                parent_label = _safe_output_shape(parent)
                child_label  = _safe_input_shapes(op)
                edge_label   = f"{parent_label} → {child_label}"
                g.edge(idx_map[parent], idx, label=edge_label, fontsize="8")

    # No extra dashed edges needed – leaf‑level wiring already handled.

    # ------------------------------------------------------------------
    #  Add pseudo Input/Output nodes for *top‑level* operators only
    # ------------------------------------------------------------------

    # Helper to reach leaf nodes inside composites
    def _first_leaf(o):
        while hasattr(o, "sub_ops") and o.sub_ops:
            o = o.sub_ops[0]
        return o

    def _last_leaf(o):
        while hasattr(o, "sub_ops") and o.sub_ops:
            o = o.sub_ops[-1]
        return o

    inner_nodes = set()
    for _, subs in composites:
        inner_nodes.update(subs)

    for op, idx in idx_map.items():
        if op in inner_nodes:
            continue  # skip internal sub‑ops

        # No parents → attach an "Input" node to first leaf
        if not getattr(op, "parents", []):
            pseudo_in_id = f"in_{idx}"
            in_label = _safe_input_shapes(op)
            g.node(pseudo_in_id, label="Input", shape="plaintext")
            first_leaf = _first_leaf(op)
            g.edge(pseudo_in_id, idx_map[first_leaf], label=in_label, fontsize="8")

        # No children → attach an "Output" node from last leaf
        if not getattr(op, "children", []):
            pseudo_out_id = f"out_{idx}"
            out_label = _safe_output_shape(op)
            g.node(pseudo_out_id, label="Output", shape="plaintext")
            last_leaf = _last_leaf(op)
            g.edge(idx_map[last_leaf], pseudo_out_id, label=out_label, fontsize="8")

    # ------------------------------------------------------------------
    #  Group composite operators into subgraph clusters
    # ------------------------------------------------------------------
    for ci, (comp, subs) in enumerate(composites):
        subg = Digraph(name=f"cluster_{ci}")
        subg.attr(style="rounded", color="gray")
        subg.attr(label=comp.get_op_type())

        # Optionally include an invisible node representing the composite
        comp_id = idx_map[comp]
        subg.node(comp_id, label="", shape="circle", width="0", height="0", style="invis")

        for s in subs:
            if s in idx_map:
                subg.node(idx_map[s])

        g.subgraph(subg)

    return g

class NodeList(list):
    """List-like container that provides an .add() method for compatibility with Operator auto-registration."""
    def add(self, node):
        self.append(node)


# ─────────────────────────────────────────────────────────────────────────────
#  Graph containers
# ─────────────────────────────────────────────────────────────────────────────
class OperatorGraph:
    """Minimal graph container to collect Operators as nodes."""
    def __init__(self):
        self.nodes = NodeList()

    # allow iteration directly over the graph
    def __iter__(self):
        return iter(self.nodes)

    def __len__(self):
        return len(self.nodes)

    # ------------------------------------------------------------------
    #  Visualisation
    # ------------------------------------------------------------------
    def plot_graph(self, title: str = "Operator Graph", save_path: Optional[str] = None):
        """Plot *coarse* operator DAG (one node per category)."""
        # Determine categories in order of first appearance
        cat_nodes = []
        cat_seen = {}
        for op in self.nodes:
            cat = _get_category(op)
            if cat not in cat_seen:
                node_id = f"cat_{len(cat_nodes)}"
                cat_seen[cat] = node_id
                cat_nodes.append((node_id, cat))

        g = Digraph(graph_attr={"rankdir": "TB", "nodesep": "0.8", "ranksep": "1.0"})

        # add category nodes
        for node_id, cat in cat_nodes:
            g.node(node_id, label=cat, shape="box", style="filled", fillcolor=COLOR_MAP.get(cat, COLOR_MAP["Unknown"]), width="1.2", height="0.6", fontsize="12")

        # add edges if any parent-child cross categories
        for op in self.nodes:
            src_cat = _get_category(op)
            for child in getattr(op, "children", []):
                dst_cat = _get_category(child)
                if src_cat != dst_cat:
                    g.edge(cat_seen[src_cat], cat_seen[dst_cat])

        if save_path:
            base, ext = os.path.splitext(save_path)
            if ext.lower() == ".png":
                save_base = base
            else:
                save_base = save_path
            g.render(save_base, format="png", cleanup=True)
            print(f"Saved pipeline graph → {save_base}.png")
        else:
            g.view()

    # ------------------------------------------------------------------
    # Convenience fine‑graph wrapper
    # ------------------------------------------------------------------
    def plot_fine_graph(self, title: str = "Fine Operator Graph", save_path: Optional[str] = None):
        """Plot detailed operator graph leveraging FineOperatorGraph."""
        fine = FineOperatorGraph()
        fine.nodes.extend(self.nodes)  # share same node objects
        fine.plot_graph(title, save_path)


# ------------------------------------------------------------------
#  Fine‑grained graph (inherits)
# ------------------------------------------------------------------

class FineOperatorGraph(OperatorGraph):
    """Plots all individual operators instead of coarse categories."""

    def plot_graph(self, title: str = "Fine Operator Graph", save_path: Optional[str] = None):
        g = _build_graphviz(self.nodes)

        # Legend same as before
        legend_rows = []
        cats_in_graph = { _get_category(op) for op in self.nodes }
        for cat in cats_in_graph:
            color = COLOR_MAP.get(cat, COLOR_MAP["Unknown"])
            legend_rows.append(f"<tr><td>{cat}</td><td port='c' bgcolor='{color}' width='20'></td></tr>")
        if legend_rows:
            legend_label = "<<table border='0' cellborder='1' cellspacing='0'>" + "".join(legend_rows) + "</table>>"
            g.node("legend", label=legend_label, shape="plaintext")

        if save_path:
            base, ext = os.path.splitext(save_path)
            if ext.lower() == ".png":
                save_base = base
            else:
                save_base = save_path
            g.render(save_base, format="png", cleanup=True)
            print(f"Saved fine pipeline graph → {save_base}.png")
        else:
            g.view() 