from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Mapping

from .baseline import audit_existing_fixed_csv
from .controller import XiWangController
from .core import XiWangConfig
from .execution import FigSubprocessAdapter, run_learning


_LEARNING_KEYS = {
    "config", "levels", "command", "workdir", "base_env", "log_dir",
    "timeout_seconds", "checkpoint_path",
}


def load_learning_manifest(path: Path | str) -> dict[str, object]:
    source = Path(path)
    raw = json.loads(source.read_text(encoding="utf-8"))
    if not isinstance(raw, dict):
        raise ValueError("learning manifest must be a JSON object")
    missing = sorted(_LEARNING_KEYS - set(raw))
    extra = sorted(set(raw) - _LEARNING_KEYS)
    if missing:
        raise ValueError("missing learning manifest fields: " + ", ".join(missing))
    if extra:
        raise ValueError("unknown learning manifest fields: " + ", ".join(extra))
    XiWangConfig.from_dict(raw["config"])
    levels = tuple(int(level) for level in raw["levels"])
    if not levels or len(levels) != len(set(levels)):
        raise ValueError("manifest levels must be nonempty and unique")
    if not isinstance(raw["command"], list) or not raw["command"]:
        raise ValueError("manifest command must be a nonempty argv list")
    if not isinstance(raw["base_env"], dict):
        raise ValueError("manifest base_env must be an object")
    return raw


def _path(base: Path, value: object) -> Path:
    result = Path(str(value))
    return result if result.is_absolute() else base / result


def _run_learning(manifest_path: Path) -> dict[str, object]:
    raw = load_learning_manifest(manifest_path)
    base = manifest_path.resolve().parent
    config = XiWangConfig.from_dict(raw["config"])
    checkpoint = _path(base, raw["checkpoint_path"])
    if checkpoint.is_file():
        snapshot = json.loads(checkpoint.read_text(encoding="utf-8"))
        controller = XiWangController.restore(snapshot, config=config)
    else:
        controller = XiWangController(levels=raw["levels"], config=config)
    adapter = FigSubprocessAdapter(
        command=raw["command"],
        workdir=_path(base, raw["workdir"]),
        base_env={str(k): str(v) for k, v in raw["base_env"].items()},
        log_dir=_path(base, raw["log_dir"]),
        timeout_seconds=int(raw["timeout_seconds"]),
    )
    result = run_learning(controller, adapter, checkpoint_path=checkpoint)
    return {
        "status": result.status,
        "frozen_plan": result.frozen_plan,
        "completed_batches": result.completed_batches,
        "config_sha256": config.sha256,
        "checkpoint_path": str(checkpoint.resolve()),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="xiwang-clean")
    subparsers = parser.add_subparsers(dest="command_name", required=True)
    learn = subparsers.add_parser("learn")
    learn.add_argument("--manifest", type=Path, required=True)
    audit = subparsers.add_parser("audit-baseline")
    audit.add_argument("--csv", type=Path, required=True)
    audit.add_argument("--expected-models", type=int, required=True)
    audit.add_argument("--expected-seeds-per-model", type=int, required=True)
    args = parser.parse_args(argv)
    if args.command_name == "learn":
        output = _run_learning(args.manifest)
    else:
        result = audit_existing_fixed_csv(
            args.csv,
            expected_models=args.expected_models,
            expected_seeds_per_model=args.expected_seeds_per_model,
        )
        output = {
            "source": result.source,
            "rows": result.rows,
            "models": result.models,
            "seeds_per_model": result.seeds_per_model,
            "counts": result.counts,
            "new_fixed_tasks": 0,
        }
    print(json.dumps(output, sort_keys=True, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
