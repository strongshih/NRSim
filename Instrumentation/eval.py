# Copyright 2022 the Regents of the University of California, Nerfstudio Team and contributors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#!/usr/bin/env python
"""
eval.py
"""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, Tuple

# ---------------------------------------------------------------------------
#  Automatic execution tracing integration
#  --------------------------------------------------------------
#  If a module `nerfstudio.instrumentation.tracing` exists, we automatically
#  import it and load the JSON located beside it.  No environment variables
#  are required.  This must happen *before* Nerfstudio sub-modules import the
#  functions we intend to patch.
# ---------------------------------------------------------------------------

from pathlib import Path
import importlib.util

# Tracing will be loaded *later* if the user enables it via CLI flag.
tracing_mod = None  # will hold imported module when enabled

import tyro

from nerfstudio.utils.eval_utils import eval_setup
from nerfstudio.utils.rich_utils import CONSOLE

@dataclass
class ComputePSNR:
    """Load a checkpoint, compute some PSNR metrics, and save it to a JSON file."""

    # Path to config YAML file.
    load_config: Path
    # Name of the output file.
    output_path: Path = Path("output.json")
    # Optional path to save rendered outputs to.
    render_output_path: Optional[Path] = None

    # Optional list/tuple of image indices to evaluate. When provided, ns-eval will
    # restrict computation to ONLY these images instead of the entire evaluation
    # split. Example CLI usage:
    #   ns-eval --load-config ... --eval-image-indices "(0,)"
    # Using a Python-style tuple literal is convenient because Tyro parses it.
    eval_image_indices: Optional[Tuple[int, ...]] = None

    # Enable function-level tracing / DAG generation
    enable_trace: bool = False
    # Path to JSON with functions_to_trace; defaults to file beside instrumentation/tracing.py
    trace_config_path: Optional[Path] = None

    def main(self) -> None:
        """Main function."""
        # ------------------------------------------------------------------
        #  Optionally enable tracing *before* pipeline/eval starts so that
        #  decorators are attached in time.
        # ------------------------------------------------------------------
        global tracing_mod  # noqa: PLW0603
        if self.enable_trace and tracing_mod is None:
            try:
                ns_root = Path(__file__).resolve().parent.parent
                instr_dir = ns_root / "instrumentation"
                tracing_py = instr_dir / "tracing.py"

                spec = importlib.util.spec_from_file_location("nerfstudio.instrumentation.tracing", tracing_py)
                tracing_mod = importlib.util.module_from_spec(spec)  # type: ignore
                assert spec and spec.loader
                spec.loader.exec_module(tracing_mod)  # type: ignore

                cfg_json = self.trace_config_path if self.trace_config_path else (instr_dir / "trace_config.json")
                if cfg_json.exists():
                    tracing_mod.load_trace_config(str(cfg_json), unique_calls=False)
                else:
                    print(f"[Tracing] Warning: trace config '{cfg_json}' not found – tracing active but no functions registered.")

            except Exception as e:
                print(f"[Tracing] Failed to initialise: {e}")

        config, pipeline, checkpoint_path, _ = eval_setup(self.load_config)
        assert self.output_path.suffix == ".json"
        if self.render_output_path is not None:
            self.render_output_path.mkdir(parents=True, exist_ok=True)

        # ------------------------------------------------------------------
        #  If the user specified a subset of image indices, replace the default
        #  FixedIndicesEvalDataloader with one restricted to this subset.
        # ------------------------------------------------------------------
        if self.eval_image_indices is not None:
            from nerfstudio.data.utils.dataloaders import FixedIndicesEvalDataloader

            dm = pipeline.datamanager  # shorthand
            assert hasattr(dm, "eval_dataset") and dm.eval_dataset is not None, "Eval dataset missing"

            dm.fixed_indices_eval_dataloader = FixedIndicesEvalDataloader(
                input_dataset=dm.eval_dataset,
                image_indices=self.eval_image_indices,
                device=dm.device,
                num_workers=getattr(dm, "world_size", 1) * 4,
            )

        metrics_dict = pipeline.get_average_eval_image_metrics(output_path=self.render_output_path, get_std=True)

        # ------------------------------------------------------------------
        #  Persist execution DAG if tracing was enabled
        # ------------------------------------------------------------------
        if self.enable_trace and tracing_mod:
            try:
                tracing_mod.save_dag("execution_dag.pkl")  # type: ignore
                # Additionally plot the DAG to a PNG next to the saved .pkl file
                tracing_mod.plot_dag("execution_dag.png")  # type: ignore
            except Exception as e:
                print(f"[Tracing] Could not save DAG: {e}")

        self.output_path.parent.mkdir(parents=True, exist_ok=True)
        # Get the output and define the names to save to
        benchmark_info = {
            "experiment_name": config.experiment_name,
            "method_name": config.method_name,
            "checkpoint": str(checkpoint_path),
            "results": metrics_dict,
        }
        # Save output to output file
        self.output_path.write_text(json.dumps(benchmark_info, indent=2), "utf8")
        CONSOLE.print(f"Saved results to: {self.output_path}")


def entrypoint():
    """Entrypoint for use with pyproject scripts."""
    tyro.extras.set_accent_color("bright_yellow")
    tyro.cli(ComputePSNR).main()


if __name__ == "__main__":
    entrypoint()

# For sphinx docs
get_parser_fn = lambda: tyro.extras.get_parser(ComputePSNR)  # noqa
