#!/usr/bin/env python3
"""Resumable offline orchestration for worldgen climate-calibration workers."""

from __future__ import annotations

import argparse
import concurrent.futures
import csv
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import uuid

import worldgen_climate_calibration_contract as contract


RUNNER_SCHEMA_VERSION = "worldgen-climate-calibration-runner-v1"
REPEAT_PROJECTION_SCHEMA_VERSION = "worldgen-climate-repeat-projection-v1"
PROCESSOR_TRANSACTION_SCHEMA_VERSION = "worldgen-climate-processor-transaction-v1"
RAW_IMPORT_SCHEMA_VERSION = "worldgen-climate-raw-import-v1"
BASE_COLUMNS = (
    "schema_version",
    "run_id",
    "source_head",
    "source_manifest_hash",
    "executable_hash",
    "scripts_manifest_hash",
    "config_manifest_hash",
    "seed",
    "seed_kind",
    "shard_config_start",
    "shard_config_count",
    "shard_row_index",
    "config_index",
    "ocean",
    "continent",
    "relief",
    "vegetation",
    "bias_forest",
    "bias_desert",
    "bias_mountain",
    "bias_wetland",
    "moisture",
    "drought",
    "random_seed",
    "config_multiplicity",
    "map_size",
    "map_size_name",
    "width",
    "height",
    "success",
    "failure_stage",
    "failure_reason",
    "world_diagnostics_valid",
    "land_mask_diagnostics_valid",
    "moisture_diagnostics_valid",
    "river_diagnostics_valid",
    "metrics_valid",
)
WORLD_COLUMNS = (
    "world_elevation_ms",
    "world_mountain_ms",
    "world_climate_ms",
    "world_hydrology_ms",
    "world_classification_ms",
    "world_commit_ms",
    "world_total_ms",
    "world_context_bytes",
    "world_hydrology_bytes",
    "world_staged_path_bytes",
    "world_peak_bytes",
    "physical_hash",
    "world_land_tiles",
    "world_ocean_tiles",
    "world_mountain_tiles",
    "world_river_tiles",
    "world_river_segments_required",
    "world_river_segments_copied",
)
ATTEMPT_COLUMNS = (
    "attempt_id",
    "attempt_stage",
    "attempt_last_failure_stage",
    "attempt_last_failure_reason",
    "attempt_active",
    "attempt_success",
    "attempt_world_committed",
    "attempt_snapshot_published",
    "attempt_snapshot_attempts",
    "attempt_deferred_snapshot_pending",
    "attempt_deferred_snapshot_attempts",
    "attempt_deferred_snapshot_succeeded",
    "attempt_prewarm_attempted",
    "attempt_prewarm_succeeded",
    "attempt_prewarm_attempts",
    "attempt_lazy_fallback_required",
    "attempt_target_width",
    "attempt_target_height",
    "attempt_target_land_tiles",
    "attempt_actual_land_tiles",
    "attempt_target_ocean_tiles",
    "attempt_actual_ocean_tiles",
    "attempt_river_channel_tiles",
    "attempt_river_paths_required",
    "attempt_river_paths_copied",
    "attempt_river_path_capacity",
    "attempt_context_allocated_bytes",
    "attempt_context_allocation_failure_field",
    "attempt_staged_allocation_bytes",
    "attempt_peak_allocation_bytes",
    "attempt_elapsed_ms",
    "attempt_previous_world_generated",
    "attempt_previous_physical_revision",
    "attempt_previous_physical_hash",
)
LAND_COLUMNS = (
    "land_target_tiles",
    "land_initial_tiles",
    "land_final_tiles",
    "land_threshold_elevation",
    "land_threshold_candidates",
    "land_threshold_selected",
    "land_frontier_added",
    "land_frontier_removed",
    "land_frontier_resolutions",
    "land_forced_frontier_seeds",
    "land_coastal_lowland_tiles",
    "land_components",
    "land_water_components",
    "land_lattice_cells",
    "land_comb_cells",
    "land_mesh_cells",
    "land_tendril_cells",
    "land_threshold_lattice_cells",
    "land_threshold_comb_cells",
    "land_threshold_mesh_cells",
    "land_threshold_tendril_cells",
    "land_topology_errors",
    "land_target_drift",
    "land_mask_hash",
    "land_failure",
)
MOISTURE_DIAGNOSTIC_COLUMNS = (
    "moisture_diag_tile_count",
    "moisture_diag_climate_seed",
    "moisture_diag_solved_land_tiles",
    "moisture_diag_cycle_tiles",
    "moisture_diag_cycle_count",
    "moisture_diag_cycle_iterations",
    "moisture_diag_max_cycle_iterations",
    "moisture_diag_unconverged_cycles",
    "moisture_diag_ocean_reached_land_tiles",
    "moisture_diag_ocean_reached_beyond_20",
    "moisture_diag_max_ocean_chain_length",
    "moisture_diag_diffusion_passes",
    "moisture_diag_advection_rounds",
    "moisture_diag_subtile_advection_samples",
    "moisture_diag_subtile_lateral_samples",
    "moisture_diag_advection_weight_total",
    "moisture_diag_lateral_mix_weight_total",
    "moisture_diag_orographic_precipitation_total",
    "moisture_diag_lee_drying_total",
)
RIVER_COLUMNS = (
    "river_transient_bytes",
    "river_workspace_bytes_required",
    "river_workspace_bytes_allocated",
    "river_workspace_allocation_failure_field",
    "river_workspace_allocation_errors",
    "river_land_cells",
    "river_topological_cells",
    "river_depression_cells",
    "river_lake_candidate_components",
    "river_lake_qualified_components",
    "river_lake_rejected_components",
    "river_lake_pruned_cells",
    "river_lake_rejected_cells",
    "river_lake_reject_area",
    "river_lake_reject_depth",
    "river_lake_reject_deep_cells",
    "river_lake_reject_catchment",
    "river_lake_reject_support",
    "river_lake_reject_shape",
    "river_lake_final_invalid_components",
    "river_lake_cells",
    "river_closed_basins",
    "river_salt_lakes",
    "river_channel_cells",
    "river_sources",
    "river_confluences",
    "river_mouths",
    "river_deltas",
    "river_distributaries",
    "river_distributary_allocation_errors",
    "river_segment_allocation_errors",
    "river_ordinary_segments",
    "river_invalid_receivers",
    "river_inland_dead_ends",
    "river_cycle_errors",
    "river_flow_conservation_errors",
    "river_width_regressions",
    "river_order_errors",
    "river_duplicate_edges",
    "river_crossing_repairs",
    "river_crossing_errors",
    "river_receiver_edges",
    "river_flat_receiver_edges",
    "river_receiver_lower_index_edges",
    "river_flat_receiver_lower_index_edges",
    "river_max_same_direction_run",
    "river_max_flat_same_direction_run",
    "river_legacy_paths_required",
    "river_legacy_paths_truncated",
    "river_channel_threshold",
    "river_max_flow",
    "river_max_order",
    "river_max_width",
    "river_bounded_influence_visits",
)
METRIC_COLUMNS = (
    "tile_count",
    "land_mask_tiles",
    "terrestrial_tiles",
    "ocean_tiles",
    "lake_tiles",
    "land_mask_nonbinary",
    "invalid_geography",
    "invalid_climate",
    "invalid_ecology",
    "invalid_continuous_value",
)
GEOGRAPHY_NAMES = (
    "ocean", "coast", "plain", "hill", "mountain", "plateau", "basin",
    "canyon", "volcano", "lake", "bay", "delta", "wetland", "oasis", "island",
)
CLIMATE_NAMES = (
    "tropical_rainforest", "tropical_monsoon", "tropical_savanna", "desert",
    "semi_arid", "mediterranean", "oceanic", "temperate_monsoon",
    "continental", "subarctic", "tundra", "ice_cap", "alpine",
    "highland_plateau",
)
ECOLOGY_NAMES = (
    "none", "forest", "rainforest", "grassland", "desert", "tundra",
    "swamp", "bamboo", "mangrove",
)
DISPLAY_NAMES = (
    "icefield", "tundra", "tropical_rainforest", "monsoon", "desert",
    "forest", "temperate_grassland", "other_transition",
)
DISTRIBUTION_SUFFIXES = ("count", "sum", "min", "max", "mean", "p10", "p50", "p90")
EXPECTED_COLUMNS = (
    BASE_COLUMNS
    + WORLD_COLUMNS
    + ATTEMPT_COLUMNS
    + LAND_COLUMNS
    + MOISTURE_DIAGNOSTIC_COLUMNS
    + RIVER_COLUMNS
    + METRIC_COLUMNS
    + tuple(f"river_receiver_direction_{index}" for index in range(8))
    + tuple(f"river_flat_direction_{index}" for index in range(8))
    + tuple(f"geo_{name}" for name in GEOGRAPHY_NAMES)
    + tuple(f"climate_{name}" for name in CLIMATE_NAMES)
    + tuple(f"ecology_{name}" for name in ECOLOGY_NAMES)
    + tuple(f"display_{name}" for name in DISPLAY_NAMES)
    + tuple(f"temperature_{suffix}" for suffix in DISTRIBUTION_SUFFIXES)
    + tuple(f"moisture_{suffix}" for suffix in DISTRIBUTION_SUFFIXES)
    + tuple(f"precipitation_{suffix}" for suffix in DISTRIBUTION_SUFFIXES)
)
EXPECTED_COLUMN_COUNT = 283
if len(EXPECTED_COLUMNS) != EXPECTED_COLUMN_COUNT:
    raise RuntimeError("Python/C climate calibration schema column count drift")
VOLATILE_SUFFIXES = ("_ms", "_milliseconds", "_seconds")
VOLATILE_EXACT: tuple[str, ...] = ()
EXCLUDED_DIRTY_PREFIXES = ("build/validation/", "logs/")
EXCLUDED_DIRTY_PATHS = ("world_sim.exe",)
EXCLUDED_DIRTY_COMPONENTS = ("__pycache__",)
EXCLUDED_DIRTY_SUFFIXES = (".pyc", ".pyo")
GIB = 1024**3


class CalibrationRunError(RuntimeError):
    """Raised when evidence cannot be safely resumed or published."""


class CalibrationProofError(CalibrationRunError):
    """Raised when a required interruption or repeat proof fails."""


class CalibrationSelfTestError(CalibrationRunError):
    """Raised when runner self-tests fail."""


class InsufficientDiskError(CalibrationRunError):
    """Raised for a recoverable live free-space shortfall."""


class ProcessorResumeBlockedError(CalibrationRunError):
    """Raised when an exact or ambiguously identified processor may be alive."""


class _InjectedProcessorInterruption(RuntimeError):
    """Self-test-only simulated loss of the orchestration process."""


@dataclass(frozen=True)
class ShardSpec:
    seed_kind: str
    seed: int
    shard_index: int
    config_start: int
    config_count: int

    @property
    def world_count(self) -> int:
        return self.config_count * len(contract.MAP_SIZES)

    @property
    def key(self) -> str:
        return (
            f"{self.seed_kind}:seed={self.seed}:shard={self.shard_index}:"
            f"start={self.config_start}:count={self.config_count}"
        )


@dataclass
class ShardValidation:
    row_count: int
    failure_count: int
    csv_sha256: str
    projection_sha256: str
    header_sha256: str
    per_size: dict[str, dict[str, float | int]]


@dataclass(frozen=True)
class RawOrigin:
    root: Path
    binding: dict[str, object]
    import_manifest: dict[str, object] | None
    import_sha256: str

    @property
    def imported(self) -> bool:
        return self.import_manifest is not None

    def resolve(self, relative: str) -> Path:
        logical = Path(relative)
        if logical.is_absolute():
            raise CalibrationRunError(f"raw path must be relative: {relative}")
        candidate = (self.root / logical).resolve()
        if not candidate.is_relative_to(self.root):
            raise CalibrationRunError(
                f"raw path escapes its source attempt: {relative}")
        return candidate


def canonical_json_bytes(value: object) -> bytes:
    return contract.canonical_json_bytes(value)


def sha256_file(path: Path) -> str:
    return contract.sha256_file(path)


def sha256_bytes(data: bytes) -> str:
    return contract.sha256_bytes(data)


