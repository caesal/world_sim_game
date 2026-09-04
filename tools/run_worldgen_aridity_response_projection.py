#!/usr/bin/env python3
"""Run the bounded Ver0.3.7.a expanded aridity-response projection pilot."""

from __future__ import annotations

import argparse
import csv
import json
import os
import re
import subprocess
import sys
import time
from fractions import Fraction
from pathlib import Path
from typing import Any, Iterable, Sequence


sys.dont_write_bytecode = True
TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import run_worldgen_aridity_response_pilot as legacy


ACK = "VER037A_ARIDITY_RESPONSE_PROJECTION"
COMMAND = "--probe-worldgen-aridity-response-projection"
ACTION_ENV = "WORLD_SIM_ARIDITY_RESPONSE_PROJECTION_ACTION"
ACK_ENV = "WORLD_SIM_ARIDITY_CALIBRATION_ACK"
OUTPUT_ENV = "WORLD_SIM_ARIDITY_PROBE_DIR"
PARAM_ENV = {
    "arid_base": "WORLD_SIM_ARIDITY_RESPONSE_PROJECTION_ARID_BASE",
    "desert_bias_span":
        "WORLD_SIM_ARIDITY_RESPONSE_PROJECTION_DESERT_BIAS_SPAN",
    "drought_divisor":
        "WORLD_SIM_ARIDITY_RESPONSE_PROJECTION_DROUGHT_DIVISOR",
    "moisture_compression_span":
        "WORLD_SIM_ARIDITY_RESPONSE_PROJECTION_MOISTURE_COMPRESSION_SPAN",
    "oasis_drop": "WORLD_SIM_ARIDITY_RESPONSE_PROJECTION_OASIS_DROP",
    "transition_margin":
        "WORLD_SIM_ARIDITY_RESPONSE_PROJECTION_TRANSITION_MARGIN",
}
ARTIFACT_ENV = "WORLD_SIM_ARIDITY_RESPONSE_PROJECTION_EMIT_ARTIFACTS"
EXPECTED_BASELINE_SHA256 = (
    "28C6756A2AF053E4B106A5E4762022666D03B4C5E44C98E802515DF655362C0C"
)
BASELINE_SEED = 2026072301
SCREEN_SEEDS = (2026082301, 2026082302)
MAX_WORLDS = 1136

MAPS = legacy.MAPS
CASES = legacy.CASES
CASE_MEAN_RANGES = legacy.CASE_MEAN_RANGES
CASE_CENTERS = legacy.CASE_CENTERS
PER_WORLD_CEILINGS = legacy.PER_WORLD_CEILINGS
BASELINE_COLUMNS = legacy.BASELINE_COLUMNS
OASIS_PAIRS = legacy.OASIS_PAIRS
OASIS_COLUMN_BY_PAIR = legacy.OASIS_COLUMN_BY_PAIR

ARID_BASES = (30, 31, 32, 33, 34)
DESERT_BIAS_SPANS = (4, 6, 8, 10, 12, 14)
DROUGHT_DIVISORS = (6, 8, 10, 12, 14, 16, 20, 24)
MOISTURE_COMPRESSION_SPANS = (10, 12, 14, 16, 18, 20, 22, 24)
CANDIDATES = tuple(
    {
        "candidate_id": f"a{base}_b{bias:02d}_d{divisor:02d}_c{compression:02d}",
        "arid_base": base,
        "desert_bias_span": bias,
        "drought_divisor": divisor,
        "moisture_compression_span": compression,
    }
    for base in ARID_BASES
    for bias in DESERT_BIAS_SPANS
    for divisor in DROUGHT_DIVISORS
    for compression in MOISTURE_COMPRESSION_SPANS
)

HISTOGRAM_COLUMNS = tuple(f"hist_{value:03d}" for value in range(101))
PREFIX_COLUMNS = tuple(f"prefix_{value:03d}" for value in range(101))
CARRIER_COLUMNS = (
    "row", "seed", "map_size", "map_name", "width", "height",
    "carrier_role", "moisture", "drought", "bias_desert",
    "drought_divisor", "generated", "failure_stage", "failure_reason",
    "physical_hash", "climate_input_hash", "land_count",
    "arid_eligible_count", *HISTOGRAM_COLUMNS, *PREFIX_COLUMNS, "ok",
)
ACTUAL_REQUIRED_COLUMNS = (
    "row", "seed", "map_size", "map_name", "width", "height", "case_id",
    "candidate_id", "pair_id", "moisture", "drought", "bias_desert",
    "arid_base", "desert_bias_span", "drought_divisor",
    "moisture_compression_span", "oasis_drop", "transition_margin",
    "combined_arid_limit", "semi_arid_band", "desert_limit",
    "semi_arid_limit", "oasis_limit", "oasis_transition_limit",
    "generated", "failure_stage", "failure_reason", "physical_hash",
    "climate_input_hash", "land_count", "desert_count", "semi_arid_count",
    "combined_arid_count", "non_arid_count", "oasis_count", "wetland_count",
    "river_channel_count", "oasis_semantic_errors", "ok",
    *(OASIS_COLUMN_BY_PAIR[pair_id] for pair_id, _, _ in OASIS_PAIRS),
)
PROJECTED_COLUMNS = (
    "candidate_id", "arid_base", "desert_bias_span", "drought_divisor",
    "moisture_compression_span", "seed", "map_size", "map_name", "width",
    "height", "case_id", "moisture", "drought", "bias_desert",
    "carrier_role", "climate_input_hash", "physical_hash", "land_count",
    "arid_eligible_count", "combined_arid_limit", "semi_arid_band",
    "desert_limit", "semi_arid_limit", "desert_count", "semi_arid_count",
    "combined_arid_count", "non_arid_count", "desert_share",
    "semi_arid_share", "combined_arid_share", "non_arid_share",
)
ARTIFACT_REQUIRED_COLUMNS = (
    "candidate_id", "pair_id",
    "seed", "map_size", "map_name", "case_id", "width", "height",
    "physical_hash", "climate_input_hash", "oasis_count", "geography_file",
    "geography_pixel_hash", "climate_file", "climate_pixel_hash",
)


class ProjectionError(RuntimeError):
    pass


class ZeroClimateSurvivors(ProjectionError):
    pass


class InsufficientOasisCandidates(ProjectionError):
    pass


def _sha256_file(path: Path) -> str:
    return legacy._sha256_file(path)


def _write_json_new(path: Path, value: Any) -> None:
    legacy._write_json_new(path, value)


def _write_text_new(path: Path, text: str) -> None:
    legacy._write_text_new(path, text)


def _write_bytes_new(path: Path, data: bytes) -> None:
    legacy._write_bytes_new(path, data)


def _fraction_payload(value: Fraction) -> dict[str, Any]:
    return legacy._fraction_payload(value)


def _payload_fraction(value: dict[str, Any]) -> Fraction:
    return legacy._payload_fraction(value)


def _json_line_bytes(value: Any) -> bytes:
    return legacy._json_line_bytes(value)


def _artifact_manifest(root: Path, output: Path) -> None:
    legacy._artifact_manifest(root, output)


