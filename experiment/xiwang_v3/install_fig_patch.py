from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import hashlib
import json
import os
from pathlib import Path
import shutil


@dataclass(frozen=True)
class InstallReceipt:
    source_path: str
    backup_path: str
    header_path: str
    before_sha256: str
    after_sha256: str


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise ValueError(f"expected exactly one {label} anchor, found {count}")
    return text.replace(old, new, 1)


def install(
    source_path: Path | str,
    include_dir: Path | str,
    *,
    expected_sha256: str,
) -> InstallReceipt:
    source = Path(source_path).resolve()
    include = Path(include_dir).resolve()
    before = _sha256(source)
    if before.lower() != str(expected_sha256).lower():
        raise ValueError(
            f"source SHA-256 mismatch: expected {expected_sha256}, actual {before}"
        )
    original = source.read_text(encoding="utf-8")
    if "XiWangPopulationPlan" in original or "BATS_XIWANG_PLAN" in original:
        raise ValueError("source already contains a XiWang patch")
    if "SimulationEngineSFE::reset() const" not in original:
        raise ValueError("current server interface is missing SimulationEngineSFE::reset()")
    if "SimulationEngineSFE::transient_simulations(" not in original:
        raise ValueError("current server interface is missing transient_simulations()")

    patched = _replace_once(
        original,
        "#include <BatsDiagnostics.h>",
        "#include <BatsDiagnostics.h>\n#include <XiWangPopulationPlan.h>",
        "BatsDiagnostics include",
    )
    function_marker = "SimulationEngineSFE::fixed_effort("
    function_start = patched.find(function_marker)
    if function_start < 0:
        raise ValueError("fixed_effort implementation was not found")
    prefix, tail = patched[:function_start], patched[function_start:]
    assertion = "\tassert(nullptr != impFun_);"
    plan_setup = (
        assertion + "\n"
        "\tstatic const fig::XiWangPopulationPlan xiwangPlan =\n"
        "\t\tfig::XiWangPopulationPlan::from_environment();"
    )
    tail = _replace_once(tail, assertion, plan_setup, "fixed_effort assertion")

    override_anchor = "\t\tconst bool populationOverrideActive ="
    if tail.count(override_anchor) != 1:
        raise ValueError(
            "current server source lacks the unique modern population override anchor"
        )
    override = (
        "\t\tif (!toBuildThresholds_ && xiwangPlan.enabled())\n"
        "\t\t\trequestedLevelPopulation = xiwangPlan.population(\n"
        "\t\t\t\tstatic_cast<size_t>(lvl), nominalLevelEffort);\n"
    )
    tail = tail.replace(override_anchor, override + override_anchor, 1)
    patched = prefix + tail

    backup = source.with_name(source.name + f".xiwang_clean.{before}.bak")
    if backup.exists():
        if _sha256(backup) != before:
            raise ValueError("existing XiWang backup does not match the frozen source")
    else:
        shutil.copy2(source, backup)
    temporary = source.with_name(source.name + ".xiwang_clean.tmp")
    temporary.write_text(patched, encoding="utf-8", newline="\n")
    os.replace(temporary, source)

    include.mkdir(parents=True, exist_ok=True)
    packaged_header = Path(__file__).resolve().parent / "fig_patch" / "XiWangPopulationPlan.h"
    installed_header = include / packaged_header.name
    header_tmp = include / (packaged_header.name + ".tmp")
    shutil.copyfile(packaged_header, header_tmp)
    os.replace(header_tmp, installed_header)
    return InstallReceipt(
        source_path=str(source),
        backup_path=str(backup),
        header_path=str(installed_header),
        before_sha256=before,
        after_sha256=_sha256(source),
    )


def migrate_strict_to_sparse(
    source_path: Path | str,
    include_dir: Path | str,
    *,
    expected_sha256: str,
) -> InstallReceipt:
    source = Path(source_path).resolve()
    include = Path(include_dir).resolve()
    before = _sha256(source)
    if before.lower() != str(expected_sha256).lower():
        raise ValueError(
            f"source SHA-256 mismatch: expected {expected_sha256}, actual {before}"
        )
    original = source.read_text(encoding="utf-8")
    strict = (
        "\tif (!toBuildThresholds_ && xiwangPlan.enabled())\n"
        "\t\txiwangPlan.validate_complete(impFun_->thresholds().size());\n"
    )
    if original.count(strict) != 1:
        raise ValueError("strict XiWang completeness block was not found exactly once")
    migrated = original.replace(strict, "", 1)
    if "xiwangPlan.population(" not in migrated:
        raise ValueError("installed XiWang population hook is missing")
    backup = source.with_name(source.name + f".xiwang_strict.{before}.bak")
    if not backup.exists():
        shutil.copy2(source, backup)
    temporary = source.with_name(source.name + ".xiwang_sparse.tmp")
    temporary.write_text(migrated, encoding="utf-8", newline="\n")
    os.replace(temporary, source)
    include.mkdir(parents=True, exist_ok=True)
    packaged_header = Path(__file__).resolve().parent / "fig_patch" / "XiWangPopulationPlan.h"
    installed_header = include / packaged_header.name
    header_tmp = include / (packaged_header.name + ".tmp")
    shutil.copyfile(packaged_header, header_tmp)
    os.replace(header_tmp, installed_header)
    return InstallReceipt(
        source_path=str(source),
        backup_path=str(backup),
        header_path=str(installed_header),
        before_sha256=before,
        after_sha256=_sha256(source),
    )


