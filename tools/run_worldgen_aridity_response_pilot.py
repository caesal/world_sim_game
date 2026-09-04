#!/usr/bin/env python3
"""Run the bounded Ver0.3.7.a aridity-response pilot."""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import os
import re
import struct
import subprocess
import sys
import time
from decimal import Decimal, localcontext
from fractions import Fraction
from pathlib import Path
from typing import Any, Sequence


sys.dont_write_bytecode = True

ACK = "VER037A_ARIDITY_RESPONSE_PILOT"
COMMAND = "--probe-worldgen-aridity-response-pilot"
EXPECTED_ACCEPTED_MATRIX_SHA256 = (
    "EAC0390C1130AB1930DA36E2889F1A6F526DE6D5DA42BECC938473735C4A5058"
)
EXPECTED_ACCEPTED_SEED_SUBSET_SHA256 = (
    "28C6756A2AF053E4B106A5E4762022666D03B4C5E44C98E802515DF655362C0C"
)
EXPECTED_FREEZE_SUMMARY_SHA256 = (
    "B979FD60F90A424C3A66E6C58A28D4FBB9FA31CEA97E61D60CF5A338AA4F2384"
)
EXPECTED_FREEZE_HASH_RECORD_SHA256 = (
    "E0C148630962500CBAF25623EA8A66F7AA687449A0C87E341813651770D0E05A"
)
FREEZE_HASH_RECORD_RELATIVE = Path(
    "build/validation/ver037a_aridity_model_expansion_20260823/"
    "11_freeze_audit/accepted_non_aridity_freeze.json"
)
BASELINE_SEED = 2026072301
DIAGNOSTIC_SEEDS = (2026082301, 2026082302)
MAX_WORLDS = 600
MAPS = (
    (0, "Small", 576, 400),
    (1, "Medium", 720, 500),
    (2, "Large", 864, 600),
    (3, "Extreme", 1152, 800),
)
CASES = (
    ("A", 50, 0, 0),
    ("B", 50, 0, 100),
    ("C", 50, 100, 0),
    ("D", 50, 100, 100),
    ("E", 25, 100, 100),
    ("F", 75, 100, 100),
)
CASE_BY_VALUES = {(m, d, b): name for name, m, d, b in CASES}
CANDIDATES = tuple(
    (f"d{divisor}_c{compression}", divisor, compression)
    for divisor in (20, 24, 28)
    for compression in (6, 8, 10)
)
OASIS_PAIRS = tuple(
    (f"o{drop:02d}_t{margin:02d}", drop, margin)
    for drop in (8, 12, 16)
    for margin in (0, 5, 10, 15)
)
OASIS_COLUMN_BY_PAIR = {
    pair_id: f"projected_oasis_{pair_id}"
    for pair_id, _, _ in OASIS_PAIRS
}
CASE_MEAN_RANGES = {
    "A": (Fraction(1, 100), Fraction(6, 100)),
    "B": (Fraction(10, 100), Fraction(25, 100)),
    "C": (Fraction(12, 100), Fraction(28, 100)),
    "D": (Fraction(25, 100), Fraction(45, 100)),
    "E": (Fraction(45, 100), Fraction(65, 100)),
    "F": (Fraction(3, 100), Fraction(15, 100)),
}
CASE_CENTERS = {
    "A": Fraction(35, 1000),
    "B": Fraction(175, 1000),
    "C": Fraction(20, 100),
    "D": Fraction(35, 100),
    "E": Fraction(55, 100),
    "F": Fraction(9, 100),
}
PER_WORLD_CEILINGS = {
    "A": Fraction(12, 100),
    "B": Fraction(35, 100),
    "C": Fraction(40, 100),
    "D": Fraction(55, 100),
    "E": Fraction(75, 100),
    "F": Fraction(25, 100),
}
BASELINE_COLUMNS = (
    "row", "seed", "map_size", "map_name", "width", "height",
    "moisture", "drought", "bias_desert", "desert_limit",
    "semi_arid_limit", "oasis_limit", "generated", "failure_stage",
    "failure_reason", "physical_hash", "land_count", "terrestrial_count",
    "lake_count", "desert_count", "semi_arid_count",
    "combined_arid_count", "non_arid_count", "desert_share",
    "semi_arid_share", "combined_arid_share", "non_arid_share",
    "oasis_count", "wetland_count", "arid_channel_count",
    "oasis_predicate_count", "oasis_reachable_count",
    "oasis_suppressed_count", "oasis_semantic_errors", "ok",
    "drought_divisor", "desert_base", "desert_bias_span",
    "semi_arid_width", "oasis_transition_margin",
    "oasis_transition_limit", "river_channel_count",
    "projected_visible_oasis_margin_0",
    "projected_visible_oasis_margin_5",
    "projected_visible_oasis_margin_10",
    "projected_visible_oasis_margin_15",
    "projected_visible_oasis_margin_20",
    "projected_visible_oasis_margin_25",
    "projected_visible_oasis_margin_30",
)
RESPONSE_COLUMNS = (
    "row", "seed", "map_size", "map_name", "width", "height",
    "moisture", "drought", "bias_desert", "combined_arid_limit",
    "semi_arid_band", "desert_limit", "semi_arid_limit", "oasis_limit",
    "oasis_transition_limit", "generated", "failure_stage",
    "failure_reason", "physical_hash", "land_count", "terrestrial_count",
    "lake_count", "desert_count", "semi_arid_count",
    "combined_arid_count", "non_arid_count", "desert_share",
    "semi_arid_share", "combined_arid_share", "non_arid_share",
    "oasis_count", "wetland_count", "arid_channel_count",
    "oasis_predicate_count", "oasis_reachable_count",
    "oasis_suppressed_count", "oasis_semantic_errors", "ok",
    "drought_divisor", "moisture_compression_span", "oasis_drop",
    "transition_margin", "river_channel_count",
    *(OASIS_COLUMN_BY_PAIR[pair_id] for pair_id, _, _ in OASIS_PAIRS),
)
ARTIFACT_COLUMNS = (
    "seed", "map_size", "map_name", "case_id", "width", "height",
    "physical_hash", "oasis_count", "geography_file",
    "geography_pixel_hash", "climate_file", "climate_pixel_hash",
)


class PilotError(RuntimeError):
    pass


class ZeroSurvivors(PilotError):
    pass


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def _sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def _write_bytes_new(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("xb") as handle:
        handle.write(data)


def _write_text_new(path: Path, text: str) -> None:
    _write_bytes_new(path, text.encode("utf-8"))


def _json_bytes(value: Any) -> bytes:
    return (
        json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    ).encode("utf-8")


def _json_line_bytes(value: Any) -> bytes:
    return (
        json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) +
        "\n"
    ).encode("utf-8")


def _write_json_new(path: Path, value: Any) -> None:
    _write_bytes_new(path, _json_bytes(value))


def _fraction_payload(value: Fraction) -> dict[str, Any]:
    with localcontext() as context:
        context.prec = 28
        decimal = Decimal(value.numerator) / Decimal(value.denominator)
    return {
        "numerator": value.numerator,
        "denominator": value.denominator,
        "decimal": format(decimal, ".12f"),
        "percent": format(decimal * Decimal(100), ".9f"),
    }


def _mean(values: Sequence[Fraction]) -> Fraction:
    if not values:
        raise PilotError("cannot calculate an empty mean")
    return sum(values, Fraction(0, 1)) / len(values)


