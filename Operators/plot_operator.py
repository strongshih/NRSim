"""Utility to visualise operator pipelines as dependency graphs.

Two examples are generated automatically:
    • neurex_pipeline (hash‑grid baseline)
    • srender_pipeline (staged render)
    • gscore_pipeline (culled‑SH‑blend)

Graphs are saved as PNGs beside the script.
"""
# Builds pipelines and leverages their internal plotting utility.

from pipelines.neurex_pipeline import build_neurex_pipeline
from pipelines.gaurast_pipeline import build_gaurast_pipeline
from pipelines.gscore_pipeline import build_gscore_pipeline
from pipelines.srender_pipeline import build_srender_pipeline
from pipelines.icarus_pipeline import build_icarus_pipeline
from pipelines.cicero_pipeline import build_cicero_pipeline
from utils.operator_graph import FineOperatorGraph


def main():
    dim = (4, 64)  # example batch/sample dims

    # Icarus pipeline
    icarus_graph = build_icarus_pipeline((800*800, 192))
    icarus_graph.plot_graph("Icarus Pipeline (Coarse)", "icarus_pipeline_graph.png")
    icarus_graph.plot_fine_graph("Icarus Pipeline (Fine)", "icarus_pipeline_graph_fine.png")

    # Neurex pipeline
    neurex_graph = build_neurex_pipeline(dim)
    neurex_graph.plot_graph("Neurex Pipeline (Coarse)", "neurex_pipeline_graph.png")
    neurex_graph.plot_fine_graph("Neurex Pipeline (Fine)", "neurex_pipeline_graph_fine.png")

    # Gaurast pipeline
    gaurast_graph = build_gaurast_pipeline((400,400))
    gaurast_graph.plot_graph("Gaurast Pipeline (Coarse)", "gaurast_pipeline_graph.png")
    gaurast_graph.plot_fine_graph("Gaurast Pipeline (Fine)", "gaurast_pipeline_graph_fine.png")

    # GSCore pipeline
    gscore_graph = build_gscore_pipeline((400,400))
    gscore_graph.plot_graph("GSCore Pipeline (Coarse)", "gscore_pipeline_graph.png")
    gscore_graph.plot_fine_graph("GSCore Pipeline (Fine)", "gscore_pipeline_graph_fine.png")

    # S‑render pipeline
    srender_graph = build_srender_pipeline(dim)
    srender_graph.plot_graph("Staged‑Render Pipeline (Coarse)", "srender_pipeline_graph.png")
    srender_graph.plot_fine_graph("Staged‑Render Pipeline (Fine)", "srender_pipeline_graph_fine.png")

    # CICERO pipeline
    cicero_graph = build_cicero_pipeline(dim)
    cicero_graph.plot_graph("CICERO Pipeline (Coarse)", "cicero_pipeline_graph.png")
    cicero_graph.plot_fine_graph("CICERO Pipeline (Fine)", "cicero_pipeline_graph_fine.png")


if __name__ == "__main__":
    main() 