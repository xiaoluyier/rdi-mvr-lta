# RDI / MVR-LTA research artifact (release candidate)

Prepared for manuscript comment R3-8. This repository is a release candidate; the full historical simulation pipeline is not yet verified. See the release blockers below.

Contents: experiment contains unchanged XiWang v3 COLD frozen Python/patch sources, 44 FT models, controller parameters and seeds. XiWang v3 COLD is the historical MVR-LTA implementation; N4 denotes RDI. ASES is a different algorithm. analysis contains 6720 seed-level records and the historical analysis script, which selects 3360 cold/fixed records. rdi_historical contains the 2026-07-29 frozen RDI compiler; iosa_current_dependency.py is a later dependency, not a verified historical source. engine_snapshot contains later FIG sources and a one-click runner. provenance contains original formal-run manifests and patch receipts.

Offline commands (run from this directory):

    python -m pip install -r requirements.txt
    python verify_artifact.py
    python analysis/paper_v3cold_vs_fixed.py

Set PYTHONPATH to the experiment directory, then run:

    python -B -m unittest discover -s tests -v

Ten compatible existing algorithm tests passed. All Python files passed syntax parsing. Two files targeting newer APIs are preserved in audit/newer_version_tests: the initial mixed-version test run had three interface errors; frozen algorithm code was not modified to satisfy those tests.

Recomputed RHW median ratios: STR=0.7907504889913044, RDI=0.777741182253902, each over 41 valid models. This verifies MVR-LTA versus fixed-population RHW only, not standalone RDI or combined-method NRMSE/RHW claims.

Exact settings and seeds are in experiment/controllers.jsonl: 42 applicable models, 20 sequential batches/seeds per importance function, 120 seconds each, initial population 24, maximum 48, planning at most 10 batches, patience 2, eta 0.005, minimum 30 clusters, planner bootstrap 2000. Statistical reporting bootstrap is 10000. Two CPS models have no adjustable intermediate thresholds.

Historical simulation entry point (not yet validated for this assembled package):

    cd experiment
    python -m xiwang_v3.formal_runner --help
    python -m xiwang_v3.formal_runner --package . --server 29072 --runner ../engine_snapshot/artifact/experiments/scripts/run_phase_pand_reuse_oneclick.sh --parallel-jobs 16

The numeric shard label is historical metadata, not a remote endpoint. FIG build requirements are in engine_snapshot/artifact/setup/fig/README.md. Its build entry point is bash build_fig.sh -p main -m release -c gcc. Linux compilation and full simulations were not performed in this Windows task.

Release blockers:

1. Frozen Python/patch hashes match the archived formal run, but the later FIG snapshot differs from that run. The formal patched SimulationEngineSFE.cpp SHA256 is b916322d93eea4089c9f8f134387be7b0c5c2ac10504fd9438f8cfb3d5d940e7. Recover or validate the historical engine and RDI integration, then test a clean Linux build and smoke run. Do not blindly reapply historical patches to the later snapshot.
2. data/reference_truth_44.csv labels its method exact_ctmc_uniformization_pand_state; the manuscript describes SBDD. Resolve this provenance discrepancy before claiming these are the manuscript reference probabilities.
3. Confirm author licensing for Python code/models/data and choose a public repository/archive. FIG GPLv3 and bundled dependency notices are retained; no new blanket license is invented.
4. Verify standalone RDI and combined-method paper claims with their corresponding evidence.

A keyword scan found no password/API-key/access-token assignments; this is not an exhaustive privacy guarantee. No SSH or cloud experiments were performed. Original frozen source/model/controller files were preserved without edits.
