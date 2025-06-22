import builtins
import atexit
import networkx as nx
import os
import pickle
import json
import importlib
from functools import wraps
import matplotlib.pyplot as plt
import inspect
import torch  # Import torch to handle torch.Size
import time
import contextlib
import hashlib  # Import hashlib for consistent hashing
from pathlib import Path

# === Custom Print Setup Start ===

# Open log.txt in append mode so that logs are preserved across multiple runs
log_file_path = "log.txt"  # You can specify an absolute path if needed
try:
    log_file = open(log_file_path, "a")
except Exception as e:
    builtins.print(f"Failed to open log file '{log_file_path}': {e}")
    log_file = None  # Fallback if the file can't be opened

def custom_print(*args, **kwargs):
    """
    Custom print function that writes to log.txt instead of the console.
    If log.txt cannot be written, it falls back to the standard print.
    """
    if log_file:
        try:
            # Convert all arguments to strings and join them with spaces
            message = " ".join(str(arg) for arg in args)
            log_file.write(message + "\n")
            log_file.flush()  # Ensure it's written immediately
        except Exception as e:
            builtins.print(f"Failed to write to log file: {e}")
    else:
        # Fallback to standard print if log_file is not available
        builtins.print(*args, **kwargs)

# Override the print function in this module
print = custom_print

@atexit.register
def close_log_file():
    """
    Ensures that the log file is properly closed when the script exits.
    """
    global log_file
    if log_file:
        try:
            log_file.close()
        except Exception as e:
            builtins.print(f"Error closing log file: {e}")

# === Custom Print Setup End ===

# === Rest of your tracing.py code starts here ===

# Create a directed graph to represent the DAG of function/module dependencies
execution_dag = nx.DiGraph()
call_stack = []  # To keep track of call hierarchy

# Initialize a global dictionary to keep track of call counts for each function name
call_counts = {}

# Optional: Implement caching for unique IDs to improve performance
unique_id_cache = {}

def get_unique_id(obj):
    """
    Generates a unique ID for a given object based on its content.
    Handles torch.Tensor, list, tuple, dict, primitive types, and other objects.
    For torch.Tensor, uses a fingerprint based on specific statistics.
    For other objects, attempts to serialize and hash their content.
    Falls back to using the object's string representation if serialization fails.
    """
    obj_id = id(obj)
    
    # Check if the object has already been processed
    if obj_id in unique_id_cache:
        return unique_id_cache[obj_id]
    
    def tensor_fingerprint(tensor):
        if tensor.numel() == 0:
            raise ValueError("Cannot generate unique ID for an empty tensor.")
        tensor_sum = tensor.sum().item()
        tensor_mean = tensor.mean().item()
        tensor_max = tensor.max().item()
        tensor_min = tensor.min().item()
        flat_tensor = tensor.flatten()
        first_element = flat_tensor[0].item() if flat_tensor.numel() > 0 else 0
        last_element = flat_tensor[-1].item() if flat_tensor.numel() > 0 else 0
        return (tensor_sum, tensor_mean, tensor_max, tensor_min, first_element, last_element)

    def list_fingerprint(lst):
        return tuple(get_unique_id(item) for item in lst)

    def tuple_fingerprint(tpl):
        return tuple(get_unique_id(item) for item in tpl)

    def dict_fingerprint(dct):
        # Sort the dictionary by keys to ensure consistent ordering
        sorted_items = sorted(dct.items())
        return tuple((key, get_unique_id(value)) for key, value in sorted_items)

    def custom_object_fingerprint(obj):
        if hasattr(obj, '__dict__'):
            return get_unique_id(obj.__dict__)
        else:
            return str(obj)

    try:
        if isinstance(obj, torch.Tensor):
            stats = tensor_fingerprint(obj)
            fingerprint = ('torch.Tensor', stats)
        elif isinstance(obj, list):
            fingerprint = ('list', list_fingerprint(obj))
        elif isinstance(obj, tuple):
            fingerprint = ('tuple', tuple_fingerprint(obj))
        elif isinstance(obj, dict):
            fingerprint = ('dict', dict_fingerprint(obj))
        elif isinstance(obj, (int, float, str, bool)):
            fingerprint = ('primitive', obj)
        else:
            # Attempt to serialize the object using pickle
            try:
                serialized_obj = pickle.dumps(obj)
                fingerprint = ('serialized_object', serialized_obj)
            except (pickle.PicklingError, AttributeError, TypeError) as e:
                # Fallback to using the string representation and type
                fingerprint = ('unserializable_object', f"{type(obj).__name__}:{str(obj)}")

        # Convert the fingerprint to a bytes object for hashing
        if isinstance(fingerprint[1], tuple) or isinstance(fingerprint[1], list):
            # For nested fingerprints, serialize using JSON
            fingerprint_bytes = json.dumps(fingerprint, sort_keys=True, default=str).encode('utf-8')
        elif isinstance(fingerprint[1], bytes):
            # For serialized objects
            fingerprint_bytes = fingerprint[1]
        else:
            # For primitive and fallback fingerprints
            fingerprint_bytes = str(fingerprint).encode('utf-8')

        # Compute SHA-256 hash of the fingerprint
        unique_hash = hashlib.sha256(fingerprint_bytes).hexdigest()

        #print(f"Generated unique_id={unique_hash} for object of type {type(obj).__name__}")

        # Cache the unique ID
        unique_id_cache[obj_id] = unique_hash

        return unique_hash

    except Exception as e:
        # In case of unexpected errors, use a fallback unique ID
        fallback_hash = hashlib.sha256(f"{type(obj).__name__}:{str(obj)}".encode('utf-8')).hexdigest()
        #print(f"Error generating unique_id for {type(obj).__name__}: {e}. Using fallback unique_id={fallback_hash}")
        unique_id_cache[obj_id] = fallback_hash
        return fallback_hash