def _clamp(value: int, low: int, high: int) -> int:
    return max(low, min(high, value))


def _c_div(numerator: int, denominator: int) -> int:
    if denominator <= 0:
        raise PilotError("C-integer emulation requires a positive denominator")
    magnitude = abs(numerator) // denominator
    return -magnitude if numerator < 0 else magnitude


def _case_name(row: dict[str, str]) -> str:
    values = (int(row["moisture"]), int(row["drought"]), int(row["bias_desert"]))
    try:
        return CASE_BY_VALUES[values]
    except KeyError as error:
        raise PilotError(f"unexpected configuration {values}") from error


def _row_key(row: dict[str, str]) -> tuple[int, int, str]:
    return int(row["seed"]), int(row["map_size"]), _case_name(row)


def _share(row: dict[str, str], count_column: str) -> Fraction:
    land = int(row["land_count"])
    return Fraction(int(row[count_column]), land) if land > 0 else Fraction(0, 1)


def _channel_share(oases: int, row: dict[str, str]) -> Fraction:
    channels = int(row["river_channel_count"])
    return Fraction(oases, channels) if channels > 0 else Fraction(0, 1)


def _load_csv(path: Path, columns: Sequence[str]) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        if tuple(reader.fieldnames or ()) != tuple(columns):
            raise PilotError(
                f"CSV schema mismatch in {path}: got {reader.fieldnames}, "
                f"expected {len(columns)} exact columns"
            )
        rows = list(reader)
    if not rows:
        raise PilotError(f"CSV contains no rows: {path}")
    return rows


def _serialize_csv(rows: Sequence[dict[str, str]], columns: Sequence[str]) -> bytes:
    text = io.StringIO(newline="")
    writer = csv.DictWriter(text, fieldnames=columns, lineterminator="\n")
    writer.writeheader()
    for row in rows:
        writer.writerow({column: row[column] for column in columns})
    return text.getvalue().encode("utf-8")


def _validate_shape(rows: Sequence[dict[str, str]], seeds: Sequence[int]) -> None:
    expected = [
        (seed, map_index, case_id)
        for seed in seeds
        for map_index, _, _, _ in MAPS
        for case_id, _, _, _ in CASES
    ]
    actual = [_row_key(row) for row in rows]
    if actual != expected:
        raise PilotError(
            f"matrix shape/order mismatch: expected {len(expected)}, got {len(actual)}"
        )
    map_by_index = {index: (name, width, height) for index, name, width, height in MAPS}
    for row in rows:
        expected_map = map_by_index[int(row["map_size"])]
        actual_map = (row["map_name"], int(row["width"]), int(row["height"]))
        if actual_map != expected_map:
            raise PilotError(f"map identity mismatch at {_row_key(row)}")


def _artifact_manifest(root: Path, output: Path) -> None:
    records = []
    output_resolved = output.resolve()
    for path in sorted(item for item in root.rglob("*") if item.is_file()):
        if path.resolve() == output_resolved:
            continue
        records.append({
            "path": path.relative_to(root).as_posix(),
            "size": path.stat().st_size,
            "sha256": _sha256_file(path),
        })
    _write_json_new(output, {"root": str(root.resolve()), "files": records})


def _scrubbed_environment(settings: dict[str, str]) -> dict[str, str]:
    child = {
        key: value for key, value in os.environ.items()
        if not key.upper().startswith("WORLD_SIM_ARIDITY_")
    }
    child["PYTHONDONTWRITEBYTECODE"] = "1"
    child.update(settings)
    return child


class WorldBudget:
    def __init__(self, ledger: Path) -> None:
        self.ledger = ledger
        self.used = 0
        self.row_count = 0
        self.generated_count = 0
        self.launch_count = 0
        ledger.parent.mkdir(parents=True, exist_ok=True)
        with ledger.open("xb"):
            pass

    def reserve(self, stage: str, identifier: str, count: int) -> None:
        if count < 0 or self.used + count > MAX_WORLDS:
            raise PilotError(
                f"world budget would exceed {MAX_WORLDS}: "
                f"{self.used}+{count} for {stage}/{identifier}"
            )
        before = self.used
        self.used += count
        record = {
            "event": "launch_reserved",
            "stage": stage,
            "identifier": identifier,
            "planned_worlds": count,
            "before": before,
            "after": self.used,
            "maximum": MAX_WORLDS,
        }
        with self.ledger.open("ab") as handle:
            handle.write(_json_line_bytes(record))

    def record_result(self, stage: str, identifier: str, rows: int,
                      generated: int, exit_code: int) -> None:
        self.row_count += rows
        self.generated_count += generated
        self.launch_count += 1
        record = {
            "event": "launch_result",
            "stage": stage,
            "identifier": identifier,
            "row_count": rows,
            "generated_count": generated,
            "exit_code": exit_code,
            "reserved_total": self.used,
            "emitted_row_total": self.row_count,
            "generated_world_total": self.generated_count,
            "launch_count": self.launch_count,
        }
        with self.ledger.open("ab") as handle:
            handle.write(_json_line_bytes(record))