def install_sparse_diagnostics(
    source_path: Path | str,
    include_dir: Path | str,
    *,
    expected_sha256: str,
) -> InstallReceipt:
    source = Path(source_path).resolve()
    include = Path(include_dir).resolve()
    before = _sha256(source)
    if before.lower() != str(expected_sha256).lower():
        raise ValueError(
            f"source SHA-256 mismatch: expected {expected_sha256}, actual {before}"
        )
    original = source.read_text(encoding="utf-8")
    if "xiwangDiagnosticInvocationCounter" in original:
        raise ValueError("bounded XiWang diagnostics are already installed")
    plan_setup = (
        "\tstatic const fig::XiWangPopulationPlan xiwangPlan =\n"
        "\t\tfig::XiWangPopulationPlan::from_environment();"
    )
    diagnostic_setup = plan_setup + (
        "\n\tstatic std::atomic<unsigned long long> "
        "xiwangDiagnosticInvocationCounter(0ull);\n"
        "\tconst bool xiwangDiagnosticInvocation =\n"
        "\t\t!toBuildThresholds_ && fig::xiwang_diagnostics_enabled() &&\n"
        "\t\txiwangDiagnosticInvocationCounter.fetch_add(1ull) <\n"
        "\t\t\tfig::xiwang_diagnostic_root_limit();"
    )
    patched = _replace_once(
        original, plan_setup, diagnostic_setup, "XiWang plan setup",
    )
    full_diag_anchor = "\t\tif (bats_diag_enabled() && !toBuildThresholds_) {"
    compact = (
        "\t\tif (xiwangDiagnosticInvocation) {\n"
        "\t\t\tstd::ostringstream xiwangMsg;\n"
        "\t\t\txiwangMsg << \"XIWANG_DIAG SFE_LEVEL\"\n"
        "\t\t\t          << \" sweep_step=\" << diagStep\n"
        "\t\t\t          << \" level=\" << lvl\n"
        "\t\t\t          << \" effort=\" << LVL_EFFORT\n"
        "\t\t\t          << \" successes=\" << numSuccesses;\n"
        "\t\t\tModelSuite::tech_log(xiwangMsg.str() + \"\\n\");\n"
        "\t\t}\n"
    )
    patched = _replace_once(
        patched, full_diag_anchor, compact + full_diag_anchor,
        "full SFE diagnostic block",
    )
    backup = source.with_name(source.name + f".xiwang_diag.{before}.bak")
    if not backup.exists():
        shutil.copy2(source, backup)
    temporary = source.with_name(source.name + ".xiwang_diag.tmp")
    temporary.write_text(patched, encoding="utf-8", newline="\n")
    os.replace(temporary, source)
    include.mkdir(parents=True, exist_ok=True)
    packaged_header = Path(__file__).resolve().parent / "fig_patch" / "XiWangPopulationPlan.h"
    installed_header = include / packaged_header.name
    header_tmp = include / (packaged_header.name + ".tmp")
    shutil.copyfile(packaged_header, header_tmp)
    os.replace(header_tmp, installed_header)
    return InstallReceipt(
        source_path=str(source), backup_path=str(backup),
        header_path=str(installed_header), before_sha256=before,
        after_sha256=_sha256(source),
    )


def install_legacy_population_hook(
    source_path: Path | str,
    include_dir: Path | str,
    *,
    expected_sha256: str,
) -> InstallReceipt:
    """Install the population override for the legacy SFE interface."""
    source = Path(source_path).resolve()
    include = Path(include_dir).resolve()
    before = _sha256(source)
    if before.lower() != str(expected_sha256).lower():
        raise ValueError(f"source SHA-256 mismatch: expected {expected_sha256}, actual {before}")
    original = source.read_text(encoding="utf-8")
    if "XiWangPopulationPlan.h" in original or "xiwangPlan" in original:
        raise ValueError("source already contains a XiWang patch")
    patched = _replace_once(
        original,
        "#include <BatsDiagnostics.h>",
        "#include <BatsDiagnostics.h>\n#include <XiWangPopulationPlan.h>",
        "BatsDiagnostics include",
    )
    assertion = "\tassert(nullptr != impFun_);"
    patched = _replace_once(
        patched, assertion,
        assertion + "\n\tstatic const fig::XiWangPopulationPlan xiwangPlan =\n"
        "\t\tfig::XiWangPopulationPlan::from_environment();",
        "fixed_effort assertion",
    )
    old_effort = "const size_t LVL_EFFORT = lvl_effort(static_cast<unsigned>(LE));"
    new_effort = (
        "const size_t LVL_EFFORT = xiwangPlan.enabled() && !toBuildThresholds_\n"
        "\t\t\t? xiwangPlan.population(static_cast<size_t>(lvl),\n"
        "\t\t\t\t\tlvl_effort(static_cast<unsigned>(LE)))\n"
        "\t\t\t: lvl_effort(static_cast<unsigned>(LE));"
    )
    patched = _replace_once(patched, old_effort, new_effort, "legacy population effort")
    backup = source.with_name(source.name + f".xiwang_legacy.{before}.bak")
    if not backup.exists():
        shutil.copy2(source, backup)
    temporary = source.with_name(source.name + ".xiwang_legacy.tmp")
    temporary.write_text(patched, encoding="utf-8", newline="\n")
    os.replace(temporary, source)
    include.mkdir(parents=True, exist_ok=True)
    packaged_header = Path(__file__).resolve().parent / "fig_patch" / "XiWangPopulationPlan.h"
    installed_header = include / packaged_header.name
    shutil.copy2(packaged_header, installed_header)
    return InstallReceipt(str(source), str(backup), str(installed_header), before, _sha256(source))