def _fsync_directory(path: Path) -> None:
    if os.name == "nt":
        return
    descriptor = os.open(path, os.O_RDONLY)
    try:
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def _atomic_create(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.parent / f".t_{uuid.uuid4().hex[:12]}"
    with temporary.open("xb") as handle:
        handle.write(data)
        handle.flush()
        os.fsync(handle.fileno())
    if path.exists():
        raise CalibrationRunError(f"refusing to overwrite evidence: {path}")
    os.replace(temporary, path)
    _fsync_directory(path.parent)


def _atomic_replace(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.parent / f".t_{uuid.uuid4().hex[:12]}"
    with temporary.open("xb") as handle:
        handle.write(data)
        handle.flush()
        os.fsync(handle.fileno())
    os.replace(temporary, path)
    _fsync_directory(path.parent)


def _create_or_verify(path: Path, data: bytes) -> None:
    if path.exists():
        if path.read_bytes() != data:
            raise CalibrationRunError(f"existing evidence differs: {path}")
        return
    _atomic_create(path, data)


def _run_git(repo: Path, *arguments: str, check: bool = True) -> subprocess.CompletedProcess[bytes]:
    result = subprocess.run(
        ["git", *arguments],
        cwd=repo,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if check and result.returncode != 0:
        raise CalibrationRunError(
            f"git {' '.join(arguments)} failed ({result.returncode}): "
            f"{result.stderr.decode('utf-8', 'replace').strip()}"
        )
    return result


def _repo_root() -> Path:
    result = _run_git(Path.cwd(), "rev-parse", "--show-toplevel")
    return Path(result.stdout.decode("utf-8").strip()).resolve()


def _decode_path(raw: bytes) -> str:
    return raw.decode("utf-8", "surrogateescape").replace("\\", "/")


def _dirty_entries(repo: Path) -> list[dict[str, object]]:
    status = _run_git(
        repo, "status", "--porcelain=v1", "-z", "--untracked-files=all"
    ).stdout
    items = status.split(b"\0")
    entries: list[dict[str, object]] = []
    index = 0
    while index < len(items):
        item = items[index]
        index += 1
        if not item:
            continue
        if len(item) < 4:
            raise CalibrationRunError("cannot parse NUL-delimited git status")
        code = item[:2].decode("ascii", "replace")
        path = _decode_path(item[3:])
        source_path = None
        if "R" in code or "C" in code:
            if index >= len(items) or not items[index]:
                raise CalibrationRunError("rename/copy status is missing source path")
            source_path = _decode_path(items[index])
            index += 1
        if path in EXCLUDED_DIRTY_PATHS or any(
            path.startswith(prefix) for prefix in EXCLUDED_DIRTY_PREFIXES
        ) or any(
            component in EXCLUDED_DIRTY_COMPONENTS
            for component in Path(path).parts
        ) or path.endswith(EXCLUDED_DIRTY_SUFFIXES):
            continue
        absolute = repo / Path(path)
        record: dict[str, object] = {"status": code, "path": path}
        if source_path is not None:
            record["source_path"] = source_path
        if absolute.is_file():
            record["bytes"] = absolute.stat().st_size
            record["sha256"] = sha256_file(absolute)
        else:
            record["bytes"] = None
            record["sha256"] = None
        entries.append(record)
    entries.sort(key=lambda entry: (str(entry["path"]), str(entry["status"])))
    return entries


def _source_manifest(repo: Path) -> dict[str, object]:
    staged = _run_git(repo, "diff", "--cached", "--quiet", check=False)
    if staged.returncode not in (0, 1):
        raise CalibrationRunError("cannot audit staged-file state")
    if staged.returncode == 1:
        raise CalibrationRunError("staged files are forbidden for calibration freeze")
    head = _run_git(repo, "rev-parse", "HEAD").stdout.decode("ascii").strip()
    entries = _dirty_entries(repo)
    payload = {
        "base_head": head,
        "excluded_generated_prefixes": list(EXCLUDED_DIRTY_PREFIXES),
        "excluded_generated_paths": list(EXCLUDED_DIRTY_PATHS),
        "excluded_generated_components": list(EXCLUDED_DIRTY_COMPONENTS),
        "excluded_generated_suffixes": list(EXCLUDED_DIRTY_SUFFIXES),
        "entries": entries,
    }
    return {
        **payload,
        "manifest_hash": sha256_bytes(canonical_json_bytes(payload)),
    }


def _scripts_manifest(repo: Path) -> dict[str, object]:
    required = (
        "worldgen_climate_calibration_contract.py",
        "run_worldgen_climate_calibration.py",
        "process_worldgen_climate_calibration.py",
    )
    paths = [repo / "tools" / name for name in required]
    missing = [path.name for path in paths if not path.is_file()]
    if missing:
        raise CalibrationRunError(f"missing calibration Python scripts: {missing}")
    entries = [
        {
            "path": path.relative_to(repo).as_posix(),
            "bytes": path.stat().st_size,
            "sha256": sha256_file(path),
        }
        for path in paths
    ]
    payload = {"scripts": entries}
    return {**payload, "manifest_hash": sha256_bytes(canonical_json_bytes(payload))}


def _schema_manifest() -> dict[str, object]:
    payload = {
        "row_schema_version": contract.ROW_SCHEMA_VERSION,
        "expected_column_count": EXPECTED_COLUMN_COUNT,
        "expected_columns": list(EXPECTED_COLUMNS),
        "volatile_repeat_columns": {
            "exact": list(VOLATILE_EXACT),
            "suffixes": list(VOLATILE_SUFFIXES),
        },
    }
    return {**payload, "manifest_hash": sha256_bytes(canonical_json_bytes(payload))}


def _binding_candidate(
    repo: Path, run_root: Path, executable: Path
) -> tuple[dict[str, object], dict[str, object], dict[str, object], dict[str, object]]:
    source = _source_manifest(repo)
    scripts = _scripts_manifest(repo)
    schema = _schema_manifest()
    config_manifest_path = run_root / "config_manifest.json"
    config_manifest_hash = sha256_file(config_manifest_path)
    if not executable.is_file():
        raise CalibrationRunError(f"calibration executable is missing: {executable}")
    payload = {
        "runner_schema_version": RUNNER_SCHEMA_VERSION,
        "base_head": source["base_head"],
        "source_manifest_hash": source["manifest_hash"],
        "executable_path": str(executable),
        "executable_bytes": executable.stat().st_size,
        "executable_hash": sha256_file(executable),
        "scripts_manifest_hash": scripts["manifest_hash"],
        "config_manifest_hash": config_manifest_hash,
        "schema_manifest_hash": schema["manifest_hash"],
        "row_schema_version": contract.ROW_SCHEMA_VERSION,
    }
    binding = {
        **payload,
        "run_id": sha256_bytes(canonical_json_bytes(payload))[:24],
    }
    return binding, source, scripts, schema


def _freeze_or_verify_binding(
    repo: Path, run_root: Path, executable: Path
) -> dict[str, object]:
    binding, source, scripts, schema = _binding_candidate(
        repo, run_root, executable
    )
    binding_dir = run_root / "binding"
    binding_path = binding_dir / "binding.json"
    if binding_path.exists():
        try:
            frozen = json.loads(binding_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            raise CalibrationRunError(f"cannot read frozen binding: {exc}") from exc
        if frozen != binding:
            raise CalibrationRunError(
                "source, executable, scripts, schema, or config binding changed; "
                "preserve this attempt and start a new numbered attempt"
            )
        expected = {
            "source_manifest.json": source,
            "scripts_manifest.json": scripts,
            "schema_manifest.json": schema,
        }
        for name, value in expected.items():
            path = binding_dir / name
            if not path.is_file() or path.read_bytes() != canonical_json_bytes(value):
                raise CalibrationRunError(f"frozen binding component mismatch: {path}")
        return binding
    binding_dir.mkdir(parents=True, exist_ok=True)
    _atomic_create(binding_dir / "source_manifest.json", canonical_json_bytes(source))
    _atomic_create(binding_dir / "scripts_manifest.json", canonical_json_bytes(scripts))
    _atomic_create(binding_dir / "schema_manifest.json", canonical_json_bytes(schema))
    _atomic_create(binding_path, canonical_json_bytes(binding))
    return binding


def _assert_live_binding(
    repo: Path, run_root: Path, executable: Path, frozen: dict[str, object]
) -> None:
    current, _source, _scripts, _schema = _binding_candidate(
        repo, run_root, executable
    )
    if current != frozen:
        raise CalibrationRunError(
            "live source/executable/script/schema binding changed during execution"
        )


def _read_json_object(path: Path, description: str) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise CalibrationRunError(
            f"invalid {description} {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise CalibrationRunError(f"{description} must be an object: {path}")
    return value


def _load_binding_bundle(
    attempt: Path,
) -> tuple[
    dict[str, object],
    dict[str, object],
    dict[str, object],
    dict[str, object],
]:
    binding_dir = attempt / "binding"
    binding_path = binding_dir / "binding.json"
    binding = _read_json_object(binding_path, "binding")
    payload = dict(binding)
    run_id = payload.pop("run_id", None)
    if run_id != sha256_bytes(canonical_json_bytes(payload))[:24]:
        raise CalibrationRunError(f"binding run_id mismatch: {binding_path}")
    components: dict[str, dict[str, object]] = {}
    for filename, binding_field in (
        ("source_manifest.json", "source_manifest_hash"),
        ("scripts_manifest.json", "scripts_manifest_hash"),
        ("schema_manifest.json", "schema_manifest_hash"),
    ):
        path = binding_dir / filename
        component = _read_json_object(path, filename)
        component_payload = dict(component)
        recorded_hash = component_payload.pop("manifest_hash", None)
        calculated_hash = sha256_bytes(canonical_json_bytes(component_payload))
        if (
            recorded_hash != calculated_hash
            or binding.get(binding_field) != calculated_hash
            or path.read_bytes() != canonical_json_bytes(component)
        ):
            raise CalibrationRunError(f"binding component mismatch: {path}")
        components[filename] = component
    config_path = attempt / "config_manifest.json"
    if (
        not config_path.is_file()
        or sha256_file(config_path) != binding.get("config_manifest_hash")
    ):
        raise CalibrationRunError(
            f"binding config-manifest mismatch: {config_path}")
    contract.materialize_contract(attempt)
    executable = Path(str(binding.get("executable_path", ""))).resolve()
    if (
        not executable.is_file()
        or executable.stat().st_size != binding.get("executable_bytes")
        or sha256_file(executable) != binding.get("executable_hash")
    ):
        raise CalibrationRunError(
            f"bound source executable is unavailable or changed: {executable}")
    return (
        binding,
        components["source_manifest.json"],
        components["scripts_manifest.json"],
        components["schema_manifest.json"],
    )


def _source_projection(
    manifest: dict[str, object],
) -> list[dict[str, object]]:
    allowed = {
        "tools/run_worldgen_climate_calibration.py",
        "tools/process_worldgen_climate_calibration.py",
    }
    entries = manifest.get("entries")
    if not isinstance(entries, list):
        raise CalibrationRunError("source manifest lacks a dirty-source inventory")
    projected = []
    for entry in entries:
        if not isinstance(entry, dict):
            raise CalibrationRunError("source manifest has a malformed entry")
        paths = {str(entry.get("path", "")), str(entry.get("source_path", ""))}
        if paths.intersection(allowed):
            continue
        projected.append(entry)
    return projected


def _script_entry(
    manifest: dict[str, object], path: str
) -> dict[str, object]:
    scripts = manifest.get("scripts")
    if not isinstance(scripts, list):
        raise CalibrationRunError("scripts manifest lacks an inventory")
    matches = [
        entry for entry in scripts
        if isinstance(entry, dict) and entry.get("path") == path
    ]
    if len(matches) != 1:
        raise CalibrationRunError(f"scripts manifest lacks exactly one {path}")
    return matches[0]


def _verify_raw_reuse_compatibility(
    source_binding: dict[str, object],
    source_source: dict[str, object],
    source_scripts: dict[str, object],
    source_schema: dict[str, object],
    processing_binding: dict[str, object],
    processing_source: dict[str, object],
    processing_scripts: dict[str, object],
    processing_schema: dict[str, object],
) -> dict[str, object]:
    invariant_fields = (
        "base_head",
        "executable_bytes",
        "executable_hash",
        "config_manifest_hash",
        "schema_manifest_hash",
        "row_schema_version",
    )
    mismatches = [
        field for field in invariant_fields
        if source_binding.get(field) != processing_binding.get(field)
    ]
    if mismatches:
        raise CalibrationRunError(
            f"raw reuse compatibility mismatch: {mismatches}")
    if source_schema != processing_schema:
        raise CalibrationRunError("raw reuse changed the row-schema manifest")
    if _source_projection(source_source) != _source_projection(processing_source):
        raise CalibrationRunError(
            "raw reuse changed source outside the runner/processor tools")
    contract_script = "tools/worldgen_climate_calibration_contract.py"
    if _script_entry(source_scripts, contract_script) != _script_entry(
            processing_scripts, contract_script):
        raise CalibrationRunError("raw reuse changed the enumeration contract")
    if source_binding.get("run_id") == processing_binding.get("run_id"):
        raise CalibrationRunError(
            "processor-only recovery requires a fresh processing binding")
    return {
        "status": "PASS",
        "unchanged_binding_fields": list(invariant_fields),
        "allowed_source_changes": [
            "tools/run_worldgen_climate_calibration.py",
            "tools/process_worldgen_climate_calibration.py",
        ],
        "source_projection_sha256": sha256_bytes(
            canonical_json_bytes(_source_projection(source_source))),
        "processing_projection_sha256": sha256_bytes(
            canonical_json_bytes(_source_projection(processing_source))),
        "contract_script_sha256": _script_entry(
            source_scripts, contract_script)["sha256"],
    }


def _source_failure_lineage(source_attempt: Path) -> dict[str, object]:
    state_path = source_attempt / "attempt_state.json"
    state = _read_json_object(state_path, "source attempt state")
    if (
        state.get("terminal") is not True
        or state.get("status") != "FAIL"
        or state.get("mode") != "full"
        or "processor failed" not in str(state.get("error", ""))
    ):
        raise CalibrationRunError(
            "raw import source is not a terminal full-processor failure")
    relative = Path(str(state.get("failure_evidence_path", "")))
    failure_path = (source_attempt / relative).resolve()
    if (
        relative.is_absolute()
        or not failure_path.is_relative_to(source_attempt)
        or not failure_path.is_file()
        or sha256_file(failure_path) != state.get("failure_evidence_sha256")
    ):
        raise CalibrationRunError("source failure evidence hash/path mismatch")
    failure = _read_json_object(failure_path, "source failure evidence")
    if failure.get("mode") != "full" or "processor failed" not in str(
            failure.get("error", "")):
        raise CalibrationRunError("source failure is not processor-only")
    child_failures = sorted(
        (source_attempt / "processor_recovery" / "f").rglob(
            "child_failure.json"))
    verified_children = []
    for path in child_failures:
        child = _read_json_object(path, "processor child failure")
        if (
            child.get("phase") == "full"
            and child.get("status") == "FAIL"
            and int(child.get("exit_code", 0)) != 0
        ):
            verified_children.append({
                "path": path.relative_to(source_attempt).as_posix(),
                "sha256": sha256_file(path),
            })
    if not verified_children:
        raise CalibrationRunError(
            "source attempt lacks a verified full-processor child failure")
    return {
        "attempt_state_path": state_path.relative_to(source_attempt).as_posix(),
        "attempt_state_sha256": sha256_file(state_path),
        "failure_path": failure_path.relative_to(source_attempt).as_posix(),
        "failure_sha256": sha256_file(failure_path),
        "processor_child_failures": verified_children,
    }


def _config_rows(run_root: Path) -> dict[int, dict[str, int]]:
    rows: dict[int, dict[str, int]] = {}
    path = run_root / "effective_config_multiplicities.csv"
    with path.open("r", encoding="utf-8", newline="") as handle:
        for raw in csv.DictReader(handle):
            row = {key: int(value) for key, value in raw.items()}
            index = row["config_index"]
            if index in rows:
                raise CalibrationRunError(f"duplicate effective config index: {index}")
            rows[index] = row
    if set(rows) != set(range(contract.EFFECTIVE_CONFIG_COUNT)):
        raise CalibrationRunError("effective config table is incomplete")
    return rows


def _all_shards(seed_kind: str, seeds: tuple[int, ...]) -> list[ShardSpec]:
    shard_count = math.ceil(
        contract.EFFECTIVE_CONFIG_COUNT / contract.SHARD_CONFIG_COUNT
    )
    result = []
    for seed in seeds:
        for shard_index in range(shard_count):
            start = shard_index * contract.SHARD_CONFIG_COUNT
            count = min(
                contract.SHARD_CONFIG_COUNT,
                contract.EFFECTIVE_CONFIG_COUNT - start,
            )
            result.append(
                ShardSpec(seed_kind, seed, shard_index, start, count)
            )
    return result


def _shard_paths(run_root: Path, spec: ShardSpec) -> tuple[Path, Path]:
    directory = run_root / "raw" / spec.seed_kind / f"seed_{spec.seed}"
    stem = f"shard_{spec.shard_index:04d}"
    return directory / f"{stem}.csv", directory / f"{stem}.done"


def _hidden_process_options() -> dict[str, object]:
    if os.name != "nt":
        return {}
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    return {
        "creationflags": getattr(subprocess, "CREATE_NO_WINDOW", 0),
        "startupinfo": startup,
    }


def build_worker_command(
    executable: Path,
    output_path: Path,
    spec: ShardSpec,
    binding: dict[str, object],
) -> list[str]:
    """Single adapter for the hidden C worker's intentionally narrow CLI."""
    return [
        str(executable),
        "--worldgen-climate-calibration-worker",
        "--output",
        str(output_path),
        "--run-id",
        str(binding["run_id"]),
        "--source-head",
        str(binding["base_head"]),
        "--source-manifest-hash",
        str(binding["source_manifest_hash"]),
        "--executable-hash",
        str(binding["executable_hash"]),
        "--scripts-manifest-hash",
        str(binding["scripts_manifest_hash"]),
        "--config-manifest-hash",
        str(binding["config_manifest_hash"]),
        "--schema-version",
        str(binding["row_schema_version"]),
        "--seed",
        str(spec.seed),
        "--seed-kind",
        spec.seed_kind,
        "--config-start",
        str(spec.config_start),
        "--config-count",
        str(spec.config_count),
    ]


def _parse_nonnegative_float(value: str, field: str) -> float:
    try:
        parsed = float(value)
    except ValueError as exc:
        raise CalibrationRunError(f"{field} is not numeric: {value!r}") from exc
    if not math.isfinite(parsed) or parsed < 0:
        raise CalibrationRunError(f"{field} must be finite and nonnegative")
    return parsed


def _parse_int(value: str, field: str) -> int:
    try:
        return int(value, 10)
    except ValueError as exc:
        raise CalibrationRunError(f"{field} is not an integer: {value!r}") from exc


def _is_volatile_column(name: str) -> bool:
    return name in VOLATILE_EXACT or name.endswith(VOLATILE_SUFFIXES)


def validate_shard_csv(
    path: Path,
    spec: ShardSpec,
    binding: dict[str, object],
    configs: dict[int, dict[str, int]],
) -> ShardValidation:
    if not path.is_file():
        raise CalibrationRunError(f"worker did not create shard output: {path}")
    raw_lines = path.read_bytes().splitlines(keepends=True)
    if not raw_lines:
        raise CalibrationRunError(f"empty shard output: {path}")
    try:
        header = next(
            csv.reader([raw_lines[0].decode("utf-8-sig").rstrip("\r\n")])
        )
    except (UnicodeDecodeError, csv.Error) as exc:
        raise CalibrationRunError(f"invalid UTF-8 CSV header: {path}: {exc}") from exc
    if len(header) != len(set(header)):
        raise CalibrationRunError("worker CSV contains duplicate column names")
    if tuple(header) != EXPECTED_COLUMNS:
        missing = sorted(set(EXPECTED_COLUMNS) - set(header))
        extra = sorted(set(header) - set(EXPECTED_COLUMNS))
        first_mismatch = next(
            (
                index
                for index, (observed, expected) in enumerate(
                    zip(header, EXPECTED_COLUMNS)
                )
                if observed != expected
            ),
            min(len(header), len(EXPECTED_COLUMNS)),
        )
        raise CalibrationRunError(
            "worker CSV schema/order mismatch; "
            f"first_index={first_mismatch} missing={missing} extra={extra}"
        )
    header_sha = sha256_bytes((",".join(header) + "\n").encode("utf-8"))
    projection_fields = [name for name in header if not _is_volatile_column(name)]
    projection_digest = hashlib.sha256()
    projection_digest.update(canonical_json_bytes(projection_fields))
    expected_sizes = {
        name: (size_index, width, height)
        for size_index, (name, width, height) in enumerate(contract.MAP_SIZES)
    }
    expected_keys = {
        (config_index, size_name)
        for config_index in range(
            spec.config_start, spec.config_start + spec.config_count
        )
        for size_name in expected_sizes
    }
    observed_keys: set[tuple[int, str]] = set()
    per_size: dict[str, dict[str, float | int]] = {
        name: {"worlds": 0, "row_bytes": 0, "total_ms": 0.0}
        for name in expected_sizes
    }
    failures = 0
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames != header:
            raise CalibrationRunError("worker CSV header changed while reading")
        for row_number, row in enumerate(reader, start=2):
            if None in row:
                raise CalibrationRunError(f"extra CSV fields at row {row_number}")
            if any(value is None for value in row.values()):
                raise CalibrationRunError(f"missing CSV field at row {row_number}")
            expected_identity = {
                "run_id": binding["run_id"],
                "source_head": binding["base_head"],
                "source_manifest_hash": binding["source_manifest_hash"],
                "executable_hash": binding["executable_hash"],
                "scripts_manifest_hash": binding["scripts_manifest_hash"],
                "config_manifest_hash": binding["config_manifest_hash"],
                "schema_version": binding["row_schema_version"],
                "seed": spec.seed,
                "seed_kind": spec.seed_kind,
            }
            for field, expected in expected_identity.items():
                if row[field] != str(expected):
                    raise CalibrationRunError(
                        f"identity mismatch at row {row_number}, field {field}"
                    )
            index = _parse_int(row["config_index"], "config_index")
            if not spec.config_start <= index < spec.config_start + spec.config_count:
                raise CalibrationRunError(f"config index outside shard at row {row_number}")
            if _parse_int(
                row["shard_config_start"], "shard_config_start"
            ) != spec.config_start or _parse_int(
                row["shard_config_count"], "shard_config_count"
            ) != spec.config_count:
                raise CalibrationRunError(f"shard bounds mismatch at row {row_number}")
            config = configs[index]
            fixed_values = {
                "ocean": 50,
                "continent": 50,
                "relief": 50,
                "vegetation": 50,
                "bias_mountain": 50,
                "bias_wetland": 50,
                "random_seed": 0,
            }
            for field, expected in fixed_values.items():
                if _parse_int(row[field], field) != expected:
                    raise CalibrationRunError(
                        f"fixed config mismatch at row {row_number}, field {field}"
                    )
            for field in (
                "bias_forest",
                "bias_desert",
                "moisture",
                "drought",
                "config_multiplicity",
            ):
                if _parse_int(row[field], field) != config[field]:
                    raise CalibrationRunError(
                        f"config contract mismatch at row {row_number}, field {field}"
                    )
            size = row["map_size_name"]
            if size not in expected_sizes:
                raise CalibrationRunError(
                    f"unknown map size at row {row_number}: {size}"
                )
            size_index, width, height = expected_sizes[size]
            if _parse_int(row["map_size"], "map_size") != size_index:
                raise CalibrationRunError(f"map-size index mismatch at row {row_number}")
            expected_row_index = (index - spec.config_start) * 4 + size_index
            if _parse_int(
                row["shard_row_index"], "shard_row_index"
            ) != expected_row_index or expected_row_index != row_number - 2:
                raise CalibrationRunError(f"shard row order mismatch at row {row_number}")
            if _parse_int(row["width"], "width") != width or _parse_int(
                row["height"], "height"
            ) != height:
                raise CalibrationRunError(f"dimension mismatch at row {row_number}")
            key = (index, size)
            if key in observed_keys:
                raise CalibrationRunError(f"duplicate world row at row {row_number}")
            observed_keys.add(key)
            success = _parse_int(row["success"], "success")
            if success not in (0, 1):
                raise CalibrationRunError("success must be 0 or 1")
            if success == 0:
                failures += 1
                if not row["failure_stage"] or not row["failure_reason"]:
                    raise CalibrationRunError("failed row lacks stage or reason")
            elif (
                len(row["physical_hash"]) != 16
                or any(character not in "0123456789abcdefABCDEF"
                       for character in row["physical_hash"])
                or int(row["physical_hash"], 16) == 0
            ):
                raise CalibrationRunError("successful row has invalid physical hash")
            if success and any(
                _parse_int(row[field], field) != 1
                for field in (
                    "world_diagnostics_valid",
                    "land_mask_diagnostics_valid",
                    "moisture_diagnostics_valid",
                    "river_diagnostics_valid",
                    "metrics_valid",
                    "attempt_success",
                )
            ):
                raise CalibrationRunError(
                    f"successful row has invalid diagnostics at row {row_number}"
                )
            if any(
                _parse_int(row[field], field) != 0
                for field in (
                    "world_commit_ms",
                    "attempt_active",
                    "attempt_world_committed",
                    "attempt_snapshot_published",
                    "attempt_snapshot_attempts",
                    "attempt_deferred_snapshot_pending",
                    "attempt_deferred_snapshot_attempts",
                    "attempt_deferred_snapshot_succeeded",
                    "attempt_prewarm_attempted",
                    "attempt_prewarm_succeeded",
                    "attempt_prewarm_attempts",
                    "attempt_lazy_fallback_required",
                )
            ):
                raise CalibrationRunError(
                    f"forbidden commit/snapshot/prewarm state at row {row_number}"
                )
            _parse_nonnegative_float(row["world_total_ms"], "world_total_ms")
            if _parse_int(
                row["attempt_peak_allocation_bytes"],
                "attempt_peak_allocation_bytes",
            ) < 0:
                raise CalibrationRunError(
                    "attempt_peak_allocation_bytes must be nonnegative"
                )
            for field in (
                "tile_count",
                "land_mask_tiles",
                "terrestrial_tiles",
                "ocean_tiles",
                "lake_tiles",
            ):
                if _parse_int(row[field], field) < 0:
                    raise CalibrationRunError(f"{field} must be nonnegative")
            if success:
                tile_count = _parse_int(row["tile_count"], "tile_count")
                land_count = _parse_int(
                    row["land_mask_tiles"], "land_mask_tiles"
                )
                terrestrial_count = _parse_int(
                    row["terrestrial_tiles"], "terrestrial_tiles"
                )
                ocean_count = _parse_int(row["ocean_tiles"], "ocean_tiles")
                lake_count = _parse_int(row["lake_tiles"], "lake_tiles")
                if (
                    tile_count != width * height
                    or land_count + ocean_count != tile_count
                    or terrestrial_count + lake_count != land_count
                ):
                    raise CalibrationRunError(
                        f"tile accounting mismatch at row {row_number}"
                    )
                count_sets = (
                    ("geo", GEOGRAPHY_NAMES),
                    ("climate", CLIMATE_NAMES),
                    ("ecology", ECOLOGY_NAMES),
                )
                for prefix, names in count_sets:
                    total = sum(
                        _parse_int(row[f"{prefix}_{name}"], f"{prefix}_{name}")
                        for name in names
                    )
                    if total != tile_count:
                        raise CalibrationRunError(
                            f"{prefix} counts mismatch at row {row_number}"
                        )
                display_total = sum(
                    _parse_int(row[f"display_{name}"], f"display_{name}")
                    for name in DISPLAY_NAMES
                )
                if display_total != terrestrial_count:
                    raise CalibrationRunError(
                        f"display counts mismatch at row {row_number}"
                    )
                for prefix in ("temperature", "moisture", "precipitation"):
                    count = _parse_int(
                        row[f"{prefix}_count"], f"{prefix}_count"
                    )
                    if count != terrestrial_count:
                        raise CalibrationRunError(
                            f"{prefix} distribution count mismatch "
                            f"at row {row_number}"
                        )
            for name in header:
                if _is_volatile_column(name) and row[name]:
                    _parse_nonnegative_float(row[name], name)
            projection_digest.update(
                canonical_json_bytes([row[field] for field in projection_fields])
            )
            size_stats = per_size[size]
            size_stats["worlds"] = int(size_stats["worlds"]) + 1
            size_stats["total_ms"] = float(size_stats["total_ms"]) + float(
                row["world_total_ms"]
            )

    if observed_keys != expected_keys:
        missing_keys = sorted(expected_keys - observed_keys)[:10]
        extra_keys = sorted(observed_keys - expected_keys)[:10]
        raise CalibrationRunError(
            f"shard row-set mismatch; missing={missing_keys}, extra={extra_keys}"
        )
    size_column = header.index("map_size_name")
    for raw_line in raw_lines[1:]:
        try:
            values = next(
                csv.reader([raw_line.decode("utf-8").rstrip("\r\n")])
            )
        except (UnicodeDecodeError, csv.Error) as exc:
            raise CalibrationRunError(f"invalid CSV row encoding: {exc}") from exc
        if len(values) != len(header):
            raise CalibrationRunError("CSV byte-accounting parse changed field count")
        per_size[values[size_column]]["row_bytes"] = int(
            per_size[values[size_column]]["row_bytes"]
        ) + len(raw_line)
    return ShardValidation(
        row_count=len(observed_keys),
        failure_count=failures,
        csv_sha256=sha256_file(path),
        projection_sha256=projection_digest.hexdigest().upper(),
        header_sha256=header_sha,
        per_size=per_size,
    )


def _done_payload(
    spec: ShardSpec,
    binding: dict[str, object],
    validation: ShardValidation,
) -> dict[str, object]:
    return {
        "status": "PASS",
        "shard": {
            "seed_kind": spec.seed_kind,
            "seed": spec.seed,
            "shard_index": spec.shard_index,
            "config_start": spec.config_start,
            "config_count": spec.config_count,
            "world_count": spec.world_count,
        },
        "binding_run_id": binding["run_id"],
        "row_count": validation.row_count,
        "failure_count": validation.failure_count,
        "csv_sha256": validation.csv_sha256,
        "projection_sha256": validation.projection_sha256,
        "header_sha256": validation.header_sha256,
        "per_size": validation.per_size,
    }


def _verify_done(
    csv_path: Path,
    done_path: Path,
    spec: ShardSpec,
    binding: dict[str, object],
    configs: dict[int, dict[str, int]],
) -> ShardValidation | None:
    csv_exists = csv_path.exists()
    done_exists = done_path.exists()
    if not csv_exists and not done_exists:
        return None
    if csv_exists != done_exists:
        raise CalibrationRunError(
            f"incomplete published shard must remain as failed-attempt evidence: "
            f"{csv_path}, {done_path}"
        )
    try:
        recorded = json.loads(done_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise CalibrationRunError(f"invalid .done marker: {done_path}: {exc}") from exc
    validation = validate_shard_csv(csv_path, spec, binding, configs)
    if recorded != _done_payload(spec, binding, validation):
        raise CalibrationRunError(f".done marker does not match shard: {done_path}")
    return validation


def _publish_shard(
    temporary: Path,
    csv_path: Path,
    done_path: Path,
    spec: ShardSpec,
    binding: dict[str, object],
    validation: ShardValidation,
) -> None:
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    if csv_path.exists() or done_path.exists():
        raise CalibrationRunError(f"refusing to overwrite published shard: {csv_path}")
    os.replace(temporary, csv_path)
    _fsync_directory(csv_path.parent)
    _atomic_create(
        done_path, canonical_json_bytes(_done_payload(spec, binding, validation))
    )


def _failure_signatures(path: Path) -> list[tuple[str, str, str, str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        return sorted(
            (
                row["config_index"],
                row["map_size_name"],
                row["failure_stage"],
                row["failure_reason"],
            )
            for row in csv.DictReader(handle)
            if row["success"] == "0"
        )


def _confirm_worker_failures(
    repo: Path,
    executable: Path,
    spec: ShardSpec,
    binding: dict[str, object],
    configs: dict[int, dict[str, int]],
    staging: Path,
    first_output: Path,
) -> dict[str, object]:
    confirmation = staging / "f"
    confirmation.mkdir()
    output = confirmation / "o.csv.tmp"
    command = build_worker_command(executable, output, spec, binding)
    with (confirmation / "stdout.txt").open("xb") as stdout, (
        confirmation / "stderr.txt"
    ).open("xb") as stderr:
        result = subprocess.run(
            command,
            cwd=repo,
            stdin=subprocess.DEVNULL,
            stdout=stdout,
            stderr=stderr,
            check=False,
            **_hidden_process_options(),
        )
        stdout.flush()
        stderr.flush()
        os.fsync(stdout.fileno())
        os.fsync(stderr.fileno())
    repeat_validation = None
    repeat_signatures: list[tuple[str, str, str, str]] = []
    if result.returncode in (0, 3) and output.is_file():
        repeat_validation = validate_shard_csv(
            output, spec, binding, configs
        )
        repeat_signatures = _failure_signatures(output)
    first_signatures = _failure_signatures(first_output)
    repeated = bool(first_signatures) and first_signatures == repeat_signatures
    payload = {
        "confirmation_exit_code": result.returncode,
        "first_failure_signatures": first_signatures,
        "repeat_failure_signatures": repeat_signatures,
        "repeatable_valid_config_failure": repeated,
        "repeat_csv_sha256": (
            repeat_validation.csv_sha256 if repeat_validation else None
        ),
        "repeat_projection_sha256": (
            repeat_validation.projection_sha256 if repeat_validation else None
        ),
    }
    _atomic_create(staging / "failure_confirmation.json", canonical_json_bytes(payload))
    return payload


def _execute_worker(
    repo: Path,
    run_root: Path,
    executable: Path,
    spec: ShardSpec,
    binding: dict[str, object],
    configs: dict[int, dict[str, int]],
    purpose: str = "dataset",
) -> tuple[Path, Path, ShardValidation]:
    staging = (
        run_root
        / "w"
        / uuid.uuid4().hex[:12]
    )
    staging.mkdir(parents=True)
    temporary = staging / "shard.csv.tmp"
    stdout_path = staging / "stdout.txt"
    stderr_path = staging / "stderr.txt"
    command = build_worker_command(executable, temporary, spec, binding)
    with stdout_path.open("xb") as stdout, stderr_path.open("xb") as stderr:
        result = subprocess.run(
            command,
            cwd=repo,
            stdin=subprocess.DEVNULL,
            stdout=stdout,
            stderr=stderr,
            check=False,
            **_hidden_process_options(),
        )
        stdout.flush()
        stderr.flush()
        os.fsync(stdout.fileno())
        os.fsync(stderr.fileno())
    _atomic_create(
        staging / "command.json",
        canonical_json_bytes(
            {"command": command, "exit_code": result.returncode, "spec": spec.key}
        ),
    )
    if result.returncode not in (0, 3):
        raise CalibrationRunError(
            f"worker failed for {spec.key} with exit code {result.returncode}; "
            f"evidence: {staging}"
        )
    validation = validate_shard_csv(temporary, spec, binding, configs)
    _atomic_create(
        staging / "validation.json",
        canonical_json_bytes(_done_payload(spec, binding, validation)),
    )
    if result.returncode == 3 or validation.failure_count:
        confirmation = _confirm_worker_failures(
            repo,
            executable,
            spec,
            binding,
            configs,
            staging,
            temporary,
        )
        raise CalibrationRunError(
            "worker reported "
            f"{'repeatable' if confirmation['repeatable_valid_config_failure'] else 'non-repeatable'} "
            "valid-config generation failures for "
            f"{spec.key}; rows={validation.failure_count}; evidence: {staging}"
        )
    return staging, temporary, validation


def _run_and_publish(
    repo: Path,
    run_root: Path,
    executable: Path,
    spec: ShardSpec,
    binding: dict[str, object],
    configs: dict[int, dict[str, int]],
) -> tuple[ShardSpec, ShardValidation]:
    _assert_live_binding(repo, run_root, executable, binding)
    csv_path, done_path = _shard_paths(run_root, spec)
    existing = _verify_done(csv_path, done_path, spec, binding, configs)
    if existing is not None:
        return spec, existing
    _staging, temporary, validation = _execute_worker(
        repo, run_root, executable, spec, binding, configs
    )
    _assert_live_binding(repo, run_root, executable, binding)
    _publish_shard(
        temporary, csv_path, done_path, spec, binding, validation
    )
    return spec, validation


def _proof_manifest_valid(path: Path, binding: dict[str, object]) -> bool:
    if not path.is_file():
        return False
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return False
    if value.get("status") != "PASS" or value.get("binding_run_id") != binding["run_id"]:
        return False
    for artifact in value.get("artifacts", []):
        candidate = path.parent / artifact["path"]
        if not candidate.is_file() or sha256_file(candidate) != artifact["sha256"]:
            return False
    return True


def _terminate_owned_process_after_output(
    process: subprocess.Popen[bytes],
    output: Path,
    *,
    observation_timeout_seconds: float = 30.0,
    termination_timeout_seconds: float = 30.0,
) -> dict[str, object]:
    started = time.monotonic()
    deadline = started + observation_timeout_seconds
    observed_bytes = 0
    while time.monotonic() < deadline:
        returncode = process.poll()
        if returncode is not None:
            raise CalibrationRunError(
                "worker completed before the forced interruption; "
                f"exit_code={returncode}"
            )
        try:
            observed_bytes = output.stat().st_size
        except FileNotFoundError:
            observed_bytes = 0
        if observed_bytes > 0:
            break
        time.sleep(0.05)
    if observed_bytes <= 0:
        process.terminate()
        try:
            process.wait(timeout=termination_timeout_seconds)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=termination_timeout_seconds)
        raise CalibrationRunError(
            "forced-interruption worker did not expose nonempty .tmp output "
            f"within {observation_timeout_seconds:.3f} seconds"
        )
    if process.poll() is not None:
        raise CalibrationRunError(
            "worker completed after output observation but before interruption"
        )
    process.terminate()
    termination_method = "terminate"
    try:
        returncode = process.wait(timeout=termination_timeout_seconds)
    except subprocess.TimeoutExpired:
        process.kill()
        termination_method = "kill_after_terminate_timeout"
        returncode = process.wait(timeout=termination_timeout_seconds)
    if returncode in (0, 3):
        raise CalibrationRunError(
            "worker completed normally before the forced interruption took effect; "
            f"exit_code={returncode}"
        )
    if process.poll() is None:
        raise CalibrationRunError("owned worker remained alive after interruption")
    return {
        "observable_tmp_bytes": observed_bytes,
        "observation_wait_seconds": time.monotonic() - started,
        "termination_method": termination_method,
        "terminated_exit_code": returncode,
    }


def _repeat_projection_bytes(path: Path) -> bytes:
    payload = bytearray()
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        if tuple(reader.fieldnames or ()) != EXPECTED_COLUMNS:
            raise CalibrationRunError(
                f"repeat projection input has unexpected schema: {path}"
            )
        fields = [
            field for field in reader.fieldnames or ()
            if not _is_volatile_column(field)
        ]
        payload.extend(canonical_json_bytes({
            "schema_version": REPEAT_PROJECTION_SCHEMA_VERSION,
            "fields": fields,
        }))
        for row in reader:
            if None in row or any(value is None for value in row.values()):
                raise CalibrationRunError(
                    f"repeat projection input contains malformed row: {path}"
                )
            payload.extend(
                canonical_json_bytes([row[field] for field in fields])
            )
    return bytes(payload)


def _write_repeat_projection(source: Path, destination: Path) -> str:
    _atomic_create(destination, _repeat_projection_bytes(source))
    return sha256_file(destination)


def _forced_interruption_resume_proof(
    repo: Path,
    run_root: Path,
    executable: Path,
    binding: dict[str, object],
    configs: dict[int, dict[str, int]],
) -> None:
    proof_dir = run_root / "proofs" / "resume"
    manifest_path = proof_dir / "proof.json"
    if _proof_manifest_valid(manifest_path, binding):
        return
    if proof_dir.exists():
        raise CalibrationRunError(
            f"incomplete resume proof is preserved; use a new attempt: {proof_dir}"
        )
    proof_dir.mkdir(parents=True)
    spec = ShardSpec("calibration", contract.CALIBRATION_SEEDS[0], 0, 0, 8)
    interrupted_dir = proof_dir / "i"
    interrupted_dir.mkdir()
    interrupted_output = interrupted_dir / "shard.csv.tmp"
    command = build_worker_command(executable, interrupted_output, spec, binding)
    stdout_path = interrupted_dir / "stdout.txt"
    stderr_path = interrupted_dir / "stderr.txt"
    interruption: dict[str, object]
    with stdout_path.open("xb") as stdout, stderr_path.open("xb") as stderr:
        process = subprocess.Popen(
            command,
            cwd=repo,
            stdin=subprocess.DEVNULL,
            stdout=stdout,
            stderr=stderr,
            **_hidden_process_options(),
        )
        try:
            interruption = _terminate_owned_process_after_output(
                process, interrupted_output
            )
        finally:
            stdout.flush()
            stderr.flush()
            os.fsync(stdout.fileno())
            os.fsync(stderr.fileno())
    interrupted_done = interrupted_dir / "shard.done"
    if (
        not interrupted_output.is_file()
        or interrupted_output.stat().st_size <= 0
        or interrupted_done.exists()
    ):
        raise CalibrationRunError(
            "forced interruption did not preserve nonempty .tmp-only evidence"
        )
    incomplete_rejected = False
    try:
        _verify_done(
            interrupted_output,
            interrupted_done,
            spec,
            binding,
            configs,
        )
    except CalibrationRunError:
        incomplete_rejected = True
    if not incomplete_rejected:
        raise CalibrationRunError(
            "resume verifier accepted interrupted output without .done"
        )
    resumed_dir = proof_dir / "r"
    resumed_dir.mkdir()
    resumed_output = resumed_dir / "shard.csv.tmp"
    resumed_command = build_worker_command(
        executable, resumed_output, spec, binding
    )
    with (resumed_dir / "stdout.txt").open("xb") as stdout, (
        resumed_dir / "stderr.txt"
    ).open("xb") as stderr:
        result = subprocess.run(
            resumed_command,
            cwd=repo,
            stdin=subprocess.DEVNULL,
            stdout=stdout,
            stderr=stderr,
            check=False,
            **_hidden_process_options(),
        )
        stdout.flush()
        stderr.flush()
        os.fsync(stdout.fileno())
        os.fsync(stderr.fileno())
    if result.returncode != 0:
        raise CalibrationRunError(
            f"resume-proof worker failed with exit code {result.returncode}"
        )
    resumed = validate_shard_csv(resumed_output, spec, binding, configs)
    if resumed.failure_count:
        raise CalibrationRunError("resume proof encountered generation failures")
    artifacts = []
    for path in sorted(proof_dir.rglob("*")):
        if path.is_file() and path != manifest_path:
            artifacts.append(
                {
                    "path": path.relative_to(proof_dir).as_posix(),
                    "bytes": path.stat().st_size,
                    "sha256": sha256_file(path),
                }
            )
    payload = {
        "status": "PASS",
        "binding_run_id": binding["run_id"],
        "worker_was_terminated": True,
        "interruption": interruption,
        "publication_was_interrupted_before_done": True,
        "interrupted_done_marker_absent": True,
        "incomplete_output_was_skipped": False,
        "incomplete_output_rejected": incomplete_rejected,
        "resumed_row_count": resumed.row_count,
        "resumed_csv_sha256": resumed.csv_sha256,
        "artifacts": artifacts,
    }
    _atomic_create(manifest_path, canonical_json_bytes(payload))


def _repeat_shard_proof(
    repo: Path,
    run_root: Path,
    executable: Path,
    binding: dict[str, object],
    configs: dict[int, dict[str, int]],
) -> None:
    proof_dir = run_root / "proofs" / "repeat"
    manifest_path = proof_dir / "proof.json"
    if _proof_manifest_valid(manifest_path, binding):
        return
    if proof_dir.exists():
        raise CalibrationRunError(
            f"incomplete repeat proof is preserved; use a new attempt: {proof_dir}"
        )
    proof_dir.mkdir(parents=True)
    spec = _all_shards("calibration", (contract.CALIBRATION_SEEDS[0],))[0]
    validations: list[ShardValidation] = []
    projection_hashes: list[str] = []
    for repeat in (1, 2):
        repeat_dir = proof_dir / str(repeat)
        repeat_dir.mkdir()
        output = repeat_dir / "shard.csv.tmp"
        command = build_worker_command(executable, output, spec, binding)
        with (repeat_dir / "stdout.txt").open("xb") as stdout, (
            repeat_dir / "stderr.txt"
        ).open("xb") as stderr:
            result = subprocess.run(
                command,
                cwd=repo,
                stdin=subprocess.DEVNULL,
                stdout=stdout,
                stderr=stderr,
                check=False,
                **_hidden_process_options(),
            )
            stdout.flush()
            stderr.flush()
            os.fsync(stdout.fileno())
            os.fsync(stderr.fileno())
        if result.returncode != 0:
            raise CalibrationRunError(
                f"repeat-proof process {repeat} failed ({result.returncode})"
            )
        validation = validate_shard_csv(output, spec, binding, configs)
        if validation.failure_count:
            raise CalibrationRunError("repeat proof encountered generation failures")
        validations.append(validation)
        projection_hashes.append(
            _write_repeat_projection(
                output, repeat_dir / "deterministic_projection.jsonl"
            )
        )
    if (
        validations[0].projection_sha256 != validations[1].projection_sha256
        or projection_hashes[0] != projection_hashes[1]
    ):
        raise CalibrationRunError(
            "separate-process worker deterministic projection artifacts differ"
        )
    artifacts = []
    for path in sorted(proof_dir.rglob("*")):
        if path.is_file() and path != manifest_path:
            artifacts.append(
                {
                    "path": path.relative_to(proof_dir).as_posix(),
                    "bytes": path.stat().st_size,
                    "sha256": sha256_file(path),
                }
            )
    payload = {
        "status": "PASS",
        "binding_run_id": binding["run_id"],
        "spec": spec.key,
        "raw_csv_hashes": [
            validations[0].csv_sha256,
            validations[1].csv_sha256,
        ],
        "raw_csv_hash_equality_required": False,
        "raw_csv_hashes_may_differ_due_to_volatile_columns": True,
        "deterministic_projection_schema_version": (
            REPEAT_PROJECTION_SCHEMA_VERSION
        ),
        "deterministic_projection_artifact_hashes": projection_hashes,
        "deterministic_projection_artifacts_identical": True,
        "canonical_projection_sha256": validations[0].projection_sha256,
        "excluded_volatile_columns": {
            "exact": list(VOLATILE_EXACT),
            "suffixes": list(VOLATILE_SUFFIXES),
        },
        "artifacts": artifacts,
    }
    _atomic_create(manifest_path, canonical_json_bytes(payload))


def _completed_specs(
    run_root: Path,
    specs: list[ShardSpec],
    binding: dict[str, object],
    configs: dict[int, dict[str, int]],
) -> dict[str, ShardValidation]:
    completed: dict[str, ShardValidation] = {}
    for spec in specs:
        csv_path, done_path = _shard_paths(run_root, spec)
        validation = _verify_done(csv_path, done_path, spec, binding, configs)
        if validation is not None:
            completed[spec.key] = validation
    return completed


def _progress_message(
    phase: str,
    completed_worlds: int,
    total_worlds: int,
    baseline_worlds: int,
    failures: int,
    started: float,
    per_size: dict[str, dict[str, float | int]],
) -> None:
    elapsed = max(time.monotonic() - started, 1e-9)
    throughput = (completed_worlds - baseline_worlds) / elapsed
    remaining = total_worlds - completed_worlds
    eta_seconds = remaining / throughput if throughput > 0 else None
    print(
        json.dumps(
            {
                "phase": phase,
                "completed_worlds": completed_worlds,
                "remaining_worlds": remaining,
                "failure_count": failures,
                "throughput_worlds_per_second": throughput,
                "eta_seconds": eta_seconds,
                "per_size_total_ms": {
                    name: values["total_ms"] for name, values in per_size.items()
                },
            },
            sort_keys=True,
        ),
        flush=True,
    )


def _merge_size_stats(
    target: dict[str, dict[str, float | int]],
    source: dict[str, dict[str, float | int]],
) -> None:
    for name, values in source.items():
        for key in ("worlds", "row_bytes"):
            target[name][key] = int(target[name][key]) + int(values[key])
        target[name]["total_ms"] = float(target[name]["total_ms"]) + float(
            values["total_ms"]
        )


def _run_specs(
    phase: str,
    repo: Path,
    run_root: Path,
    executable: Path,
    specs: list[ShardSpec],
    workers: int,
    resume: bool,
    binding: dict[str, object],
    configs: dict[int, dict[str, int]],
) -> dict[str, ShardValidation]:
    existing = _completed_specs(run_root, specs, binding, configs)
    if existing and not resume:
        raise CalibrationRunError(
            f"{phase} already has {len(existing)} completed shards; rerun with --resume"
        )
    pending = [spec for spec in specs if spec.key not in existing]
    total_worlds = sum(spec.world_count for spec in specs)
    completed_worlds = sum(value.row_count for value in existing.values())
    baseline_worlds = completed_worlds
    failures = sum(value.failure_count for value in existing.values())
    per_size = {
        name: {"worlds": 0, "row_bytes": 0, "total_ms": 0.0}
        for name, _width, _height in contract.MAP_SIZES
    }
    for validation in existing.values():
        _merge_size_stats(per_size, validation.per_size)
    started = time.monotonic()
    _progress_message(
        phase,
        completed_worlds,
        total_worlds,
        baseline_worlds,
        failures,
        started,
        per_size,
    )
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
        future_to_spec = {
            pool.submit(
                _run_and_publish,
                repo,
                run_root,
                executable,
                spec,
                binding,
                configs,
            ): spec
            for spec in pending
        }
        for future in concurrent.futures.as_completed(future_to_spec):
            spec = future_to_spec[future]
            try:
                _spec, validation = future.result()
            except Exception as exc:
                for other in future_to_spec:
                    other.cancel()
                raise CalibrationRunError(
                    f"shard execution failed for {spec.key}: {exc}"
                ) from exc
            existing[spec.key] = validation
            completed_worlds += validation.row_count
            failures += validation.failure_count
            _merge_size_stats(per_size, validation.per_size)
            _progress_message(
                phase,
                completed_worlds,
                total_worlds,
                baseline_worlds,
                failures,
                started,
                per_size,
            )
    if completed_worlds != total_worlds or len(existing) != len(specs):
        raise CalibrationRunError(f"{phase} completion accounting mismatch")
    return existing


def _raw_manifest_payload(
    run_root: Path,
    phase: str,
    specs: list[ShardSpec],
    validations: dict[str, ShardValidation],
    binding: dict[str, object],
) -> dict[str, object]:
    shards = []
    headers = set()
    total_rows = 0
    failure_count = 0
    for spec in specs:
        validation = validations[spec.key]
        csv_path, done_path = _shard_paths(run_root, spec)
        headers.add(validation.header_sha256)
        total_rows += validation.row_count
        failure_count += validation.failure_count
        shards.append(
            {
                "seed_kind": spec.seed_kind,
                "seed": spec.seed,
                "shard_index": spec.shard_index,
                "config_start": spec.config_start,
                "config_count": spec.config_count,
                "row_count": validation.row_count,
                "failure_count": validation.failure_count,
                "csv_path": csv_path.relative_to(run_root).as_posix(),
                "csv_bytes": csv_path.stat().st_size,
                "csv_sha256": validation.csv_sha256,
                "projection_sha256": validation.projection_sha256,
                "done_path": done_path.relative_to(run_root).as_posix(),
                "done_sha256": sha256_file(done_path),
                "header_sha256": validation.header_sha256,
                "per_size": validation.per_size,
            }
        )
    if len(headers) != 1:
        raise CalibrationRunError(
            f"worker emitted multiple CSV schemas in {phase}: {sorted(headers)}"
        )
    payload: dict[str, object] = {
        "phase": phase,
        "binding_run_id": binding["run_id"],
        "row_schema_version": binding["row_schema_version"],
        "header_sha256": next(iter(headers)),
        "shard_count": len(shards),
        "row_count": total_rows,
        "failure_count": failure_count,
        "shards": shards,
    }
    payload["manifest_payload_sha256"] = sha256_bytes(canonical_json_bytes(payload))
    return payload


def _strict_source_raw_validation(
    source_attempt: Path,
    binding: dict[str, object],
) -> dict[str, object]:
    configs = _config_rows(source_attempt)
    calibration_specs = _all_shards(
        "calibration", contract.CALIBRATION_SEEDS)
    holdout_specs = _all_shards("holdout", contract.HOLDOUT_SEEDS)
    full_specs = calibration_specs + holdout_specs
    validations: dict[str, ShardValidation] = {}
    for spec in full_specs:
        csv_path, done_path = _shard_paths(source_attempt, spec)
        validation = _verify_done(
            csv_path, done_path, spec, binding, configs)
        if validation is None:
            raise CalibrationRunError(
                f"source raw shard is missing: {spec.key}")
        validations[spec.key] = validation
    expected_csv = {
        _shard_paths(source_attempt, spec)[0].resolve()
        for spec in full_specs
    }
    expected_done = {
        _shard_paths(source_attempt, spec)[1].resolve()
        for spec in full_specs
    }
    raw_root = source_attempt / "raw"
    actual_csv = {path.resolve() for path in raw_root.rglob("shard_*.csv")}
    actual_done = {path.resolve() for path in raw_root.rglob("shard_*.done")}
    if actual_csv != expected_csv or actual_done != expected_done:
        raise CalibrationRunError(
            "source raw inventory contains missing or extra CSV/.done files")
    phase_specs = {
        "qualification": _all_shards(
            "calibration", (contract.CALIBRATION_SEEDS[0],)),
        "calibration": calibration_specs,
        "holdout": holdout_specs,
        "full": full_specs,
    }
    phase_identities: dict[str, object] = {}
    for phase, specs in phase_specs.items():
        phase_validations = {
            spec.key: validations[spec.key] for spec in specs}
        calculated = _raw_manifest_payload(
            source_attempt, phase, specs, phase_validations, binding)
        path = (
            source_attempt / "manifests" / f"raw_manifest_{phase}.json")
        recorded = _read_json_object(path, f"source {phase} raw manifest")
        if (
            recorded != calculated
            or path.read_bytes() != canonical_json_bytes(recorded)
        ):
            raise CalibrationRunError(
                f"source {phase} raw manifest does not reconstruct exactly")
        if int(recorded["failure_count"]) != 0:
            raise CalibrationRunError(
                f"source {phase} raw manifest contains generation failures")
        phase_identities[phase] = {
            "path": path.relative_to(source_attempt).as_posix(),
            "sha256": sha256_file(path),
            "payload_sha256": recorded["manifest_payload_sha256"],
            "shard_count": recorded["shard_count"],
            "row_count": recorded["row_count"],
            "failure_count": recorded["failure_count"],
        }
    full_manifest = _read_json_object(
        source_attempt / "manifests" / "raw_manifest_full.json",
        "source full raw manifest",
    )
    inventory = [
        {
            "csv_path": item["csv_path"],
            "csv_sha256": item["csv_sha256"],
            "csv_bytes": item["csv_bytes"],
            "done_path": item["done_path"],
            "done_sha256": item["done_sha256"],
            "row_count": item["row_count"],
        }
        for item in full_manifest["shards"]
    ]
    return {
        "status": "PASS",
        "strictly_validated_shards": len(full_specs),
        "strictly_validated_rows": sum(
            validation.row_count for validation in validations.values()),
        "failure_count": sum(
            validation.failure_count for validation in validations.values()),
        "raw_inventory_sha256": sha256_bytes(canonical_json_bytes(inventory)),
        "raw_csv_bytes": sum(int(item["csv_bytes"]) for item in inventory),
        "phase_manifests": phase_identities,
    }


def _create_or_verify_raw_import(
    repo: Path,
    run_root: Path,
    source_attempt: Path,
    processing_binding: dict[str, object],
) -> RawOrigin:
    source_attempt = source_attempt.resolve()
    if (
        not source_attempt.is_dir()
        or not _is_numbered_attempt(source_attempt)
        or source_attempt.parent != run_root.parent
        or int(source_attempt.name[8:]) >= int(run_root.name[8:])
    ):
        raise CalibrationRunError(
            "--source-attempt must be an older numbered sibling attempt")
    (
        source_binding,
        source_source,
        source_scripts,
        source_schema,
    ) = _load_binding_bundle(source_attempt)
    (
        observed_processing_binding,
        processing_source,
        processing_scripts,
        processing_schema,
    ) = _load_binding_bundle(run_root)
    if observed_processing_binding != processing_binding:
        raise CalibrationRunError("live processing binding changed before import")
    compatibility = _verify_raw_reuse_compatibility(
        source_binding,
        source_source,
        source_scripts,
        source_schema,
        processing_binding,
        processing_source,
        processing_scripts,
        processing_schema,
    )
    raw_validation = _strict_source_raw_validation(
        source_attempt, source_binding)
    failure_lineage = _source_failure_lineage(source_attempt)
    payload: dict[str, object] = {
        "schema_version": RAW_IMPORT_SCHEMA_VERSION,
        "status": "PASS",
        "mode": "processor-only-recovery",
        "source_attempt_path": str(source_attempt),
        "source_attempt_name": source_attempt.name,
        "source_binding_path": "binding/binding.json",
        "source_binding_sha256": sha256_file(
            source_attempt / "binding" / "binding.json"),
        "source_binding_run_id": source_binding["run_id"],
        "source_manifest_hash": source_binding["source_manifest_hash"],
        "source_executable_hash": source_binding["executable_hash"],
        "source_scripts_manifest_hash": source_binding["scripts_manifest_hash"],
        "source_config_manifest_hash": source_binding["config_manifest_hash"],
        "source_schema_manifest_hash": source_binding["schema_manifest_hash"],
        "source_row_schema_version": source_binding["row_schema_version"],
        "processing_binding_path": "binding/binding.json",
        "processing_binding_sha256": sha256_file(
            run_root / "binding" / "binding.json"),
        "processing_binding_run_id": processing_binding["run_id"],
        "compatibility": compatibility,
        "source_failure_lineage": failure_lineage,
        "raw_validation": raw_validation,
        "generation_launch_count": 0,
    }
    manifest = {
        **payload,
        "manifest_payload_sha256": sha256_bytes(canonical_json_bytes(payload)),
    }
    path = run_root / "raw_import.json"
    encoded = canonical_json_bytes(manifest)
    if path.exists():
        if path.read_bytes() != encoded:
            raise CalibrationRunError(
                f"existing raw import manifest differs: {path}")
    else:
        _atomic_create(path, encoded)
    return _load_raw_origin(run_root)


def _load_raw_origin(run_root: Path) -> RawOrigin:
    import_path = run_root / "raw_import.json"
    if not import_path.is_file():
        binding = _read_json_object(
            run_root / "binding" / "binding.json", "local binding")
        return RawOrigin(run_root, binding, None, "")
    manifest = _read_json_object(import_path, "raw import manifest")
    recorded_hash = manifest.get("manifest_payload_sha256")
    payload = dict(manifest)
    payload.pop("manifest_payload_sha256", None)
    if (
        manifest.get("schema_version") != RAW_IMPORT_SCHEMA_VERSION
        or manifest.get("status") != "PASS"
        or recorded_hash != sha256_bytes(canonical_json_bytes(payload))
    ):
        raise CalibrationRunError(f"raw import manifest is invalid: {import_path}")
    processing_binding_path = run_root / "binding" / "binding.json"
    processing_binding = _read_json_object(
        processing_binding_path, "processing binding")
    if (
        sha256_file(processing_binding_path) !=
            manifest.get("processing_binding_sha256")
        or processing_binding.get("run_id") !=
            manifest.get("processing_binding_run_id")
    ):
        raise CalibrationRunError("raw import processing binding changed")
    source_attempt = Path(str(manifest.get("source_attempt_path", ""))).resolve()
    source_binding_relative = Path(str(manifest.get("source_binding_path", "")))
    source_binding_path = (source_attempt / source_binding_relative).resolve()
    if (
        source_binding_relative.is_absolute()
        or not source_binding_path.is_relative_to(source_attempt)
        or not source_binding_path.is_file()
        or sha256_file(source_binding_path) != manifest.get("source_binding_sha256")
    ):
        raise CalibrationRunError("raw import source binding changed")
    source_binding = _read_json_object(source_binding_path, "source binding")
    if source_binding.get("run_id") != manifest.get("source_binding_run_id"):
        raise CalibrationRunError("raw import source run identity changed")
    phase_manifests = manifest.get("raw_validation", {})
    if not isinstance(phase_manifests, dict):
        raise CalibrationRunError("raw import validation metadata is invalid")
    phase_manifests = phase_manifests.get("phase_manifests")
    if not isinstance(phase_manifests, dict):
        raise CalibrationRunError("raw import lacks phase-manifest identities")
    for identity in phase_manifests.values():
        if not isinstance(identity, dict):
            raise CalibrationRunError("raw import phase identity is invalid")
        path = (source_attempt / str(identity.get("path", ""))).resolve()
        if (
            not path.is_relative_to(source_attempt)
            or not path.is_file()
            or sha256_file(path) != identity.get("sha256")
        ):
            raise CalibrationRunError("raw import source manifest changed")
    return RawOrigin(
        source_attempt, source_binding, manifest, sha256_file(import_path))


def _raw_manifest(
    run_root: Path,
    phase: str,
    specs: list[ShardSpec],
    validations: dict[str, ShardValidation],
    binding: dict[str, object],
) -> dict[str, object]:
    payload = _raw_manifest_payload(
        run_root, phase, specs, validations, binding)
    phase_path = run_root / "manifests" / f"raw_manifest_{phase}.json"
    encoded = canonical_json_bytes(payload)
    if phase_path.exists():
        if phase_path.read_bytes() != encoded:
            raise CalibrationRunError(
                f"existing phase raw manifest differs: {phase_path}"
            )
    else:
        _atomic_create(phase_path, encoded)
    _atomic_replace(run_root / "raw_manifest.json", encoded)
    return payload


def _qualification_observations(
    run_root: Path,
    validations: dict[str, ShardValidation],
    binding: dict[str, object],
) -> dict[str, object]:
    size_totals = {
        name: {"worlds": 0, "row_bytes": 0, "total_ms": 0.0}
        for name, _width, _height in contract.MAP_SIZES
    }
    for validation in validations.values():
        _merge_size_stats(size_totals, validation.per_size)
    sizes = {}
    for name, totals in size_totals.items():
        worlds = int(totals["worlds"])
        if worlds != contract.EFFECTIVE_CONFIG_COUNT:
            raise CalibrationRunError(
                f"qualification size count mismatch for {name}: {worlds}"
            )
        sizes[name] = {
            **totals,
            "observed_bytes_per_world": int(totals["row_bytes"]) / worlds,
            "observed_ms_per_world": float(totals["total_ms"]) / worlds,
        }
    payload = {
        "status": "PASS",
        "binding_run_id": binding["run_id"],
        "world_count": sum(int(value["worlds"]) for value in size_totals.values()),
        "sizes": sizes,
    }
    path = run_root / "qualification_observed.json"
    encoded = canonical_json_bytes(payload)
    if path.exists():
        if path.read_bytes() != encoded:
            raise CalibrationRunError("qualification observations changed")
    else:
        _atomic_create(path, encoded)
    return payload


def _qualification_complete_bytes(
    run_root: Path, binding: dict[str, object]
) -> bytes:
    return canonical_json_bytes({
        "status": "PASS",
        "binding_run_id": binding["run_id"],
        "world_count": contract.ONE_SEED_WORLD_COUNT,
        "raw_manifest_sha256": sha256_file(
            run_root / "manifests" / "raw_manifest_qualification.json"
        ),
    })


def _verify_qualification_complete(
    run_root: Path, binding: dict[str, object]
) -> None:
    path = run_root / "qualification.complete"
    expected = _qualification_complete_bytes(run_root, binding)
    if not path.is_file() or path.read_bytes() != expected:
        raise CalibrationRunError(
            "full mode requires an exact binding-matched qualification.complete"
        )


def _next_numbered_evidence_path(directory: Path, stem: str) -> tuple[int, Path]:
    directory.mkdir(parents=True, exist_ok=True)
    highest = 0
    for candidate in directory.glob(f"{stem}_*.json"):
        parts = candidate.stem.split("_")
        if len(parts) >= 3 and parts[2].isdigit():
            highest = max(highest, int(parts[2]))
    number = highest + 1
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    path = directory / f"{stem}_{number:06d}_{timestamp}.json"
    while path.exists():
        number += 1
        path = directory / f"{stem}_{number:06d}_{timestamp}.json"
    return number, path


def _disk_forecast(
    run_root: Path,
    observations: dict[str, object],
    binding: dict[str, object],
    *,
    _observed_free_bytes: int | None = None,
) -> dict[str, object]:
    remaining_seeds = (
        len(contract.CALIBRATION_SEEDS) + len(contract.HOLDOUT_SEEDS) - 1
    )
    remaining_per_size = contract.EFFECTIVE_CONFIG_COUNT * remaining_seeds
    projected = 0.0
    projected_runtime_ms = 0.0
    for values in observations["sizes"].values():
        projected += float(values["observed_bytes_per_world"]) * remaining_per_size
        projected_runtime_ms += (
            float(values["observed_ms_per_world"]) * remaining_per_size
        )
    required = math.ceil(projected * 1.25 + 5 * GIB)
    initial_free = (
        shutil.disk_usage(run_root).free
        if _observed_free_bytes is None
        else _observed_free_bytes
    )
    calculated = {
        "kind": "immutable_qualification_forecast",
        "binding_run_id": binding["run_id"],
        "qualification_observations_sha256": sha256_bytes(
            canonical_json_bytes(observations)
        ),
        "remaining_worlds": contract.FULL_WORLD_COUNT
        - contract.ONE_SEED_WORLD_COUNT,
        "remaining_worlds_per_size": remaining_per_size,
        "projected_remaining_output_bytes": math.ceil(projected),
        "projected_remaining_runtime_seconds": projected_runtime_ms / 1000.0,
        "required_free_bytes": required,
        "formula": "free >= 125% projected remaining output + 5 GiB",
    }
    path = run_root / "disk_forecast.json"
    if path.exists():
        try:
            frozen = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            raise CalibrationRunError(f"invalid immutable disk forecast: {exc}") from exc
        recorded_forecast_hash = frozen.get("forecast_payload_sha256")
        frozen_payload = dict(frozen)
        frozen_payload.pop("forecast_payload_sha256", None)
        if recorded_forecast_hash != sha256_bytes(
            canonical_json_bytes(frozen_payload)
        ):
            raise CalibrationRunError("immutable disk forecast hash mismatch")
        for field, expected in calculated.items():
            if frozen.get(field) != expected:
                raise CalibrationRunError(f"disk forecast binding changed: {field}")
        if (
            frozen.get("initial_status") not in ("PASS", "INSUFFICIENT_DISK")
            or not isinstance(frozen.get("initial_observed_free_bytes"), int)
            or not isinstance(frozen.get("created_utc"), str)
        ):
            raise CalibrationRunError("immutable disk forecast metadata is invalid")
    else:
        frozen_payload = {
            **calculated,
            "created_utc": datetime.now(timezone.utc).isoformat(),
            "initial_observed_free_bytes": initial_free,
            "initial_status": (
                "PASS" if initial_free >= required else "INSUFFICIENT_DISK"
            ),
        }
        frozen = {
            **frozen_payload,
            "forecast_payload_sha256": sha256_bytes(
                canonical_json_bytes(frozen_payload)
            ),
        }
        _atomic_create(path, canonical_json_bytes(frozen))
    current_free = (
        shutil.disk_usage(run_root).free
        if _observed_free_bytes is None
        else _observed_free_bytes
    )
    audit_number, audit_path = _next_numbered_evidence_path(
        run_root / "disk_audits", "disk_audit"
    )
    audit = {
        "status": "PASS" if current_free >= required else "INSUFFICIENT_DISK",
        "audit_number": audit_number,
        "utc": datetime.now(timezone.utc).isoformat(),
        "binding_run_id": binding["run_id"],
        "disk_forecast_path": path.relative_to(run_root).as_posix(),
        "disk_forecast_sha256": sha256_file(path),
        "required_free_bytes": required,
        "observed_free_bytes": current_free,
        "formula": calculated["formula"],
    }
    _atomic_create(audit_path, canonical_json_bytes(audit))
    if audit["status"] != "PASS":
        raise InsufficientDiskError(
            "insufficient disk for full run: "
            f"free={current_free} required={required}; live_audit={audit_path}"
        )
    return {
        "forecast": frozen,
        "forecast_sha256": sha256_file(path),
        "live_audit": audit,
        "live_audit_path": audit_path.relative_to(run_root).as_posix(),
    }


def _processor_output_directories(run_root: Path, phase: str) -> tuple[Path, ...]:
    if phase in ("qualification", "calibration"):
        return (run_root / "processed" / phase,)
    if phase == "full":
        return (
            run_root / "processed" / "holdout",
            run_root / "processed" / "full",
        )
    raise CalibrationRunError(f"unknown processor phase: {phase}")


def _processor_raw_manifest_name(output_name: str) -> str:
    if output_name in ("qualification", "calibration", "holdout", "full"):
        return output_name
    raise CalibrationRunError(
        f"unknown processed output directory: {output_name}")


def _load_processor_raw_manifest(
    run_root: Path, manifest_name: str
) -> tuple[dict[str, object], dict[str, dict[str, object]]]:
    origin = _load_raw_origin(run_root)
    path = origin.root / "manifests" / f"raw_manifest_{manifest_name}.json"
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise CalibrationRunError(
            f"invalid processor raw manifest {path}: {exc}") from exc
    recorded_payload_hash = manifest.get("manifest_payload_sha256")
    payload = dict(manifest)
    payload.pop("manifest_payload_sha256", None)
    calculated_payload_hash = sha256_bytes(canonical_json_bytes(payload))
    if recorded_payload_hash != calculated_payload_hash:
        raise CalibrationRunError(
            f"processor raw manifest payload hash mismatch: {path}")
    if manifest.get("phase") != manifest_name:
        raise CalibrationRunError(
            f"processor raw manifest phase mismatch: {path}")
    if (
        manifest.get("binding_run_id") != origin.binding.get("run_id")
        or manifest.get("row_schema_version") !=
            origin.binding.get("row_schema_version")
    ):
        raise CalibrationRunError(
            f"processor raw manifest binding mismatch: {path}")
    shards = manifest.get("shards")
    if not isinstance(shards, list):
        raise CalibrationRunError(
            f"processor raw manifest lacks shard inventory: {path}")
    expected: dict[str, dict[str, object]] = {}
    for item in shards:
        if not isinstance(item, dict):
            raise CalibrationRunError(
                f"processor raw manifest has malformed shard: {path}")
        relative = item.get("csv_path")
        if not isinstance(relative, str) or not relative:
            raise CalibrationRunError(
                f"processor raw manifest has invalid CSV path: {path}")
        candidate = origin.resolve(relative)
        if not candidate.is_file():
            raise CalibrationRunError(
                f"processor raw shard is missing: {candidate}")
        normalized = Path(relative).as_posix()
        if normalized in expected:
            raise CalibrationRunError(
                f"processor raw manifest duplicates shard: {normalized}")
        try:
            expected_bytes = int(item["csv_bytes"])
            expected_rows = int(item["row_count"])
        except (KeyError, TypeError, ValueError) as exc:
            raise CalibrationRunError(
                f"processor raw manifest has invalid shard counts: {path}"
            ) from exc
        expected_hash = str(item.get("csv_sha256", "")).upper()
        actual_hash = sha256_file(candidate)
        if (
            expected_bytes != candidate.stat().st_size
            or expected_hash != actual_hash
        ):
            raise CalibrationRunError(
                f"processor raw shard hash/size mismatch: {candidate}")
        done_relative = item.get("done_path")
        if not isinstance(done_relative, str) or not done_relative:
            raise CalibrationRunError(
                f"processor raw manifest has invalid .done path: {path}")
        done_path = origin.resolve(done_relative)
        if (
            not done_path.is_file()
            or sha256_file(done_path) != str(
                item.get("done_sha256", "")).upper()
        ):
            raise CalibrationRunError(
                f"processor raw .done hash mismatch: {done_path}")
        expected[normalized] = {
            "path": normalized,
            "sha256": actual_hash,
            "bytes": expected_bytes,
            "rows": expected_rows,
        }
    if (
        len(expected) != manifest.get("shard_count")
        or sum(int(item["rows"]) for item in expected.values())
        != manifest.get("row_count")
    ):
        raise CalibrationRunError(
            f"processor raw manifest totals mismatch: {path}")
    identity = {
        "name": manifest_name,
        "path": path.relative_to(origin.root).as_posix(),
        "origin": "imported" if origin.imported else "local",
        "raw_binding_run_id": origin.binding["run_id"],
        "raw_import_sha256": origin.import_sha256,
        "sha256": sha256_file(path),
        "bytes": path.stat().st_size,
        "payload_sha256": calculated_payload_hash,
        "shard_count": len(expected),
        "row_count": manifest["row_count"],
    }
    return identity, expected


def _processor_phase_inputs(
    run_root: Path, phase: str
) -> dict[str, object]:
    origin = _load_raw_origin(run_root)
    manifest_names = {
        "qualification": ("qualification",),
        "calibration": ("calibration",),
        "full": ("calibration", "holdout", "full"),
    }.get(phase)
    if manifest_names is None:
        raise CalibrationRunError(f"unknown processor phase: {phase}")
    manifests = [
        _load_processor_raw_manifest(run_root, name)[0]
        for name in manifest_names
    ]
    calibration_freeze: list[dict[str, object]] = []
    if phase == "full":
        calibration_dir = run_root / "processed" / "calibration"
        for name in (
            "calibration_surface.csv",
            "calibration_surface.sha256",
            "calibration_model.json",
            "calibration_model.sha256",
            "calibration_freeze_manifest.json",
            "summary_lineage.csv",
        ):
            path = calibration_dir / name
            if not path.is_file():
                raise CalibrationRunError(
                    f"full processor input is missing: {path}")
            calibration_freeze.append({
                "path": path.relative_to(run_root).as_posix(),
                "sha256": sha256_file(path),
                "bytes": path.stat().st_size,
            })
    return {
        "phase": phase,
        "raw_origin": {
            "kind": "imported" if origin.imported else "local",
            "binding_run_id": origin.binding["run_id"],
            "import_sha256": origin.import_sha256,
            "source_attempt_path": (
                str(origin.root) if origin.imported else ""),
        },
        "raw_manifests": manifests,
        "calibration_freeze": calibration_freeze,
    }


def _processor_lineage_inventory(
    run_root: Path, phase: str, staging_root: Path | None = None
) -> list[dict[str, object]]:
    inventory: list[dict[str, object]] = []
    origin = _load_raw_origin(run_root)
    for published_dir in _processor_output_directories(run_root, phase):
        output_dir = (
            published_dir if staging_root is None else
            staging_root / "processed" / published_dir.name
        )
        lineage_path = output_dir / "summary_lineage.csv"
        if not lineage_path.is_file():
            raise CalibrationRunError(
                f"processor output lacks summary lineage: {lineage_path}"
            )
        recorded_outputs: list[dict[str, object]] = []
        recorded_raw: dict[str, dict[str, object]] = {}
        recorded_processed: dict[str, dict[str, object]] = {}
        observed_paths: set[str] = set()
        with lineage_path.open("r", encoding="utf-8", newline="") as handle:
            reader = csv.DictReader(handle)
            if tuple(reader.fieldnames or ()) != (
                "kind", "path", "sha256", "bytes", "rows", "model_sha256"
            ):
                raise CalibrationRunError(
                    f"processor lineage schema mismatch: {lineage_path}"
                )
            for row_number, row in enumerate(reader, start=2):
                if None in row or any(value is None for value in row.values()):
                    raise CalibrationRunError(
                        f"malformed processor lineage row {row_number}: "
                        f"{lineage_path}"
                    )
                if row["kind"] not in ("raw_shard", "processed_output"):
                    raise CalibrationRunError(
                        f"unknown processor lineage kind at row {row_number}: "
                        f"{row['kind']}"
                    )
                logical_path = Path(row["path"])
                if logical_path.is_absolute():
                    raise CalibrationRunError(
                        f"processor lineage path must be stable and relative: "
                        f"{logical_path}"
                    )
                if row["kind"] == "raw_shard":
                    published_path = origin.resolve(row["path"])
                    relative = logical_path.as_posix()
                else:
                    published_path = (run_root / logical_path).resolve()
                    if not published_path.is_relative_to(run_root):
                        raise CalibrationRunError(
                            f"processor lineage path is outside attempt: "
                            f"{published_path}"
                        )
                    relative = published_path.relative_to(run_root).as_posix()
                if relative in observed_paths:
                    raise CalibrationRunError(
                        f"processor lineage duplicates output: {relative}"
                    )
                observed_paths.add(relative)
                try:
                    recorded_bytes = int(row["bytes"], 10)
                except ValueError as exc:
                    raise CalibrationRunError(
                        f"processor lineage byte count is invalid: {relative}"
                    ) from exc
                if row["kind"] == "raw_shard":
                    candidate = published_path
                else:
                    if not published_path.is_relative_to(published_dir):
                        raise CalibrationRunError(
                            f"processed lineage path is outside phase output: "
                            f"{published_path}"
                        )
                    candidate = (
                        output_dir /
                        published_path.relative_to(published_dir)
                    )
                if not candidate.is_file():
                    raise CalibrationRunError(
                        f"processor lineage target is missing: {candidate}")
                actual_hash = sha256_file(candidate)
                if (
                    recorded_bytes != candidate.stat().st_size
                    or row["sha256"].upper() != actual_hash
                ):
                    raise CalibrationRunError(
                        f"processor lineage hash/size mismatch: {candidate}"
                    )
                recorded = {
                    "kind": row["kind"],
                    "path": relative,
                    "sha256": actual_hash,
                    "bytes": recorded_bytes,
                    "rows": row["rows"],
                    "model_sha256": row["model_sha256"],
                }
                recorded_outputs.append(recorded)
                if row["kind"] == "raw_shard":
                    try:
                        raw_rows = int(row["rows"], 10)
                    except ValueError as exc:
                        raise CalibrationRunError(
                            f"processor raw lineage row count is invalid: "
                            f"{relative}") from exc
                    recorded_raw[relative] = {
                        "path": relative,
                        "sha256": actual_hash,
                        "bytes": recorded_bytes,
                        "rows": raw_rows,
                    }
                else:
                    recorded_processed[relative] = recorded
        if not recorded_outputs:
            raise CalibrationRunError(
                f"processor lineage has no recorded outputs: {lineage_path}"
            )
        manifest_name = _processor_raw_manifest_name(published_dir.name)
        _identity, expected_raw = _load_processor_raw_manifest(
            run_root, manifest_name)
        if recorded_raw != expected_raw:
            raise CalibrationRunError(
                f"processor raw lineage does not exactly match "
                f"raw_manifest_{manifest_name}.json")
        actual_processed_paths: set[str] = set()
        for path in output_dir.rglob("*"):
            if path.is_file() and path != lineage_path:
                published_path = (
                    published_dir / path.relative_to(output_dir)
                ).resolve()
                actual_processed_paths.add(
                    published_path.relative_to(run_root).as_posix())
        if set(recorded_processed) != actual_processed_paths:
            missing = sorted(actual_processed_paths - set(recorded_processed))
            extra = sorted(set(recorded_processed) - actual_processed_paths)
            raise CalibrationRunError(
                "processor processed lineage/output tree mismatch: "
                f"missing_lineage={missing} extra_lineage={extra}")
        inventory.append({
            "path": (
                published_dir / "summary_lineage.csv"
            ).relative_to(run_root).as_posix(),
            "sha256": sha256_file(lineage_path),
            "bytes": lineage_path.stat().st_size,
            "recorded_output_count": len(recorded_outputs),
            "recorded_outputs_sha256": sha256_bytes(
                canonical_json_bytes(recorded_outputs)
            ),
        })
    return inventory


def _query_process_identity(
    pid: int,
) -> tuple[str, dict[str, object] | None]:
    if pid <= 0:
        return "missing", None
    if os.name == "nt":
        import ctypes
        from ctypes import wintypes

        class FILETIME(ctypes.Structure):
            _fields_ = (
                ("dwLowDateTime", wintypes.DWORD),
                ("dwHighDateTime", wintypes.DWORD),
            )

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        open_process = kernel32.OpenProcess
        open_process.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
        open_process.restype = wintypes.HANDLE
        close_handle = kernel32.CloseHandle
        close_handle.argtypes = (wintypes.HANDLE,)
        get_process_times = kernel32.GetProcessTimes
        get_process_times.argtypes = (
            wintypes.HANDLE,
            ctypes.POINTER(FILETIME),
            ctypes.POINTER(FILETIME),
            ctypes.POINTER(FILETIME),
            ctypes.POINTER(FILETIME),
        )
        get_exit_code = kernel32.GetExitCodeProcess
        get_exit_code.argtypes = (
            wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD))
        query_image = kernel32.QueryFullProcessImageNameW
        query_image.argtypes = (
            wintypes.HANDLE,
            wintypes.DWORD,
            wintypes.LPWSTR,
            ctypes.POINTER(wintypes.DWORD),
        )
        handle = open_process(0x1000, False, pid)
        if not handle:
            error = ctypes.get_last_error()
            if error in (87, 1168):
                return "missing", None
            return "unknown", {"pid": pid, "win32_error": error}
        try:
            creation = FILETIME()
            exit_time = FILETIME()
            kernel_time = FILETIME()
            user_time = FILETIME()
            exit_code = wintypes.DWORD()
            if not get_process_times(
                handle,
                ctypes.byref(creation),
                ctypes.byref(exit_time),
                ctypes.byref(kernel_time),
                ctypes.byref(user_time),
            ) or not get_exit_code(handle, ctypes.byref(exit_code)):
                return "unknown", {
                    "pid": pid, "win32_error": ctypes.get_last_error()}
            size = wintypes.DWORD(32768)
            buffer = ctypes.create_unicode_buffer(size.value)
            image_path = ""
            if query_image(handle, 0, buffer, ctypes.byref(size)):
                image_path = str(Path(buffer.value).resolve()).casefold()
            identity = {
                "pid": pid,
                "creation_token": (
                    f"{creation.dwHighDateTime:08X}"
                    f"{creation.dwLowDateTime:08X}"
                ),
                "image_path": image_path,
            }
            return (
                "running" if exit_code.value == 259 else "exited",
                identity,
            )
        finally:
            close_handle(handle)
    proc_path = Path("/proc") / str(pid)
    try:
        stat_text = (proc_path / "stat").read_text(encoding="ascii")
        tail = stat_text[stat_text.rfind(")") + 2:].split()
        start_ticks = tail[19]
        image_path = str((proc_path / "exe").resolve()).casefold()
    except FileNotFoundError:
        return "missing", None
    except (OSError, IndexError):
        return "unknown", {"pid": pid}
    return "running", {
        "pid": pid,
        "creation_token": start_ticks,
        "image_path": image_path,
    }


def _same_process_identity(
    expected: object, observed: dict[str, object] | None
) -> bool:
    if not isinstance(expected, dict) or observed is None:
        return False
    if (
        expected.get("pid") != observed.get("pid")
        or not expected.get("creation_token")
        or expected.get("creation_token") != observed.get("creation_token")
    ):
        return False
    expected_image = str(expected.get("image_path", "")).casefold()
    observed_image = str(observed.get("image_path", "")).casefold()
    return (
        not expected_image
        or not observed_image
        or expected_image == observed_image
    )


def _legacy_processor_liveness(
    processor: Path, run_root: Path, phase: str
) -> tuple[str, list[dict[str, object]]]:
    processes: list[dict[str, object]] = []
    if os.name == "nt":
        script = (
            "$ErrorActionPreference='Stop';"
            "@(Get-CimInstance Win32_Process | "
            "Select-Object ProcessId,CreationDate,ExecutablePath,CommandLine)"
            "| ConvertTo-Json -Compress"
        )
        result = subprocess.run(
            ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            **_hidden_process_options(),
        )
        if result.returncode != 0:
            return "unknown", [{
                "query_error": result.stderr.decode(
                    "utf-8", "replace").strip()
            }]
        try:
            decoded = json.loads(
                result.stdout.decode("utf-8", "strict") or "[]")
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            return "unknown", [{"query_error": str(exc)}]
        candidates = decoded if isinstance(decoded, list) else [decoded]
        for candidate in candidates:
            if not isinstance(candidate, dict):
                continue
            command_line = str(candidate.get("CommandLine") or "")
            folded = command_line.casefold()
            if (
                str(processor).casefold() in folded
                and str(run_root).casefold() in folded
                and phase.casefold() in folded
            ):
                try:
                    pid = int(candidate["ProcessId"])
                except (KeyError, TypeError, ValueError):
                    return "unknown", [candidate]
                status, identity = _query_process_identity(pid)
                processes.append({
                    "pid": pid,
                    "status": status,
                    "identity": identity,
                    "creation_date": candidate.get("CreationDate"),
                    "executable_path": candidate.get("ExecutablePath"),
                })
    else:
        proc_root = Path("/proc")
        if not proc_root.is_dir():
            return "unknown", [{"query_error": "/proc is unavailable"}]
        try:
            proc_entries = list(proc_root.iterdir())
        except OSError as exc:
            return "unknown", [{"query_error": str(exc)}]
        for entry in proc_entries:
            if not entry.name.isdigit():
                continue
            try:
                command_line = (
                    entry / "cmdline"
                ).read_bytes().replace(b"\0", b" ").decode(
                    "utf-8", "surrogateescape")
            except (FileNotFoundError, PermissionError):
                continue
            folded = command_line.casefold()
            if (
                str(processor).casefold() in folded
                and str(run_root).casefold() in folded
                and phase.casefold() in folded
            ):
                status, identity = _query_process_identity(int(entry.name))
                processes.append({
                    "pid": int(entry.name),
                    "status": status,
                    "identity": identity,
                })
    active = [
        process for process in processes
        if process.get("status") in ("running", "unknown")
    ]
    return ("active" if active else "absent"), active


def _next_processor_directory(
    parent: Path, prefix: str
) -> Path:
    parent.mkdir(parents=True, exist_ok=True)
    highest = 0
    for candidate in parent.iterdir():
        if not candidate.is_dir() or not candidate.name.startswith(prefix):
            continue
        suffix = candidate.name[len(prefix):].split("_", 1)[0]
        if suffix.isdigit():
            highest = max(highest, int(suffix))
    number = highest + 1
    while True:
        path = parent / f"{prefix}{number:06d}"
        try:
            path.mkdir()
            return path
        except FileExistsError:
            number += 1


def _processor_phase_slug(phase: str) -> str:
    try:
        return {
            "qualification": "q",
            "calibration": "c",
            "full": "f",
        }[phase]
    except KeyError as exc:
        raise CalibrationRunError(
            f"unknown processor phase: {phase}") from exc


def _processor_stages(run_root: Path, phase: str) -> list[Path]:
    parent = run_root / "processor_staging" / _processor_phase_slug(phase)
    if not parent.is_dir():
        return []
    return sorted(
        (path for path in parent.iterdir() if path.is_dir()),
        key=lambda path: path.name,
    )


def _recursive_evidence_inventory(path: Path) -> list[dict[str, object]]:
    if path.is_file():
        return [{
            "path": path.name,
            "sha256": sha256_file(path),
            "bytes": path.stat().st_size,
        }]
    return [{
        "path": item.relative_to(path).as_posix(),
        "sha256": sha256_file(item),
        "bytes": item.stat().st_size,
    } for item in sorted(path.rglob("*")) if item.is_file()]


def _canonical_processor_evidence(
    run_root: Path, phase: str
) -> list[Path]:
    log_dir = run_root / "processor_runs"
    return [
        *_processor_output_directories(run_root, phase),
        log_dir / f"{phase}_stdout.txt",
        log_dir / f"{phase}_stderr.txt",
        log_dir / f"{phase}_result.json",
    ]


def _archive_processor_evidence(
    run_root: Path,
    phase: str,
    reason: str,
    *,
    stages: Sequence[Path] = (),
    include_canonical: bool = False,
) -> Path:
    recovery = _next_processor_directory(
        run_root / "processor_recovery" / _processor_phase_slug(phase),
        "recovery_",
    )
    moved: list[dict[str, object]] = []
    sources: list[tuple[Path, Path]] = []
    for stage_index, stage in enumerate(stages, start=1):
        if stage.exists():
            sources.append((stage, Path(f"s{stage_index}")))
    if include_canonical:
        for source in _canonical_processor_evidence(run_root, phase):
            if source.exists():
                sources.append((
                    source,
                    Path("final") / source.relative_to(run_root),
                ))
    for source, relative_destination in sources:
        before = _recursive_evidence_inventory(source)
        destination = recovery / relative_destination
        destination.parent.mkdir(parents=True, exist_ok=True)
        os.replace(source, destination)
        _fsync_directory(destination.parent)
        moved.append({
            "source": str(source),
            "archived_path": destination.relative_to(run_root).as_posix(),
            "inventory": before,
        })
    payload = {
        "schema_version": PROCESSOR_TRANSACTION_SCHEMA_VERSION,
        "status": "RECOVERED",
        "phase": phase,
        "reason": reason,
        "utc": datetime.now(timezone.utc).isoformat(),
        "moved": moved,
    }
    _atomic_create(
        recovery / "recovery_manifest.json", canonical_json_bytes(payload))
    return recovery


def _read_processor_lease(stage: Path) -> dict[str, object]:
    path = stage / "in_progress.json"
    try:
        lease = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ProcessorResumeBlockedError(
            f"processor stage has unreadable liveness lease; "
            f"manual process audit required: {stage}: {exc}") from exc
    if (
        lease.get("schema_version") != PROCESSOR_TRANSACTION_SCHEMA_VERSION
        or lease.get("phase") not in ("qualification", "calibration", "full")
    ):
        raise ProcessorResumeBlockedError(
            f"processor stage has an untrusted liveness lease: {stage}")
    return lease


def _processor_stage_liveness(stage: Path) -> tuple[str, str]:
    lease = _read_processor_lease(stage)
    child_spawned = lease.get("child_spawned")
    if child_spawned is False:
        return "dead", "lease proves child was not spawned"
    if child_spawned != True:
        return "unknown", "lease was interrupted during child spawn"
    try:
        pid = int(lease["child_pid"])
    except (KeyError, TypeError, ValueError):
        return "unknown", "lease lacks a valid child PID"
    status, observed = _query_process_identity(pid)
    if status in ("missing", "exited"):
        return "dead", f"leased child is {status}"
    if status == "unknown":
        return "unknown", "leased child liveness cannot be queried safely"
    if _same_process_identity(lease.get("child_identity"), observed):
        return "active", "exact PID and creation identity are still running"
    return "dead", "PID was reused or creation identity differs"


def _processor_stage_complete_payload(
    stage: Path,
    phase: str,
    binding: dict[str, object],
    command: list[str],
    execution_command: list[str],
    processor_hash: str,
    phase_inputs: dict[str, object],
    lineages: list[dict[str, object]],
    returncode: int,
) -> dict[str, object]:
    stdout_path = stage / "stdout.txt"
    stderr_path = stage / "stderr.txt"
    return {
        "schema_version": PROCESSOR_TRANSACTION_SCHEMA_VERSION,
        "status": "STAGE_COMPLETE",
        "phase": phase,
        "binding_run_id": binding["run_id"],
        "command": command,
        "execution_command": execution_command,
        "processor_sha256": processor_hash,
        "phase_inputs": phase_inputs,
        "exit_code": returncode,
        "stdout_sha256": sha256_file(stdout_path),
        "stderr_sha256": sha256_file(stderr_path),
        "lineages": lineages,
    }


def _verify_processor_stage_complete(
    run_root: Path,
    stage: Path,
    phase: str,
    binding: dict[str, object],
    command: list[str],
    processor_hash: str,
    phase_inputs: dict[str, object],
) -> dict[str, object]:
    path = stage / "stage_complete.json"
    try:
        completed = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise CalibrationRunError(
            f"invalid processor stage-complete manifest: {path}: {exc}"
        ) from exc
    expected = {
        "schema_version": PROCESSOR_TRANSACTION_SCHEMA_VERSION,
        "status": "STAGE_COMPLETE",
        "phase": phase,
        "binding_run_id": binding["run_id"],
        "command": command,
        "processor_sha256": processor_hash,
        "phase_inputs": phase_inputs,
        "exit_code": 0,
    }
    if any(completed.get(field) != value for field, value in expected.items()):
        raise CalibrationRunError(
            f"processor stage-complete identity mismatch: {path}")
    for name, field in (
        ("stdout.txt", "stdout_sha256"),
        ("stderr.txt", "stderr_sha256"),
    ):
        log_path = stage / name
        if not log_path.is_file() or completed.get(field) != sha256_file(log_path):
            raise CalibrationRunError(
                f"processor staged log hash mismatch: {log_path}")
    lineages = _processor_lineage_inventory(
        run_root, phase, staging_root=stage)
    if completed.get("lineages") != lineages:
        raise CalibrationRunError(
            f"processor staged output inventory changed: {stage}")
    return completed


def _verify_cached_processor_result(
    run_root: Path,
    phase: str,
    result_path: Path,
    stdout_path: Path,
    stderr_path: Path,
    binding: dict[str, object],
    command: list[str],
    processor_hash: str,
    phase_inputs: dict[str, object],
) -> None:
    try:
        prior = json.loads(result_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise CalibrationRunError(f"invalid processor result: {exc}") from exc
    expected_identity = {
        "status": "PASS",
        "phase": phase,
        "binding_run_id": binding["run_id"],
        "command": command,
        "processor_sha256": processor_hash,
        "phase_inputs": phase_inputs,
        "exit_code": 0,
    }
    if any(prior.get(field) != value for field, value in expected_identity.items()):
        raise CalibrationRunError(
            f"prior processor result is not reusable: {result_path}"
        )
    for path, field in (
        (stdout_path, "stdout_sha256"),
        (stderr_path, "stderr_sha256"),
    ):
        if not path.is_file() or prior.get(field) != sha256_file(path):
            raise CalibrationRunError(
                f"cached processor log hash mismatch: {path}"
            )
    current_lineages = _processor_lineage_inventory(run_root, phase)
    if prior.get("lineages") != current_lineages:
        raise CalibrationRunError(
            f"cached processor lineage/output hashes changed: {result_path}"
        )


def _publish_processor_stage(
    run_root: Path,
    stage: Path,
    phase: str,
    binding: dict[str, object],
    command: list[str],
    processor_hash: str,
    phase_inputs: dict[str, object],
    *,
    _test_fault: str | None = None,
) -> None:
    completed = _verify_processor_stage_complete(
        run_root,
        stage,
        phase,
        binding,
        command,
        processor_hash,
        phase_inputs,
    )
    targets = _processor_output_directories(run_root, phase)
    for target in targets:
        if target.exists():
            raise CalibrationRunError(
                f"refusing to overwrite published processor output: {target}")
    moved_count = 0
    for target in targets:
        source = stage / "processed" / target.name
        if not source.is_dir():
            raise CalibrationRunError(
                f"processor stage lacks output directory: {source}")
        target.parent.mkdir(parents=True, exist_ok=True)
        os.replace(source, target)
        _fsync_directory(target.parent)
        moved_count += 1
        if (
            _test_fault == "after_first_output_promotion"
            and moved_count == 1
        ):
            raise _InjectedProcessorInterruption(
                "simulated interruption after first output promotion")
    published_lineages = _processor_lineage_inventory(run_root, phase)
    if published_lineages != completed["lineages"]:
        raise CalibrationRunError(
            "processor output inventory changed during publication")
    if _processor_phase_inputs(run_root, phase) != phase_inputs:
        raise CalibrationRunError(
            "processor phase inputs changed during publication")
    log_dir = run_root / "processor_runs"
    log_dir.mkdir(parents=True, exist_ok=True)
    stdout_path = log_dir / f"{phase}_stdout.txt"
    stderr_path = log_dir / f"{phase}_stderr.txt"
    result_path = log_dir / f"{phase}_result.json"
    for path in (stdout_path, stderr_path, result_path):
        if path.exists():
            raise CalibrationRunError(
                f"refusing to overwrite processor evidence: {path}")
    os.replace(stage / "stdout.txt", stdout_path)
    os.replace(stage / "stderr.txt", stderr_path)
    _fsync_directory(log_dir)
    if _test_fault == "after_logs_promotion":
        raise _InjectedProcessorInterruption(
            "simulated interruption after log promotion")
    payload = {
        "schema_version": PROCESSOR_TRANSACTION_SCHEMA_VERSION,
        "status": "PASS",
        "phase": phase,
        "binding_run_id": binding["run_id"],
        "command": command,
        "processor_sha256": processor_hash,
        "phase_inputs": phase_inputs,
        "exit_code": 0,
        "stdout_sha256": sha256_file(stdout_path),
        "stderr_sha256": sha256_file(stderr_path),
        "lineages": published_lineages,
        "subattempt": stage.relative_to(run_root).as_posix(),
        "stage_complete_sha256": sha256_file(
            stage / "stage_complete.json"),
    }
    _atomic_create(result_path, canonical_json_bytes(payload))
    _atomic_create(
        stage / "published.json",
        canonical_json_bytes({
            "schema_version": PROCESSOR_TRANSACTION_SCHEMA_VERSION,
            "status": "PUBLISHED",
            "phase": phase,
            "result_path": result_path.relative_to(run_root).as_posix(),
            "result_sha256": sha256_file(result_path),
        }),
    )


def _invoke_processor(
    repo: Path,
    run_root: Path,
    phase: str,
    processor: Path,
    executable: Path,
    binding: dict[str, object],
    *,
    _test_fault: str | None = None,
) -> None:
    command = [
        sys.executable,
        str(processor),
        phase,
        "--run-root",
        str(run_root),
    ]
    processor_hash = sha256_file(processor)
    phase_inputs = _processor_phase_inputs(run_root, phase)
    log_dir = run_root / "processor_runs"
    stdout_path = log_dir / f"{phase}_stdout.txt"
    stderr_path = log_dir / f"{phase}_stderr.txt"
    result_path = log_dir / f"{phase}_result.json"
    _assert_live_binding(repo, run_root, executable, binding)
    if result_path.exists():
        try:
            _verify_cached_processor_result(
                run_root,
                phase,
                result_path,
                stdout_path,
                stderr_path,
                binding,
                command,
                processor_hash,
                phase_inputs,
            )
        except CalibrationRunError as exc:
            published_stages = [
                stage for stage in _processor_stages(run_root, phase)
                if (stage / "published.json").exists()
            ]
            _archive_processor_evidence(
                run_root,
                phase,
                f"invalid cached PASS: {exc}",
                stages=published_stages,
                include_canonical=True,
            )
        else:
            _assert_live_binding(repo, run_root, executable, binding)
            return
    elif any(
        path.exists() for path in _canonical_processor_evidence(run_root, phase)
    ):
        existing_stages = _processor_stages(run_root, phase)
        completed_stage_exists = any(
            (stage / "stage_complete.json").is_file()
            for stage in existing_stages
        )
        if not completed_stage_exists:
            legacy_status, legacy_processes = _legacy_processor_liveness(
                processor, run_root, phase)
            if legacy_status != "absent":
                raise ProcessorResumeBlockedError(
                    "processor resume stopped because canonical partial "
                    "evidence lacks a completion lease and the exact child "
                    "may still be alive; no process was manipulated: "
                    f"{legacy_processes}")
        _archive_processor_evidence(
            run_root,
            phase,
            "canonical processor evidence exists without a valid PASS result",
            include_canonical=True,
        )

    for stage in _processor_stages(run_root, phase):
        if (stage / "published.json").exists():
            _archive_processor_evidence(
                run_root,
                phase,
                "published subattempt lacks a reusable canonical PASS",
                stages=(stage,),
            )
            continue
        if (stage / "stage_complete.json").exists():
            try:
                _publish_processor_stage(
                    run_root,
                    stage,
                    phase,
                    binding,
                    command,
                    processor_hash,
                    phase_inputs,
                    _test_fault=_test_fault,
                )
            except _InjectedProcessorInterruption:
                raise
            except CalibrationRunError as exc:
                _archive_processor_evidence(
                    run_root,
                    phase,
                    f"unpublishable completed stage: {exc}",
                    stages=(stage,),
                    include_canonical=True,
                )
                continue
            _verify_cached_processor_result(
                run_root,
                phase,
                result_path,
                stdout_path,
                stderr_path,
                binding,
                command,
                processor_hash,
                phase_inputs,
            )
            return
        liveness, reason = _processor_stage_liveness(stage)
        if liveness in ("active", "unknown"):
            raise ProcessorResumeBlockedError(
                f"processor resume stopped without manipulating child; "
                f"stage={stage} liveness={liveness}: {reason}")
        _archive_processor_evidence(
            run_root,
            phase,
            f"stale interrupted stage: {reason}",
            stages=(stage,),
        )

    stage = _next_processor_directory(
        run_root / "processor_staging" / _processor_phase_slug(phase),
        "subattempt_",
    )
    execution_command = [
        *command,
        "--output-root",
        str(stage),
    ]
    launcher_status, launcher_identity = _query_process_identity(os.getpid())
    lease = {
        "schema_version": PROCESSOR_TRANSACTION_SCHEMA_VERSION,
        "status": "PREPARED",
        "phase": phase,
        "binding_run_id": binding["run_id"],
        "command": execution_command,
        "processor_sha256": processor_hash,
        "phase_inputs": phase_inputs,
        "launcher_pid": os.getpid(),
        "launcher_identity_status": launcher_status,
        "launcher_identity": launcher_identity,
        "child_spawned": False,
        "child_pid": None,
        "child_identity": None,
    }
    lease_path = stage / "in_progress.json"
    _atomic_create(lease_path, canonical_json_bytes(lease))
    staged_stdout = stage / "stdout.txt"
    staged_stderr = stage / "stderr.txt"
    with staged_stdout.open("xb") as stdout, staged_stderr.open("xb") as stderr:
        lease = {**lease, "status": "LOGS_CREATED"}
        _atomic_replace(lease_path, canonical_json_bytes(lease))
        if _test_fault == "after_log_creation":
            stdout.flush()
            stderr.flush()
            os.fsync(stdout.fileno())
            os.fsync(stderr.fileno())
            raise _InjectedProcessorInterruption(
                "simulated interruption after processor log creation")
        lease = {
            **lease,
            "status": "SPAWNING",
            "child_spawned": "unknown",
        }
        _atomic_replace(lease_path, canonical_json_bytes(lease))
        child = subprocess.Popen(
            execution_command,
            cwd=repo,
            stdin=subprocess.DEVNULL,
            stdout=stdout,
            stderr=stderr,
            **_hidden_process_options(),
        )
        child_status, child_identity = _query_process_identity(child.pid)
        if child_status == "unknown":
            raise ProcessorResumeBlockedError(
                "spawned processor identity could not be captured safely; "
                f"stage={stage} pid={child.pid}")
        lease = {
            **lease,
            "status": "RUNNING",
            "child_spawned": True,
            "child_pid": child.pid,
            "child_identity": child_identity,
            "child_identity_status_at_launch": child_status,
        }
        _atomic_replace(lease_path, canonical_json_bytes(lease))
        returncode = child.wait()
        stdout.flush()
        stderr.flush()
        os.fsync(stdout.fileno())
        os.fsync(stderr.fileno())
    lease = {
        **lease,
        "status": "CHILD_EXITED",
        "child_exit_code": returncode,
    }
    _atomic_replace(lease_path, canonical_json_bytes(lease))
    if returncode != 0:
        _atomic_create(
            stage / "child_failure.json",
            canonical_json_bytes({
                "status": "FAIL",
                "phase": phase,
                "exit_code": returncode,
                "stdout_sha256": sha256_file(staged_stdout),
                "stderr_sha256": sha256_file(staged_stderr),
            }),
        )
        recovery = _archive_processor_evidence(
            run_root,
            phase,
            f"processor child failed with exit code {returncode}",
            stages=(stage,),
        )
        raise CalibrationRunError(
            f"{phase} processor failed ({returncode}); evidence: {recovery}")
    _assert_live_binding(repo, run_root, executable, binding)
    if _processor_phase_inputs(run_root, phase) != phase_inputs:
        raise CalibrationRunError(
            "processor phase inputs changed while processor was running")
    lineages = _processor_lineage_inventory(
        run_root, phase, staging_root=stage)
    completed = _processor_stage_complete_payload(
        stage,
        phase,
        binding,
        command,
        execution_command,
        processor_hash,
        phase_inputs,
        lineages,
        returncode,
    )
    _atomic_create(
        stage / "stage_complete.json", canonical_json_bytes(completed))
    if _test_fault == "after_stage_complete":
        raise _InjectedProcessorInterruption(
            "simulated interruption after processor stage completion")
    _publish_processor_stage(
        run_root,
        stage,
        phase,
        binding,
        command,
        processor_hash,
        phase_inputs,
        _test_fault=_test_fault,
    )
    _assert_live_binding(repo, run_root, executable, binding)
    _verify_cached_processor_result(
        run_root,
        phase,
        result_path,
        stdout_path,
        stderr_path,
        binding,
        command,
        processor_hash,
        phase_inputs,
    )


def _failure_classification(
    error: BaseException, mode: str
) -> tuple[str, bool]:
    if isinstance(error, InsufficientDiskError):
        return "recoverable_insufficient_disk", False
    if isinstance(error, ProcessorResumeBlockedError):
        return "recoverable_processor_active_or_ambiguous", False
    if isinstance(error, CalibrationProofError):
        return "proof_failure", True
    if isinstance(error, CalibrationSelfTestError) or mode == "self-test":
        return "self_test_failure", True
    return "production_failure", True


def _record_failure(
    run_root: Path, error: BaseException, mode: str
) -> tuple[Path, dict[str, object]] | None:
    if not run_root.exists():
        return None
    directory = run_root / "automation_failures"
    directory.mkdir(parents=True, exist_ok=True)
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    failure_class, terminal = _failure_classification(error, mode)
    payload = {
        "status": "FAIL" if terminal else "RECOVERABLE_BLOCKED",
        "utc": timestamp,
        "mode": mode,
        "failure_class": failure_class,
        "terminal": terminal,
        "error_type": type(error).__name__,
        "error": str(error),
    }
    path = directory / f"f_{timestamp}_{uuid.uuid4().hex[:8]}.json"
    _atomic_create(path, canonical_json_bytes(payload))
    return path, payload


def _record_terminal_attempt_state(
    run_root: Path,
    failure_path: Path,
    failure: dict[str, object],
) -> None:
    if not failure["terminal"]:
        return
    state_path = run_root / "attempt_state.json"
    if state_path.exists():
        try:
            existing = json.loads(state_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            raise CalibrationRunError(
                f"invalid terminal attempt state: {state_path}: {exc}"
            ) from exc
        if existing.get("terminal") is not True:
            raise CalibrationRunError(
                f"attempt state is not a valid terminal manifest: {state_path}"
            )
        return
    payload = {
        "status": "FAIL",
        "terminal": True,
        "mode": failure["mode"],
        "failure_class": failure["failure_class"],
        "error_type": failure["error_type"],
        "error": failure["error"],
        "utc": failure["utc"],
        "failure_evidence_path": failure_path.relative_to(run_root).as_posix(),
        "failure_evidence_sha256": sha256_file(failure_path),
    }
    _atomic_create(state_path, canonical_json_bytes(payload))


def _assert_attempt_not_terminal(run_root: Path) -> None:
    state_path = run_root / "attempt_state.json"
    if not state_path.exists():
        return
    try:
        state = json.loads(state_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise CalibrationRunError(
            f"cannot read terminal attempt state: {state_path}: {exc}"
        ) from exc
    if state.get("terminal") is not True or state.get("status") != "FAIL":
        raise CalibrationRunError(f"invalid attempt-state manifest: {state_path}")
    raise CalibrationRunError(
        "attempt is terminally failed and cannot be resumed; "
        f"failure_class={state.get('failure_class')}"
    )


def _is_numbered_attempt(path: Path) -> bool:
    return (
        path.name.startswith("attempt_")
        and path.name[8:].isdigit()
        and int(path.name[8:]) > 0
    )


def _resolve_run_root(
    path: Path, *, allow_nested_self_test: bool = False
) -> Path:
    resolved = path.resolve()
    valid = _is_numbered_attempt(resolved)
    if allow_nested_self_test:
        valid = valid or (
            _is_numbered_attempt(resolved.parent)
            and resolved.name not in ("", ".", "..")
        )
    if not valid:
        raise CalibrationRunError(
            "--run-root must name an explicit positive numbered attempt_N "
            "directory (self-test may use one direct child)"
        )
    resolved.mkdir(parents=True, exist_ok=True)
    return resolved


def _synthetic_header() -> list[str]:
    return list(EXPECTED_COLUMNS)


def _synthetic_rows(
    spec: ShardSpec,
    binding: dict[str, object],
    configs: dict[int, dict[str, int]],
) -> list[dict[str, str]]:
    header = _synthetic_header()
    rows = []
    for size_index, (size_name, width, height) in enumerate(contract.MAP_SIZES):
        config = configs[spec.config_start]
        row = {field: "0" for field in header}
        row.update(
            {
                "run_id": str(binding["run_id"]),
                "source_head": str(binding["base_head"]),
                "source_manifest_hash": str(binding["source_manifest_hash"]),
                "executable_hash": str(binding["executable_hash"]),
                "scripts_manifest_hash": str(binding["scripts_manifest_hash"]),
                "config_manifest_hash": str(binding["config_manifest_hash"]),
                "schema_version": str(binding["row_schema_version"]),
                "seed": str(spec.seed),
                "seed_kind": spec.seed_kind,
                "shard_config_start": str(spec.config_start),
                "shard_config_count": str(spec.config_count),
                "shard_row_index": str(size_index),
                "config_index": str(spec.config_start),
                "ocean": "50",
                "continent": "50",
                "relief": "50",
                "vegetation": "50",
                "bias_forest": str(config["bias_forest"]),
                "bias_desert": str(config["bias_desert"]),
                "bias_mountain": "50",
                "bias_wetland": "50",
                "moisture": str(config["moisture"]),
                "drought": str(config["drought"]),
                "random_seed": "0",
                "config_multiplicity": str(config["config_multiplicity"]),
                "map_size": str(size_index),
                "map_size_name": size_name,
                "width": str(width),
                "height": str(height),
                "success": "1",
                "failure_stage": "none",
                "failure_reason": "none",
                "world_diagnostics_valid": "1",
                "land_mask_diagnostics_valid": "1",
                "moisture_diagnostics_valid": "1",
                "river_diagnostics_valid": "1",
                "metrics_valid": "1",
                "attempt_success": "1",
                "physical_hash": "0123456789ABCDEF",
                "world_total_ms": "1",
                "attempt_peak_allocation_bytes": "4096",
                "tile_count": str(width * height),
                "land_mask_tiles": str(width * height // 2),
                "terrestrial_tiles": str(width * height // 2),
                "ocean_tiles": str(width * height // 2),
                "lake_tiles": "0",
                "geo_ocean": str(width * height // 2),
                "geo_plain": str(width * height // 2),
                "climate_oceanic": str(width * height),
                "ecology_none": str(width * height),
                "display_other_transition": str(width * height // 2),
                "temperature_count": str(width * height // 2),
                "moisture_count": str(width * height // 2),
                "precipitation_count": str(width * height // 2),
            }
        )
        rows.append(row)
    return rows


def _write_synthetic_csv(
    path: Path, rows: list[dict[str, str]], header: list[str] | None = None
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fields = _synthetic_header() if header is None else header
    with path.open("x", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle, fieldnames=fields, extrasaction="raise", lineterminator="\n"
        )
        writer.writeheader()
        writer.writerows(rows)
        handle.flush()
        os.fsync(handle.fileno())


def _expect_validation_failure(
    name: str,
    path: Path,
    spec: ShardSpec,
    binding: dict[str, object],
    configs: dict[int, dict[str, int]],
) -> dict[str, str]:
    try:
        validate_shard_csv(path, spec, binding, configs)
    except CalibrationRunError as exc:
        return {"case": name, "status": "PASS", "observed_error": str(exc)}
    raise CalibrationRunError(f"self-test corruption case unexpectedly passed: {name}")


def _run_self_test(repo: Path, run_root: Path) -> None:
    self_test = run_root / "self_test"
    result_path = self_test / "summary.json"
    if result_path.is_file():
        result = json.loads(result_path.read_text(encoding="utf-8"))
        if result.get("status") == "PASS":
            print(json.dumps(result, sort_keys=True))
            return
        raise CalibrationRunError(f"prior self-test failure is preserved: {result_path}")
    if self_test.exists():
        raise CalibrationRunError(
            f"incomplete self-test evidence is preserved; use a new attempt: {self_test}"
        )
    self_test.mkdir(parents=True)

    deterministic_hash_sets = []
    for label in ("e1", "e2"):
        directory = self_test / label
        manifest = contract.materialize_contract(directory)
        deterministic_hash_sets.append(
            {
                name: manifest["files"][name]["sha256"]
                for name in contract.CONTRACT_OUTPUT_NAMES
            }
        )
    if deterministic_hash_sets[0] != deterministic_hash_sets[1]:
        raise CalibrationRunError("independent contract enumerations are not deterministic")

    config_root = self_test / "e1"
    configs = _config_rows(config_root)
    source_first = _source_manifest(repo)
    scripts_first = _scripts_manifest(repo)
    schema_first = _schema_manifest()
    source_second = _source_manifest(repo)
    scripts_second = _scripts_manifest(repo)
    schema_second = _schema_manifest()
    if source_first != source_second:
        raise CalibrationRunError("source manifest is not stable during self-test")
    if scripts_first != scripts_second or schema_first != schema_second:
        raise CalibrationRunError("script/schema binding is not stable during self-test")

    fake_executable = self_test / "fake_world_sim.exe"
    _atomic_create(fake_executable, b"climate-calibration-self-test-fixture\n")
    config_hash = sha256_file(config_root / "config_manifest.json")
    binding_payload = {
        "runner_schema_version": RUNNER_SCHEMA_VERSION,
        "base_head": source_first["base_head"],
        "source_manifest_hash": source_first["manifest_hash"],
        "executable_path": str(fake_executable),
        "executable_bytes": fake_executable.stat().st_size,
        "executable_hash": sha256_file(fake_executable),
        "scripts_manifest_hash": scripts_first["manifest_hash"],
        "config_manifest_hash": config_hash,
        "schema_manifest_hash": schema_first["manifest_hash"],
        "row_schema_version": contract.ROW_SCHEMA_VERSION,
    }
    binding = {
        **binding_payload,
        "run_id": sha256_bytes(canonical_json_bytes(binding_payload))[:24],
    }
    spec = ShardSpec("calibration", contract.CALIBRATION_SEEDS[0], 0, 0, 1)
    command = build_worker_command(fake_executable, Path("synthetic.tmp"), spec, binding)
    required_switches = {
        "--worldgen-climate-calibration-worker",
        "--output",
        "--run-id",
        "--source-head",
        "--source-manifest-hash",
        "--executable-hash",
        "--scripts-manifest-hash",
        "--config-manifest-hash",
        "--schema-version",
        "--seed",
        "--seed-kind",
        "--config-start",
        "--config-count",
    }
    if not required_switches.issubset(command):
        raise CalibrationRunError("worker command construction omitted a bound switch")

    validator_dir = self_test / "validator"
    valid_path = validator_dir / "valid.csv"
    rows = _synthetic_rows(spec, binding, configs)
    _write_synthetic_csv(valid_path, rows)
    validation = validate_shard_csv(valid_path, spec, binding, configs)
    if validation.row_count != 4 or validation.failure_count != 0:
        raise CalibrationRunError("valid synthetic shard did not validate")

    corruption_results = []
    duplicate_path = validator_dir / "duplicate.csv"
    _write_synthetic_csv(duplicate_path, [*rows, rows[0].copy()])
    corruption_results.append(
        _expect_validation_failure(
            "duplicate_row", duplicate_path, spec, binding, configs
        )
    )
    identity_rows = [row.copy() for row in rows]
    identity_rows[0]["executable_hash"] = "BAD"
    identity_path = validator_dir / "identity_mismatch.csv"
    _write_synthetic_csv(identity_path, identity_rows)
    corruption_results.append(
        _expect_validation_failure(
            "identity_mismatch", identity_path, spec, binding, configs
        )
    )
    missing_path = validator_dir / "missing_size.csv"
    _write_synthetic_csv(missing_path, rows[:-1])
    corruption_results.append(
        _expect_validation_failure(
            "missing_size", missing_path, spec, binding, configs
        )
    )
    missing_header = [
        field for field in _synthetic_header() if field != "physical_hash"
    ]
    missing_header_rows = [
        {key: value for key, value in row.items() if key in missing_header}
        for row in rows
    ]
    missing_column_path = validator_dir / "missing_column.csv"
    _write_synthetic_csv(missing_column_path, missing_header_rows, missing_header)
    corruption_results.append(
        _expect_validation_failure(
            "missing_required_column",
            missing_column_path,
            spec,
            binding,
            configs,
        )
    )

    resume_dir = self_test / "resume"
    csv_path = resume_dir / "shard_0000.csv"
    done_path = resume_dir / "shard_0000.done"
    _write_synthetic_csv(csv_path, rows)
    incomplete_rejected = False
    try:
        _verify_done(csv_path, done_path, spec, binding, configs)
    except CalibrationRunError:
        incomplete_rejected = True
    if not incomplete_rejected:
        raise CalibrationRunError("resume verifier skipped a shard without .done")
    resume_valid = validate_shard_csv(csv_path, spec, binding, configs)
    _atomic_create(
        done_path,
        canonical_json_bytes(_done_payload(spec, binding, resume_valid)),
    )
    if _verify_done(csv_path, done_path, spec, binding, configs) is None:
        raise CalibrationRunError("resume verifier did not accept a valid shard")

    corrupt_done_dir = self_test / "resume_corrupt_done"
    corrupt_csv = corrupt_done_dir / "shard_0000.csv"
    corrupt_done = corrupt_done_dir / "shard_0000.done"
    _write_synthetic_csv(corrupt_csv, rows)
    bad_payload = _done_payload(spec, binding, resume_valid)
    bad_payload["csv_sha256"] = "0" * 64
    _atomic_create(corrupt_done, canonical_json_bytes(bad_payload))
    corrupt_done_rejected = False
    try:
        _verify_done(corrupt_csv, corrupt_done, spec, binding, configs)
    except CalibrationRunError:
        corrupt_done_rejected = True
    if not corrupt_done_rejected:
        raise CalibrationRunError("resume verifier accepted a corrupt .done marker")

    projection_dir = self_test / "repeat_projection"
    projection_dir.mkdir()
    repeat_rows = [row.copy() for row in rows]
    for row in repeat_rows:
        for field in EXPECTED_COLUMNS:
            if _is_volatile_column(field) and field != "world_commit_ms":
                row[field] = format(float(row[field] or "0") + 7.0, ".17g")
    repeat_path = projection_dir / "repeat.csv"
    _write_synthetic_csv(repeat_path, repeat_rows)
    repeat_validation = validate_shard_csv(
        repeat_path, spec, binding, configs
    )
    if repeat_validation.projection_sha256 != validation.projection_sha256:
        raise CalibrationRunError(
            "volatile timing changes altered semantic projection digest"
        )
    projection_hashes = [
        _write_repeat_projection(
            valid_path, projection_dir / "projection_1.jsonl"
        ),
        _write_repeat_projection(
            repeat_path, projection_dir / "projection_2.jsonl"
        ),
    ]
    if projection_hashes[0] != projection_hashes[1]:
        raise CalibrationRunError(
            "deterministic projection artifact retained volatile timing"
        )
    changed_rows = [row.copy() for row in rows]
    changed_rows[0]["physical_hash"] = "1123456789ABCDEF"
    changed_path = projection_dir / "semantic_change.csv"
    _write_synthetic_csv(changed_path, changed_rows)
    changed_projection_hash = _write_repeat_projection(
        changed_path, projection_dir / "projection_changed.jsonl"
    )
    if changed_projection_hash == projection_hashes[0]:
        raise CalibrationRunError(
            "deterministic projection did not detect semantic change"
        )

    interruption_dir = self_test / "interruption"
    interruption_dir.mkdir()
    observable_output = interruption_dir / "observable.tmp"
    fixture = (
        "import os,sys,time\n"
        "with open(sys.argv[1], 'wb') as handle:\n"
        " handle.write(b'observable')\n"
        " handle.flush()\n"
        " os.fsync(handle.fileno())\n"
        " time.sleep(30)\n"
    )
    owned = subprocess.Popen(
        [sys.executable, "-c", fixture, str(observable_output)],
        cwd=repo,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        **_hidden_process_options(),
    )
    interruption = _terminate_owned_process_after_output(
        owned,
        observable_output,
        observation_timeout_seconds=5.0,
        termination_timeout_seconds=5.0,
    )
    if owned.poll() is None or interruption["observable_tmp_bytes"] <= 0:
        raise CalibrationRunError(
            "owned-worker interruption self-test did not terminate cleanly"
        )
    completed = subprocess.Popen(
        [sys.executable, "-c", "pass"],
        cwd=repo,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        **_hidden_process_options(),
    )
    natural_completion_rejected = False
    try:
        _terminate_owned_process_after_output(
            completed,
            interruption_dir / "never_created.tmp",
            observation_timeout_seconds=2.0,
            termination_timeout_seconds=2.0,
        )
    except CalibrationRunError:
        natural_completion_rejected = True
    if not natural_completion_rejected or completed.poll() is None:
        raise CalibrationRunError(
            "forced-interruption helper accepted natural completion"
        )

    disk_root = self_test / "disk"
    disk_root.mkdir()
    disk_observations = {
        "sizes": {
            name: {
                "observed_bytes_per_world": 1.0,
                "observed_ms_per_world": 1.0,
            }
            for name, _width, _height in contract.MAP_SIZES
        }
    }
    ample_free = 100 * GIB
    first_disk = _disk_forecast(
        disk_root,
        disk_observations,
        binding,
        _observed_free_bytes=ample_free,
    )
    frozen_forecast = (disk_root / "disk_forecast.json").read_bytes()
    second_disk = _disk_forecast(
        disk_root,
        disk_observations,
        binding,
        _observed_free_bytes=ample_free,
    )
    if (
        frozen_forecast != (disk_root / "disk_forecast.json").read_bytes()
        or first_disk["forecast_sha256"] != second_disk["forecast_sha256"]
    ):
        raise CalibrationRunError("immutable disk forecast changed on resume")
    insufficient_disk_recoverable = False
    try:
        _disk_forecast(
            disk_root,
            disk_observations,
            binding,
            _observed_free_bytes=0,
        )
    except InsufficientDiskError:
        insufficient_disk_recoverable = True
    disk_audits = sorted((disk_root / "disk_audits").glob("disk_audit_*.json"))
    if (
        not insufficient_disk_recoverable
        or len(disk_audits) != 3
        or len({path.name for path in disk_audits}) != 3
        or frozen_forecast != (disk_root / "disk_forecast.json").read_bytes()
    ):
        raise CalibrationRunError(
            "live disk audits were not unique, current, and recoverable"
        )

    fake_processor = self_test / "fake_processor.py"
    fake_processor_source = r"""
import argparse,csv,hashlib,json,os,time
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument("phase")
p.add_argument("--run-root",required=True,type=Path)
p.add_argument("--output-root",required=True,type=Path)
a=p.parse_args()
time.sleep(0.2)
outputs={"qualification":("qualification",),"calibration":("calibration",),
         "full":("holdout","full")}[a.phase]
for output_name in outputs:
    manifest_name=output_name
    manifest=json.loads((a.run_root/"manifests"/
        f"raw_manifest_{manifest_name}.json").read_text(encoding="utf-8"))
    output=a.output_root/"processed"/output_name
    output.mkdir(parents=True,exist_ok=False)
    artifact=output/f"{output_name}_surface.csv"
    payload=f"phase,value\n{output_name},1\n".encode("ascii")
    with artifact.open("xb") as handle:
        handle.write(payload); handle.flush(); os.fsync(handle.fileno())
    rows=[]
    for shard in manifest["shards"]:
        rows.append({"kind":"raw_shard","path":shard["csv_path"],
            "sha256":shard["csv_sha256"],"bytes":shard["csv_bytes"],
            "rows":shard["row_count"],"model_sha256":""})
    rows.append({"kind":"processed_output",
        "path":f"processed/{output_name}/{artifact.name}",
        "sha256":hashlib.sha256(payload).hexdigest().upper(),
        "bytes":len(payload),"rows":"","model_sha256":""})
    lineage=output/"summary_lineage.csv"
    with lineage.open("x",encoding="utf-8",newline="") as handle:
        writer=csv.DictWriter(handle,
            fieldnames=("kind","path","sha256","bytes","rows","model_sha256"),
            lineterminator="\n")
        writer.writeheader(); writer.writerows(rows)
        handle.flush(); os.fsync(handle.fileno())
print(f"synthetic processor PASS: {a.phase}")
""".lstrip()
    _atomic_create(fake_processor, fake_processor_source.encode("ascii"))

    def prepare_processor_root(
        name: str, manifest_names: Sequence[str],
        *, calibration_freeze: bool = False,
    ) -> tuple[Path, dict[str, str]]:
        root = self_test / name
        root.mkdir()
        _atomic_create(
            root / "config_manifest.json",
            (config_root / "config_manifest.json").read_bytes(),
        )
        _atomic_create(
            root / "binding" / "binding.json",
            canonical_json_bytes(binding),
        )
        raw_csv, raw_done = _shard_paths(root, spec)
        _write_synthetic_csv(raw_csv, rows)
        raw_validation = validate_shard_csv(
            raw_csv, spec, binding, configs)
        _atomic_create(
            raw_done,
            canonical_json_bytes(
                _done_payload(spec, binding, raw_validation)),
        )
        validations = {spec.key: raw_validation}
        for manifest_name in manifest_names:
            _raw_manifest(
                root, manifest_name, [spec], validations, binding)
        if calibration_freeze:
            frozen_dir = root / "processed" / "calibration"
            for frozen_name, payload in (
                ("calibration_surface.csv", b"x,y\n0,0\n"),
                ("calibration_surface.sha256", b"A" * 64 + b"\n"),
                ("calibration_model.json", b"{}\n"),
                ("calibration_model.sha256", b"B" * 64 + b"\n"),
                ("calibration_freeze_manifest.json", b"{}\n"),
                ("summary_lineage.csv", b"kind,path,sha256,bytes,rows,model_sha256\n"),
            ):
                _atomic_create(frozen_dir / frozen_name, payload)
        raw_hashes = {
            path.relative_to(root).as_posix(): sha256_file(path)
            for path in sorted((root / "raw").rglob("*"))
            if path.is_file()
        }
        return root, raw_hashes

    log_interrupt_root, log_raw_hashes = prepare_processor_root(
        "p1", ("qualification",))
    log_interrupted = False
    try:
        _invoke_processor(
            repo,
            log_interrupt_root,
            "qualification",
            fake_processor,
            fake_executable,
            binding,
            _test_fault="after_log_creation",
        )
    except _InjectedProcessorInterruption:
        log_interrupted = True
    if not log_interrupted:
        raise CalibrationRunError(
            "processor log-creation interruption was not injected")
    _invoke_processor(
        repo,
        log_interrupt_root,
        "qualification",
        fake_processor,
        fake_executable,
        binding,
    )
    log_retry_raw_hashes = {
        path.relative_to(log_interrupt_root).as_posix(): sha256_file(path)
        for path in sorted((log_interrupt_root / "raw").rglob("*"))
        if path.is_file()
    }
    if log_retry_raw_hashes != log_raw_hashes:
        raise CalibrationRunError(
            "processor retry changed raw shard inputs after log interruption")
    log_stage_count = len(_processor_stages(
        log_interrupt_root, "qualification"))
    _invoke_processor(
        repo,
        log_interrupt_root,
        "qualification",
        fake_processor,
        fake_executable,
        binding,
    )
    if len(_processor_stages(
        log_interrupt_root, "qualification")) != log_stage_count:
        raise CalibrationRunError(
            "cached processor PASS launched an extra subattempt")

    stage_complete_root, stage_complete_raw = prepare_processor_root(
        "p2", ("qualification",))
    stage_complete_interrupted = False
    try:
        _invoke_processor(
            repo,
            stage_complete_root,
            "qualification",
            fake_processor,
            fake_executable,
            binding,
            _test_fault="after_stage_complete",
        )
    except _InjectedProcessorInterruption:
        stage_complete_interrupted = True
    if not stage_complete_interrupted:
        raise CalibrationRunError(
            "processor stage-complete interruption was not injected")
    completed_stage_count = len(_processor_stages(
        stage_complete_root, "qualification"))
    _invoke_processor(
        repo,
        stage_complete_root,
        "qualification",
        fake_processor,
        fake_executable,
        binding,
    )
    if (
        len(_processor_stages(
            stage_complete_root, "qualification")) != completed_stage_count
        or {
            path.relative_to(stage_complete_root).as_posix(): sha256_file(path)
            for path in sorted((stage_complete_root / "raw").rglob("*"))
            if path.is_file()
        } != stage_complete_raw
    ):
        raise CalibrationRunError(
            "completed processor stage was not resumed without raw changes")

    full_interrupt_root, full_raw_hashes = prepare_processor_root(
        "p3",
        ("calibration", "holdout", "full"),
        calibration_freeze=True,
    )
    full_interrupted = False
    try:
        _invoke_processor(
            repo,
            full_interrupt_root,
            "full",
            fake_processor,
            fake_executable,
            binding,
            _test_fault="after_first_output_promotion",
        )
    except _InjectedProcessorInterruption:
        full_interrupted = True
    if (
        not full_interrupted
        or not (full_interrupt_root / "processed" / "holdout").is_dir()
        or (full_interrupt_root / "processed" / "full").exists()
    ):
        raise CalibrationRunError(
            "full promotion interruption did not preserve the expected window")
    _invoke_processor(
        repo,
        full_interrupt_root,
        "full",
        fake_processor,
        fake_executable,
        binding,
    )
    if not all(
        (full_interrupt_root / "processed" / name).is_dir()
        for name in ("holdout", "full")
    ):
        raise CalibrationRunError(
            "full processor retry did not publish both output directories")
    if {
        path.relative_to(full_interrupt_root).as_posix(): sha256_file(path)
        for path in sorted((full_interrupt_root / "raw").rglob("*"))
        if path.is_file()
    } != full_raw_hashes:
        raise CalibrationRunError(
            "full processor recovery changed raw shard inputs")

    processor_logs = log_interrupt_root / "processor_runs"
    processor_stdout = processor_logs / "qualification_stdout.txt"
    processor_stderr = processor_logs / "qualification_stderr.txt"
    processor_result = processor_logs / "qualification_result.json"
    processor_command = [
        sys.executable,
        str(fake_processor),
        "qualification",
        "--run-root",
        str(log_interrupt_root),
    ]
    processor_hash = sha256_file(fake_processor)
    processor_inputs = _processor_phase_inputs(
        log_interrupt_root, "qualification")
    _verify_cached_processor_result(
        log_interrupt_root,
        "qualification",
        processor_result,
        processor_stdout,
        processor_stderr,
        binding,
        processor_command,
        processor_hash,
        processor_inputs,
    )
    processed_fixture = (
        log_interrupt_root / "processed" / "qualification" /
        "qualification_surface.csv"
    )
    _atomic_replace(processed_fixture, b"phase,value\nqualification,2\n")
    cached_output_mutation_rejected = False
    try:
        _verify_cached_processor_result(
            log_interrupt_root,
            "qualification",
            processor_result,
            processor_stdout,
            processor_stderr,
            binding,
            processor_command,
            processor_hash,
            processor_inputs,
        )
    except CalibrationRunError:
        cached_output_mutation_rejected = True
    if not cached_output_mutation_rejected:
        raise CalibrationRunError(
            "cached processor PASS accepted a mutated recorded output")

    promoted_log_root, promoted_log_raw = prepare_processor_root(
        "p4", ("qualification",))
    promoted_log_interrupted = False
    try:
        _invoke_processor(
            repo,
            promoted_log_root,
            "qualification",
            fake_processor,
            fake_executable,
            binding,
            _test_fault="after_logs_promotion",
        )
    except _InjectedProcessorInterruption:
        promoted_log_interrupted = True
    if (
        not promoted_log_interrupted
        or not (
            promoted_log_root / "processed" / "qualification"
        ).is_dir()
        or (
            promoted_log_root / "processor_runs" /
            "qualification_result.json"
        ).exists()
    ):
        raise CalibrationRunError(
            "processor post-promotion interruption window was not preserved")
    _invoke_processor(
        repo,
        promoted_log_root,
        "qualification",
        fake_processor,
        fake_executable,
        binding,
    )
    if {
        path.relative_to(promoted_log_root).as_posix(): sha256_file(path)
        for path in sorted((promoted_log_root / "raw").rglob("*"))
        if path.is_file()
    } != promoted_log_raw:
        raise CalibrationRunError(
            "post-promotion recovery changed raw shard inputs")

    liveness_root = self_test / "p5"
    liveness_stage = _next_processor_directory(
        liveness_root / "processor_staging" / "q",
        "subattempt_",
    )
    sleeper = subprocess.Popen(
        [sys.executable, "-c", "import time; time.sleep(1.0)"],
        cwd=repo,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        **_hidden_process_options(),
    )
    sleeper_status, sleeper_identity = _query_process_identity(sleeper.pid)
    if sleeper_status != "running":
        raise CalibrationRunError(
            "processor liveness fixture did not remain running")
    _atomic_create(
        liveness_stage / "in_progress.json",
        canonical_json_bytes({
            "schema_version": PROCESSOR_TRANSACTION_SCHEMA_VERSION,
            "status": "RUNNING",
            "phase": "qualification",
            "child_spawned": True,
            "child_pid": sleeper.pid,
            "child_identity": sleeper_identity,
        }),
    )
    exact_live_status, _exact_live_reason = _processor_stage_liveness(
        liveness_stage)
    sleeper.wait()
    exact_dead_status, _exact_dead_reason = _processor_stage_liveness(
        liveness_stage)
    if exact_live_status != "active" or exact_dead_status != "dead":
        raise CalibrationRunError(
            "processor exact-child liveness lease was not fail-closed")
    _archive_processor_evidence(
        liveness_root,
        "qualification",
        "self-test exact child exited naturally",
        stages=(liveness_stage,),
    )

    mismatch_stage = _next_processor_directory(
        liveness_root / "processor_staging" / "q",
        "subattempt_",
    )
    mismatch_sleeper = subprocess.Popen(
        [sys.executable, "-c", "import time; time.sleep(1.0)"],
        cwd=repo,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        **_hidden_process_options(),
    )
    mismatch_status, mismatch_identity = _query_process_identity(
        mismatch_sleeper.pid)
    if mismatch_status != "running" or mismatch_identity is None:
        raise CalibrationRunError(
            "processor PID-reuse fixture did not remain running")
    mismatched_identity = dict(mismatch_identity)
    mismatched_identity["creation_token"] = (
        str(mismatched_identity["creation_token"]) + "-MISMATCH")
    _atomic_create(
        mismatch_stage / "in_progress.json",
        canonical_json_bytes({
            "schema_version": PROCESSOR_TRANSACTION_SCHEMA_VERSION,
            "status": "RUNNING",
            "phase": "qualification",
            "child_spawned": True,
            "child_pid": mismatch_sleeper.pid,
            "child_identity": mismatched_identity,
        }),
    )
    pid_reuse_status, _pid_reuse_reason = _processor_stage_liveness(
        mismatch_stage)
    mismatch_sleeper.wait()
    if pid_reuse_status != "dead":
        raise CalibrationRunError(
            "processor liveness accepted a reused/mismatched PID")
    _archive_processor_evidence(
        liveness_root,
        "qualification",
        "self-test PID creation identity mismatch",
        stages=(mismatch_stage,),
    )
    ambiguous_stage = _next_processor_directory(
        liveness_root / "processor_staging" / "q",
        "subattempt_",
    )
    _atomic_create(
        ambiguous_stage / "in_progress.json",
        canonical_json_bytes({
            "schema_version": PROCESSOR_TRANSACTION_SCHEMA_VERSION,
            "status": "SPAWNING",
            "phase": "qualification",
            "child_spawned": "unknown",
            "child_pid": None,
            "child_identity": None,
        }),
    )
    ambiguous_status, _ambiguous_reason = _processor_stage_liveness(
        ambiguous_stage)
    if ambiguous_status != "unknown":
        raise CalibrationRunError(
            "ambiguous processor spawn state was not fail-closed")

    qualification_root = self_test / "qualification_marker"
    raw_manifest = (
        qualification_root
        / "manifests"
        / "raw_manifest_qualification.json"
    )
    _atomic_create(raw_manifest, b"{\"fixture\":true}\n")
    _atomic_create(
        qualification_root / "qualification.complete",
        _qualification_complete_bytes(qualification_root, binding),
    )
    _verify_qualification_complete(qualification_root, binding)
    _assert_attempt_not_terminal(qualification_root)
    corrupt_qualification_root = self_test / "qualification_marker_corrupt"
    corrupt_manifest = (
        corrupt_qualification_root
        / "manifests"
        / "raw_manifest_qualification.json"
    )
    _atomic_create(corrupt_manifest, b"{\"fixture\":true}\n")
    _atomic_create(
        corrupt_qualification_root / "qualification.complete",
        b"{\"status\":\"PASS\"}\n",
    )
    corrupt_qualification_rejected = False
    try:
        _verify_qualification_complete(corrupt_qualification_root, binding)
    except CalibrationRunError:
        corrupt_qualification_rejected = True
    if not corrupt_qualification_rejected:
        raise CalibrationRunError(
            "full transition accepted an inexact qualification.complete"
        )

    terminal_root = self_test / "terminal_state"
    terminal_root.mkdir()
    terminal_record = _record_failure(
        terminal_root,
        CalibrationProofError("synthetic proof failure"),
        "qualify",
    )
    if terminal_record is None:
        raise CalibrationRunError("terminal failure was not recorded")
    _record_terminal_attempt_state(
        terminal_root, terminal_record[0], terminal_record[1]
    )
    terminal_state = json.loads(
        (terminal_root / "attempt_state.json").read_text(encoding="utf-8")
    )
    terminal_resume_rejected = False
    try:
        _assert_attempt_not_terminal(terminal_root)
    except CalibrationRunError:
        terminal_resume_rejected = True
    recoverable_root = self_test / "recoverable_state"
    recoverable_root.mkdir()
    recoverable_record = _record_failure(
        recoverable_root,
        InsufficientDiskError("synthetic insufficient disk"),
        "full",
    )
    if recoverable_record is None:
        raise CalibrationRunError("recoverable failure was not recorded")
    _record_terminal_attempt_state(
        recoverable_root, recoverable_record[0], recoverable_record[1]
    )
    _assert_attempt_not_terminal(recoverable_root)
    if (
        terminal_state.get("failure_class") != "proof_failure"
        or not terminal_resume_rejected
        or (recoverable_root / "attempt_state.json").exists()
        or _failure_classification(
            CalibrationSelfTestError("synthetic"), "self-test"
        ) != ("self_test_failure", True)
    ):
        raise CalibrationRunError(
            "terminal/recoverable attempt-state classification failed"
        )

    recovery_forbidden = {
        "_forced_interruption_resume_proof",
        "_repeat_shard_proof",
        "_run_specs",
        "_run_and_publish",
        "build_worker_command",
    }
    recovery_names = set(_run_processor_only_recovery.__code__.co_names)
    recovery_names.update(_create_or_verify_raw_import.__code__.co_names)
    recovery_names.update(_strict_source_raw_validation.__code__.co_names)
    if recovery_names.intersection(recovery_forbidden):
        raise CalibrationRunError(
            "processor-only recovery references a generation entry point")
    parsed_recovery = _parse_args([
        "recover-processed",
        "--run-root",
        str(run_root.parent / "attempt_16"),
        "--source-attempt",
        str(run_root.parent / "attempt_14"),
    ])
    if (
        parsed_recovery.mode != "recover-processed"
        or parsed_recovery.source_attempt.name != "attempt_14"
    ):
        raise CalibrationRunError("processor-only recovery CLI parse failed")
    corrupt_import_root = self_test / "corrupt_import"
    _atomic_create(
        corrupt_import_root / "raw_import.json",
        canonical_json_bytes({
            "schema_version": RAW_IMPORT_SCHEMA_VERSION,
            "status": "PASS",
            "manifest_payload_sha256": "0" * 64,
        }),
    )
    corrupt_import_rejected = False
    try:
        _load_raw_origin(corrupt_import_root)
    except CalibrationRunError:
        corrupt_import_rejected = True
    if not corrupt_import_rejected:
        raise CalibrationRunError("corrupt raw import manifest was accepted")

    summary = {
        "status": "PASS",
        "runner_schema_version": RUNNER_SCHEMA_VERSION,
        "enumeration_deterministic": True,
        "enumeration_hashes": deterministic_hash_sets[0],
        "raw_arrangements": contract.RAW_ARRANGEMENT_COUNT,
        "effective_configurations": contract.EFFECTIVE_CONFIG_COUNT,
        "source_binding_stable": True,
        "scripts_binding_stable": True,
        "schema_binding_stable": True,
        "worker_command_construction": "PASS",
        "cross_language_expected_column_count": EXPECTED_COLUMN_COUNT,
        "validator_valid_fixture": "PASS",
        "validator_corruption_cases": corruption_results,
        "synthetic_resume_incomplete_rejected": incomplete_rejected,
        "synthetic_resume_valid_accepted": True,
        "synthetic_resume_corrupt_done_rejected": corrupt_done_rejected,
        "forced_interruption_observable_tmp_and_termination": "PASS",
        "forced_interruption_natural_completion_rejected": (
            natural_completion_rejected
        ),
        "repeat_projection_artifact_schema": (
            REPEAT_PROJECTION_SCHEMA_VERSION
        ),
        "repeat_projection_artifacts_identical": True,
        "repeat_projection_semantic_change_detected": True,
        "immutable_disk_forecast": "PASS",
        "live_disk_audit_count": len(disk_audits),
        "live_insufficient_disk_recoverable": insufficient_disk_recoverable,
        "cached_processor_lineage_and_hashes_verified": True,
        "cached_processor_output_mutation_rejected": (
            cached_output_mutation_rejected
        ),
        "processor_interruption_after_log_creation_recovered": (
            log_interrupted
        ),
        "processor_completed_stage_resumed_without_relaunch": (
            stage_complete_interrupted
        ),
        "processor_full_partial_promotion_recovered": full_interrupted,
        "processor_post_promotion_pre_result_recovered": (
            promoted_log_interrupted
        ),
        "processor_retry_preserved_raw_shards": True,
        "processor_cached_pass_launched_no_subattempt": True,
        "processor_exact_child_liveness_fail_closed": (
            exact_live_status == "active" and exact_dead_status == "dead"
        ),
        "processor_pid_creation_identity_mismatch_rejected": (
            pid_reuse_status == "dead"
        ),
        "processor_ambiguous_spawn_fail_closed": (
            ambiguous_status == "unknown"
        ),
        "qualification_complete_exact_payload_verified": True,
        "qualification_to_full_transition_open": True,
        "terminal_proof_failure_recorded": terminal_resume_rejected,
        "recoverable_disk_did_not_terminally_fail_attempt": True,
        "self_test_failure_classification": "PASS",
        "processor_only_recovery_generation_entrypoints_absent": True,
        "processor_only_recovery_cli": "PASS",
        "corrupt_raw_import_rejected": corrupt_import_rejected,
    }
    _atomic_create(result_path, canonical_json_bytes(summary))
    print(json.dumps(summary, sort_keys=True))


def _parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "mode",
        choices=("self-test", "qualify", "full", "recover-processed"),
    )
    parser.add_argument("--run-root", required=True, type=Path)
    parser.add_argument("--workers", type=int, default=2)
    parser.add_argument("--resume", action="store_true")
    parser.add_argument("--source-attempt", type=Path)
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--processor", type=Path)
    return parser.parse_args(argv)


def _processor_recovery_complete_bytes(
    run_root: Path,
    binding: dict[str, object],
    origin: RawOrigin,
    phase: str,
    model_hash: str = "",
) -> bytes:
    if not origin.imported:
        raise CalibrationRunError("processor recovery requires imported raw data")
    manifest_name = "qualification" if phase == "qualification" else "full"
    raw_manifest = (
        origin.root / "manifests" / f"raw_manifest_{manifest_name}.json")
    payload: dict[str, object] = {
        "status": "PASS",
        "mode": "processor-only-recovery",
        "phase": phase,
        "processing_binding_run_id": binding["run_id"],
        "raw_binding_run_id": origin.binding["run_id"],
        "raw_import_sha256": origin.import_sha256,
        "raw_manifest_path": raw_manifest.relative_to(
            origin.root).as_posix(),
        "raw_manifest_sha256": sha256_file(raw_manifest),
        "generation_launch_count": 0,
        "world_count": (
            contract.ONE_SEED_WORLD_COUNT
            if phase == "qualification" else contract.FULL_WORLD_COUNT),
    }
    if model_hash:
        payload["calibration_model_sha256"] = model_hash
        payload["conceptual_weighted_raw_cases"] = (
            contract.CONCEPTUAL_WEIGHTED_WORLD_COUNT)
    return canonical_json_bytes(payload)


def _run_processor_only_recovery(
    repo: Path,
    run_root: Path,
    source_attempt: Path,
    processor: Path,
    executable: Path,
    binding: dict[str, object],
) -> None:
    if (run_root / "raw").exists():
        raise CalibrationRunError(
            "processor-only recovery refuses a destination raw directory")
    origin = _create_or_verify_raw_import(
        repo, run_root, source_attempt, binding)
    _assert_live_binding(repo, run_root, executable, binding)
    _invoke_processor(
        repo, run_root, "qualification", processor, executable, binding)
    _create_or_verify(
        run_root / "qualification.complete",
        _processor_recovery_complete_bytes(
            run_root, binding, origin, "qualification"),
    )
    _assert_live_binding(repo, run_root, executable, binding)
    if _load_raw_origin(run_root) != origin:
        raise CalibrationRunError("raw import changed after qualification")
    _invoke_processor(
        repo, run_root, "calibration", processor, executable, binding)
    model_path = (
        run_root / "processed" / "calibration" / "calibration_model.json")
    model_hash_path = model_path.with_suffix(".sha256")
    if not model_path.is_file() or not model_hash_path.is_file():
        raise CalibrationRunError(
            "recovered calibration processor did not freeze its model")
    model_hash = sha256_file(model_path)
    if model_hash_path.read_text(
            encoding="ascii").strip().split()[0].upper() != model_hash:
        raise CalibrationRunError("recovered calibration model hash mismatch")
    _assert_live_binding(repo, run_root, executable, binding)
    if _load_raw_origin(run_root) != origin:
        raise CalibrationRunError("raw import changed after calibration")
    _invoke_processor(repo, run_root, "full", processor, executable, binding)
    _assert_live_binding(repo, run_root, executable, binding)
    if _load_raw_origin(run_root) != origin:
        raise CalibrationRunError("raw import changed after full processing")
    if (run_root / "raw").exists():
        raise CalibrationRunError(
            "processor-only recovery unexpectedly created raw output")
    _create_or_verify(
        run_root / "full.complete",
        _processor_recovery_complete_bytes(
            run_root, binding, origin, "full", model_hash),
    )


def run(arguments: argparse.Namespace) -> None:
    if arguments.workers < 1 or arguments.workers > 2:
        raise CalibrationRunError("--workers must be 1 or 2")
    if arguments.mode == "full" and not arguments.resume:
        raise CalibrationRunError("full mode requires --resume")
    if arguments.mode == "recover-processed":
        if arguments.source_attempt is None:
            raise CalibrationRunError(
                "recover-processed requires --source-attempt")
        if arguments.resume:
            raise CalibrationRunError(
                "recover-processed resumes transactionally without --resume")
    elif arguments.source_attempt is not None:
        raise CalibrationRunError(
            "--source-attempt is valid only for recover-processed")
    repo = _repo_root()
    run_root = _resolve_run_root(
        arguments.run_root,
        allow_nested_self_test=arguments.mode == "self-test",
    )
    _assert_attempt_not_terminal(run_root)
    if arguments.mode == "self-test":
        try:
            _run_self_test(repo, run_root)
        except (
            CalibrationRunError,
            contract.ContractError,
            OSError,
        ) as exc:
            raise CalibrationSelfTestError(str(exc)) from exc
        return
    executable = (
        arguments.executable.resolve()
        if arguments.executable
        else (repo / "world_sim.exe").resolve()
    )
    processor = (
        arguments.processor.resolve()
        if arguments.processor
        else (repo / "tools" / "process_worldgen_climate_calibration.py").resolve()
    )
    contract.materialize_contract(run_root)
    binding = _freeze_or_verify_binding(repo, run_root, executable)
    configs = _config_rows(run_root)
    _assert_live_binding(repo, run_root, executable, binding)

    if arguments.mode == "recover-processed":
        _run_processor_only_recovery(
            repo,
            run_root,
            arguments.source_attempt,
            processor,
            executable,
            binding,
        )
        return

    qualification_specs = _all_shards(
        "calibration", (contract.CALIBRATION_SEEDS[0],)
    )
    try:
        _forced_interruption_resume_proof(
            repo, run_root, executable, binding, configs
        )
        _repeat_shard_proof(repo, run_root, executable, binding, configs)
    except (CalibrationRunError, OSError) as exc:
        raise CalibrationProofError(str(exc)) from exc
    _assert_live_binding(repo, run_root, executable, binding)
    qualification = _run_specs(
        "qualification",
        repo,
        run_root,
        executable,
        qualification_specs,
        arguments.workers,
        arguments.resume,
        binding,
        configs,
    )
    qualification_manifest = _raw_manifest(
        run_root, "qualification", qualification_specs, qualification, binding
    )
    if qualification_manifest["row_count"] != contract.ONE_SEED_WORLD_COUNT:
        raise CalibrationRunError("qualification world count mismatch")
    if qualification_manifest["failure_count"] != 0:
        raise CalibrationRunError(
            "qualification contains valid-config generation failures"
        )
    observations = _qualification_observations(run_root, qualification, binding)
    _invoke_processor(
        repo, run_root, "qualification", processor, executable, binding
    )
    if arguments.mode == "qualify":
        _assert_live_binding(repo, run_root, executable, binding)
        _create_or_verify(
            run_root / "qualification.complete",
            _qualification_complete_bytes(run_root, binding),
        )
        return

    _verify_qualification_complete(run_root, binding)
    _disk_forecast(run_root, observations, binding)
    calibration_specs = _all_shards(
        "calibration", contract.CALIBRATION_SEEDS
    )
    calibration = _run_specs(
        "calibration",
        repo,
        run_root,
        executable,
        calibration_specs,
        arguments.workers,
        True,
        binding,
        configs,
    )
    calibration_manifest = _raw_manifest(
        run_root, "calibration", calibration_specs, calibration, binding
    )
    if calibration_manifest["row_count"] != contract.CALIBRATION_WORLD_COUNT:
        raise CalibrationRunError("calibration world count mismatch")
    if calibration_manifest["failure_count"] != 0:
        raise CalibrationRunError("calibration contains generation failures")
    _invoke_processor(
        repo, run_root, "calibration", processor, executable, binding
    )
    model_path = (
        run_root / "processed" / "calibration" / "calibration_model.json"
    )
    if not model_path.is_file():
        raise CalibrationRunError(
            "calibration processor did not freeze calibration_model.json"
        )
    model_hash = sha256_file(model_path)
    model_hash_path = model_path.with_suffix(".sha256")
    if not model_hash_path.is_file():
        raise CalibrationRunError(
            "calibration processor did not freeze calibration_model.sha256"
        )
    recorded_model_hash = model_hash_path.read_text(
        encoding="ascii"
    ).strip().split()[0].upper()
    if recorded_model_hash != model_hash:
        raise CalibrationRunError("calibration model hash file does not match model")
    _assert_live_binding(repo, run_root, executable, binding)

    holdout_specs = _all_shards("holdout", contract.HOLDOUT_SEEDS)
    holdout = _run_specs(
        "holdout",
        repo,
        run_root,
        executable,
        holdout_specs,
        arguments.workers,
        True,
        binding,
        configs,
    )
    holdout_manifest = _raw_manifest(
        run_root, "holdout", holdout_specs, holdout, binding
    )
    if holdout_manifest["row_count"] != contract.HOLDOUT_WORLD_COUNT:
        raise CalibrationRunError("holdout world count mismatch")
    if holdout_manifest["failure_count"] != 0:
        raise CalibrationRunError("holdout contains generation failures")
    if sha256_file(model_path) != model_hash:
        raise CalibrationRunError("calibration model changed before holdout evaluation")

    full_specs = calibration_specs + holdout_specs
    full_validations = {**calibration, **holdout}
    full_manifest = _raw_manifest(
        run_root, "full", full_specs, full_validations, binding
    )
    if full_manifest["row_count"] != contract.FULL_WORLD_COUNT:
        raise CalibrationRunError("full world count mismatch")
    if full_manifest["failure_count"] != 0:
        raise CalibrationRunError("full dataset contains generation failures")
    _invoke_processor(repo, run_root, "full", processor, executable, binding)
    _assert_live_binding(repo, run_root, executable, binding)
    _create_or_verify(
        run_root / "full.complete",
        canonical_json_bytes(
            {
                "status": "PASS",
                "binding_run_id": binding["run_id"],
                "world_count": contract.FULL_WORLD_COUNT,
                "conceptual_weighted_raw_cases": (
                    contract.CONCEPTUAL_WEIGHTED_WORLD_COUNT
                ),
                "calibration_model_sha256": model_hash,
                "raw_manifest_sha256": sha256_file(
                    run_root / "manifests" / "raw_manifest_full.json"
                ),
            }
        ),
    )


def main(argv: list[str] | None = None) -> int:
    arguments = _parse_args(sys.argv[1:] if argv is None else argv)
    run_root = arguments.run_root.resolve()
    try:
        run(arguments)
    except (CalibrationRunError, contract.ContractError, OSError) as exc:
        try:
            recorded = _record_failure(run_root, exc, arguments.mode)
            if recorded is not None:
                failure_path, failure = recorded
                _record_terminal_attempt_state(
                    run_root, failure_path, failure
                )
        except Exception as recording_error:
            print(
                f"warning: could not record failure evidence: {recording_error}",
                file=sys.stderr,
            )
        print(f"calibration run error: {exc}", file=sys.stderr)
        return 2
    print(
        json.dumps(
            {
                "status": "PASS",
                "mode": arguments.mode,
                "run_root": str(run_root),
            },
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
