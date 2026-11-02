from __future__ import annotations

import csv
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class FixedBaselineAudit:
    source: str
    rows: int
    models: int
    seeds_per_model: int
    counts: dict[str, int]
    normalized_rows: tuple[dict[str, object], ...]


def _true(value: object) -> bool:
    return str(value).strip().lower() in {"1", "true", "yes"}


def audit_existing_fixed_csv(
    path: Path | str, *, expected_models: int, expected_seeds_per_model: int,
) -> FixedBaselineAudit:
    source = Path(path)
    selected: list[dict[str, str]] = []
    with source.open("r", encoding="utf-8-sig", newline="") as stream:
        for row in csv.DictReader(stream):
            if row.get("config_id") in {"STR_DP0", "DP0"}:
                selected.append(row)
    expected_total = expected_models * expected_seeds_per_model * 2
    if len(selected) != expected_total:
        raise ValueError(
            f"existing FIXED inventory is incomplete: {len(selected)} != {expected_total}"
        )
    normalized: list[dict[str, object]] = []
    seen: set[tuple[str, str, int]] = set()
    by_arm_model: dict[tuple[str, str], set[int]] = {}
    models_by_arm: dict[str, set[str]] = {"STR_FIXED": set(), "RDI_FIXED": set()}
    for row in selected:
        config_id = row["config_id"]
        if config_id == "STR_DP0":
            method, ifun, expected_ifun, expected_kind = "STR_FIXED", "STR", "M1", "STR"
        else:
            method, ifun, expected_ifun, expected_kind = "RDI_FIXED", "RDI", "N4", "N4"
        if row.get("ifun") != expected_ifun or row.get("ifun_kind") != expected_kind:
            raise ValueError("existing FIXED importance-function identity mismatch")
        if not _true(row.get("complete")) or int(row.get("return_code", -1)) != 0:
            raise ValueError("existing FIXED result is incomplete or failed")
        if int(float(row.get("budget_seconds", -1))) != 120:
            raise ValueError("existing FIXED result is not a 120-second run")
        if int(float(row.get("global_effort", -1))) != 5:
            raise ValueError("existing FIXED global effort is not the frozen value 5")
        if row.get("pool_mode") != "modal" or row.get("property_execution_mode") != "single":
            raise ValueError("existing FIXED SFE execution factors mismatch")
        model_id = row["model_id"]
        seed = int(row["seed"])
        key = (method, model_id, seed)
        if key in seen:
            raise ValueError("duplicate existing FIXED identity")
        seen.add(key)
        models_by_arm[method].add(model_id)
        by_arm_model.setdefault((method, model_id), set()).add(seed)
        common = {
            "engine": "sfe",
            "pool_mode": row["pool_mode"],
            "stop_time": 120,
            "property_execution_mode": row["property_execution_mode"],
            "model_sha256": row.get("ft_sha256", ""),
            "threshold_chain_sha256": row.get("threshold_chain_sha256", ""),
            "fixed_population": 3 * max(8, int(float(row["global_effort"]))),
        }
        normalized.append({
            "method": method,
            "ifun": ifun,
            "model_id": model_id,
            "seed": seed,
            "common": common,
            "source_config_id": config_id,
        })
    if any(len(models) != expected_models for models in models_by_arm.values()):
        raise ValueError("existing FIXED model coverage mismatch")
    if models_by_arm["STR_FIXED"] != models_by_arm["RDI_FIXED"]:
        raise ValueError("STR and RDI FIXED model sets differ")
    for model_id in models_by_arm["STR_FIXED"]:
        str_seeds = by_arm_model.get(("STR_FIXED", model_id), set())
        rdi_seeds = by_arm_model.get(("RDI_FIXED", model_id), set())
        if len(str_seeds) != expected_seeds_per_model or str_seeds != rdi_seeds:
            raise ValueError("existing FIXED seed coverage is not paired and complete")
    return FixedBaselineAudit(
        source=str(source.resolve()),
        rows=len(normalized),
        models=expected_models,
        seeds_per_model=expected_seeds_per_model,
        counts={"STR_FIXED": expected_models * expected_seeds_per_model,
                "RDI_FIXED": expected_models * expected_seeds_per_model},
        normalized_rows=tuple(normalized),
    )
