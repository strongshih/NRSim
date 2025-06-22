import argparse
from typing import Tuple
import sys

# Ensure relative imports work when executed as script
from pathlib import Path
PROJECT_ROOT = Path(__file__).resolve().parent.parent  # NeReSim_operators/
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from pipelines.srender_pipeline import build_srender_pipeline
from pipelines.cicero_pipeline import build_cicero_pipeline
from pipelines.icarus_pipeline import build_icarus_pipeline
from pipelines.gscore_pipeline import build_gscore_pipeline
from pipelines.gaurast_pipeline import build_gaurast_pipeline  # may not exist but imported for completeness
from pipelines.neurex_pipeline import build_neurex_pipeline

from utils.export_graph import pipeline_to_json


PIPELINE_BUILDERS = {
    "srender": build_srender_pipeline,
    "cicero": build_cicero_pipeline,
    "icarus": build_icarus_pipeline,
    "gscore": build_gscore_pipeline,
    "gaurast": build_gaurast_pipeline,
    "neurex": build_neurex_pipeline,
}


def parse_dim(dim_str: str) -> Tuple[int, int]:
    try:
        parts = dim_str.split(",")
        if len(parts) != 2:
            raise ValueError
        return int(parts[0]), int(parts[1])
    except Exception:
        raise argparse.ArgumentTypeError("Dimension must be in form B,N (e.g. 2048,64)")


def main():
    parser = argparse.ArgumentParser(description="Build a neural‑rendering pipeline and export its operator graph to JSON.")
    parser.add_argument("pipeline", choices=PIPELINE_BUILDERS.keys(), help="Pipeline name to build")
    parser.add_argument("output", help="Path to the output JSON file")
    parser.add_argument("--dim", type=parse_dim, default="128,64", help="Batch and samples per ray as 'B,N' (default 128,64)")

    args = parser.parse_args()

    builder = PIPELINE_BUILDERS[args.pipeline]

    if isinstance(args.dim, str):  # default case when not parsed by custom type (click bug fix)
        dim = parse_dim(args.dim)
    else:
        dim = args.dim

    graph = builder(dim)

    # Convert to simple list preserving insertion order
    pipeline_ops = list(graph.nodes)

    pipeline_to_json(pipeline_ops, args.output)


if __name__ == "__main__":
    main() 