def install_legacy_diagnostics(
    source_path: Path | str,
    include_dir: Path | str,
    *,
    expected_sha256: str,
) -> InstallReceipt:
    source = Path(source_path).resolve()
    include = Path(include_dir).resolve()
    before = _sha256(source)
    if before.lower() != str(expected_sha256).lower():
        raise ValueError(f"source SHA-256 mismatch: expected {expected_sha256}, actual {before}")
    original = source.read_text(encoding="utf-8")
    if "xiwangDiagnosticInvocation" in original:
        raise ValueError("legacy XiWang diagnostics are already installed")
    plan_setup = (
        "\tstatic const fig::XiWangPopulationPlan xiwangPlan =\n"
        "\t\tfig::XiWangPopulationPlan::from_environment();"
    )
    diag_setup = plan_setup + (
        "\n\tstatic std::atomic<unsigned long long> xiwangDiagnosticInvocationCounter(0ull);\n"
        "\tconst bool xiwangDiagnosticInvocation =\n"
        "\t\t!toBuildThresholds_ && fig::xiwang_diagnostics_enabled() &&\n"
        "\t\txiwangDiagnosticInvocationCounter.fetch_add(1ull) <\n"
        "\t\t\tfig::xiwang_diagnostic_root_limit();"
    )
    patched = _replace_once(original, plan_setup, diag_setup, "legacy XiWang plan setup")
    legacy_diag = "\t\tif (bats_diag_enabled() && !toBuildThresholds_) {"
    compact = (
        "\t\tif (xiwangDiagnosticInvocation) {\n"
        "\t\t\tstd::ostringstream xiwangMsg;\n"
        "\t\t\txiwangMsg << \"XIWANG_DIAG SFE_LEVEL\"\n"
        "\t\t\t          << \" sweep_step=\" << diagStep\n"
        "\t\t\t          << \" level=\" << lvl\n"
        "\t\t\t          << \" effort=\" << LVL_EFFORT\n"
        "\t\t\t          << \" successes=\" << numSuccesses;\n"
        "\t\t\tModelSuite::tech_log(xiwangMsg.str() + \"\\n\");\n"
        "\t\t}\n"
    )
    patched = _replace_once(patched, legacy_diag, compact + legacy_diag, "legacy diagnostic block")
    backup = source.with_name(source.name + f".xiwang_legacy_diag.{before}.bak")
    if not backup.exists(): shutil.copy2(source, backup)
    temporary = source.with_name(source.name + ".xiwang_legacy_diag.tmp")
    temporary.write_text(patched, encoding="utf-8", newline="\n")
    os.replace(temporary, source)
    return InstallReceipt(str(source), str(backup), str(include / "XiWangPopulationPlan.h"), before, _sha256(source))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Apply the minimal XiWang hook to the restored server FIG source"
    )
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--include-dir", type=Path, required=True)
    parser.add_argument("--expected-source-sha256", required=True)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--migrate-strict-to-sparse", action="store_true")
    parser.add_argument("--install-sparse-diagnostics", action="store_true")
    parser.add_argument("--install-legacy-population", action="store_true")
    parser.add_argument("--install-legacy-diagnostics", action="store_true")
    args = parser.parse_args(argv)
    if sum(bool(v) for v in (args.migrate_strict_to_sparse, args.install_sparse_diagnostics, args.install_legacy_population, args.install_legacy_diagnostics)) > 1:
        parser.error("choose only one migration operation")
    operation = (
        install_legacy_diagnostics if args.install_legacy_diagnostics
        else install_legacy_population_hook if args.install_legacy_population
        else install_sparse_diagnostics if args.install_sparse_diagnostics
        else migrate_strict_to_sparse if args.migrate_strict_to_sparse
        else install
    )
    receipt = operation(
        args.source, args.include_dir, expected_sha256=args.expected_source_sha256,
    )
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(
        json.dumps(asdict(receipt), sort_keys=True, indent=2), encoding="utf-8"
    )
    print(json.dumps(asdict(receipt), sort_keys=True, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
