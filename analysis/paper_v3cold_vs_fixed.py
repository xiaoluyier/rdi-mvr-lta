from __future__ import annotations

import json
from pathlib import Path
import statistics

import numpy as np
import pandas as pd
from scipy import stats


ROOT = Path(__file__).resolve().parent
INPUT = ROOT / "paired_seed_metrics.csv"
RNG = np.random.default_rng(20260902)
BOOT = 10000


def family(model: str) -> str:
    if "m1_sym_paths" in model:
        return "M1-symmetric"
    if "m2_bottleneck" in model:
        return "M2-bottleneck"
    if "m3_shared" in model:
        return "M3-shared"
    if "PAND" in model or model == "TREE_P3_s0001":
        return "PAND-special"
    return "other"


def bootstrap_median(values):
    values = np.asarray(values, dtype=float)
    draws = RNG.choice(values, size=(BOOT, len(values)), replace=True)
    medians = np.median(draws, axis=1)
    return float(np.quantile(medians, 0.025)), float(np.quantile(medians, 0.975))


def model_ratio(frame, metric, method="V3_COLD", reference="FIXED"):
    wide = frame.pivot_table(index=["model_id", "ifun", "seed"], columns="method",
                             values=metric, aggfunc="first")
    wide = wide[(wide[method] > 0) & (wide[reference] > 0)].copy()
    wide["ratio"] = wide[method] / wide[reference]
    return wide.groupby(["model_id", "ifun"])["ratio"].median()


def summarize_ratios(series):
    values = series.to_numpy(float)
    ci = bootstrap_median(values)
    test = stats.wilcoxon(np.log(values))
    return {
        "models": len(values), "median_ratio": float(np.median(values)),
        "ci_low": ci[0], "ci_high": ci[1],
        "models_better": int(np.sum(values < 1)), "p_wilcoxon": float(test.pvalue),
        "equal_time_efficiency": float(1 / np.median(values) ** 2),
    }


def main():
    data = pd.read_csv(INPUT)
    data["family"] = data["model_id"].map(family)
    cold = data[data["method"].isin(["V3_COLD", "FIXED"])].copy()
    if len(cold) != 3360:
        raise ValueError(f"expected 3360 method rows, got {len(cold)}")

    overall = []
    for ifun in ("STR", "RDI"):
        subset = cold[cold["ifun"] == ifun]
        rhw = model_ratio(subset, "rhw")
        root = model_ratio(subset, "root")
        estimate = model_ratio(subset, "estimate")
        summary = summarize_ratios(rhw)
        estimate_summary = summarize_ratios(estimate)
        root_fom = (1 / rhw.pow(2)) / root.reindex(rhw.index)
        summary.update({
            "ifun": ifun, "root_ratio": float(root.median()),
            "root_normalized_fom": float(root_fom.median()),
            "estimate_ratio": float(estimate.median()),
            "estimate_ci_low": estimate_summary["ci_low"],
            "estimate_ci_high": estimate_summary["ci_high"],
            "estimate_p": estimate_summary["p_wilcoxon"],
        })
        overall.append(summary)

    phase_rows = []
    cold_batch = data[data["method"] == "V3_COLD"][["model_id", "ifun", "seed", "batch"]]
    merged = cold.merge(cold_batch, on=["model_id", "ifun", "seed"], how="left", suffixes=("", "_cold"))
    for ifun in ("STR", "RDI"):
        for label, lo, hi in (("batch-1", 1, 1), ("early-1-5", 1, 5),
                              ("adapt-6-10", 6, 10), ("steady-11-20", 11, 20)):
            frame = merged[(merged["ifun"] == ifun) & merged["batch_cold"].between(lo, hi)]
            rhw = model_ratio(frame, "rhw")
            root = model_ratio(frame, "root")
            summary = summarize_ratios(rhw)
            summary.update({"ifun": ifun, "phase": label,
                            "root_ratio": float(root.median()),
                            "root_normalized_fom": float(((1/rhw.pow(2))/root.reindex(rhw.index)).median())})
            phase_rows.append(summary)

    strata = []
    for column in ("difficulty", "family"):
        for ifun in ("STR", "RDI"):
            for level in sorted(cold[column].dropna().astype(str).unique()):
                frame = cold[(cold["ifun"] == ifun) & (cold[column].astype(str) == level)]
                rhw = model_ratio(frame, "rhw")
                if len(rhw) < 2:
                    continue
                item = summarize_ratios(rhw)
                item.update({"stratum": column, "level": level, "ifun": ifun})
                strata.append(item)

    wide_nz = cold.pivot_table(index=["model_id", "ifun", "seed"], columns="method",
                               values="nonzero", aggfunc="first")
    nonzero = []
    for ifun in ("STR", "RDI"):
        x = wide_nz.xs(ifun, level="ifun")
        cold_only = int(((x["V3_COLD"] == 1) & (x["FIXED"] == 0)).sum())
        fixed_only = int(((x["V3_COLD"] == 0) & (x["FIXED"] == 1)).sum())
        discordant = cold_only + fixed_only
        pvalue = float(stats.binomtest(cold_only, discordant, 0.5).pvalue) if discordant else 1.0
        nonzero.append({"ifun": ifun, "cold_nonzero": int(x["V3_COLD"].sum()),
                        "fixed_nonzero": int(x["FIXED"].sum()), "cold_only": cold_only,
                        "fixed_only": fixed_only, "mcnemar_exact_p": pvalue})

    models = []
    for ifun in ("STR", "RDI"):
        ratios = model_ratio(cold[cold["ifun"] == ifun], "rhw")
        for (model, _), value in ratios.items():
            models.append({"model_id": model, "ifun": ifun, "family": family(model),
                           "rhw_ratio": float(value), "better": int(value < 1)})

    summary = {"overall": overall, "phases": phase_rows, "strata": strata,
               "nonzero": nonzero, "models": models}
    (ROOT / "v3cold_vs_fixed_paper_summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
    for name, rows in (("v3cold_vs_fixed_overall.csv", overall),
                       ("v3cold_vs_fixed_phases.csv", phase_rows),
                       ("v3cold_vs_fixed_strata.csv", strata),
                       ("v3cold_vs_fixed_nonzero.csv", nonzero),
                       ("v3cold_vs_fixed_models.csv", models)):
        pd.DataFrame(rows).to_csv(ROOT / name, index=False, encoding="utf-8-sig")
    print(json.dumps(summary, ensure_ascii=False))


if __name__ == "__main__":
    main()