def _write_flat_csv(
    path: Path, rows: Iterable[dict[str, Any]], fields: Sequence[str],
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        for row in rows:
            writer.writerow({field: row.get(field, "") for field in fields})


def _load_csv_exact(path: Path, columns: Sequence[str]) -> list[dict[str, str]]:
    return legacy._load_csv(path, columns)


def _load_csv_required(
    path: Path, required: Sequence[str],
) -> tuple[list[dict[str, str]], tuple[str, ...]]:
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        fields = tuple(reader.fieldnames or ())
        missing = [field for field in required if field not in fields]
        if missing:
            raise ProjectionError(f"missing CSV columns in {path}: {missing}")
        rows = list(reader)
    return rows, fields


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
        self.reserved = 0
        self.rows = 0
        self.generated = 0
        self.launches = 0
        ledger.parent.mkdir(parents=True, exist_ok=True)
        with ledger.open("xb"):
            pass

    def _append(self, value: dict[str, Any]) -> None:
        with self.ledger.open("ab") as handle:
            handle.write(_json_line_bytes(value))

    def reserve(self, stage: str, identifier: str, planned: int) -> None:
        if planned < 0 or self.reserved + planned > MAX_WORLDS:
            raise ProjectionError(
                f"world budget would exceed {MAX_WORLDS}: "
                f"{self.reserved}+{planned} for {stage}/{identifier}"
            )
        before = self.reserved
        self.reserved += planned
        self._append({
            "event": "launch_reserved", "stage": stage,
            "identifier": identifier, "planned_worlds": planned,
            "before": before, "after": self.reserved, "maximum": MAX_WORLDS,
        })

    def record(
        self, stage: str, identifier: str, row_count: int,
        generated_count: int, exit_code: int,
    ) -> None:
        self.rows += row_count
        self.generated += generated_count
        self.launches += 1
        self._append({
            "event": "launch_result", "stage": stage,
            "identifier": identifier, "row_count": row_count,
            "generated_count": generated_count, "exit_code": exit_code,
            "reserved_total": self.reserved, "emitted_row_total": self.rows,
            "generated_world_total": self.generated,
            "launch_count": self.launches,
        })

    def record_error(
        self, stage: str, identifier: str, error: BaseException,
    ) -> None:
        self.launches += 1
        self._append({
            "event": "launch_error", "stage": stage,
            "identifier": identifier, "row_count": None,
            "generated_count": None, "exception_type": type(error).__name__,
            "message": str(error), "reserved_total": self.reserved,
            "emitted_row_total": self.rows,
            "generated_world_total": self.generated,
            "launch_count": self.launches,
        })

    def inherit(self, source_ledger: Path) -> None:
        if self.reserved or self.rows or self.generated or self.launches:
            raise ProjectionError("world budget inheritance must be first")
        events = [
            json.loads(line) for line in source_ledger.read_text(
                encoding="utf-8",
            ).splitlines() if line.strip()
        ]
        results = [event for event in events if event.get("event") == "launch_result"]
        if not results:
            raise ProjectionError("resume ledger has no completed launch")
        last = results[-1]
        expected = {
            "reserved_total": 224, "emitted_row_total": 224,
            "generated_world_total": 224, "launch_count": 10,
        }
        if any(last.get(key) != value for key, value in expected.items()):
            raise ProjectionError(f"resume ledger totals mismatch: {last}")
        self.reserved = self.rows = self.generated = 224
        self.launches = 10
        self._append({
            "event": "inherited_world_budget", "source": str(source_ledger),
            "source_sha256": _sha256_file(source_ledger),
            "reserved_total": self.reserved, "emitted_row_total": self.rows,
            "generated_world_total": self.generated,
            "launch_count": self.launches, "maximum": MAX_WORLDS,
        })


def _candidate_environment(candidate: dict[str, Any]) -> dict[str, str]:
    return {
        PARAM_ENV[dimension]: str(candidate[dimension])
        for dimension in (
            "arid_base", "desert_bias_span", "drought_divisor",
            "moisture_compression_span",
        )
    }


def _pair_environment(pair_id: str) -> dict[str, str]:
    try:
        _, drop, margin = next(pair for pair in OASIS_PAIRS if pair[0] == pair_id)
    except StopIteration as error:
        raise ProjectionError(f"unknown oasis pair {pair_id}") from error
    return {
        PARAM_ENV["oasis_drop"]: str(drop),
        PARAM_ENV["transition_margin"]: str(margin),
    }


def _run_action(
    exe: Path, directory: Path, action: str, budget: WorldBudget,
    planned_worlds: int, identifier: str,
    candidate: dict[str, Any] | None = None, pair_id: str | None = None,
    emit_artifacts: bool = False, ack_value: str | None = ACK,
) -> dict[str, Any]:
    if directory.exists():
        raise ProjectionError(f"refusing to overwrite run directory {directory}")
    directory.mkdir(parents=True)
    budget.reserve(action, identifier, planned_worlds)
    settings = {OUTPUT_ENV: str(directory.resolve()), ACTION_ENV: action}
    if ack_value is not None:
        settings[ACK_ENV] = ack_value
    if candidate:
        settings.update(_candidate_environment(candidate))
    if pair_id:
        settings.update(_pair_environment(pair_id))
    if emit_artifacts:
        settings[ARTIFACT_ENV] = "1"
    argv = [str(exe.resolve()), COMMAND]
    _write_json_new(directory / "command.json", {
        "argv": argv, "cwd": str(exe.resolve().parent),
        "exe_sha256": _sha256_file(exe), "environment": settings,
        "scrubbed_prefix": "WORLD_SIM_ARIDITY_",
        "planned_worlds": planned_worlds,
    })
    started = time.perf_counter()
    try:
        completed = subprocess.run(
            argv, cwd=exe.resolve().parent, env=_scrubbed_environment(settings),
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
        )
    except BaseException as error:
        budget.record_error(action, identifier, error)
        _write_json_new(directory / "launch_error.json", {
            "exception_type": type(error).__name__, "message": str(error),
        })
        raise
    elapsed = time.perf_counter() - started
    _write_bytes_new(directory / "stdout.txt", completed.stdout)
    _write_bytes_new(directory / "stderr.txt", completed.stderr)
    _write_text_new(directory / "exit_code.txt", f"{completed.returncode}\n")
    csv_path: Path | None = None
    rows: list[dict[str, str]] | None = None
    fields: tuple[str, ...] | None = None
    parse_error = None
    try:
        if action == "baseline":
            csv_path = directory / "aridity_matrix.csv"
            rows = _load_csv_exact(csv_path, BASELINE_COLUMNS)
            fields = tuple(BASELINE_COLUMNS)
        elif action == "carriers":
            csv_path = directory / "aridity_response_projection_carriers.csv"
            rows, fields = _load_csv_required(csv_path, CARRIER_COLUMNS)
        elif action in ("oasis", "confirm"):
            csv_path = directory / "aridity_response_projection_actual.csv"
            rows, fields = _load_csv_required(csv_path, ACTUAL_REQUIRED_COLUMNS)
    except Exception as error:
        parse_error = f"{type(error).__name__}: {error}"
    generated = sum(int(row.get("generated", "0")) for row in rows or ())
    budget.record(
        action, identifier, len(rows or ()), generated, completed.returncode,
    )
    summary = directory / "aridity_response_projection_summary.txt"
    result = {
        "action": action, "identifier": identifier,
        "exit_code": completed.returncode, "elapsed_seconds": elapsed,
        "planned_worlds": planned_worlds, "row_count": len(rows or ()),
        "generated_count": generated, "parse_error": parse_error,
        "csv_path": str(csv_path.resolve()) if csv_path and csv_path.is_file() else None,
        "csv_sha256": _sha256_file(csv_path) if csv_path and csv_path.is_file() else None,
        "csv_fields": list(fields or ()),
        "summary_path": str(summary.resolve()) if summary.is_file() else None,
        "summary_sha256": _sha256_file(summary) if summary.is_file() else None,
        "candidate": candidate, "pair_id": pair_id,
        "emit_artifacts": emit_artifacts,
    }
    _write_json_new(directory / "run_result.json", result)
    _artifact_manifest(directory, directory / "artifact_manifest.json")
    result["rows"] = rows
    return result


def _run_formula(exe: Path, output: Path, budget: WorldBudget) -> dict[str, Any]:
    exact = _run_action(
        exe, output / "exact_ack", "formula", budget, 0,
        "projection_formula_ack_reset",
    )
    text = ""
    for name in (
        "stdout.txt", "stderr.txt", "aridity_response_projection_summary.txt",
        "aridity_response_projection_formula.txt",
    ):
        path = output / "exact_ack" / name
        if path.is_file():
            text += path.read_text(encoding="utf-8", errors="replace") + "\n"
    required = {
        "ack_ok": r"\back_ok=1\b",
        "candidate_cases": r"\bcandidate_cases=1920\b",
        "oasis_pair_cases": r"\boasis_pair_cases=12\b",
        "formula_ok": r"\bformula_ok=1\b",
        "lifecycle_ok": r"\blifecycle_ok=1\b",
        "worlds_generated": r"\bworlds_generated=0\b",
        "overall_ok": r"\boverall_ok=1\b",
    }
    tokens = {name: re.search(pattern, text) is not None
              for name, pattern in required.items()}
    rejection_values = (
        ("unset", None), ("empty", ""),
        ("wrong_case", "ver037a_aridity_response_projection"),
        ("prefix", "prefix_VER037A_ARIDITY_RESPONSE_PROJECTION"),
        ("suffix", "VER037A_ARIDITY_RESPONSE_PROJECTION_extra"),
        ("old_pilot_ack", "VER037A_ARIDITY_RESPONSE_PILOT"),
        ("arbitrary", "VER037A_ARIDITY_RECALIBRATION"),
    )
    rejections = []
    for name, value in rejection_values:
        run = _run_action(
            exe, output / "ack_rejections" / name, "formula", budget, 0,
            f"projection_ack_reject_{name}", ack_value=value,
        )
        rejection_text = ""
        for stream in ("stdout.txt", "stderr.txt"):
            rejection_text += (output / "ack_rejections" / name / stream).read_text(
                encoding="utf-8", errors="replace",
            )
        ok = (
            run["exit_code"] != 0 and run["row_count"] == 0 and
            run["generated_count"] == 0 and
            re.search(r"\bworlds_generated=0\b", rejection_text) is not None and
            re.search(r"\boverall_ok=0\b", rejection_text) is not None
        )
        rejections.append({
            "case": name, "ack_value": value, "ok": ok,
            "exit_code": run["exit_code"],
            "generated_count": run["generated_count"],
        })
    result = {
        "ok": exact["exit_code"] == 0 and all(tokens.values()) and
              all(item["ok"] for item in rejections),
        "exact_run": str((output / "exact_ack" / "run_result.json").resolve()),
        "required_tokens": tokens, "negative_ack_cases": rejections,
        "worlds_generated": 0,
    }
    _write_json_new(output / "formula_result.json", result)
    if not result["ok"]:
        raise ProjectionError("formula/ACK/reset matrix failed")
    return result


def _run_baseline(
    exe: Path, output: Path, accepted_path: Path, budget: WorldBudget,
) -> dict[str, Any]:
    accepted = _load_csv_exact(accepted_path, BASELINE_COLUMNS)
    subset = [row for row in accepted if int(row["seed"]) == BASELINE_SEED]
    legacy._validate_shape(subset, (BASELINE_SEED,))
    expected_bytes = legacy._serialize_csv(subset, BASELINE_COLUMNS)
    expected_sha = legacy._sha256_bytes(expected_bytes)
    if expected_sha != EXPECTED_BASELINE_SHA256:
        raise ProjectionError(
            f"accepted baseline subset hash mismatch: {expected_sha}"
        )
    run = _run_action(
        exe, output / "run", "baseline", budget, 24,
        "production_no_ack", ack_value=None,
    )
    rows = run["rows"] or []
    actual_bytes = legacy._serialize_csv(rows, BASELINE_COLUMNS) if rows else b""
    actual_sha = legacy._sha256_bytes(actual_bytes)
    result = {
        "ok": run["exit_code"] == 0 and run["parse_error"] is None and
              len(rows) == 24 and run["generated_count"] == 24 and
              actual_bytes == expected_bytes and actual_sha == EXPECTED_BASELINE_SHA256,
        "accepted_matrix": str(accepted_path.resolve()),
        "accepted_matrix_sha256": _sha256_file(accepted_path),
        "expected_subset_sha256": expected_sha,
        "actual_subset_sha256": actual_sha,
        "row_count": len(rows), "run": {k: v for k, v in run.items() if k != "rows"},
    }
    _write_json_new(output / "baseline_identity.json", result)
    if not result["ok"]:
        raise ProjectionError("24-world no-ACK baseline identity failed")
    return result


def _carrier_key(row: dict[str, str]) -> tuple[int, int, str, int]:
    moisture = int(row["moisture"])
    drought = int(row["drought"])
    divisor = int(row["drought_divisor"])
    if moisture == 50 and drought == 0:
        role = "AB"
        if divisor != 24:
            raise ProjectionError(
                f"neutral carrier did not use production divisor 24: {divisor}"
            )
    elif moisture == 50 and drought == 100:
        role = "CD"
    elif moisture == 25 and drought == 100:
        role = "E"
    elif moisture == 75 and drought == 100:
        role = "F"
    else:
        raise ProjectionError(f"invalid carrier case values: {moisture}/{drought}")
    if row["carrier_role"] != role:
        raise ProjectionError(
            f"carrier role mismatch: expected {role}, got {row['carrier_role']}"
        )
    return int(row["seed"]), int(row["map_size"]), role, divisor


def _validate_carrier(row: dict[str, str]) -> dict[str, Any]:
    hist = [int(row[column]) for column in HISTOGRAM_COLUMNS]
    prefix = [int(row[column]) for column in PREFIX_COLUMNS]
    running = 0
    prefix_ok = True
    for index, count in enumerate(hist):
        if count < 0:
            prefix_ok = False
        running += count
        prefix_ok &= prefix[index] == running
    try:
        physical_hash_ok = int(row["physical_hash"], 16) != 0
        input_hash_ok = int(row["climate_input_hash"], 16) != 0
    except ValueError:
        physical_hash_ok = input_hash_ok = False
    eligible = int(row["arid_eligible_count"])
    land = int(row["land_count"])
    map_index = int(row["map_size"])
    dimensions_ok = (
        0 <= map_index < len(MAPS) and
        row["map_name"] == MAPS[map_index][1] and
        int(row["width"]) == MAPS[map_index][2] and
        int(row["height"]) == MAPS[map_index][3]
    )
    expected_configuration = {
        "AB": (50, 0, 0), "CD": (50, 100, 0),
        "E": (25, 100, 100), "F": (75, 100, 100),
    }.get(row["carrier_role"])
    configuration_ok = (
        int(row["seed"]) in SCREEN_SEEDS and
        expected_configuration == (
            int(row["moisture"]), int(row["drought"]),
            int(row["bias_desert"]),
        )
    )
    return {
        "ok": (
            int(row["generated"]) == 1 and int(row["ok"]) == 1 and
            physical_hash_ok and input_hash_ok and land > 0 and
            0 <= eligible <= land and running == eligible and prefix_ok and
            dimensions_ok and configuration_ok
        ),
        "histogram_total": running, "eligible_count": eligible,
        "land_count": land, "prefix_ok": prefix_ok,
        "physical_hash_ok": physical_hash_ok,
        "climate_input_hash_ok": input_hash_ok,
        "dimensions_ok": dimensions_ok, "configuration_ok": configuration_ok,
    }


def _run_carriers(
    exe: Path, output: Path, budget: WorldBudget,
) -> tuple[dict[tuple[int, int, str, int], dict[str, str]], dict[str, Any]]:
    run = _run_action(
        exe, output / "run", "carriers", budget, 200,
        "shared_climate_inputs",
    )
    rows = run["rows"] or []
    checks = []
    lookup: dict[tuple[int, int, str, int], dict[str, str]] = {}
    for row in rows:
        key = _carrier_key(row)
        if key in lookup:
            raise ProjectionError(f"duplicate carrier key {key}")
        lookup[key] = row
        checks.append({"key": list(key), **_validate_carrier(row)})
    expected = {
        (seed, map_index, "AB", 24)
        for seed in SCREEN_SEEDS for map_index, _, _, _ in MAPS
    }
    expected.update({
        (seed, map_index, role, divisor)
        for seed in SCREEN_SEEDS
        for map_index, _, _, _ in MAPS
        for divisor in DROUGHT_DIVISORS
        for role in ("CD", "E", "F")
    })
    result = {
        "ok": run["exit_code"] == 0 and run["parse_error"] is None and
              len(rows) == 200 and run["generated_count"] == 200 and
              set(lookup) == expected and all(item["ok"] for item in checks),
        "row_count": len(rows), "expected_row_count": 200,
        "unique_key_count": len(lookup), "checks": checks,
        "csv_path": run["csv_path"], "csv_sha256": run["csv_sha256"],
        "run": {k: v for k, v in run.items() if k != "rows"},
    }
    _write_json_new(output / "carrier_validation.json", result)
    if not result["ok"]:
        raise ProjectionError("200-world carrier validation failed")
    return lookup, result


def _reuse_formula(source: Path, output: Path) -> dict[str, Any]:
    source_path = source / "02_formula" / "formula_result.json"
    value = json.loads(source_path.read_text(encoding="utf-8"))
    required = (
        value.get("ok") is True and value.get("worlds_generated") == 0 and
        all(value.get("required_tokens", {}).values()) and
        all(item.get("ok") is True for item in value.get("negative_ack_cases", ()))
    )
    result = {
        "ok": required, "reused": True, "source": str(source_path.resolve()),
        "source_sha256": _sha256_file(source_path), "worlds_generated": 0,
    }
    _write_json_new(output / "reused_formula_result.json", result)
    if not result["ok"]:
        raise ProjectionError("reused formula evidence failed validation")
    return result


def _reuse_baseline(
    source: Path, output: Path, accepted_path: Path,
) -> dict[str, Any]:
    accepted = _load_csv_exact(accepted_path, BASELINE_COLUMNS)
    subset = [row for row in accepted if int(row["seed"]) == BASELINE_SEED]
    legacy._validate_shape(subset, (BASELINE_SEED,))
    expected_bytes = legacy._serialize_csv(subset, BASELINE_COLUMNS)
    expected_sha = legacy._sha256_bytes(expected_bytes)
    csv_path = source / "03_baseline" / "run" / "aridity_matrix.csv"
    rows = _load_csv_exact(csv_path, BASELINE_COLUMNS)
    actual_bytes = legacy._serialize_csv(rows, BASELINE_COLUMNS)
    run_path = source / "03_baseline" / "run" / "run_result.json"
    run = json.loads(run_path.read_text(encoding="utf-8"))
    result = {
        "ok": (
            expected_sha == EXPECTED_BASELINE_SHA256 and
            actual_bytes == expected_bytes and len(rows) == 24 and
            run.get("exit_code") == 0 and run.get("row_count") == 24 and
            run.get("generated_count") == 24 and
            run.get("parse_error") is None
        ),
        "reused": True, "accepted_matrix": str(accepted_path.resolve()),
        "accepted_matrix_sha256": _sha256_file(accepted_path),
        "expected_subset_sha256": expected_sha,
        "actual_subset_sha256": legacy._sha256_bytes(actual_bytes),
        "csv_path": str(csv_path.resolve()), "csv_sha256": _sha256_file(csv_path),
        "run_result": str(run_path.resolve()),
        "run_result_sha256": _sha256_file(run_path),
    }
    _write_json_new(output / "reused_baseline_identity.json", result)
    if not result["ok"]:
        raise ProjectionError("reused 24-world baseline evidence failed validation")
    return result


def _reuse_carriers(
    source: Path, output: Path,
) -> tuple[dict[tuple[int, int, str, int], dict[str, str]], dict[str, Any]]:
    csv_path = source / "04_carriers" / "run" / \
        "aridity_response_projection_carriers.csv"
    rows, _ = _load_csv_required(csv_path, CARRIER_COLUMNS)
    lookup: dict[tuple[int, int, str, int], dict[str, str]] = {}
    checks = []
    for row in rows:
        key = _carrier_key(row)
        if key in lookup:
            raise ProjectionError(f"duplicate reused carrier key {key}")
        lookup[key] = row
        checks.append({"key": list(key), **_validate_carrier(row)})
    expected = {
        (seed, map_index, "AB", 24)
        for seed in SCREEN_SEEDS for map_index, _, _, _ in MAPS
    }
    expected.update({
        (seed, map_index, role, divisor)
        for seed in SCREEN_SEEDS for map_index, _, _, _ in MAPS
        for divisor in DROUGHT_DIVISORS for role in ("CD", "E", "F")
    })
    run_path = source / "04_carriers" / "run" / "run_result.json"
    run = json.loads(run_path.read_text(encoding="utf-8"))
    result = {
        "ok": (
            len(rows) == 200 and set(lookup) == expected and
            all(item["ok"] for item in checks) and
            run.get("exit_code") == 0 and run.get("row_count") == 200 and
            run.get("generated_count") == 200 and run.get("parse_error") is None
        ),
        "reused": True, "row_count": len(rows),
        "unique_key_count": len(lookup), "checks": checks,
        "csv_path": str(csv_path.resolve()), "csv_sha256": _sha256_file(csv_path),
        "run_result": str(run_path.resolve()),
        "run_result_sha256": _sha256_file(run_path),
    }
    _write_json_new(output / "reused_carrier_validation.json", result)
    if not result["ok"]:
        raise ProjectionError("reused 200-world carrier validation failed")
    return lookup, result


def _c_div(numerator: int, denominator: int) -> int:
    return legacy._c_div(numerator, denominator)


def _clamp(value: int, low: int, high: int) -> int:
    return legacy._clamp(value, low, high)


def _limits(
    candidate: dict[str, Any], moisture: int, drought: int, bias: int,
) -> tuple[int, int, int, int]:
    combined = _clamp(
        candidate["arid_base"] + _c_div(
            bias * candidate["desert_bias_span"], 100,
        ) + _c_div(
            (moisture - 50) * candidate["moisture_compression_span"], 25,
        ), 0, 100,
    )
    band = _clamp(
        12 - _c_div(bias * 10, 100) +
        _c_div(max(0, 50 - moisture) * 8, 25), 2, 20,
    )
    desert = _clamp(combined - band, 0, 100)
    return combined, band, desert, combined


def _prefix_count(carrier: dict[str, str], exclusive_limit: int) -> int:
    if exclusive_limit <= 0:
        return 0
    index = min(exclusive_limit - 1, 100)
    return int(carrier[f"prefix_{index:03d}"])


def _projected_row(
    candidate: dict[str, Any], seed: int, map_index: int,
    case_id: str, moisture: int, drought: int, bias: int,
    carriers: dict[tuple[int, int, str, int], dict[str, str]],
) -> dict[str, Any]:
    role = "AB" if case_id in ("A", "B") else "CD" if case_id in ("C", "D") else case_id
    divisor = 24 if role == "AB" else candidate["drought_divisor"]
    carrier = carriers[(seed, map_index, role, divisor)]
    combined_limit, band, desert_limit, semi_limit = _limits(
        candidate, moisture, drought, bias,
    )
    desert = _prefix_count(carrier, desert_limit)
    combined = _prefix_count(carrier, semi_limit)
    semi = combined - desert
    land = int(carrier["land_count"])
    non_arid = land - combined
    if not 0 <= desert <= combined <= land:
        raise ProjectionError(
            f"invalid projected partition for {candidate['candidate_id']} "
            f"{seed}/{map_index}/{case_id}"
        )
    def share(count: int) -> str:
        return format(count / land, ".9f") if land else "0.000000000"
    return {
        **candidate, "seed": seed, "map_size": map_index,
        "map_name": MAPS[map_index][1], "width": MAPS[map_index][2],
        "height": MAPS[map_index][3], "case_id": case_id,
        "moisture": moisture, "drought": drought, "bias_desert": bias,
        "carrier_role": role, "climate_input_hash": carrier["climate_input_hash"],
        "physical_hash": carrier["physical_hash"], "land_count": land,
        "arid_eligible_count": int(carrier["arid_eligible_count"]),
        "combined_arid_limit": combined_limit, "semi_arid_band": band,
        "desert_limit": desert_limit, "semi_arid_limit": semi_limit,
        "desert_count": desert, "semi_arid_count": semi,
        "combined_arid_count": combined, "non_arid_count": non_arid,
        "desert_share": share(desert), "semi_arid_share": share(semi),
        "combined_arid_share": share(combined),
        "non_arid_share": share(non_arid),
    }


def _share(row: dict[str, Any], field: str) -> Fraction:
    return Fraction(int(row[field]), int(row["land_count"]))


def _evaluate_climate_candidate(
    candidate: dict[str, Any], rows: Sequence[dict[str, Any]],
) -> dict[str, Any]:
    failures: list[dict[str, Any]] = []
    by_world: dict[tuple[int, int], dict[str, dict[str, Any]]] = {}
    by_group: dict[tuple[int, str], list[dict[str, Any]]] = {}
    for row in rows:
        key = (int(row["seed"]), int(row["map_size"]))
        by_world.setdefault(key, {})[str(row["case_id"])] = row
        by_group.setdefault((int(row["map_size"]), str(row["case_id"])), []).append(row)
        case_id = str(row["case_id"])
        share = _share(row, "combined_arid_count")
        reasons = []
        if int(row["land_count"]) <= 0:
            reasons.append("zero_land")
        if int(row["non_arid_count"]) <= 0:
            reasons.append("zero_non_arid_land")
        if int(row["climate_input_hash"], 16) == 0:
            reasons.append("zero_climate_input_hash")
        if share > PER_WORLD_CEILINGS[case_id]:
            reasons.append("per_world_combined_arid_ceiling")
        if reasons:
            failures.append({
                "type": "row", "seed": row["seed"],
                "map_size": row["map_size"], "configuration": case_id,
                "reasons": reasons, "combined_arid_share": _fraction_payload(share),
            })
    relationships = (
        ("A", "B", "A<=B"), ("A", "C", "A<=C"),
        ("B", "D", "B<=D"), ("C", "D", "C<=D"),
        ("F", "D", "F<=D"), ("D", "E", "D<=E"),
    )
    for (seed, map_index), cases in sorted(by_world.items()):
        if set(cases) != {item[0] for item in CASES}:
            failures.append({"type": "missing_cases", "seed": seed, "map_size": map_index})
            continue
        shares = {name: _share(row, "combined_arid_count") for name, row in cases.items()}
        for left, right, label in relationships:
            if shares[left] > shares[right]:
                failures.append({
                    "type": "monotonic", "seed": seed, "map_size": map_index,
                    "relationship": label, "left": _fraction_payload(shares[left]),
                    "right": _fraction_payload(shares[right]),
                })
    statistics = []
    total_distance = Fraction(0, 1)
    maximum_deviation = Fraction(0, 1)
    for map_index, map_name, _, _ in MAPS:
        means: dict[str, Fraction] = {}
        desert_means: dict[str, Fraction] = {}
        for case_id, _, _, _ in CASES:
            members = by_group.get((map_index, case_id), [])
            if len(members) != 2:
                failures.append({
                    "type": "sample_count", "map_size": map_index,
                    "configuration": case_id, "actual": len(members), "expected": 2,
                })
                continue
            combined_values = [_share(row, "combined_arid_count") for row in members]
            desert_values = [_share(row, "desert_count") for row in members]
            means[case_id] = legacy._mean(combined_values)
            desert_means[case_id] = legacy._mean(desert_values)
            low, high = CASE_MEAN_RANGES[case_id]
            if not low <= means[case_id] <= high:
                failures.append({
                    "type": "mean_range", "map_size": map_index,
                    "configuration": case_id, "value": _fraction_payload(means[case_id]),
                    "low": _fraction_payload(low), "high": _fraction_payload(high),
                })
            deviation = abs(means[case_id] - CASE_CENTERS[case_id])
            total_distance += deviation
            maximum_deviation = max(maximum_deviation, deviation)
            statistics.append({
                "map_size": map_index, "map_name": map_name,
                "configuration": case_id,
                "combined_arid_mean": _fraction_payload(means[case_id]),
                "combined_arid_min": _fraction_payload(min(combined_values)),
                "combined_arid_max": _fraction_payload(max(combined_values)),
                "desert_mean": _fraction_payload(desert_means[case_id]),
            })
        if len(means) != 6:
            continue
        deltas = (
            (means["B"] - means["A"], Fraction(5, 100), "B-A>=5pp"),
            (means["C"] - means["A"], Fraction(7, 100), "C-A>=7pp"),
            (means["D"] - max(means["B"], means["C"]),
             Fraction(5, 100), "D-max(B,C)>=5pp"),
        )
        for value, minimum, label in deltas:
            if value < minimum:
                failures.append({
                    "type": "response_delta", "map_size": map_index,
                    "relationship": label, "value": _fraction_payload(value),
                    "minimum": _fraction_payload(minimum),
                })
        if not Fraction(8, 100) <= desert_means["D"] <= Fraction(25, 100):
            failures.append({
                "type": "configuration_d_desert_mean", "map_size": map_index,
                "value": _fraction_payload(desert_means["D"]),
                "low": _fraction_payload(Fraction(8, 100)),
                "high": _fraction_payload(Fraction(25, 100)),
            })
    return {
        "candidate_id": candidate["candidate_id"], "ok": not failures,
        "failure_count": len(failures), "failures": failures,
        "statistics": statistics,
        "ranking_metrics": {
            "total_center_distance": _fraction_payload(total_distance),
            "maximum_size_case_deviation": _fraction_payload(maximum_deviation),
        },
    }


def _candidate_rank(item: dict[str, Any]) -> tuple[Fraction, Fraction, str]:
    metrics = item["evaluation"]["ranking_metrics"]
    return (
        _payload_fraction(metrics["total_center_distance"]),
        _payload_fraction(metrics["maximum_size_case_deviation"]),
        item["candidate_id"],
    )


def _screen_candidates(
    carriers: dict[tuple[int, int, str, int], dict[str, str]], output: Path,
) -> tuple[list[dict[str, Any]], dict[str, list[dict[str, Any]]]]:
    items = []
    rows_by_candidate: dict[str, list[dict[str, Any]]] = {}
    projected_path = output / "projected_rows.csv"
    projected_path.parent.mkdir(parents=True, exist_ok=True)
    with projected_path.open("x", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle, fieldnames=PROJECTED_COLUMNS, lineterminator="\n",
        )
        writer.writeheader()
        for candidate in CANDIDATES:
            rows = []
            for seed in SCREEN_SEEDS:
                for map_index, _, _, _ in MAPS:
                    for case_id, moisture, drought, bias in CASES:
                        row = _projected_row(
                            candidate, seed, map_index, case_id, moisture,
                            drought, bias, carriers,
                        )
                        rows.append(row)
                        writer.writerow({field: row[field] for field in PROJECTED_COLUMNS})
            evaluation = _evaluate_climate_candidate(candidate, rows)
            item = {**candidate, "evaluation": evaluation}
            items.append(item)
            if evaluation["ok"]:
                rows_by_candidate[candidate["candidate_id"]] = rows
    survivors = sorted(
        (item for item in items if item["evaluation"]["ok"]),
        key=_candidate_rank,
    )
    summary_rows = []
    for item in items:
        metrics = item["evaluation"]["ranking_metrics"]
        summary_rows.append({
            **{name: item[name] for name in (
                "candidate_id", "arid_base", "desert_bias_span",
                "drought_divisor", "moisture_compression_span",
            )},
            "ok": item["evaluation"]["ok"],
            "failure_count": item["evaluation"]["failure_count"],
            "total_center_distance": metrics["total_center_distance"]["decimal"],
            "maximum_size_case_deviation":
                metrics["maximum_size_case_deviation"]["decimal"],
        })
    fields = tuple(summary_rows[0])
    _write_flat_csv(output / "climate_candidates.csv", summary_rows, fields)
    _write_json_new(output / "climate_candidates.json", {
        "candidate_count": len(items), "expected_candidate_count": 1920,
        "projected_row_count": 1920 * 48, "candidates": items,
    })
    ranking = []
    for rank, item in enumerate(survivors, 1):
        metrics = item["evaluation"]["ranking_metrics"]
        ranking.append({
            "rank": rank, "candidate_id": item["candidate_id"],
            "arid_base": item["arid_base"],
            "desert_bias_span": item["desert_bias_span"],
            "drought_divisor": item["drought_divisor"],
            "moisture_compression_span": item["moisture_compression_span"],
            "total_center_distance": metrics["total_center_distance"]["decimal"],
            "maximum_size_case_deviation":
                metrics["maximum_size_case_deviation"]["decimal"],
            "oasis_screen_eligible": rank <= 48,
        })
    ranking_fields = tuple(ranking[0]) if ranking else (
        "rank", "candidate_id", "arid_base", "desert_bias_span",
        "drought_divisor", "moisture_compression_span",
        "total_center_distance", "maximum_size_case_deviation",
        "oasis_screen_eligible",
    )
    _write_flat_csv(output / "climate_survivor_ranking.csv", ranking, ranking_fields)
    _write_json_new(output / "climate_survivors.json", {
        "survivor_count": len(survivors), "ranking": ranking,
        "projected_rows_path": str(projected_path.resolve()),
        "projected_rows_sha256": _sha256_file(projected_path),
    })
    if not survivors:
        raise ZeroClimateSurvivors("expanded climate grid has zero survivors")
    return survivors, rows_by_candidate


def _actual_key(row: dict[str, Any]) -> tuple[int, int, str]:
    return int(row["seed"]), int(row["map_size"]), str(row["case_id"])


def _actual_row_semantics(
    row: dict[str, str], candidate: dict[str, Any], pair_id: str,
) -> list[str]:
    reasons = []
    if row["candidate_id"] != candidate["candidate_id"]:
        reasons.append("candidate_id_mismatch")
    if row["pair_id"] != pair_id:
        reasons.append("pair_id_mismatch")
    for dimension in (
        "arid_base", "desert_bias_span", "drought_divisor",
        "moisture_compression_span",
    ):
        if int(row[dimension]) != int(candidate[dimension]):
            reasons.append(f"{dimension}_mismatch")
    _, drop, margin = next(pair for pair in OASIS_PAIRS if pair[0] == pair_id)
    if int(row["oasis_drop"]) != drop:
        reasons.append("oasis_drop_mismatch")
    if int(row["transition_margin"]) != margin:
        reasons.append("transition_margin_mismatch")
    if int(row["generated"]) != 1 or int(row["ok"]) != 1:
        reasons.append("generation_or_row_failure")
    if int(row["land_count"]) <= 0 or int(row["non_arid_count"]) <= 0:
        reasons.append("invalid_land_partition")
    if int(row["combined_arid_count"]) != (
        int(row["desert_count"]) + int(row["semi_arid_count"])
    ):
        reasons.append("combined_count_mismatch")
    if int(row["combined_arid_count"]) + int(row["non_arid_count"]) != int(row["land_count"]):
        reasons.append("land_partition_mismatch")
    if int(row["oasis_semantic_errors"]) != 0:
        reasons.append("oasis_semantic_errors")
    try:
        if int(row["physical_hash"], 16) == 0:
            reasons.append("zero_physical_hash")
        if int(row["climate_input_hash"], 16) == 0:
            reasons.append("zero_climate_input_hash")
    except ValueError:
        reasons.append("invalid_hash")
    if int(row["oasis_count"]) != int(row[OASIS_COLUMN_BY_PAIR[pair_id]]):
        reasons.append("active_pair_projection_mismatch")
    return reasons


def _compare_projected_actual(
    projected: Sequence[dict[str, Any]], actual: Sequence[dict[str, str]],
) -> dict[str, Any]:
    projected_lookup = {_actual_key(row): row for row in projected}
    actual_lookup = {_actual_key(row): row for row in actual}
    mismatches = []
    if set(projected_lookup) != set(actual_lookup):
        mismatches.append({
            "type": "key_set", "projected": sorted(projected_lookup),
            "actual": sorted(actual_lookup),
        })
    fields = (
        "climate_input_hash", "land_count", "combined_arid_limit",
        "semi_arid_band", "desert_limit", "semi_arid_limit", "desert_count",
        "semi_arid_count", "combined_arid_count", "non_arid_count",
    )
    for key in sorted(set(projected_lookup) & set(actual_lookup)):
        expected = projected_lookup[key]
        observed = actual_lookup[key]
        differences = {
            field: {"projected": str(expected[field]), "actual": observed[field]}
            for field in fields if str(expected[field]) != observed[field]
        }
        if differences:
            mismatches.append({"type": "row", "key": list(key), "differences": differences})
    return {
        "ok": not mismatches and len(projected) == len(actual),
        "projected_count": len(projected), "actual_count": len(actual),
        "mismatches": mismatches,
    }


def _evaluate_oasis_pair(
    rows: Sequence[dict[str, str]], pair_id: str, actual: bool,
) -> dict[str, Any]:
    column = "oasis_count" if actual else OASIS_COLUMN_BY_PAIR[pair_id]
    groups: dict[tuple[int, str], list[dict[str, str]]] = {}
    for row in rows:
        groups.setdefault((int(row["map_size"]), row["case_id"]), []).append(row)
    failures = []
    results = []
    max_land_deviation = Fraction(0, 1)
    max_channel_deviation = Fraction(0, 1)
    for map_index, map_name, _, _ in MAPS:
        for case_id in ("D", "E"):
            members = groups.get((map_index, case_id), [])
            reasons = []
            if len(members) != 2:
                reasons.append("sample_count")
                counts: list[int] = []
                land_mean = channel_mean = Fraction(0, 1)
            else:
                counts = [int(row[column]) for row in members]
                land_mean = legacy._mean([
                    Fraction(count, int(row["land_count"]))
                    for count, row in zip(counts, members)
                ])
                channel_values = []
                for count, row in zip(counts, members):
                    channels = int(row["river_channel_count"])
                    if channels <= 0:
                        reasons.append("zero_river_channels")
                    else:
                        channel_values.append(Fraction(count, channels))
                channel_mean = legacy._mean(channel_values) if len(channel_values) == 2 else Fraction(0, 1)
                if any(count <= 0 for count in counts):
                    reasons.append("individual_world_has_zero_oasis")
                if not Fraction(2, 1000) <= land_mean <= Fraction(2, 100):
                    reasons.append("two_seed_land_share_mean_out_of_range")
                if not Fraction(3, 100) <= channel_mean <= Fraction(25, 100):
                    reasons.append("two_seed_channel_share_mean_out_of_range")
            max_land_deviation = max(max_land_deviation, abs(land_mean - Fraction(1, 100)))
            max_channel_deviation = max(max_channel_deviation, abs(channel_mean - Fraction(14, 100)))
            entry = {
                "map_size": map_index, "map_name": map_name,
                "configuration": case_id, "counts": counts,
                "land_share_mean": _fraction_payload(land_mean),
                "channel_share_mean": _fraction_payload(channel_mean),
                "reasons": reasons, "ok": not reasons,
            }
            results.append(entry)
            if reasons:
                failures.append(entry)
    return {
        "pair_id": pair_id, "source": "actual" if actual else "projected",
        "ok": not failures, "groups": results, "failures": failures,
        "score": {
            "maximum_land_target_deviation": _fraction_payload(max_land_deviation),
            "maximum_channel_target_deviation": _fraction_payload(max_channel_deviation),
        },
    }


def _pair_rank(item: dict[str, Any]) -> tuple[Fraction, Fraction, str]:
    return (
        _payload_fraction(item["score"]["maximum_land_target_deviation"]),
        _payload_fraction(item["score"]["maximum_channel_target_deviation"]),
        item["pair_id"],
    )


def _candidate_from_item(item: dict[str, Any]) -> dict[str, Any]:
    return {
        name: item[name] for name in (
            "candidate_id", "arid_base", "desert_bias_span",
            "drought_divisor", "moisture_compression_span",
        )
    }


def _screen_oases(
    exe: Path, output: Path, budget: WorldBudget,
    survivors: Sequence[dict[str, Any]],
    rows_by_candidate: dict[str, list[dict[str, Any]]],
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    attempted = []
    passing = []
    for rank, item in enumerate(survivors[:48], 1):
        candidate = _candidate_from_item(item)
        run = _run_action(
            exe, output / "r" / f"{rank:03d}_{candidate['candidate_id']}",
            "oasis", budget, 16, candidate["candidate_id"], candidate,
            "o08_t00",
        )
        rows = run["rows"] or []
        projected_de = [
            row for row in rows_by_candidate[candidate["candidate_id"]]
            if row["case_id"] in ("D", "E")
        ]
        semantics = [
            {"key": list(_actual_key(row)), "reasons": _actual_row_semantics(
                row, candidate, "o08_t00",
            )}
            for row in rows
        ]
        identity = _compare_projected_actual(projected_de, rows)
        pair_results = [
            _evaluate_oasis_pair(rows, pair_id, actual=False)
            for pair_id, _, _ in OASIS_PAIRS
        ] if len(rows) == 16 else []
        passing_pairs = sorted(
            (pair for pair in pair_results if pair["ok"]), key=_pair_rank,
        )
        process_ok = (
            run["exit_code"] == 0 and run["parse_error"] is None and
            len(rows) == 16 and run["generated_count"] == 16
        )
        result = {
            "rank": rank, **candidate, "process_ok": process_ok,
            "row_semantics_ok": all(not row["reasons"] for row in semantics),
            "row_semantics": semantics, "projection_identity": identity,
            "pair_evaluations": pair_results,
            "passing_pair_count": len(passing_pairs),
            "selected_oasis_pair": passing_pairs[0] if passing_pairs else None,
            "ok": process_ok and all(not row["reasons"] for row in semantics) and
                  identity["ok"] and bool(passing_pairs),
            "csv_path": run["csv_path"], "csv_sha256": run["csv_sha256"],
            "elapsed_seconds": run["elapsed_seconds"],
        }
        attempted.append(result)
        if result["ok"]:
            passing.append(result)
        candidate_dir = output / "r" / f"{rank:03d}_{candidate['candidate_id']}"
        _write_json_new(candidate_dir / "oasis_screen_result.json", result)
        _write_json_new(output / f"progress_{rank:03d}.json", {
            "attempted_count": len(attempted), "passing_count": len(passing),
            "passing_ids": [entry["candidate_id"] for entry in passing],
        })
        if len(passing) == 3:
            break
    summary = {
        "attempted_count": len(attempted), "maximum_attempts": 48,
        "passing_count": len(passing), "attempted": attempted,
        "selected_candidate_ids": [item["candidate_id"] for item in passing],
    }
    _write_json_new(output / "oasis_screen_results.json", summary)
    rows = [{
        "rank": item["rank"], "candidate_id": item["candidate_id"],
        "process_ok": item["process_ok"],
        "projection_identity_ok": item["projection_identity"]["ok"],
        "passing_pair_count": item["passing_pair_count"],
        "selected_pair_id": (item["selected_oasis_pair"] or {}).get("pair_id", ""),
        "ok": item["ok"],
    } for item in attempted]
    _write_flat_csv(
        output / "oasis_screen_results.csv", rows,
        tuple(rows[0]) if rows else (
            "rank", "candidate_id", "process_ok", "projection_identity_ok",
            "passing_pair_count", "selected_pair_id", "ok",
        ),
    )
    if len(passing) < 3:
        raise InsufficientOasisCandidates(
            f"only {len(passing)} candidates passed after {len(attempted)} attempts"
        )
    return attempted, passing


def _audit_bmp(
    path: Path, width: int, height: int, pixel_hash: str,
) -> dict[str, Any]:
    return legacy._audit_bmp(path, width, height, pixel_hash)


def _audit_confirmation_artifacts(
    directory: Path, candidate_id: str, rows: Sequence[dict[str, str]],
) -> dict[str, Any]:
    manifest = directory / "aridity_response_projection_artifacts_manifest.csv"
    manifest_rows, _ = _load_csv_required(manifest, ARTIFACT_REQUIRED_COLUMNS)
    expected_keys = {
        (2026082301, map_index, case_id)
        for map_index in (0, 3) for case_id, _, _, _ in CASES
    }
    row_lookup = {_actual_key(row): row for row in rows}
    images = []
    failures = []
    observed = set()
    for item in manifest_rows:
        key = (int(item["seed"]), int(item["map_size"]), item["case_id"])
        observed.add(key)
        actual_row = row_lookup.get(key)
        if actual_row is None:
            failures.append({"type": "manifest_key", "key": list(key)})
            continue
        binding_fields = (
            "candidate_id", "pair_id", "physical_hash",
            "climate_input_hash", "oasis_count", "map_name", "width", "height",
        )
        differences = {
            field: {"manifest": item[field], "actual": actual_row[field]}
            for field in binding_fields if item[field] != actual_row[field]
        }
        if differences:
            failures.append({
                "type": "manifest_binding", "key": list(key),
                "differences": differences,
            })
        for mode in ("geography", "climate"):
            path = legacy._resolve_artifact(directory, item[f"{mode}_file"])
            audit = _audit_bmp(
                path, int(item["width"]), int(item["height"]),
                item[f"{mode}_pixel_hash"],
            )
            record = {
                "candidate_id": candidate_id, "seed": key[0],
                "map_size": key[1], "map_name": item["map_name"],
                "case_id": key[2], "mode": mode,
                "path": str(path.resolve()), "file_sha256": _sha256_file(path),
                **audit,
            }
            images.append(record)
            if not audit["ok"]:
                failures.append({"type": "image", "record": record})
    if observed != expected_keys:
        failures.append({
            "type": "manifest_keys", "expected": sorted(expected_keys),
            "actual": sorted(observed),
        })
    return {
        "ok": not failures and len(images) == 24,
        "manifest": str(manifest.resolve()),
        "manifest_sha256": _sha256_file(manifest),
        "image_count": len(images), "images": images, "failures": failures,
    }


def _native_contact_sheets(
    audit: dict[str, Any], output: Path,
) -> list[dict[str, Any]]:
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError as error:
        raise ProjectionError("Pillow is required for native contact sheets") from error
    output.mkdir(parents=True, exist_ok=True)
    results = []
    candidate_id = audit["images"][0]["candidate_id"]
    for map_index, map_name, width, height in (MAPS[0], MAPS[3]):
        for mode in ("climate", "geography"):
            lookup = {
                image["case_id"]: image for image in audit["images"]
                if image["map_size"] == map_index and image["mode"] == mode
            }
            if set(lookup) != {item[0] for item in CASES}:
                raise ProjectionError(
                    f"contact-sheet input mismatch {candidate_id}/{map_name}/{mode}"
                )
            gap = 8
            label_height = 24
            sheet = Image.new(
                "RGB", (gap + 6 * (width + gap), gap + label_height + height + gap),
                (28, 30, 34),
            )
            draw = ImageDraw.Draw(sheet)
            font = ImageFont.load_default()
            for column, (case_id, _, _, _) in enumerate(CASES):
                x = gap + column * (width + gap)
                draw.text((x, gap), f"{case_id} {mode}", fill=(235, 238, 242), font=font)
                with Image.open(lookup[case_id]["path"]) as source:
                    source.load()
                    if source.size != (width, height):
                        raise ProjectionError("contact sheet would require rescaling")
                    sheet.paste(source.convert("RGB"), (x, gap + label_height))
            path = output / f"{candidate_id}_{map_name.lower()}_{mode}_native.png"
            if path.exists():
                raise ProjectionError(f"refusing to overwrite contact sheet {path}")
            sheet.save(path, format="PNG")
            results.append({
                "candidate_id": candidate_id, "map_size": map_index,
                "map_name": map_name, "mode": mode,
                "path": str(path.resolve()), "sha256": _sha256_file(path),
                "width": sheet.width, "height": sheet.height,
                "source_scale": "1:1_no_resampling",
            })
    return results


def _confirm_candidates(
    exe: Path, output: Path, budget: WorldBudget,
    passing: Sequence[dict[str, Any]],
    rows_by_candidate: dict[str, list[dict[str, Any]]],
) -> list[dict[str, Any]]:
    confirmations = []
    for index, selected in enumerate(passing, 1):
        candidate = _candidate_from_item(selected)
        pair_id = selected["selected_oasis_pair"]["pair_id"]
        directory = output / f"c{index}_{candidate['candidate_id']}"
        run = _run_action(
            exe, directory, "confirm", budget, 48, candidate["candidate_id"],
            candidate, pair_id, emit_artifacts=True,
        )
        rows = run["rows"] or []
        semantics = [
            {"key": list(_actual_key(row)), "reasons": _actual_row_semantics(
                row, candidate, pair_id,
            )}
            for row in rows
        ]
        identity = _compare_projected_actual(
            rows_by_candidate[candidate["candidate_id"]], rows,
        )
        actual_projection_rows = [{
            **row, "case_id": row["case_id"],
        } for row in rows]
        climate = _evaluate_climate_candidate(candidate, actual_projection_rows)
        oasis = _evaluate_oasis_pair(rows, pair_id, actual=True)
        artifacts = _audit_confirmation_artifacts(
            directory, candidate["candidate_id"], rows,
        ) if len(rows) == 48 else {"ok": False, "images": [], "failures": [
            {"type": "row_count", "actual": len(rows), "expected": 48},
        ]}
        sheets = _native_contact_sheets(
            artifacts, directory / "contact_sheets",
        ) if artifacts["ok"] else []
        result = {
            "confirmation_index": index, **candidate, "pair_id": pair_id,
            "process_ok": run["exit_code"] == 0 and run["parse_error"] is None and
                          len(rows) == 48 and run["generated_count"] == 48,
            "row_semantics_ok": all(not item["reasons"] for item in semantics),
            "row_semantics": semantics, "projection_identity": identity,
            "climate_evaluation": climate, "actual_oasis_evaluation": oasis,
            "artifact_audit": artifacts, "contact_sheets": sheets,
            "ok": (
                run["exit_code"] == 0 and run["parse_error"] is None and
                len(rows) == 48 and run["generated_count"] == 48 and
                all(not item["reasons"] for item in semantics) and identity["ok"] and
                climate["ok"] and oasis["ok"] and artifacts["ok"] and len(sheets) == 4
            ),
            "csv_path": run["csv_path"], "csv_sha256": run["csv_sha256"],
        }
        _write_json_new(directory / "confirmation_result.json", result)
        confirmations.append(result)
    summary = {
        "candidate_count": len(confirmations),
        "passing_count": sum(item["ok"] for item in confirmations),
        "native_artifact_count": sum(
            item["artifact_audit"].get("image_count", 0) for item in confirmations
        ),
        "contact_sheet_count": sum(len(item["contact_sheets"]) for item in confirmations),
        "confirmations": confirmations,
    }
    _write_json_new(output / "confirmation_results.json", summary)
    if len(confirmations) != 3 or not all(item["ok"] for item in confirmations):
        raise ProjectionError("full actual confirmation failed")
    return confirmations


def _audit_freeze(repo: Path, freeze_record: Path) -> dict[str, Any]:
    return legacy._audit_freeze(repo, freeze_record)


def _run_all(args: argparse.Namespace, output: Path) -> dict[str, Any]:
    started = time.perf_counter()
    exe = args.exe.resolve()
    if not exe.is_file():
        raise ProjectionError(f"executable does not exist: {exe}")
    repo = exe.parent
    budget = WorldBudget(output / "world_budget_events.jsonl")
    initial_freeze = _audit_freeze(repo, args.freeze_record.resolve())
    _write_json_new(output / "01_freeze_pre" / "freeze_result.json", initial_freeze)
    if not initial_freeze["ok"]:
        raise ProjectionError("accepted 22-file freeze failed before pilot")
    if args.resume_from:
        resume = args.resume_from.resolve()
        budget.inherit(resume / "world_budget_events.jsonl")
        formula = _reuse_formula(resume, output / "02_formula")
        baseline = _reuse_baseline(
            resume, output / "03_baseline", args.accepted_matrix.resolve(),
        )
        carriers, carrier_result = _reuse_carriers(
            resume, output / "04_carriers",
        )
    else:
        formula = _run_formula(exe, output / "02_formula", budget)
        baseline = _run_baseline(
            exe, output / "03_baseline", args.accepted_matrix.resolve(), budget,
        )
        carriers, carrier_result = _run_carriers(
            exe, output / "04_carriers", budget,
        )
    survivors, rows_by_candidate = _screen_candidates(
        carriers, output / "05_projection",
    )
    attempted, passing = _screen_oases(
        exe, output / "06_oasis", budget, survivors, rows_by_candidate,
    )
    confirmations = _confirm_candidates(
        exe, output / "07_confirmation", budget, passing, rows_by_candidate,
    )
    final_freeze = _audit_freeze(repo, args.freeze_record.resolve())
    _write_json_new(output / "08_freeze_post" / "freeze_result.json", final_freeze)
    expected_worlds = 24 + 200 + 16 * len(attempted) + 3 * 48
    count_ok = (
        budget.reserved == expected_worlds and budget.rows == expected_worlds and
        budget.generated == expected_worlds and expected_worlds <= MAX_WORLDS
    )
    result = {
        "ok": (
            formula["ok"] and baseline["ok"] and carrier_result["ok"] and
            len(passing) == 3 and len(confirmations) == 3 and
            all(item["ok"] for item in confirmations) and final_freeze["ok"] and
            count_ok
        ),
        "status": "EXPANDED_PROJECTION_PILOT_PASS",
        "exe": str(exe), "exe_size": exe.stat().st_size,
        "exe_sha256": _sha256_file(exe),
        "candidate_count": len(CANDIDATES),
        "climate_survivor_count": len(survivors),
        "oasis_attempt_count": len(attempted),
        "passing_candidate_ids": [item["candidate_id"] for item in passing],
        "confirmation_count": len(confirmations),
        "expected_world_count": expected_worlds,
        "reserved_world_count": budget.reserved,
        "emitted_row_count": budget.rows,
        "generated_world_count": budget.generated,
        "maximum_world_count": MAX_WORLDS,
        "world_count_ok": count_ok,
        "initial_freeze_ok": initial_freeze["ok"],
        "final_freeze_ok": final_freeze["ok"],
        "resumed_from": str(args.resume_from.resolve()) if args.resume_from else None,
        "runtime_seconds": time.perf_counter() - started,
    }
    _write_json_new(output / "projection_pilot_result.json", result)
    if not result["ok"]:
        raise ProjectionError("expanded projection evidence incomplete")
    return result


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", choices=("all",), required=True)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--accepted-matrix", type=Path, required=True)
    parser.add_argument("--freeze-record", type=Path, required=True)
    parser.add_argument("--resume-from", type=Path)
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
            "world_count": result["generated_world_count"],
        })
        _artifact_manifest(output, output / "artifact_manifest.json")
        return 0
    except ZeroClimateSurvivors as error:
        failure = {
            "stage": args.stage, "ok": False,
            "status": "BLOCKED_ZERO_CLIMATE_SURVIVORS",
            "exception_type": type(error).__name__, "message": str(error),
        }
    except InsufficientOasisCandidates as error:
        failure = {
            "stage": args.stage, "ok": False,
            "status": "BLOCKED_FEWER_THAN_THREE_OASIS_CANDIDATES",
            "exception_type": type(error).__name__, "message": str(error),
        }
    except Exception as error:
        failure = {
            "stage": args.stage, "ok": False, "status": "FAILED",
            "exception_type": type(error).__name__, "message": str(error),
        }
    try:
        post_path = output / "08_freeze_post" / "freeze_result.json"
        if not post_path.exists():
            post_freeze = _audit_freeze(
                args.exe.resolve().parent, args.freeze_record.resolve(),
            )
            _write_json_new(post_path, post_freeze)
            failure["post_failure_freeze"] = {
                "ok": post_freeze["ok"], "path": str(post_path.resolve()),
                "sha256": _sha256_file(post_path),
            }
    except Exception as freeze_error:
        failure["post_failure_freeze"] = {
            "ok": False, "exception_type": type(freeze_error).__name__,
            "message": str(freeze_error),
        }
    try:
        _write_json_new(output / "failure.json", failure)
        _write_json_new(output / "stage_result.json", failure)
        _artifact_manifest(output, output / "artifact_manifest.json")
    except Exception as evidence_error:
        print(f"failed to preserve failure evidence: {evidence_error}", file=sys.stderr)
    print(f"{failure['exception_type']}: {failure['message']}", file=sys.stderr)
    return 3 if failure["status"].startswith("BLOCKED_") else 1


if __name__ == "__main__":
    raise SystemExit(main())
