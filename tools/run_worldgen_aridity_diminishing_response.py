#!/usr/bin/env python3
"""Run the bounded Ver0.3.7.a diminishing aridity-response validation."""

from __future__ import annotations

import argparse
import csv
import hashlib
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

import run_worldgen_aridity_response_projection as projection


ACK = "VER037A_ARIDITY_DIMINISHING_RESPONSE"
COMMAND = "--probe-worldgen-aridity-diminishing-response"
ACK_ENV = "WORLD_SIM_ARIDITY_CALIBRATION_ACK"
ACTION_ENV = "WORLD_SIM_ARIDITY_DIMINISHING_ACTION"
OUTPUT_ENV = "WORLD_SIM_ARIDITY_PROBE_DIR"
ARTIFACT_ENV = "WORLD_SIM_ARIDITY_DIMINISHING_EMIT_ARTIFACTS"
PARAM_ENV = {
    "arid_base": "WORLD_SIM_ARIDITY_DIMINISHING_ARID_BASE",
    "desert_bias_span":
        "WORLD_SIM_ARIDITY_DIMINISHING_DESERT_BIAS_SPAN",
    "drought_divisor": "WORLD_SIM_ARIDITY_DIMINISHING_DROUGHT_DIVISOR",
    "moisture_compression_span":
        "WORLD_SIM_ARIDITY_DIMINISHING_MOISTURE_COMPRESSION_SPAN",
    "drought_classification_span":
        "WORLD_SIM_ARIDITY_DIMINISHING_DROUGHT_CLASSIFICATION_SPAN",
    "oasis_drop": "WORLD_SIM_ARIDITY_DIMINISHING_OASIS_DROP",
    "transition_margin":
        "WORLD_SIM_ARIDITY_DIMINISHING_TRANSITION_MARGIN",
}
FIXED_CANDIDATE = {
    "candidate_id": "a31_b04_d24_c12_r02",
    "arid_base": 31,
    "desert_bias_span": 4,
    "drought_divisor": 24,
    "moisture_compression_span": 12,
    "drought_classification_span": 2,
}
OASIS_DROPS = (0, 4, 8, 12, 16, 20)
OASIS_TRANSITION_MARGINS = (0, 5, 10, 15, 20, 25)
OASIS_PAIRS = tuple(
    (f"o{drop:02d}_t{margin:02d}", drop, margin)
    for drop in OASIS_DROPS for margin in OASIS_TRANSITION_MARGINS
)
OASIS_COLUMN_BY_PAIR = {
    pair_id: f"projected_oasis_{pair_id}"
    for pair_id, _, _ in OASIS_PAIRS
}
OASIS_GRID_SCHEMA = ",".join(pair_id for pair_id, _, _ in OASIS_PAIRS)
OASIS_GRID_SHA256 = hashlib.sha256(
    OASIS_GRID_SCHEMA.encode("ascii"),
).hexdigest().upper()
INTEGER_OASIS_DROPS = tuple(range(21))
INTEGER_OASIS_TRANSITION_MARGINS = tuple(range(26))
INTEGER_OASIS_PAIRS = tuple(
    (f"o{drop:02d}_t{margin:02d}", drop, margin)
    for drop in INTEGER_OASIS_DROPS
    for margin in INTEGER_OASIS_TRANSITION_MARGINS
)
INTEGER_OASIS_PAIR_BY_ID = {
    pair_id: (drop, margin)
    for pair_id, drop, margin in INTEGER_OASIS_PAIRS
}
INTEGER_OASIS_COLUMN_BY_PAIR = {
    pair_id: f"projected_oasis_{pair_id}"
    for pair_id, _, _ in INTEGER_OASIS_PAIRS
}
INTEGER_OASIS_GRID_SCHEMA = ",".join(
    pair_id for pair_id, _, _ in INTEGER_OASIS_PAIRS
)
INTEGER_OASIS_GRID_SHA256 = hashlib.sha256(
    INTEGER_OASIS_GRID_SCHEMA.encode("ascii"),
).hexdigest().upper()
OASIS_HISTOGRAM_COLUMNS = tuple(
    f"hist_{moisture:03d}" for moisture in range(101)
)
OASIS_PREFIX_COLUMNS = tuple(
    f"prefix_{moisture:03d}" for moisture in range(101)
)
OASIS_HISTOGRAM_REQUIRED_COLUMNS = (
    "row", "mode", "candidate_id", "pair_id", "seed", "map_size",
    "map_name", "case_id", "drought", "combined_arid_limit",
    "desert_limit", "semi_arid_limit", "land_count",
    "river_channel_count", "eligible_count", "histogram_total",
    "prefix_total", "oracle_pair_cases", "oracle_bin_checks",
    "oracle_mismatches", "legacy_36_cases", "legacy_36_mismatches",
    "active_projected_oasis_count",
    "projected_pre_desert_count", "projected_pre_semi_arid_count",
    "projected_pre_combined_arid_count", "projected_pre_non_arid_count",
    "projection_pre_identity_ok", "histogram_ok",
    *OASIS_HISTOGRAM_COLUMNS, *OASIS_PREFIX_COLUMNS,
    *(INTEGER_OASIS_COLUMN_BY_PAIR[pair_id]
      for pair_id, _, _ in INTEGER_OASIS_PAIRS),
)
OASIS_HISTOGRAM_BINDING_FIELDS = tuple(
    field for field in OASIS_HISTOGRAM_REQUIRED_COLUMNS
    if field not in ("row", "mode")
)
CLIMATE_COUNT = 14
PRE_CLIMATE_COLUMNS = tuple(
    f"pre_climate_{climate:02d}" for climate in range(CLIMATE_COUNT)
)
POST_CLIMATE_COLUMNS = tuple(
    f"post_climate_{climate:02d}" for climate in range(CLIMATE_COUNT)
)
NONREFRESH_TRANSITION_COLUMNS = tuple(
    f"nonrefresh_transition_{source:02d}_{target:02d}"
    for source in range(CLIMATE_COUNT) for target in range(CLIMATE_COUNT)
)
REFRESH_TRANSITION_COLUMNS = tuple(
    f"refresh_transition_{source:02d}_{target:02d}"
    for source in range(CLIMATE_COUNT) for target in range(CLIMATE_COUNT)
)
CLIMATE_ACCOUNTING_COLUMNS = (
    *PRE_CLIMATE_COLUMNS, *POST_CLIMATE_COLUMNS,
    "refresh_eligible_count", "refresh_changed_count",
    "non_refresh_changed_count", "transition_land_count",
    "accounting_unexplained_count", "climate_accounting_ok",
    *NONREFRESH_TRANSITION_COLUMNS, *REFRESH_TRANSITION_COLUMNS,
)
MAPS = projection.MAPS
CASES = projection.CASES
CASE_MEAN_RANGES = projection.CASE_MEAN_RANGES
CASE_CENTERS = projection.CASE_CENTERS
PER_WORLD_CEILINGS = projection.PER_WORLD_CEILINGS
BASELINE_COLUMNS = projection.BASELINE_COLUMNS
SCREEN_SEEDS = (2026082301, 2026082302)
HOLDOUT_SEEDS = (2026082401, 2026082402)
BASELINE_SEED = 2026072301
MAX_WORLDS = 264
ADJUDICATION_STATUS = "PASS_EXACT_EVIDENCE_ARCHITECT_USER_ADJUDICATION"
HISTORICAL_HOLDOUT_STATUS = "FAIL_PRESERVED"
SUPERSEDED_RULE = "independent_two_seed_climate_mean_range_only"
REPLACEMENT_RULE = "four_seed_equal_weight_climate_mean_range"
SELECTED_PAIR_ID = "o20_t02"
FOUR_SEED_SET = (*SCREEN_SEEDS, *HOLDOUT_SEEDS)
EXPECTED_BASELINE_SHA256 = (
    "28C6756A2AF053E4B106A5E4762022666D03B4C5E44C98E802515DF655362C0C"
)
EXPECTED_PREPUBLICATION_FREEZE_RECORD_SHA256 = (
    "D1DF6F152633F2425B4701692BA159D6447B57342356AF86644D2472ABF6B336"
)
EXPECTED_PRODUCTION_FREEZE_RECORD_SHA256 = (
    "6E2ED0825C8DE407E11E4DD1F9A9B855D5060760A7C19AC3F41B29CEC363FF77"
)
PRODUCTION_FREEZE_REMOVED_PATH = (
    "src/game/game_worldgen_landform_semantics_probe.c"
)
PRODUCTION_FREEZE_REMOVED_SHA256 = (
    "C8FDF6127BC8C18A735001CC8B596B7AD98C3BA3FDAC3B0EDBB489D6B2919E87"
)
PRODUCTION_FREEZE_REMOVAL_REASON = (
    "authorized final oasis semantics fixture correction"
)
EXPECTED_SCREEN_MEANS = {
    "Small": ("1.268853", "18.286133", "18.277995", "38.313124",
              "52.899170", "11.454264"),
    "Medium": ("1.580946", "20.208116", "20.201823", "42.374132",
               "58.343316", "11.487847"),
    "Large": ("2.323706", "18.341592", "18.337824", "37.801559",
              "54.043391", "10.962969"),
    "Extreme": ("2.055952", "16.471439", "16.466014", "35.634867",
                "52.028317", "10.236782"),
}
PREPUBLICATION_RUNS = (
    ("formula", "diminishing_formula_ack_reset", 0, 0, 0, "zero"),
    *(
        ("formula", f"diminishing_ack_reject_{name}", 0, 0, 0, "nonzero")
        for name in (
            "unset", "empty", "wrong_case", "prefix", "suffix",
            "old_ack", "arbitrary",
        )
    ),
    ("baseline", "fresh_production_no_ack", 24, 24, 24, "zero"),
    ("carriers", "fresh_fixed_divisor_carriers", 32, 32, 32, "zero"),
    ("oasis", "fixed_candidate_all_oasis_pairs", 16, 16, 16, "zero"),
    ("confirm", "fixed_candidate_confirm", 48, 48, 48, "zero"),
    ("holdout", "fixed_candidate_holdout", 48, 48, 48, "zero"),
)
HISTOGRAM_COLUMNS = tuple(f"hist_{value:03d}" for value in range(101))
PREFIX_COLUMNS = tuple(f"prefix_{value:03d}" for value in range(101))
CARRIER_REQUIRED_COLUMNS = (
    "row", "carrier_role", "seed", "map_size", "map_name", "width",
    "height", "moisture", "drought", "bias_desert", "drought_divisor",
    "generated", "failure_stage", "failure_reason", "physical_hash",
    "climate_input_hash", "land_count", "arid_eligible_count",
    "active_state_ok", "restored_state_ok", "ok", *HISTOGRAM_COLUMNS,
    *PREFIX_COLUMNS,
)
ACTUAL_REQUIRED_COLUMNS = (
    "row", "mode", "candidate_id", "pair_id", "seed", "map_size",
    "map_name", "width", "height", "case_id", "moisture", "drought",
    "bias_desert", "arid_base", "desert_bias_span", "drought_divisor",
    "moisture_compression_span", "drought_classification_span",
    "dryness_response", "oasis_drop", "transition_margin",
    "combined_arid_limit", "semi_arid_band", "desert_limit",
    "semi_arid_limit", "oasis_limit", "oasis_transition_limit",
    "generated", "failure_stage", "failure_reason", "physical_hash",
    "climate_input_hash", "land_count", "desert_count",
    "semi_arid_count", "combined_arid_count", "non_arid_count",
    "oasis_count", "wetland_count", "river_channel_count",
    "oasis_semantic_errors", "active_state_ok", "restored_state_ok", "ok",
    *CLIMATE_ACCOUNTING_COLUMNS,
    *(OASIS_COLUMN_BY_PAIR[pair_id] for pair_id, _, _ in OASIS_PAIRS),
)
PROJECTED_COLUMNS = (
    "candidate_id", "arid_base", "desert_bias_span", "drought_divisor",
    "moisture_compression_span", "drought_classification_span", "seed",
    "map_size", "map_name", "width", "height", "case_id", "moisture",
    "drought", "bias_desert", "carrier_role", "climate_input_hash",
    "physical_hash", "land_count", "arid_eligible_count",
    "dryness_response", "combined_arid_limit", "semi_arid_band",
    "desert_limit", "semi_arid_limit", "desert_count", "semi_arid_count",
    "combined_arid_count", "non_arid_count", "desert_share",
    "semi_arid_share", "combined_arid_share", "non_arid_share",
)
ARTIFACT_REQUIRED_COLUMNS = (
    "candidate_id", "pair_id", "seed", "map_size", "map_name", "case_id",
    "width", "height", "physical_hash", "climate_input_hash",
    "oasis_count", "geography_file", "geography_pixel_hash",
    "climate_file", "climate_pixel_hash",
)
BINDING_FIELDS = (
    "seed", "map_size", "map_name", "width", "height", "case_id",
    "moisture", "drought", "bias_desert", "arid_base",
    "desert_bias_span", "drought_divisor", "moisture_compression_span",
    "drought_classification_span", "dryness_response", "oasis_drop",
    "transition_margin", "combined_arid_limit", "semi_arid_band",
    "desert_limit", "semi_arid_limit", "oasis_limit",
    "oasis_transition_limit", "physical_hash", "climate_input_hash",
    "land_count", "desert_count", "semi_arid_count",
    "combined_arid_count", "non_arid_count", "oasis_count",
    "wetland_count", "river_channel_count", "oasis_semantic_errors",
    "generated", "failure_stage", "failure_reason", "active_state_ok",
    "restored_state_ok", "ok",
    *CLIMATE_ACCOUNTING_COLUMNS,
    *(OASIS_COLUMN_BY_PAIR[pair_id] for pair_id, _, _ in OASIS_PAIRS),
)


class DiminishingError(RuntimeError):
    pass


def _sha256_file(path: Path) -> str:
    return projection._sha256_file(path)


def _write_json_new(path: Path, value: Any) -> None:
    projection._write_json_new(path, value)


def _write_text_new(path: Path, text: str) -> None:
    projection._write_text_new(path, text)


def _write_bytes_new(path: Path, data: bytes) -> None:
    projection._write_bytes_new(path, data)


def _fraction_payload(value: Fraction) -> dict[str, Any]:
    return projection._fraction_payload(value)


def _payload_fraction(value: dict[str, Any]) -> Fraction:
    return projection._payload_fraction(value)


def _json_line_bytes(value: Any) -> bytes:
    return projection._json_line_bytes(value)


def _artifact_manifest(root: Path, output: Path) -> None:
    projection._artifact_manifest(root, output)