@contextlib.contextmanager
def temporary_reference(obj):
    """
    Context manager to temporarily hold a reference to an object.
    """
    try:
        yield obj
    finally:
        pass  # Do nothing, allowing the object to be garbage-collected if no other references exist

def log_shape_info(name, obj, parent_node=None):
    """
    Logs information about objects, generating unique IDs based on their content.
    Handles nested containers like dicts, lists, and tuples recursively.
    If the object is a container, each item is logged individually.
    """
    with temporary_reference(obj):
        try:
            if isinstance(obj, (dict, list, tuple)):
                if isinstance(obj, dict):
                    container_type = 'dict'
                elif isinstance(obj, list):
                    container_type = 'list'
                else:
                    container_type = 'tuple'
                print(f"    {name} ({container_type}):")
                for key, value in (obj.items() if isinstance(obj, dict) else enumerate(obj)):
                    sub_name = f"{name}[{key}]" if isinstance(obj, dict) else f"{name}[{key}]"
                    log_shape_info(sub_name, value, parent_node=name)
            else:
                unique_id = get_unique_id(obj)
                info = f"{name}: Type={type(obj).__name__}, ID={unique_id}"
                # If the object has a 'shape' attribute, include it
                if hasattr(obj, 'shape'):
                    info += f", Shape={obj.shape}"
                print(f"    {info}")

                # Add the object as a node in the DAG if a parent node is specified
                if parent_node:
                    # Create a unique identifier for the output item
                    output_item_node = f"{parent_node}.{name}"
                    if output_item_node not in execution_dag.nodes:
                        execution_dag.add_node(output_item_node, type=type(obj).__name__, id=unique_id)
                        execution_dag.add_edge(parent_node, output_item_node)
        except Exception as e:
            # For objects that cannot be processed, provide basic type information
            info = f"{name}: Type={type(obj).__name__}, Unable to generate unique ID ({e})"
            print(f"    {info}")
            # Optionally, handle nested structures manually
            if isinstance(obj, dict):
                print(f"    {name} (dict):")
                for key, value in obj.items():
                    log_shape_info(f"    {key}", value, parent_node=name)
            elif isinstance(obj, (list, tuple)):
                container_type = type(obj).__name__
                print(f"    {name} ({container_type}):")
                for idx, item in enumerate(obj):
                    log_shape_info(f"    [{idx}]", item, parent_node=name)