def _run_action(
    exe: Path,
    directory: Path,
    action: str,
    budget: WorldBudget,
    planned_worlds: int,
    identifier: str,
    candidate: tuple[int, int] | None = None,
    oasis_pair: tuple[int, int] | None = None,
    emit_artifacts: bool = False,
    ack_value: str | None = ACK,
) -> dict[str, Any]:
    if directory.exists():
        raise PilotError(f"refusing to overwrite run directory {directory}")
    directory.mkdir(parents=True)
    budget.reserve(action, identifier, planned_worlds)
    settings = {
        "WORLD_SIM_ARIDITY_PROBE_DIR": str(directory.resolve()),
        "WORLD_SIM_ARIDITY_RESPONSE_ACTION": action,
    }
    if ack_value is not None:
        settings["WORLD_SIM_ARIDITY_CALIBRATION_ACK"] = ack_value
    if candidate is not None:
        settings["WORLD_SIM_ARIDITY_RESPONSE_DROUGHT_DIVISOR"] = str(candidate[0])
        settings["WORLD_SIM_ARIDITY_RESPONSE_MOISTURE_COMPRESSION_SPAN"] = str(
            candidate[1]
        )
    if oasis_pair is not None:
        settings["WORLD_SIM_ARIDITY_RESPONSE_OASIS_DROP"] = str(oasis_pair[0])
        settings["WORLD_SIM_ARIDITY_RESPONSE_TRANSITION_MARGIN"] = str(oasis_pair[1])
    if emit_artifacts:
        settings["WORLD_SIM_ARIDITY_RESPONSE_EMIT_ARTIFACTS"] = "1"
    command = [str(exe.resolve()), COMMAND]
    _write_json_new(directory / "command.json", {
        "argv": command,
        "cwd": str(exe.resolve().parent),
        "exe_sha256": _sha256_file(exe),
        "environment": settings,
        "scrubbed_prefix": "WORLD_SIM_ARIDITY_",
        "python_dont_write_bytecode": True,
        "planned_worlds": planned_worlds,
    })
    started = time.perf_counter()
    completed = subprocess.run(
        command,
        cwd=exe.resolve().parent,
        env=_scrubbed_environment(settings),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    elapsed = time.perf_counter() - started
    _write_bytes_new(directory / "stdout.txt", completed.stdout)
    _write_bytes_new(directory / "stderr.txt", completed.stderr)
    _write_text_new(directory / "exit_code.txt", f"{completed.returncode}\n")
    csv_path = None
    rows = None
    columns = None
    if action == "baseline":
        csv_path = directory / "aridity_matrix.csv"
        columns = BASELINE_COLUMNS
    elif action in ("screen", "confirm"):
        csv_path = directory / "aridity_response_matrix.csv"
        columns = RESPONSE_COLUMNS
    parse_error = None
    if csv_path is not None:
        try:
            rows = _load_csv(csv_path, columns)
        except Exception as exception:
            parse_error = f"{type(exception).__name__}: {exception}"
    generated = sum(int(row["generated"]) for row in rows or [])
    budget.record_result(
        action, identifier, len(rows or []), generated, completed.returncode,
    )
    summary_path = directory / "aridity_response_summary.txt"
    result = {
        "action": action,
        "identifier": identifier,
        "exit_code": completed.returncode,
        "elapsed_seconds": elapsed,
        "planned_worlds": planned_worlds,
        "row_count": len(rows or []),
        "generated_count": generated,
        "csv_path": str(csv_path.resolve()) if csv_path and csv_path.is_file() else None,
        "csv_sha256": _sha256_file(csv_path) if csv_path and csv_path.is_file() else None,
        "summary_path": str(summary_path.resolve()) if summary_path.is_file() else None,
        "summary_sha256": _sha256_file(summary_path) if summary_path.is_file() else None,
        "parse_error": parse_error,
        "candidate": {
            "drought_divisor": candidate[0],
            "moisture_compression_span": candidate[1],
        } if candidate else None,
        "oasis_pair": {
            "oasis_drop": oasis_pair[0],
            "transition_margin": oasis_pair[1],
        } if oasis_pair else None,
        "emit_artifacts": emit_artifacts,
    }
    _write_json_new(directory / "run_result.json", result)
    _artifact_manifest(directory, directory / "artifact_manifest.json")
    result["rows"] = rows
    return result


def _audit_freeze(repo: Path, summary_path: Path) -> dict[str, Any]:
    summary_sha = _sha256_file(summary_path)
    if summary_sha != EXPECTED_FREEZE_SUMMARY_SHA256:
        raise PilotError(
            f"post-build freeze summary hash mismatch: expected "
            f"{EXPECTED_FREEZE_SUMMARY_SHA256}, got {summary_sha}"
        )
    summary = json.loads(summary_path.read_text(encoding="utf-8"))
    summary_ok = (
        summary.get("operation") == "read_only_post_build_sha256_recheck" and
        summary.get("accepted_non_aridity_freeze_record_sha256") ==
            EXPECTED_FREEZE_HASH_RECORD_SHA256 and
        summary.get("frozen_file_count") == 22 and
        summary.get("byte_identical_count") == 22 and
        summary.get("mismatch_count") == 0 and
        summary.get("overall_ok") is True
    )
    if not summary_ok:
        raise PilotError("post-build freeze summary does not carry the accepted 22/22 PASS")
    hash_record_path = repo / FREEZE_HASH_RECORD_RELATIVE
    hash_record_sha = _sha256_file(hash_record_path)
    if hash_record_sha != EXPECTED_FREEZE_HASH_RECORD_SHA256:
        raise PilotError(
            f"path-level freeze record hash mismatch: expected "
            f"{EXPECTED_FREEZE_HASH_RECORD_SHA256}, got {hash_record_sha}"
        )
    record = json.loads(hash_record_path.read_text(encoding="utf-8"))
    rows = [row for row in record.get("rows", []) if not row["authorized_current_phase"]]
    checks = []
    for row in rows:
        path = repo / row["path"]
        actual = _sha256_file(path) if path.is_file() else None
        checks.append({
            "path": row["path"],
            "expected_sha256": row["current_sha256"],
            "actual_sha256": actual,
            "ok": actual == row["current_sha256"],
        })
    return {
        "ok": summary_ok and len(rows) == 22 and all(item["ok"] for item in checks),
        "post_build_summary": str(summary_path.resolve()),
        "post_build_summary_sha256": summary_sha,
        "path_hash_record": str(hash_record_path.resolve()),
        "path_hash_record_sha256": hash_record_sha,
        "expected_count": 22,
        "actual_count": len(rows),
        "checks": checks,
    }


def _run_formula(exe: Path, output: Path, budget: WorldBudget) -> dict[str, Any]:
    exact_directory = output / "exact_ack"
    run = _run_action(
        exe, exact_directory, "formula", budget, 0, "formula_ack_reset",
    )
    text = ""
    for name in ("stdout.txt", "stderr.txt", "aridity_response_formula.txt"):
        path = exact_directory / name
        if path.is_file():
            text += path.read_text(encoding="utf-8", errors="replace") + "\n"
    required_tokens = {
        "acknowledgement_cases": r"\backnowledgement_cases=7\b",
        "ack_ok": r"\back_ok=1\b",
        "formula_cases": r"\bformula_cases=1152\b",
        "formula_ok": r"\bformula_ok=1\b",
        "base_air_cases": r"\bbase_air_cases=10\b",
        "base_air_ok": r"\bbase_air_ok=1\b",
        "lifecycle_cases": r"\blifecycle_cases=9\b",
        "lifecycle_ok": r"\blifecycle_ok=1\b",
        "worlds_generated": r"\bworlds_generated=0\b",
        "overall_ok": r"\boverall_ok=1\b",
    }
    token_results = {
        name: re.search(pattern, text) is not None
        for name, pattern in required_tokens.items()
    }
    rejection_cases = (
        ("unset", None),
        ("empty", ""),
        ("wrong_case", "ver037a_aridity_response_pilot"),
        ("prefix", "prefix_VER037A_ARIDITY_RESPONSE_PILOT"),
        ("suffix", "VER037A_ARIDITY_RESPONSE_PILOT_extra"),
        ("arbitrary_nonempty", "VER037A_ARIDITY_RECALIBRATION"),
    )
    rejection_results = []
    for case_name, ack_value in rejection_cases:
        rejected = _run_action(
            exe, output / "ack_rejections" / case_name, "formula", budget, 0,
            f"formula_ack_reject_{case_name}", ack_value=ack_value,
        )
        rejection_text = ""
        for name in ("stdout.txt", "stderr.txt"):
            path = output / "ack_rejections" / case_name / name
            rejection_text += path.read_text(
                encoding="utf-8", errors="replace",
            ) + "\n"
        rejection_ok = (
            rejected["exit_code"] != 0 and rejected["row_count"] == 0 and
            rejected["generated_count"] == 0 and
            re.search(r"\bparse_ok=0\b", rejection_text) is not None and
            re.search(r"\bworlds_generated=0\b", rejection_text) is not None and
            re.search(r"\boverall_ok=0\b", rejection_text) is not None
        )
        rejection_results.append({
            "case": case_name,
            "ack_present": ack_value is not None,
            "ack_value": ack_value,
            "ok": rejection_ok,
            "exit_code": rejected["exit_code"],
            "row_count": rejected["row_count"],
            "generated_count": rejected["generated_count"],
            "elapsed_seconds": rejected["elapsed_seconds"],
            "run_result": str(
                (output / "ack_rejections" / case_name / "run_result.json").resolve()
            ),
        })
    result = {
        "ok": (
            run["exit_code"] == 0 and all(token_results.values()) and
            all(item["ok"] for item in rejection_results)
        ),
        "exit_code": run["exit_code"],
        "required_tokens": token_results,
        "negative_ack_case_count": len(rejection_results),
        "negative_ack_cases": rejection_results,
        "elapsed_seconds": run["elapsed_seconds"] + sum(
            item["elapsed_seconds"] for item in rejection_results
        ),
        "run_result": str((exact_directory / "run_result.json").resolve()),
    }
    _write_json_new(output / "formula_result.json", result)
    if not result["ok"]:
        raise PilotError("formula/ACK/reset probe did not prove every exact zero-world token")
    return result


def _run_baseline(
    exe: Path, output: Path, accepted: Path, budget: WorldBudget,
) -> dict[str, Any]:
    accepted_sha = _sha256_file(accepted)
    if accepted_sha != EXPECTED_ACCEPTED_MATRIX_SHA256:
        raise PilotError(
            f"accepted matrix hash mismatch: expected {EXPECTED_ACCEPTED_MATRIX_SHA256}, "
            f"got {accepted_sha}"
        )
    accepted_rows = _load_csv(accepted, BASELINE_COLUMNS)
    subset = [row for row in accepted_rows if int(row["seed"]) == BASELINE_SEED]
    _validate_shape(subset, (BASELINE_SEED,))
    subset_bytes = _serialize_csv(subset, BASELINE_COLUMNS)
    subset_sha = _sha256_bytes(subset_bytes)
    if subset_sha != EXPECTED_ACCEPTED_SEED_SUBSET_SHA256:
        raise PilotError(
            f"accepted 24-row subset hash mismatch: expected "
            f"{EXPECTED_ACCEPTED_SEED_SUBSET_SHA256}, got {subset_sha}"
        )
    run = _run_action(
        exe, output / "run", "baseline", budget, 24, "production_no_ack",
        ack_value=None,
    )
    rows = run["rows"] or []
    if rows:
        _validate_shape(rows, (BASELINE_SEED,))
    actual_bytes = _serialize_csv(rows, BASELINE_COLUMNS) if rows else b""
    row_mismatches = []
    for expected, actual in zip(subset, rows):
        differences = {
            column: {"expected": expected[column], "actual": actual[column]}
            for column in BASELINE_COLUMNS
            if expected[column] != actual[column]
        }
        if differences:
            row_mismatches.append({"key": _row_key(expected), "differences": differences})
    result = {
        "ok": run["exit_code"] == 0 and len(rows) == 24 and
        actual_bytes == subset_bytes and not row_mismatches,
        "accepted_matrix": str(accepted.resolve()),
        "accepted_matrix_sha256": accepted_sha,
        "accepted_subset_seed": BASELINE_SEED,
        "accepted_subset_row_count": len(subset),
        "accepted_subset_size": len(subset_bytes),
        "accepted_subset_sha256": subset_sha,
        "baseline_csv_sha256": _sha256_bytes(actual_bytes),
        "row_mismatches": row_mismatches,
        "physical_hashes": [row["physical_hash"] for row in rows],
        "run": {key: value for key, value in run.items() if key != "rows"},
    }
    _write_json_new(output / "baseline_identity.json", result)
    if not result["ok"]:
        raise PilotError("no-ACK baseline did not reproduce the accepted 24 rows exactly")
    return result


def _response_formula_values(
    moisture: int, drought: int, bias: int, compression: int,
    oasis_drop: int, transition_margin: int,
) -> tuple[int, int, int, int, int, int]:
    combined = _clamp(
        30 + _c_div(bias * 2, 100) +
        _c_div((moisture - 50) * compression, 25),
        0, 100,
    )
    band = _clamp(
        12 - _c_div(bias * 10, 100) +
        _c_div(max(0, 50 - moisture) * 8, 25),
        2, 20,
    )
    desert = _clamp(combined - band, 0, 100)
    oasis_limit = 42 - _c_div(drought * oasis_drop, 100)
    transition = combined + _c_div(drought * transition_margin, 100)
    return combined, band, desert, combined, oasis_limit, transition


def _group_rows(rows: Sequence[dict[str, str]]) -> dict[tuple[int, str], list[dict[str, str]]]:
    groups: dict[tuple[int, str], list[dict[str, str]]] = {}
    for row in rows:
        groups.setdefault((int(row["map_size"]), _case_name(row)), []).append(row)
    return groups


def _evaluate_response(
    rows: Sequence[dict[str, str]], divisor: int, compression: int,
    active_drop: int, active_margin: int,
) -> dict[str, Any]:
    _validate_shape(rows, DIAGNOSTIC_SEEDS)
    failures: list[dict[str, Any]] = []
    by_world: dict[tuple[int, int], dict[str, dict[str, str]]] = {}
    active_pair_id = f"o{active_drop:02d}_t{active_margin:02d}"
    active_column = OASIS_COLUMN_BY_PAIR[active_pair_id]
    for row in rows:
        seed, map_size, case_id = _row_key(row)
        by_world.setdefault((seed, map_size), {})[case_id] = row
        reasons = []
        if int(row["generated"]) != 1:
            reasons.append("generation_failed")
        if int(row["ok"]) != 1:
            reasons.append("row_not_ok")
        if int(row["land_count"]) <= 0:
            reasons.append("zero_land")
        if int(row["non_arid_count"]) <= 0:
            reasons.append("zero_non_arid_land")
        land = int(row["land_count"])
        if land > 0:
            count_share_fields = (
                ("desert_count", "desert_share"),
                ("semi_arid_count", "semi_arid_share"),
                ("combined_arid_count", "combined_arid_share"),
                ("non_arid_count", "non_arid_share"),
            )
            for count_field, share_field in count_share_fields:
                expected_share = format(int(row[count_field]) / land, ".9f")
                if row[share_field] != expected_share:
                    reasons.append(f"{share_field}_mismatch")
            if int(row["combined_arid_count"]) != (
                int(row["desert_count"]) + int(row["semi_arid_count"])
            ):
                reasons.append("combined_arid_count_mismatch")
            if int(row["combined_arid_count"]) + int(row["non_arid_count"]) != land:
                reasons.append("land_partition_mismatch")
        try:
            if int(row["physical_hash"], 16) == 0:
                reasons.append("zero_physical_hash")
        except ValueError:
            reasons.append("invalid_physical_hash")
        if int(row["oasis_semantic_errors"]) != 0:
            reasons.append("oasis_semantic_errors")
        if int(row["oasis_count"]) != int(row["oasis_reachable_count"]):
            reasons.append("oasis_reachable_mismatch")
        if int(row["drought_divisor"]) != divisor:
            reasons.append("drought_divisor_mismatch")
        if int(row["moisture_compression_span"]) != compression:
            reasons.append("compression_span_mismatch")
        if int(row["oasis_drop"]) != active_drop:
            reasons.append("active_oasis_drop_mismatch")
        if int(row["transition_margin"]) != active_margin:
            reasons.append("active_transition_margin_mismatch")
        values = _response_formula_values(
            int(row["moisture"]), int(row["drought"]), int(row["bias_desert"]),
            compression, active_drop, active_margin,
        )
        fields = (
            "combined_arid_limit", "semi_arid_band", "desert_limit",
            "semi_arid_limit", "oasis_limit", "oasis_transition_limit",
        )
        if tuple(int(row[field]) for field in fields) != values:
            reasons.append("formula_limit_mismatch")
        if int(row["oasis_count"]) != int(row[active_column]):
            reasons.append("active_pair_projection_mismatch")
        if any(int(row[column]) < 0 for column in OASIS_COLUMN_BY_PAIR.values()):
            reasons.append("negative_projected_oasis_count")
        share = _share(row, "combined_arid_count")
        if share > PER_WORLD_CEILINGS[case_id]:
            reasons.append("per_world_combined_arid_ceiling")
        if reasons:
            failures.append({
                "type": "row", "seed": seed, "map_size": map_size,
                "configuration": case_id, "reasons": reasons,
                "combined_arid_share": _fraction_payload(share),
            })
    for (seed, map_size), cases in sorted(by_world.items()):
        shares = {case_id: _share(row, "combined_arid_count") for case_id, row in cases.items()}
        relationships = (
            ("A", "B", "A<=B"), ("A", "C", "A<=C"),
            ("B", "D", "B<=D"), ("C", "D", "C<=D"),
            ("F", "D", "F<=D"), ("D", "E", "D<=E"),
        )
        for left, right, label in relationships:
            if shares[left] > shares[right]:
                failures.append({
                    "type": "monotonic", "seed": seed, "map_size": map_size,
                    "relationship": label,
                    "left": _fraction_payload(shares[left]),
                    "right": _fraction_payload(shares[right]),
                })
    groups = _group_rows(rows)
    statistics = []
    score_total = Fraction(0, 1)
    maximum_deviation = Fraction(0, 1)
    for map_index, map_name, _, _ in MAPS:
        means: dict[str, Fraction] = {}
        desert_means: dict[str, Fraction] = {}
        for case_id, _, _, _ in CASES:
            members = groups[(map_index, case_id)]
            combined = [_share(row, "combined_arid_count") for row in members]
            desert = [_share(row, "desert_count") for row in members]
            means[case_id] = _mean(combined)
            desert_means[case_id] = _mean(desert)
            low, high = CASE_MEAN_RANGES[case_id]
            if not low <= means[case_id] <= high:
                failures.append({
                    "type": "mean_range", "map_size": map_index,
                    "map_name": map_name, "configuration": case_id,
                    "value": _fraction_payload(means[case_id]),
                    "low": _fraction_payload(low), "high": _fraction_payload(high),
                })
            deviation = abs(means[case_id] - CASE_CENTERS[case_id])
            score_total += deviation
            maximum_deviation = max(maximum_deviation, deviation)
            statistics.append({
                "map_size": map_index, "map_name": map_name,
                "configuration": case_id, "sample_count": len(members),
                "combined_arid_mean": _fraction_payload(means[case_id]),
                "combined_arid_min": _fraction_payload(min(combined)),
                "combined_arid_max": _fraction_payload(max(combined)),
                "desert_mean": _fraction_payload(desert_means[case_id]),
            })
        deltas = (
            (means["B"] - means["A"], Fraction(5, 100), "B-A>=5pp"),
            (means["C"] - means["A"], Fraction(7, 100), "C-A>=7pp"),
            (means["D"] - max(means["B"], means["C"]), Fraction(5, 100),
             "D-max(B,C)>=5pp"),
        )
        for value, minimum, label in deltas:
            if value < minimum:
                failures.append({
                    "type": "response_delta", "map_size": map_index,
                    "map_name": map_name, "relationship": label,
                    "value": _fraction_payload(value),
                    "minimum": _fraction_payload(minimum),
                })
        if not Fraction(8, 100) <= desert_means["D"] <= Fraction(25, 100):
            failures.append({
                "type": "configuration_d_desert_mean", "map_size": map_index,
                "map_name": map_name,
                "value": _fraction_payload(desert_means["D"]),
                "low": _fraction_payload(Fraction(8, 100)),
                "high": _fraction_payload(Fraction(25, 100)),
            })
    return {
        "ok": not failures,
        "world_count": len(rows),
        "failures": failures,
        "statistics": statistics,
        "ranking_metrics": {
            "total_center_distance": _fraction_payload(score_total),
            "maximum_size_case_deviation": _fraction_payload(maximum_deviation),
        },
    }


def _evaluate_oasis_pair(
    rows: Sequence[dict[str, str]], pair_id: str, projected: bool,
) -> dict[str, Any]:
    column = OASIS_COLUMN_BY_PAIR[pair_id]
    groups = _group_rows(rows)
    failures = []
    group_results = []
    max_land_deviation = Fraction(0, 1)
    max_channel_deviation = Fraction(0, 1)
    for map_index, map_name, _, _ in MAPS:
        for case_id in ("D", "E", "F"):
            members = groups[(map_index, case_id)]
            counts = [
                int(row[column]) if projected else int(row["oasis_count"])
                for row in members
            ]
            land_mean = _mean([
                Fraction(count, int(row["land_count"]))
                for count, row in zip(counts, members)
            ])
            channel_mean = _mean([
                _channel_share(count, row) for count, row in zip(counts, members)
            ])
            gated = case_id in ("D", "E")
            reasons = []
            if gated and any(count <= 0 for count in counts):
                reasons.append("individual_world_has_zero_oasis")
            if gated and not Fraction(2, 1000) <= land_mean <= Fraction(2, 100):
                reasons.append("two_seed_land_share_mean_out_of_range")
            if gated and not Fraction(3, 100) <= channel_mean <= Fraction(25, 100):
                reasons.append("two_seed_channel_share_mean_out_of_range")
            if gated:
                max_land_deviation = max(
                    max_land_deviation, abs(land_mean - Fraction(1, 100)),
                )
                max_channel_deviation = max(
                    max_channel_deviation, abs(channel_mean - Fraction(14, 100)),
                )
            entry = {
                "map_size": map_index, "map_name": map_name,
                "configuration": case_id, "gated": gated,
                "source": "projected" if projected else "actual",
                "seed_values": [
                    {"seed": int(row["seed"]), "oasis_count": count}
                    for row, count in zip(members, counts)
                ],
                "land_share_mean": _fraction_payload(land_mean),
                "channel_share_mean": _fraction_payload(channel_mean),
                "reasons": reasons, "ok": not reasons,
            }
            group_results.append(entry)
            if reasons:
                failures.append(entry)
    return {
        "ok": not failures,
        "pair_id": pair_id,
        "source": "projected" if projected else "actual",
        "score": {
            "maximum_land_target_deviation": _fraction_payload(max_land_deviation),
            "maximum_channel_target_deviation": _fraction_payload(max_channel_deviation),
        },
        "groups": group_results,
        "failures": failures,
    }


def _payload_fraction(payload: dict[str, Any]) -> Fraction:
    return Fraction(int(payload["numerator"]), int(payload["denominator"]))


def _pair_ranking_key(item: dict[str, Any]) -> tuple[Fraction, Fraction, str]:
    score = item["score"]
    return (
        _payload_fraction(score["maximum_land_target_deviation"]),
        _payload_fraction(score["maximum_channel_target_deviation"]),
        item["pair_id"],
    )


def _candidate_ranking_key(item: dict[str, Any]) -> tuple[Any, ...]:
    metrics = item["response_evaluation"]["ranking_metrics"]
    pair = item["selected_oasis_pair"]
    pair_score = pair["score"]
    return (
        _payload_fraction(metrics["total_center_distance"]),
        _payload_fraction(metrics["maximum_size_case_deviation"]),
        _payload_fraction(pair_score["maximum_land_target_deviation"]),
        _payload_fraction(pair_score["maximum_channel_target_deviation"]),
        item["candidate_id"],
    )


def _screen_candidates(
    exe: Path, output: Path, budget: WorldBudget,
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    items = []
    for candidate_id, divisor, compression in CANDIDATES:
        run = _run_action(
            exe, output / "s" / candidate_id, "screen", budget, 48,
            candidate_id, (divisor, compression), (8, 0),
        )
        if (
            run["exit_code"] != 0 or run["rows"] is None or
            run["parse_error"] is not None or len(run["rows"] or ()) != 48 or
            run["generated_count"] != 48
        ):
            raise PilotError(
                f"incomplete screen subprocess for {candidate_id}: "
                f"exit={run['exit_code']} rows={run['row_count']} "
                f"generated={run['generated_count']} parse={run['parse_error']}"
            )
        response = _evaluate_response(run["rows"], divisor, compression, 8, 0)
        pair_results = [
            _evaluate_oasis_pair(run["rows"], pair_id, projected=True)
            for pair_id, _, _ in OASIS_PAIRS
        ]
        passing_pairs = sorted(
            (pair for pair in pair_results if pair["ok"]),
            key=_pair_ranking_key,
        )
        item = {
            "candidate_id": candidate_id,
            "drought_divisor": divisor,
            "moisture_compression_span": compression,
            "process_ok": True,
            "response_ok": response["ok"],
            "oasis_pair_pass_count": len(passing_pairs),
            "selected_oasis_pair": passing_pairs[0] if passing_pairs else None,
            "ok": bool(response["ok"] and passing_pairs),
            "csv_path": run["csv_path"],
            "csv_sha256": run["csv_sha256"],
            "elapsed_seconds": run["elapsed_seconds"],
            "response_evaluation": response,
            "oasis_pair_evaluations": pair_results,
            "error": None,
        }
        items.append(item)
    survivors = sorted((item for item in items if item["ok"]), key=_candidate_ranking_key)
    _write_json_new(output / "screening_candidates.json", {
        "candidate_count": len(items), "expected_candidate_count": 9,
        "expected_world_count": 432,
        "emitted_row_count": sum(item["process_ok"] * 48 for item in items),
        "generated_world_count": sum(item["process_ok"] * 48 for item in items),
        "candidates": items,
    })
    _write_json_new(output / "screening_survivors.json", {
        "survivor_count": len(survivors),
        "ranking_rule": [
            "lowest total absolute distance from the 24 frozen size/case centers",
            "lowest maximum single size/case center deviation",
            "lowest selected oasis-pair maximum land-share target deviation",
            "lowest selected oasis-pair maximum channel-share target deviation",
            "lexical candidate ID",
        ],
        "survivors": survivors,
        "confirmation_candidate_ids": [item["candidate_id"] for item in survivors[:3]],
    })
    _write_screening_csvs(output, items, survivors)
    if not survivors:
        raise ZeroSurvivors("no response candidate passed every frozen climate and oasis gate")
    return items, survivors


def _write_screening_csvs(
    output: Path, items: Sequence[dict[str, Any]], survivors: Sequence[dict[str, Any]],
) -> None:
    fields = (
        "candidate_id", "drought_divisor", "moisture_compression_span",
        "process_ok", "response_ok", "oasis_pair_pass_count", "ok",
        "selected_pair_id", "total_center_distance",
        "maximum_size_case_deviation", "csv_sha256", "elapsed_seconds", "error",
    )
    rows = []
    for item in items:
        evaluation = item["response_evaluation"] or {}
        metrics = evaluation.get("ranking_metrics", {})
        pair = item["selected_oasis_pair"] or {}
        rows.append({
            **{field: item.get(field, "") for field in fields},
            "selected_pair_id": pair.get("pair_id", ""),
            "total_center_distance": metrics.get("total_center_distance", {}).get("decimal", ""),
            "maximum_size_case_deviation": metrics.get("maximum_size_case_deviation", {}).get("decimal", ""),
        })
    _write_flat_csv(output / "screening_candidates.csv", rows, fields)
    ranking_rows = []
    for rank, item in enumerate(survivors, 1):
        metrics = item["response_evaluation"]["ranking_metrics"]
        pair = item["selected_oasis_pair"]
        ranking_rows.append({
            "rank": rank, "candidate_id": item["candidate_id"],
            "drought_divisor": item["drought_divisor"],
            "moisture_compression_span": item["moisture_compression_span"],
            "selected_pair_id": pair["pair_id"],
            "total_center_distance": metrics["total_center_distance"]["decimal"],
            "maximum_size_case_deviation": metrics["maximum_size_case_deviation"]["decimal"],
            "pair_land_target_deviation": pair["score"]["maximum_land_target_deviation"]["decimal"],
            "pair_channel_target_deviation": pair["score"]["maximum_channel_target_deviation"]["decimal"],
            "confirmation_selected": rank <= 3,
        })
    _write_flat_csv(
        output / "survivor_ranking.csv", ranking_rows,
        tuple(ranking_rows[0]) if ranking_rows else (
            "rank", "candidate_id", "drought_divisor",
            "moisture_compression_span", "selected_pair_id",
            "total_center_distance", "maximum_size_case_deviation",
            "pair_land_target_deviation", "pair_channel_target_deviation",
            "confirmation_selected",
        ),
    )


def _write_flat_csv(path: Path, rows: Sequence[dict[str, Any]], fields: Sequence[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        for row in rows:
            writer.writerow({field: row.get(field, "") for field in fields})


def _compare_confirmation(
    screen_rows: Sequence[dict[str, str]], confirm_rows: Sequence[dict[str, str]],
    pair_id: str,
) -> dict[str, Any]:
    stable_fields = (
        "seed", "map_size", "map_name", "width", "height", "moisture",
        "drought", "bias_desert", "combined_arid_limit", "semi_arid_band",
        "desert_limit", "semi_arid_limit", "generated", "failure_stage",
        "failure_reason", "land_count", "terrestrial_count", "lake_count",
        "desert_count", "semi_arid_count", "combined_arid_count",
        "non_arid_count", "desert_share", "semi_arid_share",
        "combined_arid_share", "non_arid_share",
        "river_channel_count", "drought_divisor", "moisture_compression_span",
        *(OASIS_COLUMN_BY_PAIR[item[0]] for item in OASIS_PAIRS),
    )
    mismatches = []
    if [_row_key(row) for row in screen_rows] != [_row_key(row) for row in confirm_rows]:
        mismatches.append({"type": "row_key_or_order_mismatch"})
    for screen, confirm in zip(screen_rows, confirm_rows):
        differences = {
            field: {"screen": screen[field], "confirm": confirm[field]}
            for field in stable_fields if screen[field] != confirm[field]
        }
        projected = int(screen[OASIS_COLUMN_BY_PAIR[pair_id]])
        actual = int(confirm["oasis_count"])
        if differences or projected != actual:
            mismatches.append({
                "type": "row_identity", "key": _row_key(screen),
                "differences": differences, "projected_oasis": projected,
                "actual_oasis": actual,
            })
    return {
        "ok": not mismatches and len(screen_rows) == len(confirm_rows),
        "pair_id": pair_id, "screen_count": len(screen_rows),
        "confirm_count": len(confirm_rows), "mismatches": mismatches,
    }


def _audit_bmp(
    path: Path, width: int, height: int, expected_pixel_hash: str,
) -> dict[str, Any]:
    data = path.read_bytes()
    valid = len(data) >= 54 and data[:2] == b"BM"
    actual_width = actual_height = bits = None
    pixel_hash = None
    if valid:
        pixel_offset = struct.unpack_from("<I", data, 10)[0]
        actual_width = struct.unpack_from("<i", data, 18)[0]
        actual_height = abs(struct.unpack_from("<i", data, 22)[0])
        bits = struct.unpack_from("<H", data, 28)[0]
        expected_bytes = width * height * 4
        valid = (
            actual_width == width and actual_height == height and bits == 32 and
            pixel_offset + expected_bytes <= len(data)
        )
        if valid:
            hash_value = 1469598103934665603
            for offset in range(pixel_offset, pixel_offset + expected_bytes, 4):
                pixel = struct.unpack_from("<I", data, offset)[0]
                hash_value ^= pixel & 0x00FFFFFF
                hash_value = (hash_value * 1099511628211) & 0xFFFFFFFFFFFFFFFF
            pixel_hash = f"{hash_value:016x}"
            valid = pixel_hash.lower() == expected_pixel_hash.lower()
    return {
        "path": str(path.resolve()), "size": len(data),
        "sha256": _sha256_bytes(data), "width": actual_width,
        "height": actual_height, "bits_per_pixel": bits,
        "expected_pixel_hash": expected_pixel_hash,
        "pixel_hash": pixel_hash, "ok": valid,
    }


def _resolve_artifact(directory: Path, value: str) -> Path:
    path = Path(value)
    if not path.is_absolute():
        path = directory / path
    resolved = path.resolve()
    try:
        resolved.relative_to(directory.resolve())
    except ValueError as error:
        raise PilotError(f"artifact escapes run directory: {value}") from error
    return resolved


def _audit_artifacts(
    directory: Path, candidate_id: str,
    response_rows: Sequence[dict[str, str]],
) -> dict[str, Any]:
    manifest = directory / "aridity_response_artifacts_manifest.csv"
    rows = _load_csv(manifest, ARTIFACT_COLUMNS)
    expected_keys = [
        (DIAGNOSTIC_SEEDS[0], map_index, case_id)
        for map_index in (0, 3)
        for case_id, _, _, _ in CASES
    ]
    actual_keys = [
        (int(row["seed"]), int(row["map_size"]), row["case_id"])
        for row in rows
    ]
    failures = []
    if actual_keys != expected_keys:
        failures.append({"type": "manifest_shape", "expected": expected_keys, "actual": actual_keys})
    images = []
    map_dimensions = {index: (width, height) for index, _, width, height in MAPS}
    map_names = {index: name for index, name, _, _ in MAPS}
    response_by_key = {_row_key(row): row for row in response_rows}
    for row in rows:
        map_index = int(row["map_size"])
        width, height = map_dimensions[map_index]
        key = (int(row["seed"]), map_index, row["case_id"])
        source = response_by_key.get(key)
        if (
            row["map_name"] != map_names[map_index] or
            int(row["width"]) != width or int(row["height"]) != height
        ):
            failures.append({"type": "manifest_map_identity", "key": key})
        if source is None:
            failures.append({"type": "manifest_missing_response_row", "key": key})
        elif (
            row["physical_hash"].lower() != source["physical_hash"].lower() or
            int(row["oasis_count"]) != int(source["oasis_count"])
        ):
            failures.append({
                "type": "manifest_response_identity", "key": key,
                "manifest_physical_hash": row["physical_hash"],
                "response_physical_hash": source["physical_hash"],
                "manifest_oasis_count": row["oasis_count"],
                "response_oasis_count": source["oasis_count"],
            })
        for mode, field in (("geography", "geography_file"), ("climate", "climate_file")):
            path = _resolve_artifact(directory, row[field])
            if not path.is_file():
                failures.append({"type": "missing_image", "path": str(path)})
                continue
            audit = _audit_bmp(
                path, width, height, row[f"{mode}_pixel_hash"],
            )
            audit.update({
                "candidate_id": candidate_id, "seed": int(row["seed"]),
                "map_size": int(row["map_size"]), "case_id": row["case_id"],
                "mode": mode,
            })
            images.append(audit)
            if not audit["ok"]:
                failures.append({"type": "invalid_bmp", "audit": audit})
    return {
        "ok": len(rows) == 12 and len(images) == 24 and not failures,
        "manifest": str(manifest.resolve()), "manifest_sha256": _sha256_file(manifest),
        "manifest_row_count": len(rows), "image_count": len(images),
        "images": images, "failures": failures,
    }


def _make_contact_sheets(audit: dict[str, Any], output: Path) -> list[dict[str, Any]]:
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError as error:
        raise PilotError("Pillow is required for labeled pilot contact sheets") from error
    results = []
    for map_index, map_name, _, _ in (MAPS[0], MAPS[3]):
        items = [item for item in audit["images"] if item["map_size"] == map_index]
        lookup = {(item["case_id"], item["mode"]): item for item in items}
        thumb_size = (288, 200)
        label_height = 22
        gap = 8
        sheet = Image.new(
            "RGB", (6 * (thumb_size[0] + gap) + gap,
                    2 * (thumb_size[1] + label_height + gap) + gap),
            (28, 30, 34),
        )
        draw = ImageDraw.Draw(sheet)
        font = ImageFont.load_default()
        for row_index, mode in enumerate(("climate", "geography")):
            for column_index, (case_id, _, _, _) in enumerate(CASES):
                item = lookup[(case_id, mode)]
                with Image.open(item["path"]) as source:
                    source = source.convert("RGB")
                    source.thumbnail(thumb_size)
                    x = gap + column_index * (thumb_size[0] + gap)
                    y = gap + row_index * (thumb_size[1] + label_height + gap)
                    sheet.paste(source, (x, y + label_height))
                    draw.text((x, y), f"{case_id} {mode}", fill=(235, 238, 242), font=font)
        path = output / f"{map_name.lower()}_climate_geography_contact_sheet.png"
        path.parent.mkdir(parents=True, exist_ok=True)
        if path.exists():
            raise PilotError(f"refusing to overwrite contact sheet {path}")
        sheet.save(path, format="PNG")
        results.append({
            "map_size": map_index, "map_name": map_name,
            "path": str(path.resolve()), "sha256": _sha256_file(path),
            "width": sheet.width, "height": sheet.height,
        })
    return results


def _confirm_candidates(
    exe: Path, output: Path, survivors: Sequence[dict[str, Any]],
    budget: WorldBudget,
) -> list[dict[str, Any]]:
    confirmations = []
    for item in survivors[:3]:
        candidate_id = item["candidate_id"]
        pair_id = item["selected_oasis_pair"]["pair_id"]
        _, drop, margin = next(pair for pair in OASIS_PAIRS if pair[0] == pair_id)
        directory = output / "x" / candidate_id
        run = _run_action(
            exe, directory, "confirm", budget, 48, candidate_id,
            (item["drought_divisor"], item["moisture_compression_span"]),
            (drop, margin), emit_artifacts=True,
        )
        if (
            run["exit_code"] != 0 or run["rows"] is None or
            run["parse_error"] is not None or len(run["rows"] or ()) != 48 or
            run["generated_count"] != 48
        ):
            raise PilotError(
                f"incomplete confirmation subprocess for {candidate_id}: "
                f"exit={run['exit_code']} rows={run['row_count']} "
                f"generated={run['generated_count']} parse={run['parse_error']}"
            )
        response = _evaluate_response(
            run["rows"], item["drought_divisor"],
            item["moisture_compression_span"], drop, margin,
        )
        actual_oasis = _evaluate_oasis_pair(run["rows"], pair_id, projected=False)
        screen_path = Path(item["csv_path"])
        if _sha256_file(screen_path) != item["csv_sha256"]:
            raise PilotError(f"screen matrix changed before confirmation: {candidate_id}")
        screen_rows = _load_csv(screen_path, RESPONSE_COLUMNS)
        comparison = _compare_confirmation(screen_rows, run["rows"], pair_id)
        if not comparison["ok"]:
            raise PilotError(f"screen/confirmation identity mismatch: {candidate_id}")
        artifact_audit = _audit_artifacts(directory, candidate_id, run["rows"])
        if not artifact_audit["ok"]:
            raise PilotError(f"confirmation artifact audit failed: {candidate_id}")
        sheets = _make_contact_sheets(artifact_audit, directory / "contact_sheets")
        if len(sheets) != 2:
            raise PilotError(f"contact-sheet count mismatch: {candidate_id}")
        confirmation = {
            "candidate_id": candidate_id, "pair_id": pair_id,
            "drought_divisor": item["drought_divisor"],
            "moisture_compression_span": item["moisture_compression_span"],
            "oasis_drop": drop, "transition_margin": margin,
            "ok": bool(response["ok"] and actual_oasis["ok"]),
            "response_evaluation": response,
            "actual_oasis_evaluation": actual_oasis,
            "screen_confirmation_identity": comparison,
            "artifact_audit": artifact_audit,
            "contact_sheets": sheets,
            "csv_path": run["csv_path"],
            "csv_sha256": run["csv_sha256"],
            "elapsed_seconds": run["elapsed_seconds"],
            "error": None,
        }
        confirmations.append(confirmation)
        _write_json_new(directory / "confirmation_result.json", confirmation)
    _write_json_new(output / "confirmation_results.json", {
        "candidate_count": len(confirmations),
        "passing_count": sum(item["ok"] for item in confirmations),
        "confirmations": confirmations,
    })
    return confirmations


def _run_all(args: argparse.Namespace, output: Path) -> dict[str, Any]:
    wall_started = time.perf_counter()
    exe = args.exe.resolve()
    if not exe.is_file():
        raise PilotError(f"executable does not exist: {exe}")
    repo = exe.parent
    accepted = args.accepted_matrix.resolve()
    freeze_record = args.freeze_record.resolve()
    budget = WorldBudget(output / "world_budget_events.jsonl")
    initial_freeze = _audit_freeze(repo, freeze_record)
    _write_json_new(output / "01_freeze_pre" / "freeze_result.json", initial_freeze)
    if not initial_freeze["ok"]:
        raise PilotError("accepted non-aridity file freeze failed before launch")
    formula = _run_formula(exe, output / "02_formula", budget)
    baseline = _run_baseline(exe, output / "03_baseline", accepted, budget)
    candidates, survivors = _screen_candidates(exe, output / "04_screen", budget)
    confirmations = _confirm_candidates(
        exe, output / "05_confirmation", survivors, budget,
    )
    passing = [item for item in confirmations if item["ok"]]
    final_freeze = _audit_freeze(repo, freeze_record)
    _write_json_new(output / "06_freeze_post" / "freeze_result.json", final_freeze)
    expected_world_count = 24 + 432 + 48 * len(confirmations)
    count_ok = (
        budget.used == expected_world_count and
        budget.row_count == expected_world_count and
        budget.generated_count == expected_world_count and
        budget.used <= MAX_WORLDS
    )
    overall_ok = bool(passing and final_freeze["ok"] and count_ok)
    subprocess_runtime = float(formula["elapsed_seconds"]) + sum(
        float(item.get("elapsed_seconds") or 0.0) for item in candidates
    ) + sum(float(item.get("elapsed_seconds") or 0.0) for item in confirmations) + \
        float(baseline["run"]["elapsed_seconds"])
    wall_runtime = time.perf_counter() - wall_started
    result = {
        "ok": overall_ok,
        "status": (
            "PILOT_MODEL_FAMILY_PASS" if overall_ok else
            "BLOCKED_ZERO_PILOT_SURVIVORS" if not passing else
            "PILOT_EVIDENCE_INCOMPLETE"
        ),
        "exe": str(exe), "exe_size": exe.stat().st_size,
        "exe_sha256": _sha256_file(exe),
        "accepted_matrix": str(accepted),
        "accepted_matrix_sha256": _sha256_file(accepted),
        "freeze_summary": str(freeze_record),
        "freeze_summary_sha256": _sha256_file(freeze_record),
        "freeze_hash_record": initial_freeze["path_hash_record"],
        "freeze_hash_record_sha256": initial_freeze["path_hash_record_sha256"],
        "formula_result": formula,
        "baseline_result": baseline,
        "candidate_count": len(candidates),
        "survivor_count": len(survivors),
        "survivor_ids": [item["candidate_id"] for item in survivors],
        "confirmation_count": len(confirmations),
        "confirmed_passing_count": len(passing),
        "confirmed_passing_ids": [item["candidate_id"] for item in passing],
        "expected_world_count": expected_world_count,
        "reserved_world_count": budget.used,
        "emitted_row_count": budget.row_count,
        "generated_world_count": budget.generated_count,
        "world_count": budget.generated_count,
        "maximum_world_count": MAX_WORLDS,
        "world_count_ok": count_ok,
        "subprocess_runtime_seconds": subprocess_runtime,
        "wall_runtime_seconds": wall_runtime,
        "runtime_seconds": wall_runtime,
        "initial_freeze_ok": initial_freeze["ok"],
        "final_freeze_ok": final_freeze["ok"],
    }
    _write_json_new(output / "pilot_result.json", result)
    if not passing:
        raise ZeroSurvivors("no pilot candidate passed actual confirmation")
    if not result["ok"]:
        raise PilotError("pilot evidence did not satisfy freeze or exact world-count gates")
    return result


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", choices=("all",), required=True)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--accepted-matrix", type=Path, required=True)
    parser.add_argument("--freeze-record", type=Path, required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    output = args.output.resolve()
    if output.exists():
        print(f"refusing to overwrite output directory: {output}", file=sys.stderr)
        return 2
    output.mkdir(parents=True)
    try:
        result = _run_all(args, output)
        _write_json_new(output / "stage_result.json", {
            "stage": args.stage, "ok": True, "status": result["status"],
            "world_count": result["world_count"],
        })
        _artifact_manifest(output, output / "artifact_manifest.json")
        return 0
    except ZeroSurvivors as exception:
        failure = {
            "stage": args.stage, "ok": False, "status": "BLOCKED_ZERO_PILOT_SURVIVORS",
            "exception_type": type(exception).__name__, "message": str(exception),
        }
        _write_json_new(output / "failure.json", failure)
        _write_json_new(output / "stage_result.json", failure)
        _artifact_manifest(output, output / "artifact_manifest.json")
        print(f"{type(exception).__name__}: {exception}", file=sys.stderr)
        return 3
    except Exception as exception:
        failure = {
            "stage": args.stage, "ok": False, "status": "FAILED",
            "exception_type": type(exception).__name__, "message": str(exception),
        }
        try:
            _write_json_new(output / "failure.json", failure)
            _write_json_new(output / "stage_result.json", failure)
            _artifact_manifest(output, output / "artifact_manifest.json")
        except Exception as evidence_error:
            print(f"failed to preserve failure evidence: {evidence_error}", file=sys.stderr)
        print(f"{type(exception).__name__}: {exception}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