def _write_flat_csv(
    path: Path, rows: Iterable[dict[str, Any]], fields: Sequence[str],
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        for row in rows:
            writer.writerow({field: row.get(field, "") for field in fields})


def _load_csv_required(
    path: Path, required: Sequence[str],
) -> tuple[list[dict[str, str]], tuple[str, ...]]:
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        fields = tuple(reader.fieldnames or ())
        missing = [field for field in required if field not in fields]
        if missing:
            raise DiminishingError(f"missing CSV columns in {path}: {missing}")
        return list(reader), fields


def _scrubbed_environment(settings: dict[str, str]) -> dict[str, str]:
    child = {
        key: value for key, value in os.environ.items()
        if not key.upper().startswith("WORLD_SIM_ARIDITY_")
    }
    child["PYTHONDONTWRITEBYTECODE"] = "1"
    child.update(settings)
    return child


class WorldBudget:
    """Append-only exact budget for the six world-generating actions."""

    def __init__(self, ledger: Path, resume_marker: Path | None = None) -> None:
        self.ledger = ledger.resolve()
        self.reserved = 0
        self.rows = 0
        self.generated = 0
        self.launches = 0
        self.events = 0
        ledger.parent.mkdir(parents=True, exist_ok=True)
        if resume_marker is None:
            with ledger.open("xb"):
                pass
        else:
            self._resume(resume_marker.resolve())

    def _append(self, value: dict[str, Any]) -> None:
        value = {"event_sequence": self.events, **value}
        with self.ledger.open("ab") as handle:
            handle.write(_json_line_bytes(value))
        self.events += 1

    def _resume(self, marker_path: Path) -> None:
        if not self.ledger.is_file() or not marker_path.is_file():
            raise DiminishingError("production binding requires ledger and marker")
        marker = json.loads(marker_path.read_text(encoding="utf-8"))
        if marker.get("status") != ADJUDICATION_STATUS:
            raise DiminishingError("four-seed adjudication marker is not PASS")
        failure_path = _load_bound_file(
            marker.get("historical_failure", ""),
            marker.get("historical_failure_sha256", ""),
            "historical failure",
        )
        failure = json.loads(failure_path.read_text(encoding="utf-8"))
        expected_hash = failure.get("budget_ledger_sha256")
        source_ledger = Path(failure.get("budget_ledger", "")).resolve()
        if not (
            failure.get("status") == "FAILED_STOP" and
            failure.get("stage") == "prepublication" and
            failure.get("message") == "holdout actual matrix failed" and
            source_ledger.is_file() and
            _sha256_file(source_ledger) == expected_hash and
            self.ledger != source_ledger
        ):
            raise DiminishingError("historical failure identity mismatch")
        if _sha256_file(self.ledger) != expected_hash:
            raise DiminishingError("copied prepublication ledger hash mismatch")
        events = [
            json.loads(line) for line in self.ledger.read_text(
                encoding="utf-8",
            ).splitlines() if line.strip()
        ]
        if len(events) != 2 * len(PREPUBLICATION_RUNS):
            raise DiminishingError("prepublication ledger event count mismatch")
        reserved = emitted = generated = 0
        for launch_index, specification in enumerate(PREPUBLICATION_RUNS):
            action, identifier, planned, expected_rows, expected_generated, \
                exit_contract = specification
            reservation = events[2 * launch_index]
            result = events[2 * launch_index + 1]
            reservation_ok = (
                reservation.get("event_sequence") == 2 * launch_index and
                reservation.get("event") == "reserved" and
                reservation.get("stage") == action and
                reservation.get("identifier") == identifier and
                reservation.get("planned_worlds") == planned and
                reservation.get("before") == reserved and
                reservation.get("after") == reserved + planned and
                reservation.get("maximum") == MAX_WORLDS
            )
            reserved += planned
            exit_code = result.get("exit_code")
            exit_ok = exit_code == 0 if exit_contract == "zero" else \
                isinstance(exit_code, int) and exit_code != 0
            emitted += expected_rows
            generated += expected_generated
            result_ok = (
                result.get("event_sequence") == 2 * launch_index + 1 and
                result.get("event") == "result" and
                result.get("stage") == action and
                result.get("identifier") == identifier and exit_ok and
                result.get("row_count") == expected_rows and
                result.get("generated_count") == expected_generated and
                result.get("reserved_total") == reserved and
                result.get("emitted_row_total") == emitted and
                result.get("generated_world_total") == generated and
                result.get("launch_count") == launch_index + 1
            )
            if not reservation_ok or not result_ok:
                raise DiminishingError(
                    "prepublication ledger sequence mismatch at "
                    f"{action}/{identifier}"
                )
        if (reserved, emitted, generated) != (168, 168, 168):
            raise DiminishingError("prepublication ledger totals mismatch")
        self.reserved = self.rows = self.generated = 168
        self.launches = 13
        self.events = len(events)
        self._append({
            "event": "production_resume", "marker": str(marker_path),
            "marker_sha256": _sha256_file(marker_path),
            "historical_failure": str(failure_path),
            "historical_failure_sha256": _sha256_file(failure_path),
            "ledger_prefix_sha256": expected_hash,
            "reserved_total": self.reserved,
            "emitted_row_total": self.rows,
            "generated_world_total": self.generated,
            "launch_count": self.launches,
        })

    def reserve(self, stage: str, identifier: str, planned: int) -> None:
        if planned < 0 or self.reserved + planned > MAX_WORLDS:
            raise DiminishingError(
                f"world budget would exceed {MAX_WORLDS}: "
                f"{self.reserved}+{planned} for {stage}/{identifier}"
            )
        before = self.reserved
        self.reserved += planned
        self._append({
            "event": "reserved", "stage": stage, "identifier": identifier,
            "planned_worlds": planned, "before": before,
            "after": self.reserved, "maximum": MAX_WORLDS,
        })

    def record(
        self, stage: str, identifier: str, row_count: int,
        generated_count: int, exit_code: int,
    ) -> None:
        self.rows += row_count
        self.generated += generated_count
        self.launches += 1
        self._append({
            "event": "result", "stage": stage, "identifier": identifier,
            "row_count": row_count, "generated_count": generated_count,
            "exit_code": exit_code, "reserved_total": self.reserved,
            "emitted_row_total": self.rows,
            "generated_world_total": self.generated,
            "launch_count": self.launches,
        })

    def record_error(
        self, stage: str, identifier: str, error: BaseException,
    ) -> None:
        self.launches += 1
        self._append({
            "event": "error", "stage": stage, "identifier": identifier,
            "exception_type": type(error).__name__, "message": str(error),
            "reserved_total": self.reserved, "emitted_row_total": self.rows,
            "generated_world_total": self.generated,
            "launch_count": self.launches,
        })


def _candidate_environment(include_pair: str | None = None) -> dict[str, str]:
    result = {
        PARAM_ENV[name]: str(FIXED_CANDIDATE[name])
        for name in (
            "arid_base", "desert_bias_span", "drought_divisor",
            "moisture_compression_span", "drought_classification_span",
        )
    }
    if include_pair:
        try:
            drop, margin = INTEGER_OASIS_PAIR_BY_ID[include_pair]
        except KeyError as error:
            raise DiminishingError(f"unknown oasis pair {include_pair}") from error
        result[PARAM_ENV["oasis_drop"]] = str(drop)
        result[PARAM_ENV["transition_margin"]] = str(margin)
    return result


def _output_csv_for_action(directory: Path, action: str) -> Path | None:
    if action == "baseline":
        return directory / "aridity_diminishing_actual.csv"
    if action == "carriers":
        return directory / "aridity_diminishing_carriers.csv"
    if action in ("oasis", "confirm", "holdout", "production"):
        return directory / "aridity_diminishing_actual.csv"
    return None


def _oasis_histogram_csv_for_action(
    directory: Path, action: str,
) -> Path | None:
    if action in ("oasis", "confirm", "holdout", "production"):
        return directory / "aridity_diminishing_oasis_histogram.csv"
    return None


def _run_action(
    exe: Path, directory: Path, action: str, budget: WorldBudget,
    planned_worlds: int, identifier: str, *, pair_id: str | None = None,
    emit_artifacts: bool = False, ack_value: str | None = ACK,
    include_candidate: bool = True,
) -> dict[str, Any]:
    if directory.exists():
        raise DiminishingError(f"refusing to overwrite run directory {directory}")
    directory.mkdir(parents=True)
    budget.reserve(action, identifier, planned_worlds)
    settings = {OUTPUT_ENV: str(directory.resolve()), ACTION_ENV: action}
    if ack_value is not None:
        settings[ACK_ENV] = ack_value
    if include_candidate:
        settings.update(_candidate_environment(pair_id))
    if emit_artifacts and action != "production":
        settings[ARTIFACT_ENV] = "1"
    argv = [str(exe.resolve()), COMMAND]
    _write_json_new(directory / "command.json", {
        "argv": argv, "cwd": str(exe.resolve().parent),
        "exe_size": exe.stat().st_size, "exe_sha256": _sha256_file(exe),
        "environment": settings, "scrubbed_prefix": "WORLD_SIM_ARIDITY_",
        "planned_worlds": planned_worlds,
    })
    started = time.perf_counter()
    try:
        completed = subprocess.run(
            argv, cwd=exe.resolve().parent,
            env=_scrubbed_environment(settings), stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, check=False,
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
    csv_path = _output_csv_for_action(directory, action)
    histogram_path = _oasis_histogram_csv_for_action(directory, action)
    summary_path = directory / "aridity_diminishing_summary.txt"
    rows: list[dict[str, str]] = []
    fields: tuple[str, ...] = ()
    histogram_rows: list[dict[str, str]] = []
    histogram_fields: tuple[str, ...] = ()
    parse_error = None
    try:
        if csv_path is not None:
            required = (BASELINE_COLUMNS if action == "baseline" else
                        CARRIER_REQUIRED_COLUMNS if action == "carriers" else
                        ACTUAL_REQUIRED_COLUMNS)
            rows, fields = _load_csv_required(csv_path, required)
        if histogram_path is not None:
            histogram_rows, histogram_fields = _load_csv_required(
                histogram_path, OASIS_HISTOGRAM_REQUIRED_COLUMNS,
            )
    except Exception as error:
        parse_error = f"{type(error).__name__}: {error}"
    generated = sum(int(row.get("generated", "0")) for row in rows)
    summary_text = (
        summary_path.read_text(encoding="utf-8", errors="replace")
        if summary_path.is_file() else ""
    )
    summary_tokens: dict[str, bool] = {}
    summary_ok: bool | None = None
    if action != "formula":
        summary_patterns = {
            "action": rf"\baction={re.escape(action)}\b",
            "worlds_generated": rf"\bworlds_generated={planned_worlds}\b",
            "expected_worlds": rf"\bexpected_worlds={planned_worlds}\b",
            "generation_failures": r"\bgeneration_failures=0\b",
            "hash_failures": r"\bhash_failures=0\b",
            "capture_failures": r"\bcapture_failures=0\b",
            "semantic_failures": r"\bsemantic_failures=0\b",
            "state_failures": r"\bstate_failures=0\b",
            "projection_mismatches": r"\bprojection_mismatches=0\b",
            "artifact_failures": r"\bartifact_failures=0\b",
            "overall_ok": r"\boverall_ok=1\b",
        }
        summary_tokens = {
            name: re.search(pattern, summary_text) is not None
            for name, pattern in summary_patterns.items()
        }
        summary_ok = summary_path.is_file() and all(summary_tokens.values())
    budget.record(action, identifier, len(rows), generated, completed.returncode)
    result = {
        "action": action, "identifier": identifier,
        "exit_code": completed.returncode, "elapsed_seconds": elapsed,
        "planned_worlds": planned_worlds, "row_count": len(rows),
        "generated_count": generated, "parse_error": parse_error,
        "csv_path": str(csv_path.resolve()) if csv_path and csv_path.is_file() else None,
        "csv_sha256": _sha256_file(csv_path) if csv_path and csv_path.is_file() else None,
        "csv_fields": list(fields), "pair_id": pair_id,
        "oasis_histogram_path": (
            str(histogram_path.resolve())
            if histogram_path and histogram_path.is_file() else None
        ),
        "oasis_histogram_sha256": (
            _sha256_file(histogram_path)
            if histogram_path and histogram_path.is_file() else None
        ),
        "oasis_histogram_fields": list(histogram_fields),
        "oasis_histogram_row_count": len(histogram_rows),
        "emit_artifacts": emit_artifacts,
        "summary_path": (
            str(summary_path.resolve()) if summary_path.is_file() else None
        ),
        "summary_sha256": (
            _sha256_file(summary_path) if summary_path.is_file() else None
        ),
        "summary_tokens": summary_tokens, "summary_ok": summary_ok,
    }
    _write_json_new(directory / "run_result.json", result)
    _artifact_manifest(directory, directory / "artifact_manifest.json")
    return {
        **result, "rows": rows, "oasis_histogram_rows": histogram_rows,
    }


def _assert_run_shape(
    run: dict[str, Any], expected_rows: int, expected_generated: int,
    label: str,
) -> None:
    if not (
        run["exit_code"] == 0 and run["parse_error"] is None and
        run["summary_ok"] is True and
        run["row_count"] == expected_rows and
        run["generated_count"] == expected_generated
    ):
        raise DiminishingError(f"{label} process/shape failed: {run}")


def _run_formula(exe: Path, output: Path, budget: WorldBudget) -> dict[str, Any]:
    exact = _run_action(
        exe, output / "exact_ack", "formula", budget, 0,
        "diminishing_formula_ack_reset", include_candidate=False,
    )
    text = ""
    for name in (
        "stdout.txt", "stderr.txt", "aridity_diminishing_summary.txt",
        "aridity_diminishing_formula.txt",
    ):
        path = output / "exact_ack" / name
        if path.is_file():
            text += path.read_text(encoding="utf-8", errors="replace") + "\n"
    required = {
        "ack_cases": r"\back_cases=9\b",
        "ack_ok": r"\back_ok=1\b",
        "formula_cases": r"\bformula_cases=10201\b",
        "bias_monotonic_cases": r"\bbias_monotonic_cases=10100\b",
        "drought_monotonic_cases": r"\bdrought_monotonic_cases=10100\b",
        "formula_ok": r"\bformula_ok=1\b",
        "endpoint_cases": r"\bendpoint_cases=4\b",
        "endpoints_ok": r"\bendpoints_ok=1\b",
        "matrix_cases": r"\bmatrix_cases=6\b",
        "matrix_ok": r"\bmatrix_ok=1\b",
        "base_air_cases": r"\bbase_air_cases=18\b",
        "base_air_ok": r"\bbase_air_ok=1\b",
        "oasis_grid_cases": r"\boasis_grid_cases=57\b",
        "oasis_grid_ok": r"\boasis_grid_ok=1\b",
        "oasis_window_cases": r"\boasis_window_cases=9\b",
        "oasis_window_ok": r"\boasis_window_ok=1\b",
        "lifecycle_cases": r"\blifecycle_cases=3\b",
        "lifecycle_ok": r"\blifecycle_ok=1\b",
        "integer_pair_cases": r"\binteger_pair_cases=546\b",
        "histogram_oracle_cases": r"\bhistogram_oracle_cases=55146\b",
        "histogram_endpoint_cases": r"\bhistogram_endpoint_cases=6\b",
        "histogram_boundary_cases": r"\bhistogram_boundary_cases=6\b",
        "histogram_mismatches": r"\bhistogram_mismatches=0\b",
        "histogram_ok": r"\bhistogram_ok=1\b",
        "worlds_generated": r"\bworlds_generated=0\b",
        "overall_ok": r"\boverall_ok=1\b",
    }
    tokens = {
        name: re.search(pattern, text) is not None
        for name, pattern in required.items()
    }
    rejected_values = (
        ("unset", None), ("empty", ""),
        ("wrong_case", "ver037a_aridity_diminishing_response"),
        ("prefix", "prefix_VER037A_ARIDITY_DIMINISHING_RESPONSE"),
        ("suffix", "VER037A_ARIDITY_DIMINISHING_RESPONSE_extra"),
        ("old_ack", "VER037A_ARIDITY_RESPONSE_PROJECTION"),
        ("arbitrary", "arbitrary-nonempty"),
    )
    rejections = []
    for name, value in rejected_values:
        run = _run_action(
            exe, output / "ack_rejections" / name, "formula", budget, 0,
            f"diminishing_ack_reject_{name}", ack_value=value,
            include_candidate=False,
        )
        rejection_text = ""
        for stream in ("stdout.txt", "stderr.txt"):
            rejection_text += (
                output / "ack_rejections" / name / stream
            ).read_text(encoding="utf-8", errors="replace")
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
        "ok": (
            exact["exit_code"] == 0 and exact["parse_error"] is None and
            exact["row_count"] == 0 and exact["generated_count"] == 0 and
            all(tokens.values()) and all(item["ok"] for item in rejections)
        ),
        "exact_run": str((output / "exact_ack" / "run_result.json").resolve()),
        "required_tokens": tokens, "negative_ack_cases": rejections,
        "oasis_grid_schema": OASIS_GRID_SCHEMA,
        "oasis_grid_sha256": OASIS_GRID_SHA256,
        "oasis_grid_pair_count": len(OASIS_PAIRS),
        "integer_oasis_grid_schema": INTEGER_OASIS_GRID_SCHEMA,
        "integer_oasis_grid_sha256": INTEGER_OASIS_GRID_SHA256,
        "integer_oasis_grid_pair_count": len(INTEGER_OASIS_PAIRS),
        "worlds_generated": 0,
    }
    _write_json_new(output / "formula_result.json", result)
    if not result["ok"]:
        raise DiminishingError("formula/ACK/reset matrix failed")
    return result


def _run_baseline(
    exe: Path, output: Path, accepted_path: Path, budget: WorldBudget,
) -> dict[str, Any]:
    accepted, _ = _load_csv_required(accepted_path, BASELINE_COLUMNS)
    subset = [row for row in accepted if int(row["seed"]) == BASELINE_SEED]
    projection.legacy._validate_shape(subset, (BASELINE_SEED,))
    expected_bytes = projection.legacy._serialize_csv(
        subset, BASELINE_COLUMNS,
    )
    expected_sha = projection.legacy._sha256_bytes(expected_bytes)
    if expected_sha != EXPECTED_BASELINE_SHA256:
        raise DiminishingError(
            f"accepted baseline subset hash mismatch: {expected_sha}"
        )
    run = _run_action(
        exe, output / "run", "baseline", budget, 24,
        "fresh_production_no_ack", ack_value=None, include_candidate=False,
    )
    _assert_run_shape(run, 24, 24, "baseline")
    actual_bytes = projection.legacy._serialize_csv(
        run["rows"], BASELINE_COLUMNS,
    )
    actual_sha = projection.legacy._sha256_bytes(actual_bytes)
    result = {
        "ok": actual_bytes == expected_bytes and
              actual_sha == EXPECTED_BASELINE_SHA256,
        "accepted_matrix": str(accepted_path.resolve()),
        "accepted_matrix_sha256": _sha256_file(accepted_path),
        "expected_subset_sha256": expected_sha,
        "actual_subset_sha256": actual_sha, "row_count": len(run["rows"]),
        "run_result": str((output / "run" / "run_result.json").resolve()),
    }
    _write_json_new(output / "baseline_identity.json", result)
    if not result["ok"]:
        raise DiminishingError("fresh 24-world no-ACK baseline identity failed")
    return result


def _carrier_key(row: dict[str, str]) -> tuple[int, int, str]:
    moisture = int(row["moisture"])
    drought = int(row["drought"])
    expected_configuration = {
        "AB": (50, 0, 0), "CD": (50, 100, 0),
        "E": (25, 100, 100), "F": (75, 100, 100),
    }.get(row["carrier_role"])
    observed = (moisture, drought, int(row["bias_desert"]))
    if expected_configuration != observed:
        raise DiminishingError(
            f"carrier configuration mismatch {row['carrier_role']}: {observed}"
        )
    if int(row["drought_divisor"]) != 24:
        raise DiminishingError("diminishing carrier divisor was not 24")
    return int(row["seed"]), int(row["map_size"]), row["carrier_role"]


def _validate_carrier(row: dict[str, str]) -> dict[str, Any]:
    histogram = [int(row[column]) for column in HISTOGRAM_COLUMNS]
    prefix = [int(row[column]) for column in PREFIX_COLUMNS]
    running = 0
    prefix_ok = True
    for index, count in enumerate(histogram):
        prefix_ok &= count >= 0
        running += count
        prefix_ok &= prefix[index] == running
    map_index = int(row["map_size"])
    dimensions_ok = (
        0 <= map_index < len(MAPS) and
        row["map_name"] == MAPS[map_index][1] and
        int(row["width"]) == MAPS[map_index][2] and
        int(row["height"]) == MAPS[map_index][3]
    )
    try:
        hashes_ok = int(row["physical_hash"], 16) != 0 and \
            int(row["climate_input_hash"], 16) != 0
    except ValueError:
        hashes_ok = False
    land = int(row["land_count"])
    eligible = int(row["arid_eligible_count"])
    return {
        "ok": (
            int(row["generated"]) == 1 and int(row["ok"]) == 1 and
            int(row["active_state_ok"]) == 1 and
            int(row["restored_state_ok"]) == 1 and hashes_ok and
            dimensions_ok and land > 0 and 0 <= eligible <= land and
            running == eligible and prefix_ok
        ),
        "histogram_total": running, "eligible_count": eligible,
        "land_count": land, "prefix_ok": prefix_ok,
        "dimensions_ok": dimensions_ok, "hashes_ok": hashes_ok,
    }


def _run_carriers(
    exe: Path, output: Path, budget: WorldBudget,
) -> tuple[dict[tuple[int, int, str], dict[str, str]], dict[str, Any]]:
    run = _run_action(
        exe, output / "run", "carriers", budget, 32,
        "fresh_fixed_divisor_carriers",
    )
    _assert_run_shape(run, 32, 32, "carriers")
    lookup: dict[tuple[int, int, str], dict[str, str]] = {}
    checks = []
    for row in run["rows"]:
        key = _carrier_key(row)
        if key in lookup:
            raise DiminishingError(f"duplicate carrier key {key}")
        lookup[key] = row
        checks.append({"key": list(key), **_validate_carrier(row)})
    expected = {
        (seed, map_index, role)
        for seed in SCREEN_SEEDS for map_index, _, _, _ in MAPS
        for role in ("AB", "CD", "E", "F")
    }
    result = {
        "ok": set(lookup) == expected and all(item["ok"] for item in checks),
        "row_count": len(run["rows"]), "expected_row_count": 32,
        "unique_key_count": len(lookup), "checks": checks,
        "csv_path": run["csv_path"], "csv_sha256": run["csv_sha256"],
    }
    _write_json_new(output / "carrier_validation.json", result)
    if not result["ok"]:
        raise DiminishingError("fresh 32-carrier validation failed")
    return lookup, result


def _c_div(numerator: int, denominator: int) -> int:
    return projection._c_div(numerator, denominator)


def _clamp(value: int, low: int, high: int) -> int:
    return projection._clamp(value, low, high)


def _limits(moisture: int, drought: int, bias: int) -> dict[str, int]:
    bias_numerator = (
        bias * FIXED_CANDIDATE["desert_bias_span"] * 100
    )
    drought_numerator = (
        drought * FIXED_CANDIDATE["drought_classification_span"] *
        (100 - bias)
    )
    response = _c_div(bias_numerator + drought_numerator, 10000)
    combined = _clamp(
        FIXED_CANDIDATE["arid_base"] + response + _c_div(
            (moisture - 50) *
            FIXED_CANDIDATE["moisture_compression_span"], 25,
        ), 0, 100,
    )
    band = _clamp(
        12 - _c_div(bias * 10, 100) +
        _c_div(max(0, 50 - moisture) * 8, 25), 2, 20,
    )
    return {
        "dryness_response": response, "combined_arid_limit": combined,
        "semi_arid_band": band,
        "desert_limit": _clamp(combined - band, 0, 100),
        "semi_arid_limit": combined,
    }


def _prefix_count(carrier: dict[str, str], exclusive_limit: int) -> int:
    if exclusive_limit <= 0:
        return 0
    return int(carrier[f"prefix_{min(exclusive_limit - 1, 100):03d}"])


def _project_row(
    seed: int, map_index: int, case_id: str, moisture: int, drought: int,
    bias: int, carriers: dict[tuple[int, int, str], dict[str, str]],
) -> dict[str, Any]:
    role = "AB" if case_id in ("A", "B") else \
        "CD" if case_id in ("C", "D") else case_id
    carrier = carriers[(seed, map_index, role)]
    limits = _limits(moisture, drought, bias)
    desert = _prefix_count(carrier, limits["desert_limit"])
    combined = _prefix_count(carrier, limits["semi_arid_limit"])
    semi = combined - desert
    land = int(carrier["land_count"])
    if not 0 <= desert <= combined <= land:
        raise DiminishingError(
            f"invalid projected partition {seed}/{map_index}/{case_id}"
        )
    def share(count: int) -> str:
        return format(count / land, ".9f") if land else "0.000000000"
    return {
        **FIXED_CANDIDATE, "seed": seed, "map_size": map_index,
        "map_name": MAPS[map_index][1], "width": MAPS[map_index][2],
        "height": MAPS[map_index][3], "case_id": case_id,
        "moisture": moisture, "drought": drought, "bias_desert": bias,
        "carrier_role": role, "climate_input_hash": carrier["climate_input_hash"],
        "physical_hash": carrier["physical_hash"], "land_count": land,
        "arid_eligible_count": int(carrier["arid_eligible_count"]),
        **limits, "desert_count": desert, "semi_arid_count": semi,
        "combined_arid_count": combined, "non_arid_count": land - combined,
        "desert_share": share(desert), "semi_arid_share": share(semi),
        "combined_arid_share": share(combined),
        "non_arid_share": share(land - combined),
    }


def _share(row: dict[str, Any], field: str) -> Fraction:
    return Fraction(int(row[field]), int(row["land_count"]))


def _evaluate_climate(rows: Sequence[dict[str, Any]]) -> dict[str, Any]:
    failures: list[dict[str, Any]] = []
    by_world: dict[tuple[int, int], dict[str, dict[str, Any]]] = {}
    by_group: dict[tuple[int, str], list[dict[str, Any]]] = {}
    for row in rows:
        key = (int(row["seed"]), int(row["map_size"]))
        case_id = str(row["case_id"])
        by_world.setdefault(key, {})[case_id] = row
        by_group.setdefault((int(row["map_size"]), case_id), []).append(row)
        reasons = []
        combined_share = _share(row, "combined_arid_count")
        if int(row["land_count"]) <= 0:
            reasons.append("zero_land")
        if int(row["non_arid_count"]) <= 0:
            reasons.append("zero_non_arid_land")
        try:
            if int(str(row["climate_input_hash"]), 16) == 0:
                reasons.append("zero_climate_input_hash")
        except ValueError:
            reasons.append("invalid_climate_input_hash")
        if combined_share > PER_WORLD_CEILINGS[case_id]:
            reasons.append("per_world_combined_arid_ceiling")
        if reasons:
            failures.append({
                "type": "row", "seed": row["seed"],
                "map_size": row["map_size"], "configuration": case_id,
                "reasons": reasons,
                "combined_arid_share": _fraction_payload(combined_share),
            })
    relationships = (
        ("A", "B", "A<=B"), ("A", "C", "A<=C"),
        ("B", "D", "B<=D"), ("C", "D", "C<=D"),
        ("F", "D", "F<=D"), ("D", "E", "D<=E"),
    )
    for (seed, map_index), cases in sorted(by_world.items()):
        if set(cases) != {item[0] for item in CASES}:
            failures.append({
                "type": "missing_cases", "seed": seed,
                "map_size": map_index, "actual": sorted(cases),
            })
            continue
        shares = {
            name: _share(row, "combined_arid_count")
            for name, row in cases.items()
        }
        for left, right, label in relationships:
            if shares[left] > shares[right]:
                failures.append({
                    "type": "monotonic", "seed": seed,
                    "map_size": map_index, "relationship": label,
                    "left": _fraction_payload(shares[left]),
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
                    "configuration": case_id, "actual": len(members),
                    "expected": 2,
                })
                continue
            combined_values = [
                _share(row, "combined_arid_count") for row in members
            ]
            desert_values = [_share(row, "desert_count") for row in members]
            means[case_id] = projection.legacy._mean(combined_values)
            desert_means[case_id] = projection.legacy._mean(desert_values)
            low, high = CASE_MEAN_RANGES[case_id]
            if not low <= means[case_id] <= high:
                failures.append({
                    "type": "mean_range", "map_size": map_index,
                    "configuration": case_id,
                    "value": _fraction_payload(means[case_id]),
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
        "ok": not failures, "failure_count": len(failures),
        "failures": failures, "statistics": statistics,
        "ranking_metrics": {
            "total_center_distance": _fraction_payload(total_distance),
            "maximum_size_case_deviation": _fraction_payload(maximum_deviation),
        },
    }


def _expected_diagnostic_check(
    evaluation: dict[str, Any],
) -> dict[str, Any]:
    observed: dict[str, dict[str, str]] = {}
    mismatches = []
    for statistic in evaluation["statistics"]:
        map_name = statistic["map_name"]
        case_id = statistic["configuration"]
        mean = _payload_fraction(statistic["combined_arid_mean"])
        observed.setdefault(map_name, {})[case_id] = format(
            float(mean * 100), ".6f",
        )
    for map_name, expected_values in EXPECTED_SCREEN_MEANS.items():
        for index, (case_id, _, _, _) in enumerate(CASES):
            actual = observed.get(map_name, {}).get(case_id)
            expected = expected_values[index]
            if actual != expected:
                mismatches.append({
                    "map_name": map_name, "configuration": case_id,
                    "expected_percent": expected, "actual_percent": actual,
                })
    return {
        "ok": not mismatches and len(observed) == 4,
        "unit": "percent_of_land", "expected": EXPECTED_SCREEN_MEANS,
        "observed": observed, "mismatches": mismatches,
    }


def _run_projection(
    carriers: dict[tuple[int, int, str], dict[str, str]], output: Path,
) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    rows = [
        _project_row(
            seed, map_index, case_id, moisture, drought, bias, carriers,
        )
        for seed in SCREEN_SEEDS
        for map_index, _, _, _ in MAPS
        for case_id, moisture, drought, bias in CASES
    ]
    _write_flat_csv(output / "projected_rows.csv", rows, PROJECTED_COLUMNS)
    evaluation = _evaluate_climate(rows)
    diagnostics = _expected_diagnostic_check(evaluation)
    result = {
        "ok": len(rows) == 48 and evaluation["ok"] and diagnostics["ok"],
        "candidate": FIXED_CANDIDATE, "projected_row_count": len(rows),
        "climate_evaluation": evaluation,
        "expected_diagnostic_means": diagnostics,
        "projected_rows_path": str((output / "projected_rows.csv").resolve()),
        "projected_rows_sha256": _sha256_file(output / "projected_rows.csv"),
    }
    _write_json_new(output / "projection_result.json", result)
    if not result["ok"]:
        raise DiminishingError("fixed-candidate offline climate projection failed")
    return rows, result


def _actual_key(row: dict[str, Any]) -> tuple[int, int, str]:
    return int(row["seed"]), int(row["map_size"]), str(row["case_id"])


def _prefix_at(prefix: Sequence[int], inclusive_limit: int) -> int:
    if inclusive_limit < 0:
        return 0
    if inclusive_limit >= 100:
        return prefix[100]
    return prefix[inclusive_limit]


def _integer_oasis_count(
    histogram: Sequence[int], prefix: Sequence[int], drought: int,
    combined_arid_limit: int, drop: int, margin: int,
) -> tuple[int, int, int, int]:
    oasis_limit = 42 - _c_div(drought * drop, 100)
    transition_limit = combined_arid_limit + _c_div(drought * margin, 100)
    lower_count = _prefix_at(prefix, oasis_limit)
    prefix_count = max(0, (
        prefix[100] - lower_count if drought == 0 else
        _prefix_at(prefix, transition_limit - 1) - lower_count
    ))
    oracle_count = sum(
        count for moisture, count in enumerate(histogram)
        if moisture > oasis_limit and
        (drought == 0 or moisture < transition_limit)
    )
    return prefix_count, oracle_count, oasis_limit, transition_limit


def _validate_oasis_histogram_sidecar(
    actual_rows: Sequence[dict[str, str]],
    sidecar_rows: Sequence[dict[str, str]],
) -> dict[str, Any]:
    actual_lookup = {_actual_key(row): row for row in actual_rows}
    sidecar_lookup: dict[tuple[int, int, str], dict[str, str]] = {}
    count_lookup: dict[tuple[int, int, str], dict[str, int]] = {}
    checks = []
    duplicate_keys = []
    total_prefix_cases = 0
    total_oracle_moisture_cases = 0
    for sidecar in sidecar_rows:
        key = _actual_key(sidecar)
        if key in sidecar_lookup:
            duplicate_keys.append(list(key))
        sidecar_lookup[key] = sidecar
    for key in sorted(set(actual_lookup) & set(sidecar_lookup)):
        actual = actual_lookup[key]
        sidecar = sidecar_lookup[key]
        reasons = []
        if sidecar["row"] != actual["row"]:
            reasons.append("row_identity_mismatch")
        for field in (
            "mode", "candidate_id", "pair_id", "seed", "map_size",
            "map_name", "case_id", "drought", "combined_arid_limit",
            "desert_limit", "semi_arid_limit", "land_count",
            "river_channel_count",
        ):
            if sidecar[field] != actual[field]:
                reasons.append(f"sidecar_identity_mismatch_{field}")
        histogram = [
            int(sidecar[column]) for column in OASIS_HISTOGRAM_COLUMNS
        ]
        prefix = [int(sidecar[column]) for column in OASIS_PREFIX_COLUMNS]
        eligible = int(sidecar["eligible_count"])
        running = 0
        for index, count in enumerate(histogram):
            if count < 0:
                reasons.append(f"negative_histogram_bin_{index}")
            running += count
            if prefix[index] != running:
                reasons.append(f"prefix_mismatch_{index}")
        if (
            running != eligible or
            int(sidecar["histogram_total"]) != eligible or
            int(sidecar["prefix_total"]) != eligible or
            eligible < 0 or
            eligible > int(actual["river_channel_count"])
        ):
            reasons.append("eligible_partition_mismatch")
        if int(sidecar["oracle_pair_cases"]) != len(INTEGER_OASIS_PAIRS):
            reasons.append("oracle_pair_count_mismatch")
        if int(sidecar["oracle_bin_checks"]) != \
                len(INTEGER_OASIS_PAIRS) * 101:
            reasons.append("oracle_bin_check_count_mismatch")
        if int(sidecar["oracle_mismatches"]) != 0:
            reasons.append("oracle_mismatch")
        if int(sidecar["legacy_36_cases"]) != len(OASIS_PAIRS):
            reasons.append("legacy_crosscheck_case_count_mismatch")
        if int(sidecar["legacy_36_mismatches"]) != 0:
            reasons.append("legacy_crosscheck_mismatch")
        if int(sidecar["histogram_ok"]) != 1:
            reasons.append("histogram_not_ok")
        projected_pre_desert = int(sidecar["projected_pre_desert_count"])
        projected_pre_semi = int(sidecar["projected_pre_semi_arid_count"])
        projected_pre_combined = int(
            sidecar["projected_pre_combined_arid_count"]
        )
        projected_pre_non_arid = int(
            sidecar["projected_pre_non_arid_count"]
        )
        if (
            projected_pre_desert != int(actual["pre_climate_03"]) or
            projected_pre_semi != int(actual["pre_climate_04"]) or
            projected_pre_combined != projected_pre_desert + projected_pre_semi or
            projected_pre_non_arid !=
                int(actual["land_count"]) - projected_pre_combined or
            int(sidecar["projection_pre_identity_ok"]) != 1
        ):
            reasons.append("projection_pre_identity_mismatch")
        drought = int(actual["drought"])
        combined = int(actual["combined_arid_limit"])
        pair_counts: dict[str, int] = {}
        for pair_id, drop, margin in INTEGER_OASIS_PAIRS:
            prefix_count, oracle_count, _, _ = _integer_oasis_count(
                histogram, prefix, drought, combined, drop, margin,
            )
            observed = int(sidecar[INTEGER_OASIS_COLUMN_BY_PAIR[pair_id]])
            if observed != prefix_count or observed != oracle_count or \
                    observed < 0 or observed > eligible:
                reasons.append(f"integer_pair_mismatch_{pair_id}")
            pair_counts[pair_id] = observed
            total_prefix_cases += 1
            total_oracle_moisture_cases += 101
        for pair_id, _, _ in OASIS_PAIRS:
            if int(actual[OASIS_COLUMN_BY_PAIR[pair_id]]) != pair_counts[pair_id]:
                reasons.append(f"legacy_pair_mismatch_{pair_id}")
        active_pair = str(actual["pair_id"])
        if (
            active_pair not in pair_counts or
            int(sidecar["active_projected_oasis_count"]) !=
                pair_counts.get(active_pair, -1) or
            int(sidecar["active_projected_oasis_count"]) !=
                int(actual["oasis_count"])
        ):
            reasons.append("active_pair_projection_mismatch")
        count_lookup[key] = pair_counts
        checks.append({"key": list(key), "reasons": reasons, "ok": not reasons})
    missing_sidecar = sorted(set(actual_lookup) - set(sidecar_lookup))
    unexpected_sidecar = sorted(set(sidecar_lookup) - set(actual_lookup))
    ok = (
        len(actual_rows) == len(sidecar_rows) == len(actual_lookup) ==
            len(sidecar_lookup) and not duplicate_keys and not missing_sidecar and
        not unexpected_sidecar and all(item["ok"] for item in checks)
    )
    return {
        "ok": ok, "row_count": len(sidecar_rows),
        "expected_row_count": len(actual_rows),
        "integer_pair_count": len(INTEGER_OASIS_PAIRS),
        "prefix_pair_case_count": total_prefix_cases,
        "oracle_moisture_case_count": total_oracle_moisture_cases,
        "duplicate_keys": duplicate_keys,
        "missing_sidecar_keys": [list(key) for key in missing_sidecar],
        "unexpected_sidecar_keys": [list(key) for key in unexpected_sidecar],
        "checks": checks, "count_lookup": count_lookup,
    }


def _climate_accounting(row: dict[str, str]) -> dict[str, Any]:
    pre = [int(row[column]) for column in PRE_CLIMATE_COLUMNS]
    post = [int(row[column]) for column in POST_CLIMATE_COLUMNS]
    nonrefresh = [
        [
            int(row[f"nonrefresh_transition_{source:02d}_{target:02d}"])
            for target in range(CLIMATE_COUNT)
        ]
        for source in range(CLIMATE_COUNT)
    ]
    refresh = [
        [
            int(row[f"refresh_transition_{source:02d}_{target:02d}"])
            for target in range(CLIMATE_COUNT)
        ]
        for source in range(CLIMATE_COUNT)
    ]
    land = int(row["land_count"])
    refresh_eligible = int(row["refresh_eligible_count"])
    refresh_changed = int(row["refresh_changed_count"])
    nonrefresh_changed = int(row["non_refresh_changed_count"])
    transition_land = int(row["transition_land_count"])
    unexplained = int(row["accounting_unexplained_count"])
    reasons = []
    nonrefresh_total = sum(sum(values) for values in nonrefresh)
    refresh_total = sum(sum(values) for values in refresh)
    observed_refresh_changed = sum(
        refresh[source][target]
        for source in range(CLIMATE_COUNT)
        for target in range(CLIMATE_COUNT) if source != target
    )
    observed_nonrefresh_changed = sum(
        nonrefresh[source][target]
        for source in range(CLIMATE_COUNT)
        for target in range(CLIMATE_COUNT) if source != target
    )
    if sum(pre) != land:
        reasons.append("pre_climate_partition")
    if sum(post) != land:
        reasons.append("post_climate_partition")
    if nonrefresh_total + refresh_total != land or transition_land != land:
        reasons.append("transition_land_partition")
    if refresh_total != refresh_eligible:
        reasons.append("refresh_eligible_partition")
    if nonrefresh_total != land - refresh_eligible:
        reasons.append("nonrefresh_partition")
    if observed_refresh_changed != refresh_changed:
        reasons.append("refresh_changed_count")
    if observed_nonrefresh_changed != nonrefresh_changed:
        reasons.append("nonrefresh_changed_count")
    if nonrefresh_changed != 0:
        reasons.append("nonrefresh_climate_changed")
    for source in range(CLIMATE_COUNT):
        outgoing = sum(nonrefresh[source]) + sum(refresh[source])
        if outgoing != pre[source]:
            reasons.append(f"pre_outgoing_{source:02d}")
    for target in range(CLIMATE_COUNT):
        incoming = sum(nonrefresh[source][target] for source in range(CLIMATE_COUNT)) + \
            sum(refresh[source][target] for source in range(CLIMATE_COUNT))
        if incoming != post[target]:
            reasons.append(f"post_incoming_{target:02d}")
    if unexplained != 0 or int(row["climate_accounting_ok"]) != 1:
        reasons.append("product_accounting_gate")
    if int(row["desert_count"]) != post[3]:
        reasons.append("final_desert_vs_post")
    if int(row["semi_arid_count"]) != post[4]:
        reasons.append("final_semi_arid_vs_post")
    if int(row["combined_arid_count"]) != post[3] + post[4]:
        reasons.append("final_combined_vs_post")
    if int(row["non_arid_count"]) != land - post[3] - post[4]:
        reasons.append("final_non_arid_vs_post")
    nonzero_refresh = [
        {"from": source, "to": target, "count": refresh[source][target]}
        for source in range(CLIMATE_COUNT)
        for target in range(CLIMATE_COUNT) if refresh[source][target] != 0
    ]
    nonzero_nonrefresh = [
        {"from": source, "to": target, "count": nonrefresh[source][target]}
        for source in range(CLIMATE_COUNT)
        for target in range(CLIMATE_COUNT)
        if nonrefresh[source][target] != 0 and source != target
    ]
    return {
        "ok": not reasons, "reasons": reasons,
        "pre_desert_count": pre[3], "pre_semi_arid_count": pre[4],
        "pre_combined_arid_count": pre[3] + pre[4],
        "pre_non_arid_count": land - pre[3] - pre[4],
        "post_desert_count": post[3], "post_semi_arid_count": post[4],
        "post_combined_arid_count": post[3] + post[4],
        "post_non_arid_count": land - post[3] - post[4],
        "refresh_eligible_count": refresh_eligible,
        "refresh_changed_count": refresh_changed,
        "non_refresh_changed_count": nonrefresh_changed,
        "transition_land_count": transition_land,
        "accounting_unexplained_count": unexplained,
        "nonzero_refresh_transitions": nonzero_refresh,
        "nonzero_nonrefresh_off_diagonal_transitions": nonzero_nonrefresh,
    }


def _historical_large_refresh_witness(
    accounting: Sequence[dict[str, Any]],
) -> dict[str, Any]:
    expected = {
        (2026082302, 2, "D"): (73252, 48327, 73248, 48326),
        (2026082302, 2, "E"): (2504, 168383, 2504, 168378),
    }
    observed = {
        tuple(item["key"]): item for item in accounting
        if tuple(item["key"]) in expected
    }
    checks = []
    for key, values in expected.items():
        item = observed.get(key)
        expected_pre_desert, expected_pre_semi, expected_post_desert, \
            expected_post_semi = values
        ok = item is not None and item["ok"] and \
            item["pre_desert_count"] == expected_pre_desert and \
            item["pre_semi_arid_count"] == expected_pre_semi and \
            item["post_desert_count"] == expected_post_desert and \
            item["post_semi_arid_count"] == expected_post_semi and \
            item["non_refresh_changed_count"] == 0 and \
            item["accounting_unexplained_count"] == 0 and \
            item["refresh_changed_count"] > 0
        checks.append({
            "key": list(key), "expected": {
                "pre_desert": expected_pre_desert,
                "pre_semi_arid": expected_pre_semi,
                "post_desert": expected_post_desert,
                "post_semi_arid": expected_post_semi,
            },
            "observed": item, "ok": bool(ok),
        })
    return {"ok": len(checks) == 2 and all(item["ok"] for item in checks),
            "checks": checks}


def _actual_row_reasons(
    row: dict[str, str], pair_id: str, expected_mode: str,
    require_override: bool = True,
    sidecar_counts: dict[str, int] | None = None,
) -> list[str]:
    reasons = []
    map_index = int(row["map_size"])
    case_lookup = {
        case_id: (moisture, drought, bias)
        for case_id, moisture, drought, bias in CASES
    }
    if not 0 <= map_index < len(MAPS):
        reasons.append("map_size_out_of_range")
    else:
        _, map_name, width, height = MAPS[map_index]
        if row["map_name"] != map_name or int(row["width"]) != width or \
                int(row["height"]) != height:
            reasons.append("map_geometry_mismatch")
    expected_inputs = case_lookup.get(row["case_id"])
    observed_inputs = (
        int(row["moisture"]), int(row["drought"]),
        int(row["bias_desert"]),
    )
    if expected_inputs is None or observed_inputs != expected_inputs:
        reasons.append("configuration_inputs_mismatch")
    if row["mode"] != expected_mode:
        reasons.append("mode_mismatch")
    if row["candidate_id"] != FIXED_CANDIDATE["candidate_id"]:
        reasons.append("candidate_id_mismatch")
    if row["pair_id"] != pair_id:
        reasons.append("pair_id_mismatch")
    for name in (
        "arid_base", "desert_bias_span", "drought_divisor",
        "moisture_compression_span", "drought_classification_span",
    ):
        if int(row[name]) != int(FIXED_CANDIDATE[name]):
            reasons.append(f"{name}_mismatch")
    try:
        drop, margin = INTEGER_OASIS_PAIR_BY_ID[pair_id]
    except KeyError:
        reasons.append("integer_oasis_pair_id_mismatch")
        drop = margin = -1
    if int(row["oasis_drop"]) != drop:
        reasons.append("oasis_drop_mismatch")
    if int(row["transition_margin"]) != margin:
        reasons.append("transition_margin_mismatch")
    expected_limits = _limits(
        int(row["moisture"]), int(row["drought"]), int(row["bias_desert"]),
    )
    for name, expected in expected_limits.items():
        if int(row[name]) != expected:
            reasons.append(f"{name}_mismatch")
    if drop >= 0:
        expected_oasis_limit = 42 - _c_div(int(row["drought"]) * drop, 100)
        expected_transition_limit = (
            expected_limits["combined_arid_limit"] +
            _c_div(int(row["drought"]) * margin, 100)
        )
        if int(row["oasis_limit"]) != expected_oasis_limit:
            reasons.append("oasis_limit_mismatch")
        if int(row["oasis_transition_limit"]) != expected_transition_limit:
            reasons.append("oasis_transition_limit_mismatch")
    if int(row["generated"]) != 1 or int(row["ok"]) != 1:
        reasons.append("generation_or_row_failure")
    if row["failure_stage"] != "none" or row["failure_reason"] != "none":
        reasons.append("unexpected_failure_identity")
    if int(row["active_state_ok"]) != 1 or int(row["restored_state_ok"]) != 1:
        reasons.append("state_lifecycle_failure")
    if not require_override and int(row["active_state_ok"]) != 1:
        reasons.append("production_default_state_failure")
    if int(row["land_count"]) <= 0 or int(row["non_arid_count"]) <= 0:
        reasons.append("invalid_land_partition")
    if int(row["combined_arid_count"]) != (
        int(row["desert_count"]) + int(row["semi_arid_count"])
    ):
        reasons.append("combined_count_mismatch")
    if int(row["combined_arid_count"]) + int(row["non_arid_count"]) != \
            int(row["land_count"]):
        reasons.append("land_partition_mismatch")
    if int(row["oasis_semantic_errors"]) != 0:
        reasons.append("oasis_semantic_errors")
    if not _climate_accounting(row)["ok"]:
        reasons.append("climate_refresh_accounting")
    river_channels = int(row["river_channel_count"])
    oasis_count = int(row["oasis_count"])
    if river_channels < 0 or not 0 <= oasis_count <= river_channels:
        reasons.append("invalid_oasis_channel_partition")
    projected_oases = [
        int(row[column]) for column in OASIS_COLUMN_BY_PAIR.values()
    ]
    if any(value < 0 or value > river_channels for value in projected_oases):
        reasons.append("invalid_projected_oasis_count")
    try:
        if int(row["physical_hash"], 16) == 0:
            reasons.append("zero_physical_hash")
        if int(row["climate_input_hash"], 16) == 0:
            reasons.append("zero_climate_input_hash")
    except ValueError:
        reasons.append("invalid_hash")
    selected_projection = (
        sidecar_counts.get(pair_id) if sidecar_counts is not None else
        int(row[OASIS_COLUMN_BY_PAIR[pair_id]])
        if pair_id in OASIS_COLUMN_BY_PAIR else None
    )
    if selected_projection is None or int(row["oasis_count"]) != \
            selected_projection:
        reasons.append("active_pair_projection_mismatch")
    return reasons


def _compare_projected_actual(
    projected: Sequence[dict[str, Any]], actual: Sequence[dict[str, str]],
) -> dict[str, Any]:
    expected_lookup = {_actual_key(row): row for row in projected}
    actual_lookup = {_actual_key(row): row for row in actual}
    mismatches = []
    if set(expected_lookup) != set(actual_lookup):
        mismatches.append({
            "type": "key_set", "projected": sorted(expected_lookup),
            "actual": sorted(actual_lookup),
        })
    direct_fields = (
        "climate_input_hash", "land_count", "dryness_response",
        "combined_arid_limit", "semi_arid_band", "desert_limit",
        "semi_arid_limit",
    )
    projected_to_pre = {
        "desert_count": "pre_climate_03",
        "semi_arid_count": "pre_climate_04",
    }
    for key in sorted(set(expected_lookup) & set(actual_lookup)):
        expected = expected_lookup[key]
        observed = actual_lookup[key]
        differences = {
            field: {"projected": str(expected[field]), "actual": observed[field]}
            for field in direct_fields
            if str(expected[field]) != observed[field]
        }
        for projected_field, pre_field in projected_to_pre.items():
            if str(expected[projected_field]) != observed[pre_field]:
                differences[projected_field] = {
                    "projected": str(expected[projected_field]),
                    "actual_pre_hydrology": observed[pre_field],
                }
        expected_combined = int(expected["combined_arid_count"])
        actual_pre_combined = int(observed["pre_climate_03"]) + \
            int(observed["pre_climate_04"])
        if expected_combined != actual_pre_combined:
            differences["combined_arid_count"] = {
                "projected": str(expected_combined),
                "actual_pre_hydrology": str(actual_pre_combined),
            }
        expected_non_arid = int(expected["non_arid_count"])
        actual_pre_non_arid = int(observed["land_count"]) - actual_pre_combined
        if expected_non_arid != actual_pre_non_arid:
            differences["non_arid_count"] = {
                "projected": str(expected_non_arid),
                "actual_pre_hydrology": str(actual_pre_non_arid),
            }
        if differences:
            mismatches.append({
                "type": "row", "key": list(key), "differences": differences,
            })
    return {
        "ok": not mismatches and len(projected) == len(actual),
        "projected_count": len(projected), "actual_count": len(actual),
        "mismatches": mismatches,
    }


def _evaluate_oasis_pair(
    rows: Sequence[dict[str, str]], pair_id: str, *, actual: bool,
    projected_count_lookup: dict[
        tuple[int, int, str], dict[str, int]
    ] | None = None,
) -> dict[str, Any]:
    drop, margin = INTEGER_OASIS_PAIR_BY_ID[pair_id]
    groups: dict[tuple[int, str], list[dict[str, str]]] = {}
    for row in rows:
        groups.setdefault((int(row["map_size"]), row["case_id"]), []).append(row)
    failures = []
    results = []
    max_land_deviation = Fraction(0, 1)
    max_channel_deviation = Fraction(0, 1)
    clearances: list[Fraction] = []
    normalized_center_deviations: list[Fraction] = []
    for map_index, map_name, _, _ in MAPS:
        for case_id in ("D", "E"):
            members = groups.get((map_index, case_id), [])
            reasons = []
            counts: list[int] = []
            land_mean = Fraction(0, 1)
            channel_mean = Fraction(0, 1)
            if len(members) != 2:
                reasons.append("sample_count")
            else:
                if actual:
                    counts = [int(row["oasis_count"]) for row in members]
                elif projected_count_lookup is None:
                    reasons.append("missing_projected_count_lookup")
                else:
                    try:
                        counts = [
                            projected_count_lookup[_actual_key(row)][pair_id]
                            for row in members
                        ]
                    except KeyError:
                        reasons.append("missing_projected_pair_count")
                land_mean = projection.legacy._mean([
                    Fraction(count, int(row["land_count"]))
                    for count, row in zip(counts, members)
                ]) if len(counts) == 2 else Fraction(0, 1)
                channel_values = []
                for count, row in zip(counts, members):
                    channels = int(row["river_channel_count"])
                    if channels <= 0:
                        reasons.append("zero_river_channels")
                    else:
                        channel_values.append(Fraction(count, channels))
                if len(channel_values) == 2:
                    channel_mean = projection.legacy._mean(channel_values)
                if any(count <= 0 for count in counts):
                    reasons.append("individual_world_has_zero_oasis")
                if not Fraction(2, 1000) <= land_mean <= Fraction(2, 100):
                    reasons.append("two_seed_land_share_mean_out_of_range")
                if not Fraction(3, 100) <= channel_mean <= Fraction(25, 100):
                    reasons.append("two_seed_channel_share_mean_out_of_range")
                land_clearance = min(
                    land_mean - Fraction(1, 500),
                    Fraction(1, 50) - land_mean,
                ) / Fraction(9, 500)
                channel_clearance = min(
                    channel_mean - Fraction(3, 100),
                    Fraction(1, 4) - channel_mean,
                ) / Fraction(11, 50)
                clearances.extend((land_clearance, channel_clearance))
                normalized_center_deviations.extend((
                    abs(land_mean - Fraction(1, 100)) / Fraction(9, 500),
                    abs(channel_mean - Fraction(7, 50)) / Fraction(11, 50),
                ))
            max_land_deviation = max(
                max_land_deviation, abs(land_mean - Fraction(1, 100)),
            )
            max_channel_deviation = max(
                max_channel_deviation, abs(channel_mean - Fraction(14, 100)),
            )
            entry = {
                "map_size": map_index, "map_name": map_name,
                "configuration": case_id, "counts": counts,
                "land_share_mean": _fraction_payload(land_mean),
                "channel_share_mean": _fraction_payload(channel_mean),
                "land_clearance": _fraction_payload(
                    land_clearance if len(members) == 2 and len(counts) == 2
                    else Fraction(-1, 1)
                ),
                "channel_clearance": _fraction_payload(
                    channel_clearance if len(members) == 2 and len(counts) == 2
                    else Fraction(-1, 1)
                ),
                "reasons": reasons, "ok": not reasons,
            }
            results.append(entry)
            if reasons:
                failures.append(entry)
    return {
        "pair_id": pair_id, "drop": drop, "transition_margin": margin,
        "source": "actual" if actual else "projected",
        "ok": not failures, "groups": results, "failures": failures,
        "score": {
            "maximum_land_target_deviation":
                _fraction_payload(max_land_deviation),
            "maximum_channel_target_deviation":
                _fraction_payload(max_channel_deviation),
            "robustness": _fraction_payload(
                min(clearances) if clearances else Fraction(-1, 1)
            ),
            "normalized_center_deviation": _fraction_payload(
                max(normalized_center_deviations)
                if normalized_center_deviations else Fraction(1 << 30, 1)
            ),
        },
    }


def _run_oasis(
    exe: Path, output: Path, budget: WorldBudget,
    projected: Sequence[dict[str, Any]],
) -> tuple[str, list[dict[str, str]], dict[str, Any], Path]:
    active_pair = "o00_t00"
    run = _run_action(
        exe, output / "run", "oasis", budget, 16,
        "fixed_candidate_all_oasis_pairs", pair_id=active_pair,
    )
    _assert_run_shape(run, 16, 16, "oasis")
    rows = run["rows"]
    sidecar = _validate_oasis_histogram_sidecar(
        rows, run["oasis_histogram_rows"],
    )
    count_lookup = sidecar["count_lookup"]
    sidecar_evidence = {
        key: value for key, value in sidecar.items() if key != "count_lookup"
    }
    expected_keys = {
        (seed, map_index, case_id)
        for seed in SCREEN_SEEDS for map_index, _, _, _ in MAPS
        for case_id in ("D", "E")
    }
    row_reasons = [
        {"key": list(_actual_key(row)), "reasons": _actual_row_reasons(
            row, active_pair, "oasis",
            sidecar_counts=count_lookup.get(_actual_key(row)),
        )}
        for row in rows
    ]
    accounting = [
        {"key": list(_actual_key(row)), **_climate_accounting(row)}
        for row in rows
    ]
    historical_refresh_witness = _historical_large_refresh_witness(accounting)
    projected_de = [row for row in projected if row["case_id"] in ("D", "E")]
    identity = _compare_projected_actual(projected_de, rows)
    pair_results = [
        _evaluate_oasis_pair(
            rows, pair_id, actual=False,
            projected_count_lookup=count_lookup,
        )
        for pair_id, _, _ in INTEGER_OASIS_PAIRS
    ]
    passing = [item for item in pair_results if item["ok"]]
    ranked = sorted(passing, key=lambda item: (
        -_payload_fraction(item["score"]["robustness"]),
        _payload_fraction(item["score"]["normalized_center_deviation"]),
        item["drop"], item["transition_margin"],
    ))
    for rank, item in enumerate(ranked, 1):
        item["rank"] = rank
    selected = ranked[0] if ranked else None
    disposition = (
        "AUTO_SELECTED_EXACT_RATIONAL_RANKING" if passing else
        "ZERO_SURVIVORS"
    )
    table_path = output / "integer_oasis_grid_results.json"
    _write_json_new(table_path, {
        "candidate": FIXED_CANDIDATE,
        "integer_oasis_grid_schema": INTEGER_OASIS_GRID_SCHEMA,
        "integer_oasis_grid_sha256": INTEGER_OASIS_GRID_SHA256,
        "integer_oasis_grid_pair_count": len(INTEGER_OASIS_PAIRS),
        "pair_evaluations": pair_results,
        "survivor_count": len(ranked), "survivor_ranking": ranked,
    })
    rank_by_id = {item["pair_id"]: item["rank"] for item in ranked}
    csv_rows = []
    for item in pair_results:
        robustness = item["score"]["robustness"]
        center = item["score"]["normalized_center_deviation"]
        csv_rows.append({
            "pair_id": item["pair_id"], "drop": item["drop"],
            "transition_margin": item["transition_margin"],
            "ok": int(item["ok"]), "rank": rank_by_id.get(item["pair_id"], ""),
            "failure_group_count": len(item["failures"]),
            "robustness_numerator": robustness["numerator"],
            "robustness_denominator": robustness["denominator"],
            "robustness_decimal": robustness["decimal"],
            "center_deviation_numerator": center["numerator"],
            "center_deviation_denominator": center["denominator"],
            "center_deviation_decimal": center["decimal"],
        })
    table_csv_path = output / "integer_oasis_grid_results.csv"
    _write_flat_csv(table_csv_path, csv_rows, (
        "pair_id", "drop", "transition_margin", "ok", "rank",
        "failure_group_count", "robustness_numerator",
        "robustness_denominator", "robustness_decimal",
        "center_deviation_numerator", "center_deviation_denominator",
        "center_deviation_decimal",
    ))
    result = {
        "ok": (
            set(map(_actual_key, rows)) == expected_keys and
            all(not item["reasons"] for item in row_reasons) and
            all(item["ok"] for item in accounting) and
            historical_refresh_witness["ok"] and
            identity["ok"] and sidecar_evidence["ok"] and
            len(pair_results) == len(INTEGER_OASIS_PAIRS) and bool(passing)
        ),
        "candidate": FIXED_CANDIDATE, "active_generation_pair": active_pair,
        "row_semantics": row_reasons, "projection_identity": identity,
        "climate_refresh_accounting": accounting,
        "historical_large_refresh_witness": historical_refresh_witness,
        "pair_evaluations": pair_results, "passing_pair_count": len(passing),
        "passing_pairs": ranked, "survivor_disposition": disposition,
        "oasis_grid_schema": OASIS_GRID_SCHEMA,
        "oasis_grid_sha256": OASIS_GRID_SHA256,
        "oasis_grid_pair_count": len(OASIS_PAIRS),
        "integer_oasis_grid_schema": INTEGER_OASIS_GRID_SCHEMA,
        "integer_oasis_grid_sha256": INTEGER_OASIS_GRID_SHA256,
        "integer_oasis_grid_pair_count": len(INTEGER_OASIS_PAIRS),
        "integer_oasis_grid_result": str(table_path.resolve()),
        "integer_oasis_grid_result_sha256": _sha256_file(table_path),
        "integer_oasis_grid_csv": str(table_csv_path.resolve()),
        "integer_oasis_grid_csv_sha256": _sha256_file(table_csv_path),
        "histogram_sidecar_validation": sidecar_evidence,
        "histogram_sidecar_path": run["oasis_histogram_path"],
        "histogram_sidecar_sha256": run["oasis_histogram_sha256"],
        "selected_oasis_pair": selected, "csv_path": run["csv_path"],
        "csv_sha256": run["csv_sha256"],
    }
    _write_json_new(output / "oasis_screen_result.json", result)
    if len(passing) == 0:
        raise DiminishingError("16-world oasis screen found zero survivors")
    if not result["ok"]:
        raise DiminishingError("16-world oasis screen evidence failed")
    selection = {
        "status": "FROZEN_BEFORE_SCREEN_CONFIRMATION",
        "candidate": FIXED_CANDIDATE, "selected_oasis_pair": selected,
        "oasis_grid_schema": OASIS_GRID_SCHEMA,
        "oasis_grid_sha256": OASIS_GRID_SHA256,
        "oasis_grid_pair_count": len(OASIS_PAIRS),
        "integer_oasis_grid_schema": INTEGER_OASIS_GRID_SCHEMA,
        "integer_oasis_grid_sha256": INTEGER_OASIS_GRID_SHA256,
        "integer_oasis_grid_pair_count": len(INTEGER_OASIS_PAIRS),
        "integer_oasis_grid_result": str(table_path.resolve()),
        "integer_oasis_grid_result_sha256": _sha256_file(table_path),
        "integer_oasis_grid_csv": str(table_csv_path.resolve()),
        "integer_oasis_grid_csv_sha256": _sha256_file(table_csv_path),
        "survivor_count": len(ranked), "survivor_ranking": ranked,
        "histogram_sidecar": run["oasis_histogram_path"],
        "histogram_sidecar_sha256": run["oasis_histogram_sha256"],
        "oasis_screen_result": str((output / "oasis_screen_result.json").resolve()),
        "oasis_screen_result_sha256": _sha256_file(
            output / "oasis_screen_result.json",
        ),
        "oasis_csv": run["csv_path"], "oasis_csv_sha256": run["csv_sha256"],
    }
    selection_path = output / "selected_oasis_pair.json"
    _write_json_new(selection_path, selection)
    _write_text_new(
        output / "selected_oasis_pair.sha256",
        f"{_sha256_file(selection_path)}  {selection_path.name}\n",
    )
    return selected["pair_id"], rows, result, selection_path


def _audit_bmp(
    path: Path, width: int, height: int, pixel_hash: str,
) -> dict[str, Any]:
    return projection._audit_bmp(path, width, height, pixel_hash)


def _audit_artifacts(
    directory: Path, rows: Sequence[dict[str, str]], expected_seed: int,
) -> dict[str, Any]:
    manifest = directory / "aridity_diminishing_artifacts_manifest.csv"
    manifest_rows, _ = _load_csv_required(manifest, ARTIFACT_REQUIRED_COLUMNS)
    expected_keys = {
        (expected_seed, map_index, case_id)
        for map_index in (0, 3) for case_id, _, _, _ in CASES
    }
    row_lookup = {_actual_key(row): row for row in rows}
    observed = set()
    images = []
    failures = []
    for item in manifest_rows:
        key = (int(item["seed"]), int(item["map_size"]), item["case_id"])
        observed.add(key)
        actual_row = row_lookup.get(key)
        if actual_row is None:
            failures.append({"type": "manifest_key", "key": list(key)})
            continue
        for field in (
            "candidate_id", "pair_id", "physical_hash", "climate_input_hash",
            "oasis_count", "map_name", "width", "height",
        ):
            if item[field] != actual_row[field]:
                failures.append({
                    "type": "manifest_binding", "key": list(key),
                    "field": field, "manifest": item[field],
                    "actual": actual_row[field],
                })
        for mode in ("geography", "climate"):
            path = projection.legacy._resolve_artifact(
                directory, item[f"{mode}_file"],
            )
            audit = _audit_bmp(
                path, int(item["width"]), int(item["height"]),
                item[f"{mode}_pixel_hash"],
            )
            record = {
                "seed": key[0], "map_size": key[1],
                "map_name": item["map_name"], "case_id": key[2],
                "mode": mode, "path": str(path.resolve()),
                "file_sha256": _sha256_file(path), **audit,
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
        "expected_seed": expected_seed, "image_count": len(images),
        "images": images, "failures": failures,
    }


def _native_contact_sheets(
    audit: dict[str, Any], output: Path, label: str,
) -> list[dict[str, Any]]:
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError as error:
        raise DiminishingError("Pillow is required for contact sheets") from error
    output.mkdir(parents=True, exist_ok=True)
    results = []
    for map_index, map_name, width, height in (MAPS[0], MAPS[3]):
        for mode in ("climate", "geography"):
            lookup = {
                image["case_id"]: image for image in audit["images"]
                if image["map_size"] == map_index and image["mode"] == mode
            }
            if set(lookup) != {item[0] for item in CASES}:
                raise DiminishingError(
                    f"contact-sheet input mismatch {label}/{map_name}/{mode}"
                )
            gap = 8
            label_height = 24
            sheet = Image.new(
                "RGB", (gap + 6 * (width + gap),
                        gap + label_height + height + gap), (28, 30, 34),
            )
            draw = ImageDraw.Draw(sheet)
            font = ImageFont.load_default()
            for column, (case_id, _, _, _) in enumerate(CASES):
                x = gap + column * (width + gap)
                draw.text(
                    (x, gap), f"{label} {case_id} {mode}",
                    fill=(235, 238, 242), font=font,
                )
                with Image.open(lookup[case_id]["path"]) as source:
                    source.load()
                    if source.size != (width, height):
                        raise DiminishingError("contact sheet would resample")
                    sheet.paste(source.convert("RGB"), (x, gap + label_height))
            path = output / f"{label}_{map_name.lower()}_{mode}_native.png"
            if path.exists():
                raise DiminishingError(f"refusing to overwrite {path}")
            sheet.save(path, format="PNG")
            results.append({
                "label": label, "map_size": map_index, "map_name": map_name,
                "mode": mode, "path": str(path.resolve()),
                "sha256": _sha256_file(path), "width": sheet.width,
                "height": sheet.height, "source_scale": "1:1_no_resampling",
            })
    return results


def _run_actual_matrix(
    exe: Path, output: Path, budget: WorldBudget, *, action: str,
    seeds: Sequence[int], pair_id: str, projected: Sequence[dict[str, Any]] | None,
    emit_artifacts: bool, artifact_seed: int | None,
) -> tuple[list[dict[str, str]], dict[str, Any]]:
    planned = len(seeds) * len(MAPS) * len(CASES)
    run = _run_action(
        exe, output / "run", action, budget, planned,
        f"fixed_candidate_{action}", pair_id=pair_id,
        emit_artifacts=emit_artifacts,
    )
    _assert_run_shape(run, planned, planned, action)
    rows = run["rows"]
    sidecar = _validate_oasis_histogram_sidecar(
        rows, run["oasis_histogram_rows"],
    )
    count_lookup = sidecar["count_lookup"]
    sidecar_evidence = {
        key: value for key, value in sidecar.items() if key != "count_lookup"
    }
    expected_keys = {
        (seed, map_index, case_id)
        for seed in seeds for map_index, _, _, _ in MAPS
        for case_id, _, _, _ in CASES
    }
    row_reasons = [
        {"key": list(_actual_key(row)), "reasons": _actual_row_reasons(
            row, pair_id, action,
            sidecar_counts=count_lookup.get(_actual_key(row)),
        )}
        for row in rows
    ]
    accounting = [
        {"key": list(_actual_key(row)), **_climate_accounting(row)}
        for row in rows
    ]
    historical_refresh_witness = (
        _historical_large_refresh_witness(accounting)
        if action == "confirm" and tuple(seeds) == SCREEN_SEEDS else
        {"ok": True, "not_applicable": True, "checks": []}
    )
    climate = _evaluate_climate(rows)
    oasis = _evaluate_oasis_pair(rows, pair_id, actual=True)
    identity = (
        _compare_projected_actual(projected, rows)
        if projected is not None else {
            "ok": sidecar_evidence["ok"],
            "source": "sidecar_formula_projection_to_pre_hydrology",
            "row_count": sidecar_evidence["row_count"],
            "expected_row_count": sidecar_evidence["expected_row_count"],
        }
    )
    artifacts = (
        _audit_artifacts(output / "run", rows, artifact_seed)
        if emit_artifacts and artifact_seed is not None else
        {"ok": not emit_artifacts, "image_count": 0, "images": [],
         "failures": []}
    )
    sheets = (
        _native_contact_sheets(
            artifacts, output / "contact_sheets", action,
        ) if artifacts["ok"] and emit_artifacts else []
    )
    result = {
        "ok": (
            set(map(_actual_key, rows)) == expected_keys and
            all(not item["reasons"] for item in row_reasons) and
            all(item["ok"] for item in accounting) and
            historical_refresh_witness["ok"] and
            climate["ok"] and oasis["ok"] and
            sidecar_evidence["ok"] and
            identity["ok"] and artifacts["ok"] and
            (not emit_artifacts or len(sheets) == 4)
        ),
        "action": action, "candidate": FIXED_CANDIDATE,
        "pair_id": pair_id, "seeds": list(seeds),
        "oasis_grid_schema": OASIS_GRID_SCHEMA,
        "oasis_grid_sha256": OASIS_GRID_SHA256,
        "oasis_grid_pair_count": len(OASIS_PAIRS),
        "integer_oasis_grid_schema": INTEGER_OASIS_GRID_SCHEMA,
        "integer_oasis_grid_sha256": INTEGER_OASIS_GRID_SHA256,
        "integer_oasis_grid_pair_count": len(INTEGER_OASIS_PAIRS),
        "row_semantics": row_reasons, "climate_evaluation": climate,
        "climate_refresh_accounting": accounting,
        "historical_large_refresh_witness": historical_refresh_witness,
        "oasis_evaluation": oasis, "projection_identity": identity,
        "histogram_sidecar_validation": sidecar_evidence,
        "histogram_sidecar_path": run["oasis_histogram_path"],
        "histogram_sidecar_sha256": run["oasis_histogram_sha256"],
        "artifact_audit": artifacts, "contact_sheets": sheets,
        "csv_path": run["csv_path"], "csv_sha256": run["csv_sha256"],
    }
    _write_json_new(output / f"{action}_result.json", result)
    if not result["ok"]:
        raise DiminishingError(f"{action} actual matrix failed")
    return rows, result


def _audit_freeze(
    repo: Path, freeze_record: Path, stage: str,
) -> dict[str, Any]:
    repo = repo.resolve()
    freeze_record = freeze_record.resolve()
    record_sha = _sha256_file(freeze_record)
    production = stage == "production-binding"
    if stage not in {"prepublication", "production-binding"}:
        raise DiminishingError(f"unknown freeze-audit stage: {stage}")
    expected_record_sha = (
        EXPECTED_PRODUCTION_FREEZE_RECORD_SHA256 if production else
        EXPECTED_PREPUBLICATION_FREEZE_RECORD_SHA256
    )
    expected_count = 21 if production else 22
    if record_sha != expected_record_sha:
        raise DiminishingError(
            f"accepted {expected_count}-file freeze record hash mismatch: "
            f"{record_sha}"
        )
    record = json.loads(freeze_record.read_text(encoding="utf-8"))
    source_checks = record.get("checks")
    if not isinstance(source_checks, list):
        raise DiminishingError("accepted freeze record has no checks list")
    checks = []
    seen = set()
    for source in source_checks:
        relative = Path(str(source.get("path", "")))
        path = (repo / relative).resolve()
        try:
            path.relative_to(repo)
        except ValueError as error:
            raise DiminishingError(
                f"freeze path escapes worktree: {relative}"
            ) from error
        expected = source.get("expected_sha256")
        actual = _sha256_file(path) if path.is_file() else None
        relative_text = relative.as_posix()
        duplicate = relative_text in seen
        seen.add(relative_text)
        checks.append({
            "path": relative_text, "expected_sha256": expected,
            "recorded_actual_sha256": source.get("actual_sha256"),
            "actual_sha256": actual, "duplicate": duplicate,
            "ok": (
                source.get("ok") is True and expected is not None and
                source.get("actual_sha256") == expected and
                actual == expected and not duplicate
            ),
        })
    production_transition = None
    if production:
        source_record_text = str(record.get("source_record", ""))
        source_record = (repo / Path(source_record_text)).resolve()
        try:
            source_record.relative_to(repo)
        except ValueError as error:
            raise DiminishingError("production source freeze escapes worktree") from error
        source_sha = _sha256_file(source_record) if source_record.is_file() else None
        source = (
            json.loads(source_record.read_text(encoding="utf-8"))
            if source_record.is_file() else {}
        )
        source_checks = source.get("checks", [])
        source_map = {
            str(item.get("path", "")): item for item in source_checks
            if isinstance(item, dict)
        }
        accepted_map = {
            str(item.get("path", "")): item for item in record.get("checks", [])
            if isinstance(item, dict)
        }
        removed = record.get("explicitly_removed_from_freeze", {})
        remaining_paths = set(source_map) - {PRODUCTION_FREEZE_REMOVED_PATH}
        membership_ok = set(accepted_map) == remaining_paths
        unchanged_ok = membership_ok and all(
            accepted_map[path].get("expected_sha256") ==
            source_map[path].get("expected_sha256")
            for path in remaining_paths
        )
        removed_source = source_map.get(PRODUCTION_FREEZE_REMOVED_PATH, {})
        production_transition = {
            "source_record": str(source_record),
            "source_record_sha256": source_sha,
            "source_record_hash_ok": (
                source_sha == EXPECTED_PREPUBLICATION_FREEZE_RECORD_SHA256 and
                record.get("source_record_sha256") == source_sha
            ),
            "historical_count": record.get("historical_count"),
            "revised_count": record.get("revised_count"),
            "removed_path": removed.get("path"),
            "removed_phase_start_sha256": removed.get("phase_start_sha256"),
            "removed_reason": removed.get("reason"),
            "removed_source_expected_sha256":
                removed_source.get("expected_sha256"),
            "membership_ok": membership_ok,
            "unchanged_remaining_hashes_ok": unchanged_ok,
        }
        production_transition["ok"] = (
            production_transition["source_record_hash_ok"] and
            len(source_checks) == 22 and len(source_map) == 22 and
            source.get("expected_count") == 22 and
            source.get("actual_count") == 22 and source.get("ok") is True and
            record.get("historical_count") == 22 and
            record.get("revised_count") == 21 and
            removed.get("path") == PRODUCTION_FREEZE_REMOVED_PATH and
            removed.get("phase_start_sha256") ==
                PRODUCTION_FREEZE_REMOVED_SHA256 and
            removed.get("reason") == PRODUCTION_FREEZE_REMOVAL_REASON and
            removed_source.get("expected_sha256") ==
                PRODUCTION_FREEZE_REMOVED_SHA256 and unchanged_ok
        )
    record_identity_ok = (
        record.get("ok") is True and
        record.get("expected_count") == expected_count and
        record.get("actual_count") == expected_count and
        len(checks) == expected_count and
        (not production or bool(production_transition and
                                production_transition["ok"]))
    )
    return {
        "ok": record_identity_ok and all(item["ok"] for item in checks),
        "stage": stage,
        "accepted_freeze_record": str(freeze_record),
        "accepted_freeze_record_sha256": record_sha,
        "expected_freeze_record_sha256": expected_record_sha,
        "record_identity_ok": record_identity_ok,
        "expected_count": expected_count, "actual_count": len(checks),
        "production_transition": production_transition,
        "checks": checks,
    }


def _run_prepublication(args: argparse.Namespace, output: Path) -> dict[str, Any]:
    exe = args.exe.resolve()
    if not exe.is_file():
        raise DiminishingError(f"executable does not exist: {exe}")
    budget = WorldBudget(args.budget_ledger.resolve())
    initial_freeze = _audit_freeze(
        exe.parent, args.freeze_record.resolve(), "prepublication",
    )
    _write_json_new(output / "01_freeze_pre" / "freeze_result.json", initial_freeze)
    if not initial_freeze["ok"]:
        raise DiminishingError("accepted 22-file freeze failed before validation")
    formula = _run_formula(exe, output / "02_formula", budget)
    baseline = _run_baseline(
        exe, output / "03_baseline", args.accepted_matrix.resolve(), budget,
    )
    carriers, carrier_result = _run_carriers(
        exe, output / "04_carriers", budget,
    )
    projected, projection_result = _run_projection(
        carriers, output / "05_projection",
    )
    pair_id, _, oasis_result, selection_path = _run_oasis(
        exe, output / "06_oasis", budget, projected,
    )
    screening_rows, screening_result = _run_actual_matrix(
        exe, output / "07_screening_confirmation", budget,
        action="confirm", seeds=SCREEN_SEEDS, pair_id=pair_id,
        projected=projected, emit_artifacts=True,
        artifact_seed=SCREEN_SEEDS[0],
    )
    holdout_rows, holdout_result = _run_actual_matrix(
        exe, output / "08_holdout", budget, action="holdout",
        seeds=HOLDOUT_SEEDS, pair_id=pair_id, projected=None,
        emit_artifacts=True, artifact_seed=HOLDOUT_SEEDS[0],
    )
    final_freeze = _audit_freeze(
        exe.parent, args.freeze_record.resolve(), "prepublication",
    )
    _write_json_new(output / "09_freeze_post" / "freeze_result.json", final_freeze)
    count_ok = (
        budget.reserved == 168 and budget.rows == 168 and
        budget.generated == 168 and budget.launches == 13 and
        budget.events == 26
    )
    result = {
        "ok": (
            initial_freeze["ok"] and formula["ok"] and baseline["ok"] and
            carrier_result["ok"] and projection_result["ok"] and
            oasis_result["ok"] and screening_result["ok"] and
            holdout_result["ok"] and final_freeze["ok"] and count_ok
        ),
        "status": "READY_FOR_PRODUCTION_PUBLICATION",
        "mechanical_artifact_audit_complete": True,
        "manual_original_resolution_review_required_before_publication": True,
        "exe": str(exe), "exe_size": exe.stat().st_size,
        "exe_sha256": _sha256_file(exe), "candidate": FIXED_CANDIDATE,
        "selected_pair_id": pair_id,
        "oasis_grid_schema": OASIS_GRID_SCHEMA,
        "oasis_grid_sha256": OASIS_GRID_SHA256,
        "oasis_grid_pair_count": len(OASIS_PAIRS),
        "integer_oasis_grid_schema": INTEGER_OASIS_GRID_SCHEMA,
        "integer_oasis_grid_sha256": INTEGER_OASIS_GRID_SHA256,
        "integer_oasis_grid_pair_count": len(INTEGER_OASIS_PAIRS),
        "selected_pair_record": str(selection_path.resolve()),
        "selected_pair_record_sha256": _sha256_file(selection_path),
        "screening_csv": screening_result["csv_path"],
        "screening_csv_sha256": screening_result["csv_sha256"],
        "screening_result": str(
            (output / "07_screening_confirmation" /
             "confirm_result.json").resolve()
        ),
        "screening_result_sha256": _sha256_file(
            output / "07_screening_confirmation" / "confirm_result.json",
        ),
        "holdout_csv": holdout_result["csv_path"],
        "holdout_csv_sha256": holdout_result["csv_sha256"],
        "holdout_result": str(
            (output / "08_holdout" / "holdout_result.json").resolve()
        ),
        "holdout_result_sha256": _sha256_file(
            output / "08_holdout" / "holdout_result.json",
        ),
        "screening_row_count": len(screening_rows),
        "holdout_row_count": len(holdout_rows),
        "reserved_world_count": budget.reserved,
        "emitted_row_count": budget.rows,
        "generated_world_count": budget.generated,
        "launch_count": budget.launches, "maximum_world_count": MAX_WORLDS,
        "ledger_event_count": budget.events,
        "world_count_ok": count_ok,
    }
    result_path = output / "prepublication_result.json"
    _write_json_new(result_path, result)
    if not result["ok"]:
        raise DiminishingError("prepublication evidence incomplete")
    marker = {
        **result, "prepublication_result": str(result_path.resolve()),
        "prepublication_result_sha256": _sha256_file(result_path),
        "world_budget_ledger": str(args.budget_ledger.resolve()),
        "world_budget_ledger_sha256": _sha256_file(args.budget_ledger.resolve()),
        "freeze_record": str(args.freeze_record.resolve()),
        "freeze_record_sha256": _sha256_file(args.freeze_record.resolve()),
    }
    marker_path = output / "ready_for_production_publication.json"
    _write_json_new(marker_path, marker)
    _artifact_manifest(output, output / "artifact_manifest.json")
    return {**result, "publication_marker": str(marker_path.resolve())}


def _load_bound_file(path_text: str, expected_hash: str, label: str) -> Path:
    path = Path(path_text)
    if not path.is_file() or _sha256_file(path) != expected_hash:
        raise DiminishingError(f"{label} path/hash binding failed")
    return path


def _load_bound_matrix_result(
    path_text: str, expected_hash: str, expected_action: str, *,
    adjudicated_holdout: bool = False,
) -> dict[str, Any]:
    path = _load_bound_file(path_text, expected_hash, expected_action)
    payload = json.loads(path.read_text(encoding="utf-8"))
    audit = payload.get("artifact_audit", {})
    images = audit.get("images", [])
    sheets = payload.get("contact_sheets", [])
    paths_ok = True
    for item in images:
        image_path = Path(item.get("path", ""))
        paths_ok &= (
            item.get("ok") is True and image_path.is_file() and
            _sha256_file(image_path) == item.get("file_sha256")
        )
    for item in sheets:
        sheet_path = Path(item.get("path", ""))
        paths_ok &= sheet_path.is_file() and \
            _sha256_file(sheet_path) == item.get("sha256")
    manifest_path = Path(audit.get("manifest", ""))
    manifest_ok = manifest_path.is_file() and \
        _sha256_file(manifest_path) == audit.get("manifest_sha256")
    histogram_path = Path(payload.get("histogram_sidecar_path", ""))
    histogram_ok = (
        histogram_path.is_file() and
        _sha256_file(histogram_path) == payload.get("histogram_sidecar_sha256") and
        payload.get("histogram_sidecar_validation", {}).get("ok") is True
    )
    climate = payload.get("climate_evaluation", {})
    climate_failures = climate.get("failures", [])
    historical_mean_only = (
        adjudicated_holdout and expected_action == "holdout" and
        payload.get("ok") is False and climate.get("ok") is False and
        len(climate_failures) == 3 and
        all(item.get("type") == "mean_range" for item in climate_failures) and
        all(not item.get("reasons") for item in payload.get("row_semantics", [])) and
        all(item.get("ok") is True
            for item in payload.get("climate_refresh_accounting", [])) and
        payload.get("historical_large_refresh_witness", {}).get("ok") is True and
        payload.get("oasis_evaluation", {}).get("ok") is True and
        payload.get("projection_identity", {}).get("ok") is True
    )
    result_status_ok = payload.get("ok") is True or historical_mean_only
    if not (
        result_status_ok and payload.get("action") == expected_action and
        audit.get("ok") is True and audit.get("image_count") == 24 and
        len(images) == 24 and len(sheets) == 4 and paths_ok and manifest_ok and
        histogram_ok and
        payload.get("integer_oasis_grid_schema") == INTEGER_OASIS_GRID_SCHEMA and
        payload.get("integer_oasis_grid_sha256") == INTEGER_OASIS_GRID_SHA256 and
        payload.get("integer_oasis_grid_pair_count") == len(INTEGER_OASIS_PAIRS)
    ):
        raise DiminishingError(
            f"bound {expected_action} artifact/result evidence is incomplete"
        )
    payload["adjudicated_historical_mean_only"] = historical_mean_only
    return payload


def _load_four_seed_adjudication(
    marker_path: Path,
) -> dict[str, Any]:
    marker = json.loads(marker_path.read_text(encoding="utf-8"))
    if not (
        marker.get("ok") is True and marker.get("status") == ADJUDICATION_STATUS and
        marker.get("candidate") == FIXED_CANDIDATE and
        marker.get("selected_pair_id") == SELECTED_PAIR_ID and
        marker.get("historical_holdout_result") == HISTORICAL_HOLDOUT_STATUS and
        marker.get("superseded_rule") == SUPERSEDED_RULE and
        marker.get("replacement_rule") == REPLACEMENT_RULE and
        marker.get("all_other_gates_unchanged") is True and
        marker.get("row_count") == 96 and
        marker.get("generated_world_count") == 0
    ):
        raise DiminishingError("four-seed adjudication marker identity mismatch")

    bound_payloads: dict[str, dict[str, Any]] = {}
    for label, path_field, hash_field in (
        ("architect adjudication", "architect_user_adjudication",
         "architect_user_adjudication_sha256"),
        ("four-seed means", "four_seed_climate_means_json",
         "four_seed_climate_means_json_sha256"),
        ("unchanged gates", "unchanged_gate_recheck",
         "unchanged_gate_recheck_sha256"),
        ("oasis stage", "oasis_stage_recheck", "oasis_stage_recheck_sha256"),
        ("artifact audit", "artifact_hash_audit", "artifact_hash_audit_sha256"),
    ):
        path = _load_bound_file(
            marker.get(path_field, ""), marker.get(hash_field, ""), label,
        )
        bound_payloads[path_field] = json.loads(path.read_text(encoding="utf-8"))

    adjudication = bound_payloads["architect_user_adjudication"]
    if not (
        adjudication.get("ok") is True and
        adjudication.get("candidate_id") == FIXED_CANDIDATE["candidate_id"] and
        adjudication.get("selected_pair_id") == SELECTED_PAIR_ID and
        tuple(adjudication.get("seeds", ())) == FOUR_SEED_SET and
        adjudication.get("historical_holdout_result") ==
            HISTORICAL_HOLDOUT_STATUS and
        adjudication.get("superseded_rule") == SUPERSEDED_RULE and
        adjudication.get("replacement_rule") == REPLACEMENT_RULE and
        adjudication.get("all_other_gates_unchanged") is True and
        adjudication.get("source_failure", {}).get("path") ==
            marker.get("historical_failure") and
        adjudication.get("source_failure", {}).get("sha256") ==
            marker.get("historical_failure_sha256") and
        adjudication.get("source_holdout_result", {}).get("path") ==
            marker.get("holdout_result") and
        adjudication.get("source_holdout_result", {}).get("sha256") ==
            marker.get("holdout_result_sha256")
    ):
        raise DiminishingError("architect/user adjudication contract mismatch")

    means_path = _load_bound_file(
        marker.get("four_seed_climate_means_csv", ""),
        marker.get("four_seed_climate_means_csv_sha256", ""),
        "four-seed means CSV",
    )
    means_rows, _ = _load_csv_required(
        means_path,
        ("map_size", "map_name", "configuration", "sum_fraction",
         "mean_numerator", "mean_denominator", "mean_fraction", "pass"),
    )
    expected_mean_keys = {
        (map_index, case_id) for map_index, _, _, _ in MAPS
        for case_id, _, _, _ in CASES
    }
    observed_mean_keys = {
        (int(row["map_size"]), row["configuration"]) for row in means_rows
    }
    means_arithmetic_ok = len(means_rows) == len(observed_mean_keys) == 24
    for row in means_rows:
        samples = [
            Fraction(row[f"seed_{seed}_fraction"]) for seed in FOUR_SEED_SET
        ]
        exact_mean = sum(samples, Fraction(0, 1)) / len(FOUR_SEED_SET)
        means_arithmetic_ok &= (
            Fraction(row["sum_fraction"]) == sum(samples, Fraction(0, 1)) and
            Fraction(row["mean_fraction"]) == exact_mean and
            int(row["mean_numerator"]) == exact_mean.numerator and
            int(row["mean_denominator"]) == exact_mean.denominator and
            row["pass"] == "1"
        )
    means = bound_payloads["four_seed_climate_means_json"]
    json_mean_rows = {
        (int(row["map_size"]), row["configuration"]): row
        for row in means.get("rows", ())
    }
    for row in means_rows:
        key = (int(row["map_size"]), row["configuration"])
        json_row = json_mean_rows.get(key, {})
        means_arithmetic_ok &= (
            json_row.get("equal_weight_mean", {}).get("fraction") ==
                row["mean_fraction"] and
            [sample.get("seed") for sample in json_row.get("samples", ())] ==
                list(FOUR_SEED_SET) and
            [sample.get("share", {}).get("fraction")
             for sample in json_row.get("samples", ())] ==
                [row[f"seed_{seed}_fraction"] for seed in FOUR_SEED_SET]
        )
    if not (
        means.get("ok") is True and means.get("row_count") == 24 and
        means.get("arithmetic") == "fractions.Fraction_equal_weight_by_world" and
        tuple(means.get("seeds", ())) == FOUR_SEED_SET and
        len(means.get("rows", ())) == 24 and
        all(row.get("pass") is True for row in means.get("rows", ())) and
        means.get("response_and_true_desert", {}).get("ok") is True and
        observed_mean_keys == expected_mean_keys and
        set(json_mean_rows) == expected_mean_keys and means_arithmetic_ok
    ):
        raise DiminishingError("four-seed exact-mean evidence mismatch")

    unchanged = bound_payloads["unchanged_gate_recheck"]
    for stage in ("confirm", "holdout"):
        stage_result = unchanged.get(stage, {})
        other_gate_identity = stage_result.get("recorded_other_gate_identity", {})
        if not (
            stage_result.get("ok") is True and
            stage_result.get("historical_mean_range_failure_identity_ok") is True and
            stage_result.get("refresh_accounting_ok") is True and
            stage_result.get("histogram_sidecar_validation", {}).get("ok") is True and
            stage_result.get("oasis_evaluation", {}).get("ok") is True and
            stage_result.get("artifact_audit", {}).get("ok") is True and
            not stage_result.get("row_semantic_failures") and
            not stage_result.get("unchanged_climate_failures") and
            other_gate_identity and all(other_gate_identity.values())
        ):
            raise DiminishingError(f"unchanged {stage} gate recheck mismatch")
    if not (
        unchanged.get("ok") is True and
        unchanged.get("screen_projection_identity", {}).get("ok") is True and
        unchanged.get("four_seed_response_and_true_desert", {}).get("ok") is True and
        unchanged["confirm"].get("historical_result_ok") is True and
        unchanged["confirm"].get("historical_climate_ok") is True and
        unchanged["holdout"].get("historical_result_ok") is False and
        unchanged["holdout"].get("historical_climate_ok") is False
    ):
        raise DiminishingError("unchanged-gate adjudication identity mismatch")

    oasis = bound_payloads["oasis_stage_recheck"]
    if not (
        oasis.get("ok") is True and
        oasis.get("contract") == "screening_and_holdout_evaluated_independently" and
        all(oasis.get(stage, {}).get("ok") is True and
            oasis.get(stage, {}).get("pair_id") == SELECTED_PAIR_ID
            for stage in ("confirm", "holdout"))
    ):
        raise DiminishingError("independent screening/holdout oasis gate mismatch")

    artifact_audit = bound_payloads["artifact_hash_audit"]
    bindings_ok = all(
        item.get("ok") is True and
        Path(item.get("path", "")).is_file() and
        _sha256_file(Path(item["path"])) == item.get("sha256")
        for item in artifact_audit.get("bindings", ())
    )
    artifact_images_ok = True
    for stage in ("confirm_artifacts", "holdout_artifacts"):
        audit = artifact_audit.get(stage, {})
        artifact_images_ok &= (
            audit.get("ok") is True and audit.get("image_count") == 24 and
            len(audit.get("images", ())) == 24
        )
        for item in audit.get("images", ()):
            path = Path(item.get("path", ""))
            artifact_images_ok &= (
                item.get("ok") is True and path.is_file() and
                _sha256_file(path) == item.get("file_sha256")
            )
    if not (
        artifact_audit.get("ok") is True and bindings_ok and artifact_images_ok and
        len(artifact_audit.get("matrix_shapes", ())) == 4 and
        all(item.get("ok") is True
            for item in artifact_audit.get("matrix_shapes", ())) and
        artifact_audit.get("confirm_manifest", {}).get("ok") is True and
        artifact_audit.get("holdout_manifest", {}).get("ok") is True and
        artifact_audit.get("top_manifest", {}).get("ok") is True
    ):
        raise DiminishingError("historical artifact/hash audit mismatch")

    historical_failure_path = _load_bound_file(
        marker.get("historical_failure", ""),
        marker.get("historical_failure_sha256", ""),
        "historical holdout failure",
    )
    historical_failure = json.loads(
        historical_failure_path.read_text(encoding="utf-8"),
    )
    if not (
        historical_failure.get("ok") is False and
        historical_failure.get("status") == "FAILED_STOP" and
        historical_failure.get("stage") == "prepublication" and
        historical_failure.get("message") == "holdout actual matrix failed"
    ):
        raise DiminishingError("historical holdout failure was not preserved")
    return {
        "marker": marker, "adjudication": adjudication, "means": means,
        "unchanged": unchanged, "oasis": oasis,
        "artifact_audit": artifact_audit,
        "historical_failure": historical_failure,
    }


def _compare_production_binding(
    expected_rows: Sequence[dict[str, str]],
    production_rows: Sequence[dict[str, str]],
) -> dict[str, Any]:
    expected = {_actual_key(row): row for row in expected_rows}
    observed = {_actual_key(row): row for row in production_rows}
    mismatches = []
    if set(expected) != set(observed):
        mismatches.append({
            "type": "key_set", "expected": sorted(expected),
            "actual": sorted(observed),
        })
    for key in sorted(set(expected) & set(observed)):
        differences = {
            field: {"validated": expected[key][field],
                    "production": observed[key][field]}
            for field in BINDING_FIELDS
            if expected[key][field] != observed[key][field]
        }
        if differences:
            mismatches.append({
                "type": "row", "key": list(key), "differences": differences,
            })
    return {
        "ok": not mismatches and len(expected_rows) == len(production_rows) == 96,
        "expected_count": len(expected_rows),
        "production_count": len(production_rows), "mismatches": mismatches,
    }


def _compare_histogram_binding(
    expected_rows: Sequence[dict[str, str]],
    production_rows: Sequence[dict[str, str]],
) -> dict[str, Any]:
    expected = {_actual_key(row): row for row in expected_rows}
    observed = {_actual_key(row): row for row in production_rows}
    mismatches = []
    if set(expected) != set(observed):
        mismatches.append({
            "type": "key_set", "expected": sorted(expected),
            "actual": sorted(observed),
        })
    for key in sorted(set(expected) & set(observed)):
        differences = {
            field: {
                "validated": expected[key][field],
                "production": observed[key][field],
            }
            for field in OASIS_HISTOGRAM_BINDING_FIELDS
            if expected[key][field] != observed[key][field]
        }
        if differences:
            mismatches.append({
                "type": "row", "key": list(key), "differences": differences,
            })
    return {
        "ok": not mismatches and len(expected_rows) ==
            len(production_rows) == 96,
        "expected_count": len(expected_rows),
        "production_count": len(production_rows), "mismatches": mismatches,
    }


def _audit_production_artifact_binding(
    directory: Path, rows: Sequence[dict[str, str]],
    historical_audit: dict[str, Any],
) -> dict[str, Any]:
    manifest = directory / "aridity_diminishing_artifacts_manifest.csv"
    manifest_rows, _ = _load_csv_required(manifest, ARTIFACT_REQUIRED_COLUMNS)
    expected_keys = {
        (seed, map_index, case_id)
        for seed in (SCREEN_SEEDS[0], HOLDOUT_SEEDS[0])
        for map_index in (0, 3) for case_id, _, _, _ in CASES
    }
    row_lookup = {_actual_key(row): row for row in rows}
    historical_images = {}
    for stage in ("confirm_artifacts", "holdout_artifacts"):
        for item in historical_audit.get(stage, {}).get("images", ()):
            key = (
                int(item["seed"]), int(item["map_size"]), item["case_id"],
                item["mode"],
            )
            historical_images[key] = item
    expected_image_keys = {
        (*key, mode) for key in expected_keys
        for mode in ("geography", "climate")
    }
    observed = set()
    images = []
    failures = []
    for item in manifest_rows:
        key = (int(item["seed"]), int(item["map_size"]), item["case_id"])
        observed.add(key)
        actual_row = row_lookup.get(key)
        if actual_row is None:
            failures.append({"type": "manifest_key", "key": list(key)})
            continue
        for field in (
            "candidate_id", "pair_id", "physical_hash", "climate_input_hash",
            "oasis_count", "map_name", "width", "height",
        ):
            if item[field] != actual_row[field]:
                failures.append({
                    "type": "manifest_binding", "key": list(key),
                    "field": field, "manifest": item[field],
                    "production": actual_row[field],
                })
        for mode in ("geography", "climate"):
            path = projection.legacy._resolve_artifact(
                directory, item[f"{mode}_file"],
            )
            audit = _audit_bmp(
                path, int(item["width"]), int(item["height"]),
                item[f"{mode}_pixel_hash"],
            )
            historical = historical_images.get((*key, mode))
            record = {
                "seed": key[0], "map_size": key[1],
                "map_name": item["map_name"], "case_id": key[2],
                "mode": mode, "path": str(path.resolve()),
                "file_sha256": _sha256_file(path), **audit,
                "historical_path": historical.get("path") if historical else None,
                "historical_file_sha256": (
                    historical.get("file_sha256") if historical else None
                ),
                "historical_pixel_hash": (
                    historical.get("pixel_hash") if historical else None
                ),
            }
            record["historical_identity_ok"] = bool(
                historical and historical.get("ok") is True and
                record["file_sha256"] == historical.get("file_sha256") and
                record.get("pixel_hash") == historical.get("pixel_hash") and
                record.get("width") == historical.get("width") and
                record.get("height") == historical.get("height")
            )
            images.append(record)
            if not audit["ok"] or not record["historical_identity_ok"]:
                failures.append({"type": "image_identity", "record": record})
    if observed != expected_keys:
        failures.append({
            "type": "manifest_keys", "expected": sorted(expected_keys),
            "actual": sorted(observed),
        })
    if set(historical_images) != expected_image_keys:
        failures.append({
            "type": "historical_image_keys",
            "expected": sorted(expected_image_keys),
            "actual": sorted(historical_images),
        })
    return {
        "ok": not failures and len(manifest_rows) == 24 and len(images) == 48,
        "manifest": str(manifest.resolve()),
        "manifest_sha256": _sha256_file(manifest),
        "manifest_row_count": len(manifest_rows), "image_count": len(images),
        "expected_seeds": [SCREEN_SEEDS[0], HOLDOUT_SEEDS[0]],
        "images": images, "failures": failures,
    }


def _run_production_binding(
    args: argparse.Namespace, output: Path,
) -> dict[str, Any]:
    exe = args.exe.resolve()
    marker_path = args.prepublication_marker.resolve()
    if not exe.is_file() or not marker_path.is_file():
        raise DiminishingError("production executable/marker missing")
    adjudication_evidence = _load_four_seed_adjudication(marker_path)
    marker = adjudication_evidence["marker"]
    freeze_record = args.freeze_record.resolve()
    if not freeze_record.is_file():
        raise DiminishingError("accepted production freeze record missing")
    initial_freeze = _audit_freeze(
        exe.parent, freeze_record, "production-binding",
    )
    _write_json_new(output / "freeze_pre" / "freeze_result.json",
                    initial_freeze)
    if not initial_freeze["ok"]:
        raise DiminishingError(
            "accepted 21-file production freeze failed before validation"
        )
    selection_path = _load_bound_file(
        marker["selected_pair_record"], marker["selected_pair_record_sha256"],
        "selected pair",
    )
    selection = json.loads(selection_path.read_text(encoding="utf-8"))
    if (
        selection.get("oasis_grid_schema") != OASIS_GRID_SCHEMA or
        selection.get("oasis_grid_sha256") != OASIS_GRID_SHA256 or
        selection.get("oasis_grid_pair_count") != len(OASIS_PAIRS) or
        selection.get("integer_oasis_grid_schema") != INTEGER_OASIS_GRID_SCHEMA or
        selection.get("integer_oasis_grid_sha256") != INTEGER_OASIS_GRID_SHA256 or
        selection.get("integer_oasis_grid_pair_count") != len(INTEGER_OASIS_PAIRS)
    ):
        raise DiminishingError("selected-pair oasis-grid identity mismatch")
    pair_id = selection["selected_oasis_pair"]["pair_id"]
    ranking = selection.get("survivor_ranking", [])
    if (
        selection.get("survivor_count") != len(ranking) or not ranking or
        ranking[0].get("pair_id") != pair_id or ranking[0].get("rank") != 1
    ):
        raise DiminishingError("selected-pair exact ranking binding mismatch")
    _load_bound_file(
        selection["integer_oasis_grid_result"],
        selection["integer_oasis_grid_result_sha256"],
        "integer oasis-grid result",
    )
    _load_bound_file(
        selection["integer_oasis_grid_csv"],
        selection["integer_oasis_grid_csv_sha256"],
        "integer oasis-grid CSV",
    )
    _load_bound_file(
        selection["histogram_sidecar"],
        selection["histogram_sidecar_sha256"],
        "oasis histogram sidecar",
    )
    if pair_id != marker["selected_pair_id"] or pair_id != SELECTED_PAIR_ID:
        raise DiminishingError("selected pair changed after holdout")
    screening_result = _load_bound_matrix_result(
        marker["screening_result"], marker["screening_result_sha256"],
        "confirm",
    )
    holdout_result = _load_bound_matrix_result(
        marker["holdout_result"], marker["holdout_result_sha256"],
        "holdout", adjudicated_holdout=True,
    )
    screening_path = _load_bound_file(
        marker["screening_csv"], marker["screening_csv_sha256"], "screening CSV",
    )
    holdout_path = _load_bound_file(
        marker["holdout_csv"], marker["holdout_csv_sha256"], "holdout CSV",
    )
    screening_histogram_path = _load_bound_file(
        screening_result["histogram_sidecar_path"],
        screening_result["histogram_sidecar_sha256"],
        "screening oasis histogram",
    )
    holdout_histogram_path = _load_bound_file(
        holdout_result["histogram_sidecar_path"],
        holdout_result["histogram_sidecar_sha256"],
        "holdout oasis histogram",
    )
    screening_rows, _ = _load_csv_required(screening_path, ACTUAL_REQUIRED_COLUMNS)
    holdout_rows, _ = _load_csv_required(holdout_path, ACTUAL_REQUIRED_COLUMNS)
    screening_histogram_rows, _ = _load_csv_required(
        screening_histogram_path, OASIS_HISTOGRAM_REQUIRED_COLUMNS,
    )
    holdout_histogram_rows, _ = _load_csv_required(
        holdout_histogram_path, OASIS_HISTOGRAM_REQUIRED_COLUMNS,
    )
    if (
        screening_result.get("csv_sha256") != marker["screening_csv_sha256"] or
        holdout_result.get("csv_sha256") != marker["holdout_csv_sha256"]
    ):
        raise DiminishingError("bound matrix result/CSV hash mismatch")
    expected_rows = screening_rows + holdout_rows
    if len(expected_rows) != 96:
        raise DiminishingError("validated screening/holdout rows are not 96")
    budget = WorldBudget(args.budget_ledger.resolve(), marker_path)
    run = _run_action(
        exe, output / "run", "production", budget, 96,
        "no_ack_production_binding", ack_value=None,
        include_candidate=False, emit_artifacts=True,
    )
    _assert_run_shape(run, 96, 96, "production binding")
    sidecar = _validate_oasis_histogram_sidecar(
        run["rows"], run["oasis_histogram_rows"],
    )
    count_lookup = sidecar["count_lookup"]
    sidecar_evidence = {
        key: value for key, value in sidecar.items() if key != "count_lookup"
    }
    row_reasons = [
        {"key": list(_actual_key(row)), "reasons": _actual_row_reasons(
            row, pair_id, "production", require_override=False,
            sidecar_counts=count_lookup.get(_actual_key(row)),
        )}
        for row in run["rows"]
    ]
    identity = _compare_production_binding(expected_rows, run["rows"])
    histogram_identity = _compare_histogram_binding(
        screening_histogram_rows + holdout_histogram_rows,
        run["oasis_histogram_rows"],
    )
    artifact_identity = _audit_production_artifact_binding(
        output / "run", run["rows"], adjudication_evidence["artifact_audit"],
    )
    final_freeze = _audit_freeze(
        exe.parent, args.freeze_record.resolve(), "production-binding",
    )
    _write_json_new(output / "freeze_post" / "freeze_result.json", final_freeze)
    count_ok = (
        budget.reserved == MAX_WORLDS and budget.rows == MAX_WORLDS and
        budget.generated == MAX_WORLDS and budget.launches == 14 and
        budget.events == 29
    )
    result = {
        "ok": (
            initial_freeze["ok"] and
            all(not item["reasons"] for item in row_reasons) and
            sidecar_evidence["ok"] and identity["ok"] and
            histogram_identity["ok"] and artifact_identity["ok"] and
            final_freeze["ok"] and count_ok
        ),
        "status": "DIMINISHING_RESPONSE_PRODUCTION_BINDING_PASS",
        "candidate": FIXED_CANDIDATE, "selected_pair_id": pair_id,
        "oasis_grid_schema": OASIS_GRID_SCHEMA,
        "oasis_grid_sha256": OASIS_GRID_SHA256,
        "oasis_grid_pair_count": len(OASIS_PAIRS),
        "integer_oasis_grid_schema": INTEGER_OASIS_GRID_SCHEMA,
        "integer_oasis_grid_sha256": INTEGER_OASIS_GRID_SHA256,
        "integer_oasis_grid_pair_count": len(INTEGER_OASIS_PAIRS),
        "exe": str(exe), "exe_size": exe.stat().st_size,
        "exe_sha256": _sha256_file(exe), "row_semantics": row_reasons,
        "production_identity": identity, "csv_path": run["csv_path"],
        "histogram_production_identity": histogram_identity,
        "csv_sha256": run["csv_sha256"],
        "histogram_sidecar_validation": sidecar_evidence,
        "histogram_sidecar_path": run["oasis_histogram_path"],
        "histogram_sidecar_sha256": run["oasis_histogram_sha256"],
        "production_artifact_identity": artifact_identity,
        "initial_freeze": initial_freeze,
        "final_freeze": final_freeze,
        "historical_holdout_result": HISTORICAL_HOLDOUT_STATUS,
        "superseded_rule": SUPERSEDED_RULE,
        "replacement_rule": REPLACEMENT_RULE,
        "adjudication_marker": str(marker_path),
        "adjudication_marker_sha256": _sha256_file(marker_path),
        "reserved_world_count": budget.reserved,
        "emitted_row_count": budget.rows,
        "generated_world_count": budget.generated,
        "launch_count": budget.launches, "maximum_world_count": MAX_WORLDS,
        "ledger_event_count": budget.events,
        "world_count_ok": count_ok,
        "world_budget_ledger": str(args.budget_ledger.resolve()),
        "world_budget_ledger_sha256": _sha256_file(
            args.budget_ledger.resolve(),
        ),
        "prepublication_marker": str(marker_path),
        "prepublication_marker_sha256": _sha256_file(marker_path),
    }
    _write_json_new(output / "production_binding_result.json", result)
    _artifact_manifest(output, output / "artifact_manifest.json")
    if not result["ok"]:
        raise DiminishingError("production binding failed")
    return result


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--stage", choices=("prepublication", "production-binding"),
        required=True,
    )
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--freeze-record", type=Path, required=True)
    parser.add_argument("--budget-ledger", type=Path, required=True)
    parser.add_argument("--accepted-matrix", type=Path)
    parser.add_argument("--prepublication-marker", type=Path)
    return parser


def _arguments_valid(args: argparse.Namespace) -> bool:
    if args.stage == "prepublication":
        return args.accepted_matrix is not None and \
            args.prepublication_marker is None
    return args.prepublication_marker is not None and \
        args.accepted_matrix is None


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    if not _arguments_valid(args):
        print("stage-specific argument contract failed", file=sys.stderr)
        return 2
    output = args.output.resolve()
    if output.exists():
        print(f"refusing to overwrite output directory: {output}", file=sys.stderr)
        return 2
    output.mkdir(parents=True)
    try:
        if args.stage == "prepublication":
            result = _run_prepublication(args, output)
        else:
            result = _run_production_binding(args, output)
        _write_json_new(output / "stage_result.json", {
            "stage": args.stage, "ok": True, "status": result["status"],
            "generated_world_count": result["generated_world_count"],
            "maximum_world_count": MAX_WORLDS,
        })
        _artifact_manifest(output, output / "artifact_manifest_final.json")
        return 0
    except Exception as error:
        failure = {
            "stage": args.stage, "ok": False, "status": "FAILED_STOP",
            "exception_type": type(error).__name__, "message": str(error),
            "exe": str(args.exe.resolve()),
            "exe_sha256": (
                _sha256_file(args.exe.resolve())
                if args.exe.resolve().is_file() else None
            ),
            "budget_ledger": str(args.budget_ledger.resolve()),
            "budget_ledger_sha256": (
                _sha256_file(args.budget_ledger.resolve())
                if args.budget_ledger.resolve().is_file() else None
            ),
        }
        try:
            freeze = _audit_freeze(
                args.exe.resolve().parent, args.freeze_record.resolve(),
                args.stage,
            )
            _write_json_new(output / "failure_freeze" / "freeze_result.json", freeze)
            failure["post_failure_freeze"] = {
                "ok": freeze["ok"],
                "path": str((output / "failure_freeze" /
                             "freeze_result.json").resolve()),
                "sha256": _sha256_file(
                    output / "failure_freeze" / "freeze_result.json",
                ),
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
            print(
                f"failed to preserve failure evidence: {evidence_error}",
                file=sys.stderr,
            )
        print(f"{type(error).__name__}: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