def make_trace_function(unique_calls=False):
    """Factory to create a decorator to trace function calls, with unique_calls flag."""
    def trace_function(func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            # Get the base function name
            base_name = func.__qualname__  # Qualified name includes the class name if within a class

            if unique_calls:
                # Initialize or increment the counter for this function
                if base_name not in call_counts:
                    call_counts[base_name] = 0
                unique_id = call_counts[base_name]
                call_counts[base_name] += 1

                # Create a unique node name by appending the counter ID to the function name
                node_name = f"{base_name}_{unique_id}"
            else:
                node_name = base_name

            # Add the node to the execution_dag if it doesn't already exist
            if node_name not in execution_dag.nodes:
                execution_dag.add_node(node_name, count=0)
                # print(f"Added node: {node_name}")

            # Increment the count for this specific function node
            execution_dag.nodes[node_name]['count'] += 1
            # print(f"Incremented count for node: {node_name} to {execution_dag.nodes[node_name]['count']}")

            # If there's a caller function, add an edge from the caller to the current function
            if call_stack:
                caller = call_stack[-1]

                # Capture the caller's existing children BEFORE we add the edge to the new child.
                existing_children = list(execution_dag.successors(caller))

                # Prepare a compact summary of the argument types/shapes of the *new* call.
                arg_types_str = ""
                try:
                    sig_tmp = inspect.signature(func)
                    bound_args_tmp = sig_tmp.bind_partial(*args, **kwargs)
                    bound_args_tmp.apply_defaults()
                    summaries = []
                    for _val in bound_args_tmp.arguments.values():
                        _summary = type(_val).__name__
                        if hasattr(_val, "shape"):
                            _summary += str(tuple(getattr(_val, "shape")))
                        elif hasattr(_val, "__len__") and not isinstance(_val, (str, bytes)):
                            try:
                                _summary += f"[len={len(_val)}]"
                            except Exception:
                                pass
                        summaries.append(_summary)
                    arg_types_str = "|".join(summaries)
                except Exception:
                    pass  # Fallback to empty

                # ------------------------------------------------------------------
                #  Add / increment the direct caller → callee edge
                # ------------------------------------------------------------------
                if execution_dag.has_edge(caller, node_name):
                    # Update weight *and* store latest argument summary.
                    execution_dag[caller][node_name]['weight'] += 1
                    execution_dag[caller][node_name]['arg_types'] = arg_types_str
                    # print(f"Incremented edge weight: {caller} -> {node_name} to {execution_dag[caller][node_name]['weight']}")
                else:
                    execution_dag.add_edge(caller, node_name, weight=1, arg_types=arg_types_str)
                    # print(f"Added edge: {caller} -> {node_name} with weight 1")

                # ------------------------------------------------------------------
                #  Connect siblings in a temporally ordered fashion: every existing
                #  child (called earlier) gets a directed edge to the *new* child.
                #  This preserves the order in which siblings were invoked.
                # ------------------------------------------------------------------
                if existing_children:
                    # Connect ONLY the most recent sibling to preserve a strict order chain
                    last_sibling = existing_children[-1]
                    if last_sibling != node_name and not execution_dag.has_edge(last_sibling, node_name):
                        execution_dag.add_edge(
                            last_sibling,
                            node_name,
                            weight=1,
                            relationship="sibling",
                            arg_types=arg_types_str,
                        )

            # Push the current function onto the call stack
            call_stack.append(node_name)
            # print(f"Pushed {node_name} to call stack")

            # Print input types, shapes, and IDs
            print(f"\nFunction '{func.__qualname__}' called:")
            print("  Inputs:")
            sig = inspect.signature(func)
            try:
                bound_args = sig.bind(*args, **kwargs)
                bound_args.apply_defaults()
                for arg_name, arg_value in bound_args.arguments.items():
                    log_shape_info(arg_name, arg_value, parent_node=node_name)
            except Exception as e:
                print(f"    Error inspecting arguments: {e}")

            try:
                # Execute the original function
                result = func(*args, **kwargs)

                # Print output types, shapes, and IDs
                print("  Outputs:")
                if isinstance(result, dict):
                    for key, value in result.items():
                        log_shape_info(f"Output[{key}]", value, parent_node=node_name)
                elif isinstance(result, (list, tuple)):
                    for idx, item in enumerate(result):
                        log_shape_info(f"Output[{idx}]", item, parent_node=node_name)
                else:
                    log_shape_info("Output", result, parent_node=node_name)

                return result
            finally:
                # Pop the current function from the call stack after execution
                popped = call_stack.pop()
                # print(f"Popped {popped} from call stack")
        return wrapper
    return trace_function

# Global flag for loading config once
config_loaded = False

def load_trace_config(config_path="trace_config.json", unique_calls=False, trace_list="functions_to_trace"):
    """Load trace configuration and dynamically apply tracing to specified functions."""
    global config_loaded
    if config_loaded:
        # Tracing configuration already loaded.
        return

    # Clear existing DAG and reset call counts
    execution_dag.clear()
    call_counts.clear()
    call_stack.clear()

    with open(config_path, 'r') as f:
        config = json.load(f)

    # Create the trace function decorator with the unique_calls parameter
    trace_function_decorator = make_trace_function(unique_calls)

    # Loop through each entry in the specified trace list
    for func_path in config.get(trace_list, []):
        try:
            # Separate module path, class, and function names
            parts = func_path.split(".")
            if len(parts) < 2:
                raise ValueError(f"Invalid format for {func_path}. Expected 'module.function_name' or 'module.ClassName.function_name'.")

            # Extract function name
            func_name = parts[-1]
            # Check if there's a class name
            if len(parts) >= 3:
                class_name = parts[-2]
                module_path = ".".join(parts[:-2])
            else:
                class_name = None
                module_path = ".".join(parts[:-1])

            # Import the module
            module = importlib.import_module(module_path)

            # Get the function or method
            if class_name:
                cls = getattr(module, class_name)
                func = getattr(cls, func_name)
                # Apply the decorator
                setattr(cls, func_name, trace_function_decorator(func))
            else:
                func = getattr(module, func_name)
                # Apply the decorator
                setattr(module, func_name, trace_function_decorator(func))

        except (ImportError, AttributeError, ValueError) as e:
            print(f"Error applying tracing to {func_path}: {e}")

    # Set the flag to prevent re-loading
    config_loaded = True

# ------------------------------------------------------------------
#  Load config located *next to* this tracing.py (if present)
# ------------------------------------------------------------------

_LOCAL_CFG = Path(__file__).with_suffix("").with_name("trace_config.json")
try:
    if _LOCAL_CFG.exists():
        load_trace_config(str(_LOCAL_CFG), unique_calls=False)
except Exception as _e:
    print(f"[Tracing] Skipped local config '{_LOCAL_CFG}' ({_e})")

#  Allow override via environment variable as well (optional)
_ENV_CFG = os.environ.get("TRACE_CONFIG_PATH")
if _ENV_CFG and os.path.exists(_ENV_CFG):
    try:
        load_trace_config(_ENV_CFG, unique_calls=False)
    except Exception as _e:
        print(f"[Tracing] Failed to load config from env path '{_ENV_CFG}': {_e}")

def assign_levels(dag):
    """Assigns levels to each node in the DAG for hierarchical layout."""
    levels = {}
    # Start with nodes that have no predecessors (level 0)
    current_level = 0
    nodes_at_current_level = [n for n in dag.nodes() if dag.in_degree(n) == 0]
    while nodes_at_current_level:
        next_level_nodes = []
        for node in nodes_at_current_level:
            levels[node] = current_level
            # Find children (successors) not yet assigned a level
            for successor in dag.successors(node):
                if successor not in levels:
                    next_level_nodes.append(successor)
        current_level += 1
        nodes_at_current_level = next_level_nodes
    return levels

def hierarchical_layout(dag, levels):
    """Generates a hierarchical layout for the DAG."""
    pos = {}
    # Group nodes by their level
    level_nodes = {}
    for node, level in levels.items():
        level_nodes.setdefault(level, []).append(node)
    
    # Assign positions
    y_gap = 1  # Vertical gap between levels
    x_gap = 1  # Horizontal gap between nodes
    for level, nodes in level_nodes.items():
        num_nodes = len(nodes)
        # Center nodes horizontally
        for i, node in enumerate(nodes):
            x = (i - num_nodes / 2) * x_gap
            y = -level * y_gap  # Negative y to have top-down
            pos[node] = (x, y)
    return pos

def display_dag_info():
    """Displays the DAG nodes with call counts and edges with dependency weights."""
    print("Function Call Counts:")
    for node in execution_dag.nodes(data=True):
        print(f"  Function: {node[0]}, Call Count: {node[1]['count']}")
    print("\nDependencies and Reference Counts:")
    for edge in execution_dag.edges(data=True):
        print(f"  Dependency: {edge[0]} -> {edge[1]}, Reference Count: {edge[2]['weight']}")

def save_dag(filename="execution_dag.pkl"):
    """Save the execution DAG to a file using pickle."""
    if os.path.exists(filename):
        print(f"DAG file already exists at '{filename}'. Overwriting.")
    with open(filename, 'wb') as f:
        pickle.dump(execution_dag, f)
    print(f"DAG successfully saved to '{filename}'")

def plot_dag(filename="execution_dag.png"):
    """Render the execution DAG to a PNG using Graphviz `Digraph.render`,
    matching the style used by the Operator-graph utilities.

    A ``<basename>.png`` file is generated (the intermediate ``.gv`` is deleted
    via ``cleanup=True``).
    """

    from graphviz import Digraph  # Lazy import

    # ------------------------------------------------------------------
    #  Sanity checks
    # ------------------------------------------------------------------
    if execution_dag is None or execution_dag.number_of_nodes() == 0:
        print("No execution DAG to plot – skipping.")
        return

    # Determine base file-stem (strip extension if supplied)
    base_path, ext = os.path.splitext(filename)
    if ext.lower() != ".png":
        base_path = filename  # user gave stem only

    png_path = f"{base_path}.png"
    if os.path.exists(png_path):
        print(f"Plot already exists at '{png_path}'. Skipping plotting.")
        return

    # ------------------------------------------------------------------
    #  Build Graphviz graph
    # ------------------------------------------------------------------
    g = Digraph(graph_attr={"rankdir": "TB", "nodesep": "0.6", "ranksep": "0.7"})

    for node, data in execution_dag.nodes(data=True):
        # Build node label and choose color based on number of invocations
        count = data.get("count", 1)
        label = f"{node}\nCalls: {count}"
        g.node(
            node,
            label=label,
            shape="circle",
            style="filled",
            fillcolor="orange" if count > 1 else "skyblue",
            width="1",
            height="1",
            fontsize="10",
        )

    for u, v, data in execution_dag.edges(data=True):
        weight = data.get("weight", 1)
        arg_types = data.get("arg_types", "")

        # Determine if connected nodes are both highlighted (Calls > 1)
        u_highlight = execution_dag.nodes[u].get("count", 1) > 1
        v_highlight = execution_dag.nodes[v].get("count", 1) > 1

        label_parts = [f"Refs: {weight}"]
        if arg_types:
            label_parts.append(arg_types)

        edge_kwargs = {
            "label": "\n".join(label_parts),
            "fontsize": "8",
        }
        if u_highlight and v_highlight:
            edge_kwargs.update({"color": "red", "penwidth": "2"})

        g.edge(u, v, **edge_kwargs)

    # ------------------------------------------------------------------
    #  Render using Graphviz (identical call signature to Operator graphs)
    # ------------------------------------------------------------------
    try:
        g.render(base_path, format="png", cleanup=True)
        print(f"DAG plot saved → {png_path}")
    except Exception as e:
        print(f"[Tracing] Graphviz render failed ({e}). No plot generated.")

# === Rest of your tracing.py code ends here ===