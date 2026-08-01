#!/usr/bin/env python3
"""Process deterministic world-generation climate-calibration shard evidence.

The processor consumes only completed, hash-verified CSV shards.  It applies
the raw-quadrilateral multiplicities emitted by the contract generator,
constructs the measured 9x9 corner-coordinate surfaces, freezes calibration
before reading holdout data, and writes deterministic CSV/JSON/Markdown/SVG
evidence without third-party packages.
"""

from __future__ import annotations

import argparse
import csv
import gzip
import hashlib
import json
import math
import os
import statistics
import sys
import tempfile
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable, Iterator, Mapping, Sequence


CALIBRATION_SEEDS = (2026072301, 2026072302, 2026072303, 2026072304)
HOLDOUT_SEEDS = (2026072391, 2026072392)
QUALIFICATION_SEEDS = (2026072301,)
MAP_SIZES = ("Small", "Medium", "Large", "Extreme")
GRID = (-50, -38, -25, -12, 0, 12, 25, 38, 50)
GROUPS = (
    "icefield",
    "tundra",
    "tropical_rainforest",
    "monsoon",
    "desert",
    "forest",
    "temperate_grassland",
    "other_transition",
)
GROUP_LABELS = {
    "icefield": "Icefield",
    "tundra": "Tundra",
    "tropical_rainforest": "Tropical Rainforest",
    "monsoon": "Monsoon",
    "desert": "Desert",
    "forest": "Forest",
    "temperate_grassland": "Temperate Grassland",
    "other_transition": "Other / Transition",
}
GROUP_LABELS_ZH = {
    "icefield": "冰原",
    "tundra": "苔原",
    "tropical_rainforest": "热带雨林",
    "monsoon": "季风区",
    "desert": "沙漠",
    "forest": "森林",
    "temperate_grassland": "温带草原",
    "other_transition": "其他 / 过渡",
}
CONTINUOUS = ("temperature", "moisture", "precipitation")
ROLE_ORDER = ("TL", "TR", "BL", "BR")
ROLE_ALIASES = {
    "TL": "TL", "TOP_LEFT": "TL", "TOP-LEFT": "TL",
    "TR": "TR", "TOP_RIGHT": "TR", "TOP-RIGHT": "TR",
    "BL": "BL", "BOTTOM_LEFT": "BL", "BOTTOM-LEFT": "BL",
    "BR": "BR", "BOTTOM_RIGHT": "BR", "BOTTOM-RIGHT": "BR",
}
LABEL_ANCHORS = {
    "icefield": (-38, -25),
    "tundra": (-38, 25),
    "temperate_grassland": (0, 0),
    "desert": (25, -25),
    "forest": (0, 25),
    "monsoon": (25, 0),
    "tropical_rainforest": (25, 38),
}
EPSILON = 1e-15
RAW_IMPORT_SCHEMA_VERSION = "worldgen-climate-raw-import-v1"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def canonical_json_bytes(value: Any) -> bytes:
    return (json.dumps(
        value, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    ) + "\n").encode("utf-8")


def binding_json_bytes(value: Any) -> bytes:
    return (json.dumps(
        value, ensure_ascii=True, sort_keys=True, separators=(",", ":")
    ) + "\n").encode("ascii")


def write_bytes_new(path: Path, payload: bytes) -> None:
    if path.exists():
        raise FileExistsError(f"refusing to overwrite processed evidence: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(prefix=".tmp.", dir=path.parent)
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "wb") as output:
            output.write(payload)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
    except BaseException:
        try:
            temporary.unlink()
        except FileNotFoundError:
            pass
        raise


def write_text_new(path: Path, text: str) -> None:
    write_bytes_new(path, text.encode("utf-8"))


def write_json_new(path: Path, value: Any) -> None:
    write_bytes_new(path, canonical_json_bytes(value))


def csv_bytes(rows: Sequence[Mapping[str, Any]], fields: Sequence[str]) -> bytes:
    import io
    buffer = io.StringIO(newline="")
    writer = csv.DictWriter(
        buffer, fieldnames=list(fields), extrasaction="raise", lineterminator="\n")
    writer.writeheader()
    for row in rows:
        writer.writerow({field: format_value(row.get(field, "")) for field in fields})
    return buffer.getvalue().encode("utf-8")


def write_csv_new(
    path: Path, rows: Sequence[Mapping[str, Any]], fields: Sequence[str]
) -> None:
    write_bytes_new(path, csv_bytes(rows, fields))


def format_value(value: Any) -> Any:
    if isinstance(value, float):
        if not math.isfinite(value):
            return ""
        if abs(value) < 5e-16:
            value = 0.0
        return f"{value:.12f}"
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, (dict, list, tuple)):
        return json.dumps(value, ensure_ascii=False, sort_keys=True,
                          separators=(",", ":"))
    return value


def first_present(row: Mapping[str, str], names: Sequence[str],
                  default: str | None = None) -> str:
    for name in names:
        if name in row and row[name] != "":
            return row[name]
    if default is not None:
        return default
    raise KeyError(f"none of the required fields are present: {', '.join(names)}")


def parse_int(row: Mapping[str, str], *names: str, default: int | None = None) -> int:
    fallback = None if default is None else str(default)
    value = first_present(row, names, fallback)
    return int(value, 0)


def parse_float(
    row: Mapping[str, str], *names: str, default: float | None = None
) -> float:
    fallback = None if default is None else repr(default)
    value = first_present(row, names, fallback)
    return float(value)


def parse_truth(value: str) -> bool:
    return value.strip().lower() in {"1", "true", "yes", "pass", "ok"}


def normalize_map_size(value: str) -> str:
    normalized = value.strip().lower()
    numeric = {"0": "Small", "1": "Medium", "2": "Large", "3": "Extreme"}
    if normalized in numeric:
        return numeric[normalized]
    for size in MAP_SIZES:
        if normalized == size.lower():
            return size
    raise ValueError(f"unknown map size: {value!r}")


@dataclass(frozen=True)
class RawRecord:
    config_index: int
    multiplicity: int
    terrestrial: int
    groups: tuple[int, ...]
    continuous: tuple[float, ...]


@dataclass(frozen=True)
class Shard:
    path: Path
    done_path: Path
    logical_path: str
    dataset: str
    seed: int
    sha256: str
    row_count: int


@dataclass(frozen=True)
class RawOrigin:
    root: Path
    imported: bool
    binding: Mapping[str, Any] | None
    import_sha256: str

    def resolve(self, relative: str) -> Path:
        logical = Path(relative)
        if logical.is_absolute():
            raise RuntimeError(f"raw path must be relative: {relative}")
        resolved = (self.root / logical).resolve()
        if not resolved.is_relative_to(self.root):
            raise RuntimeError(f"raw path escapes its source attempt: {relative}")
        return resolved


def load_raw_origin(run_root: Path) -> RawOrigin:
    import_path = run_root / "raw_import.json"
    if not import_path.is_file():
        binding_path = run_root / "binding" / "binding.json"
        binding = (
            json.loads(binding_path.read_text(encoding="utf-8"))
            if binding_path.is_file() else None
        )
        return RawOrigin(run_root, False, binding, "")
    raw_bytes = import_path.read_bytes()
    manifest = json.loads(raw_bytes.decode("utf-8"))
    recorded_hash = manifest.get("manifest_payload_sha256")
    payload = dict(manifest)
    payload.pop("manifest_payload_sha256", None)
    calculated_hash = hashlib.sha256(
        canonical_json_bytes(payload)).hexdigest().upper()
    if (
        manifest.get("schema_version") != RAW_IMPORT_SCHEMA_VERSION
        or manifest.get("status") != "PASS"
        or recorded_hash != calculated_hash
    ):
        raise RuntimeError(f"invalid raw import manifest: {import_path}")
    source_root = Path(str(manifest.get("source_attempt_path", ""))).resolve()
    if not source_root.is_dir() or source_root == run_root:
        raise RuntimeError("raw import source attempt is missing or self-referential")
    binding_relative = str(manifest.get("source_binding_path", ""))
    binding_path = (source_root / binding_relative).resolve()
    if not binding_path.is_relative_to(source_root) or not binding_path.is_file():
        raise RuntimeError("raw import source binding is missing or outside source")
    if sha256_file(binding_path) != str(
            manifest.get("source_binding_sha256", "")).upper():
        raise RuntimeError("raw import source binding hash mismatch")
    binding = json.loads(binding_path.read_text(encoding="utf-8"))
    if binding.get("run_id") != manifest.get("source_binding_run_id"):
        raise RuntimeError("raw import source binding identity mismatch")
    destination_binding = run_root / "binding" / "binding.json"
    if (
        not destination_binding.is_file()
        or sha256_file(destination_binding) != str(
            manifest.get("processing_binding_sha256", "")).upper()
    ):
        raise RuntimeError("raw import processing binding hash mismatch")
    return RawOrigin(source_root, True, binding, sha256_file(import_path))


@dataclass
class WeightContract:
    multiplicities: dict[int, int]
    conditions: dict[tuple[str, int, int], tuple[tuple[int, int], ...]]

    @property
    def config_count(self) -> int:
        return len(self.multiplicities)

    def coordinate_weights(self, x: int, y: int) -> list[tuple[int, int]]:
        roles = roles_for_coordinate(x, y)
        combined: dict[int, int] = defaultdict(int)
        for role in roles:
            key = (role, x, y)
            try:
                entries = self.conditions[key]
            except KeyError as exc:
                raise ValueError(f"missing corner-condition weights for {key}") from exc
            for config_index, weight in entries:
                combined[config_index] += weight
        return sorted(combined.items())


def roles_for_coordinate(x: int, y: int) -> tuple[str, ...]:
    if x < 0 and y > 0:
        return ("TL",)
    if x > 0 and y > 0:
        return ("TR",)
    if x < 0 and y < 0:
        return ("BL",)
    if x > 0 and y < 0:
        return ("BR",)
    if x == 0 and y > 0:
        return ("TL", "TR")
    if x == 0 and y < 0:
        return ("BL", "BR")
    if x < 0 and y == 0:
        return ("TL", "BL")
    if x > 0 and y == 0:
        return ("TR", "BR")
    if x == 0 and y == 0:
        return ROLE_ORDER
    raise ValueError(f"coordinate outside the 9x9 role contract: {(x, y)}")


def open_csv(path: Path):
    if path.suffix.lower() == ".gz":
        return gzip.open(path, "rt", encoding="utf-8", newline="")
    return path.open("r", encoding="utf-8", newline="")


def load_weight_contract(run_root: Path) -> WeightContract:
    multiplicity_path = run_root / "effective_config_multiplicities.csv"
    condition_path = run_root / "corner_condition_weights.csv.gz"
    if not multiplicity_path.is_file():
        raise FileNotFoundError(multiplicity_path)
    if not condition_path.is_file():
        raise FileNotFoundError(condition_path)
    manifest_path = run_root / "config_manifest.json"
    if not manifest_path.is_file():
        raise FileNotFoundError(manifest_path)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest_digest = manifest.get("manifest_payload_sha256")
    manifest_payload = dict(manifest)
    manifest_payload.pop("manifest_payload_sha256", None)
    if not isinstance(manifest_digest, str) or (
            hashlib.sha256(binding_json_bytes(manifest_payload))
            .hexdigest().upper() != manifest_digest.upper()):
        raise RuntimeError(f"config manifest payload hash mismatch: {manifest_path}")
    files = manifest.get("files")
    if not isinstance(files, dict):
        raise RuntimeError(f"config manifest has no file inventory: {manifest_path}")
    for name in (
        "effective_configs.csv", "effective_config_multiplicities.csv",
        "corner_condition_weights.csv.gz", "enumeration_proofs.json",
    ):
        metadata = files.get(name)
        path = run_root / name
        if not isinstance(metadata, dict) or not path.is_file():
            raise RuntimeError(f"config manifest is missing {name}")
        if str(metadata.get("sha256", "")).upper() != sha256_file(path):
            raise RuntimeError(f"config manifest file hash mismatch: {path}")

    multiplicities: dict[int, int] = {}
    with multiplicity_path.open("r", encoding="utf-8", newline="") as source:
        for row in csv.DictReader(source):
            index = parse_int(row, "config_index")
            weight = parse_int(row, "config_multiplicity", "multiplicity", "weight")
            if index in multiplicities:
                raise ValueError(f"duplicate effective config index {index}")
            if weight <= 0:
                raise ValueError(f"nonpositive effective multiplicity for {index}")
            multiplicities[index] = weight
    if not multiplicities or sorted(multiplicities) != list(range(len(multiplicities))):
        raise ValueError("effective config indices are not contiguous from zero")
    if len(multiplicities) == 28561 and sum(multiplicities.values()) != 390625:
        raise ValueError("effective config multiplicities do not sum to 390625")

    mutable: dict[tuple[str, int, int], list[tuple[int, int]]] = defaultdict(list)
    with gzip.open(condition_path, "rt", encoding="utf-8", newline="") as source:
        for row in csv.DictReader(source):
            role_raw = first_present(row, ("corner_role", "role", "corner")).upper()
            role = ROLE_ALIASES.get(role_raw)
            if role is None:
                raise ValueError(f"unknown corner role {role_raw!r}")
            x = parse_int(row, "x", "corner_x")
            y = parse_int(row, "y", "corner_y")
            index = parse_int(row, "config_index")
            weight = parse_int(
                row, "weight", "conditional_multiplicity", "condition_weight")
            if index not in multiplicities or weight <= 0:
                raise ValueError("corner table references an invalid config or weight")
            mutable[(role, x, y)].append((index, weight))

    conditions: dict[tuple[str, int, int], tuple[tuple[int, int], ...]] = {}
    for key, entries in mutable.items():
        entries.sort()
        if len({index for index, _ in entries}) != len(entries):
            raise ValueError(f"duplicate config in corner condition {key}")
        total = sum(weight for _, weight in entries)
        if len(multiplicities) == 28561 and total != 15625:
            raise ValueError(f"corner condition {key} sums to {total}, not 15625")
        conditions[key] = tuple(entries)
    expected_conditions = {
        (role, x, y)
        for x in GRID for y in GRID for role in roles_for_coordinate(x, y)
    }
    missing = sorted(expected_conditions - set(conditions))
    if missing:
        raise ValueError(f"corner-condition table is missing {len(missing)} grid roles")
    return WeightContract(multiplicities, conditions)


def done_value(done: Mapping[str, Any], names: Sequence[str]) -> Any:
    for name in names:
        if name in done:
            return done[name]
    return None


def discover_shards(
    run_root: Path,
    dataset: str,
    seeds: Sequence[int],
    origin: RawOrigin,
) -> list[Shard]:
    directory_aliases = {
        "calibration": ("calibration", "cal"),
        "holdout": ("holdout", "hold"),
    }
    candidates = [
        origin.root / "raw" / name
        for name in directory_aliases.get(dataset, (dataset,))
    ]
    raw_root = next((path for path in candidates if path.is_dir()), candidates[0])
    if not raw_root.is_dir():
        raise FileNotFoundError(raw_root)
    shards: list[Shard] = []
    for seed in seeds:
        seed_candidates = (raw_root / f"seed_{seed}", raw_root / str(seed))
        seed_root = next(
            (path for path in seed_candidates if path.is_dir()),
            seed_candidates[0])
        if not seed_root.is_dir():
            raise FileNotFoundError(seed_root)
        paths = sorted(seed_root.glob("shard_*.csv"))
        paths += sorted(seed_root.glob("shard_*.csv.gz"))
        if not paths:
            raise RuntimeError(f"no completed shard CSVs found in {seed_root}")
        for path in paths:
            if path.name.endswith(".csv.gz"):
                done_path = path.with_name(path.name[:-7] + ".done")
            else:
                done_path = path.with_suffix(".done")
            if not done_path.is_file():
                raise RuntimeError(f"shard has no adjacent completion marker: {path}")
            done = json.loads(done_path.read_text(encoding="utf-8"))
            if done.get("status") != "PASS":
                raise RuntimeError(f"shard completion marker is not PASS: {done_path}")
            if origin.binding is not None and done.get("binding_run_id") != (
                    origin.binding.get("run_id")):
                raise RuntimeError(
                    f"shard completion binding mismatch: {done_path}")
            expected_hash = done_value(
                done, ("sha256", "csv_sha256", "file_sha256", "output_sha256"))
            expected_rows = done_value(done, ("row_count", "rows", "output_rows"))
            actual_hash = sha256_file(path)
            if not isinstance(expected_hash, str) or (
                    expected_hash.upper() != actual_hash):
                raise RuntimeError(f"shard hash mismatch: {path}")
            if expected_rows is None:
                raise RuntimeError(f"completion marker has no row count: {done_path}")
            shard_identity = done.get("shard", {})
            if not isinstance(shard_identity, dict):
                raise RuntimeError(
                    f"completion marker shard identity is invalid: {done_path}")
            done_seed = done_value(done, ("seed",))
            if done_seed is None:
                done_seed = shard_identity.get("seed")
            if done_seed is not None and int(done_seed) != seed:
                raise RuntimeError(f"completion marker seed mismatch: {done_path}")
            done_kind = shard_identity.get("seed_kind")
            if done_kind is not None and str(done_kind) != dataset:
                raise RuntimeError(
                    f"completion marker seed kind mismatch: {done_path}")
            world_count = shard_identity.get("world_count")
            if world_count is not None and int(world_count) != int(expected_rows):
                raise RuntimeError(
                    f"completion marker world count mismatch: {done_path}")
            shards.append(Shard(
                path=path,
                done_path=done_path,
                logical_path=path.resolve().relative_to(
                    origin.root).as_posix(),
                dataset=dataset,
                seed=seed,
                sha256=actual_hash, row_count=int(expected_rows)))
    return shards


def verify_phase_manifest(
    run_root: Path,
    phase: str,
    shards: Sequence[Shard],
    origin: RawOrigin,
) -> None:
    manifest_path = (
        origin.root / "manifests" / f"raw_manifest_{phase}.json")
    if not manifest_path.is_file():
        raise FileNotFoundError(manifest_path)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    recorded_digest = manifest.get("manifest_payload_sha256")
    payload = dict(manifest)
    payload.pop("manifest_payload_sha256", None)
    if not isinstance(recorded_digest, str) or (
            hashlib.sha256(binding_json_bytes(payload)).hexdigest().upper() !=
            recorded_digest.upper()):
        raise RuntimeError(f"raw manifest payload hash mismatch: {manifest_path}")
    entries = manifest.get("shards")
    if not isinstance(entries, list):
        raise RuntimeError(f"raw manifest has no shard list: {manifest_path}")
    expected: dict[Path, Mapping[str, Any]] = {}
    for entry in entries:
        if not isinstance(entry, dict) or "csv_path" not in entry:
            raise RuntimeError(f"raw manifest has an invalid shard: {manifest_path}")
        path = origin.resolve(str(entry["csv_path"]))
        if path in expected:
            raise RuntimeError(f"raw manifest duplicates shard path: {path}")
        expected[path] = entry
    observed = {shard.path.resolve(): shard for shard in shards}
    if set(expected) != set(observed):
        missing = sorted(str(path) for path in set(expected) - set(observed))
        extra = sorted(str(path) for path in set(observed) - set(expected))
        raise RuntimeError(
            f"raw manifest shard-set mismatch; missing={missing[:3]} "
            f"extra={extra[:3]}")
    for path, shard in observed.items():
        entry = expected[path]
        if str(entry.get("csv_sha256", "")).upper() != shard.sha256:
            raise RuntimeError(f"raw manifest shard hash mismatch: {path}")
        if int(entry.get("row_count", -1)) != shard.row_count:
            raise RuntimeError(f"raw manifest shard row mismatch: {path}")
        if int(entry.get("seed", -1)) != shard.seed:
            raise RuntimeError(f"raw manifest shard seed mismatch: {path}")
        if str(entry.get("seed_kind", "")) != shard.dataset:
            raise RuntimeError(f"raw manifest shard kind mismatch: {path}")
    if int(manifest.get("row_count", -1)) != sum(
            shard.row_count for shard in shards):
        raise RuntimeError(f"raw manifest aggregate row mismatch: {manifest_path}")
    if origin.binding is not None and (
        manifest.get("binding_run_id") != origin.binding.get("run_id")
        or manifest.get("row_schema_version") !=
            origin.binding.get("row_schema_version")
    ):
        raise RuntimeError(f"raw manifest binding mismatch: {manifest_path}")


def group_count(row: Mapping[str, str], group: str) -> int:
    aliases = (
        f"group_{group}",
        f"display_{group}",
        f"group_{group}_count",
        f"display_{group}_count",
    )
    return parse_int(row, *aliases)


def continuous_mean(row: Mapping[str, str], name: str, terrestrial: int) -> float:
    direct = (f"{name}_mean", f"{name}_terrestrial_mean")
    for field in direct:
        if field in row and row[field] != "":
            return float(row[field])
    for field in (f"{name}_sum", f"{name}_terrestrial_sum"):
        if field in row and row[field] != "":
            return int(row[field]) / terrestrial if terrestrial else 0.0
    raise KeyError(f"missing {name} mean/sum")


def read_seed_records(
    shards: Sequence[Shard],
    seed: int,
    contract: WeightContract,
    origin: RawOrigin,
) -> tuple[dict[str, dict[int, RawRecord]], list[dict[str, Any]]]:
    by_size: dict[str, dict[int, RawRecord]] = {size: {} for size in MAP_SIZES}
    failures: list[dict[str, Any]] = []
    expected_rows = 0
    actual_rows = 0
    for shard in (item for item in shards if item.seed == seed):
        expected_rows += shard.row_count
        shard_rows = 0
        with open_csv(shard.path) as source:
            reader = csv.DictReader(source)
            if not reader.fieldnames:
                raise RuntimeError(f"shard has no CSV header: {shard.path}")
            for row_number, row in enumerate(reader, 2):
                shard_rows += 1
                actual_rows += 1
                if origin.binding is not None:
                    expected_identity = {
                        "run_id": origin.binding["run_id"],
                        "source_head": origin.binding["base_head"],
                        "source_manifest_hash": (
                            origin.binding["source_manifest_hash"]),
                        "executable_hash": origin.binding["executable_hash"],
                        "scripts_manifest_hash": (
                            origin.binding["scripts_manifest_hash"]),
                        "config_manifest_hash": (
                            origin.binding["config_manifest_hash"]),
                        "schema_version": origin.binding["row_schema_version"],
                    }
                    for field, expected in expected_identity.items():
                        if row.get(field) != str(expected):
                            raise RuntimeError(
                                f"{shard.path}:{row_number}: raw binding "
                                f"mismatch in {field}")
                row_seed = parse_int(row, "seed")
                if row_seed != seed:
                    raise RuntimeError(
                        f"{shard.path}:{row_number}: seed {row_seed} != {seed}")
                size = normalize_map_size(first_present(
                    row, ("map_size_name", "map_size", "size")))
                index = parse_int(row, "config_index")
                if index not in contract.multiplicities:
                    raise RuntimeError(
                        f"{shard.path}:{row_number}: invalid config index {index}")
                multiplicity = parse_int(
                    row, "config_multiplicity", "multiplicity")
                if multiplicity != contract.multiplicities[index]:
                    raise RuntimeError(
                        f"{shard.path}:{row_number}: config multiplicity mismatch")
                success = parse_truth(first_present(row, ("success",), "0"))
                if not success:
                    failures.append({
                        "dataset": shard.dataset,
                        "seed": seed,
                        "map_size": size,
                        "config_index": index,
                        "failure_stage": first_present(
                            row, ("failure_stage",), "unknown"),
                        "failure_reason": first_present(
                            row, ("failure_reason",), "unknown"),
                        "shard": str(shard.path),
                        "row_number": row_number,
                    })
                    continue
                terrestrial = parse_int(
                    row, "terrestrial_count", "terrestrial_tiles")
                if terrestrial <= 0:
                    raise RuntimeError(
                        f"{shard.path}:{row_number}: no terrestrial tiles")
                counts = tuple(group_count(row, group) for group in GROUPS)
                if any(count < 0 for count in counts) or sum(counts) != terrestrial:
                    raise RuntimeError(
                        f"{shard.path}:{row_number}: display groups do not "
                        f"partition {terrestrial} terrestrial tiles")
                means = tuple(
                    continuous_mean(row, name, terrestrial) for name in CONTINUOUS)
                if index in by_size[size]:
                    raise RuntimeError(
                        f"duplicate seed/size/config row: {seed}/{size}/{index}")
                by_size[size][index] = RawRecord(
                    index, multiplicity, terrestrial, counts, means)
        if shard_rows != shard.row_count:
            raise RuntimeError(
                f"row count mismatch for {shard.path}: "
                f"{shard_rows} != {shard.row_count}")
    if actual_rows != expected_rows:
        raise RuntimeError(f"seed {seed} aggregate shard-row count mismatch")
    if not failures:
        expected_indices = set(contract.multiplicities)
        for size, records in by_size.items():
            missing = expected_indices - set(records)
            extra = set(records) - expected_indices
            if missing or extra:
                raise RuntimeError(
                    f"{seed}/{size}: missing={len(missing)} extra={len(extra)}")
    return by_size, failures


def weighted_quantile(
    samples: Sequence[tuple[float, int]], probability: float
) -> float:
    if not samples:
        return 0.0
    ordered = sorted(samples, key=lambda item: item[0])
    total = sum(weight for _, weight in ordered)
    target = probability * total
    cumulative = 0
    for value, weight in ordered:
        cumulative += weight
        if cumulative + EPSILON >= target:
            return value
    return ordered[-1][0]


def weighted_group_stats(
    records: Mapping[int, RawRecord], weights: Sequence[tuple[int, int]],
    group_index: int
) -> dict[str, float]:
    total_weight = sum(weight for _, weight in weights)
    if total_weight <= 0:
        raise ValueError("zero conditional weight")
    shares: list[tuple[float, int]] = []
    weighted_values: list[float] = []
    occurrence = threshold_1 = threshold_5 = threshold_10 = 0
    for config_index, weight in weights:
        record = records.get(config_index)
        if record is None:
            raise ValueError(f"missing config {config_index} in measured rows")
        count = record.groups[group_index]
        share = count / record.terrestrial
        shares.append((share, weight))
        weighted_values.append(weight * share)
        occurrence += weight * (count > 0)
        threshold_1 += weight * (count * 100 >= record.terrestrial)
        threshold_5 += weight * (count * 20 >= record.terrestrial)
        threshold_10 += weight * (count * 10 >= record.terrestrial)
    mean = math.fsum(weighted_values) / total_weight
    variance = math.fsum(
        weight * (share - mean) ** 2 for share, weight in shares
    ) / total_weight
    p10 = weighted_quantile(shares, 0.10)
    p50 = weighted_quantile(shares, 0.50)
    p90 = weighted_quantile(shares, 0.90)
    return {
        "weight": float(total_weight),
        "occurrence_probability": occurrence / total_weight,
        "mean_land_share": mean,
        "p10_land_share": p10,
        "median_land_share": p50,
        "p90_land_share": p90,
        "probability_ge_1pct": threshold_1 / total_weight,
        "probability_ge_5pct": threshold_5 / total_weight,
        "probability_ge_10pct": threshold_10 / total_weight,
        "other_corner_stddev": math.sqrt(max(0.0, variance)),
        "other_corner_p90_p10": p90 - p10,
    }


def weighted_continuous_mean(
    records: Mapping[int, RawRecord], weights: Sequence[tuple[int, int]],
    metric_index: int
) -> float:
    total = sum(weight for _, weight in weights)
    return math.fsum(
        records[index].continuous[metric_index] * weight
        for index, weight in weights
    ) / total


SURFACE_METRICS = (
    "occurrence_probability",
    "mean_land_share",
    "p10_land_share",
    "median_land_share",
    "p90_land_share",
    "probability_ge_1pct",
    "probability_ge_5pct",
    "probability_ge_10pct",
    "baseline_mean_land_share",
    "lift_pp",
    "lift_ratio",
    "other_corner_stddev",
    "other_corner_p90_p10",
)


def compute_seed_size(
    dataset: str, seed: int, size: str, records: Mapping[int, RawRecord],
    contract: WeightContract
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    unconditional = sorted(contract.multiplicities.items())
    baselines = [
        weighted_group_stats(records, unconditional, group_index)
        for group_index in range(len(GROUPS))
    ]
    surface: list[dict[str, Any]] = []
    continuous: list[dict[str, Any]] = []
    for y in GRID:
        for x in GRID:
            condition = contract.coordinate_weights(x, y)
            for group_index, group in enumerate(GROUPS):
                stats = weighted_group_stats(records, condition, group_index)
                baseline = baselines[group_index]["mean_land_share"]
                stats["baseline_mean_land_share"] = baseline
                stats["lift_pp"] = 100.0 * (
                    stats["mean_land_share"] - baseline)
                stats["lift_ratio"] = (
                    stats["mean_land_share"] / baseline if baseline > 0 else 0.0)
                surface.append({
                    "dataset": dataset, "seed": seed, "map_size": size,
                    "x": x, "y": y, "group": group, **stats,
                })
            values = {
                name: weighted_continuous_mean(records, condition, metric_index)
                for metric_index, name in enumerate(CONTINUOUS)
            }
            continuous.append({
                "dataset": dataset, "seed": seed, "map_size": size,
                "x": x, "y": y, **values,
            })
    return surface, continuous


def mean_rows(
    rows: Sequence[Mapping[str, Any]], identity: Mapping[str, Any]
) -> dict[str, Any]:
    if not rows:
        raise ValueError("cannot average an empty row group")
    result = dict(identity)
    for metric in SURFACE_METRICS:
        result[metric] = math.fsum(float(row[metric]) for row in rows) / len(rows)
    values = [float(row["mean_land_share"]) for row in rows]
    result["component_min_land_share"] = min(values)
    result["component_max_land_share"] = max(values)
    result["component_range_land_share"] = max(values) - min(values)
    result["component_stddev_land_share"] = (
        statistics.pstdev(values) if len(values) > 1 else 0.0)
    return result


def combine_surface(
    rows: Sequence[Mapping[str, Any]], dataset: str, scope: str
) -> list[dict[str, Any]]:
    grouped: dict[tuple[int, int, str], list[Mapping[str, Any]]] = defaultdict(list)
    for row in rows:
        grouped[(int(row["x"]), int(row["y"]), str(row["group"]))].append(row)
    output = [
        mean_rows(group, {
            "dataset": dataset, "scope": scope,
            "x": key[0], "y": key[1], "group": key[2],
        })
        for key, group in sorted(grouped.items())
    ]
    return output


def per_seed_surface(
    rows: Sequence[Mapping[str, Any]], seed_kind: Mapping[int, str]
) -> list[dict[str, Any]]:
    grouped: dict[tuple[int, int, int, str], list[Mapping[str, Any]]] = defaultdict(list)
    for row in rows:
        grouped[(
            int(row["seed"]), int(row["x"]), int(row["y"]), str(row["group"])
        )].append(row)
    result = [
        mean_rows(group, {
            "dataset": seed_kind[key[0]], "scope": "per_seed_equal_sizes",
            "seed": key[0], "x": key[1], "y": key[2], "group": key[3],
        })
        for key, group in sorted(grouped.items())
    ]
    annotate_tendency_groups(result, ("seed", "x", "y"))
    return result


def per_size_surface(
    rows: Sequence[Mapping[str, Any]], dataset: str
) -> list[dict[str, Any]]:
    grouped: dict[tuple[str, int, int, str], list[Mapping[str, Any]]] = defaultdict(list)
    for row in rows:
        grouped[(
            str(row["map_size"]), int(row["x"]), int(row["y"]), str(row["group"])
        )].append(row)
    order = {size: index for index, size in enumerate(MAP_SIZES)}
    result = [
        mean_rows(group, {
            "dataset": dataset, "scope": "per_size_equal_seeds",
            "map_size": key[0], "x": key[1], "y": key[2], "group": key[3],
        })
        for key, group in grouped.items()
    ]
    result = sorted(result, key=lambda row: (
        order[str(row["map_size"])], int(row["x"]), int(row["y"]),
        str(row["group"])))
    annotate_tendency_groups(result, ("map_size", "x", "y"))
    return result


def winner_for_rows(rows: Sequence[Mapping[str, Any]]) -> tuple[str, str, float]:
    ranked = sorted(
        ((float(row["lift_pp"]), str(row["group"])) for row in rows),
        key=lambda item: (-item[0], GROUPS.index(item[1])))
    if not ranked or ranked[0][0] <= 0:
        return "unsupported", "unsupported", 0.0
    runner = ranked[1] if len(ranked) > 1 else (0.0, "unsupported")
    return ranked[0][1], runner[1], ranked[0][0] - runner[0]


def annotate_tendency_groups(
    rows: Sequence[dict[str, Any]], identity_fields: Sequence[str],
) -> None:
    grouped: dict[tuple[Any, ...], list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        grouped[tuple(row[field] for field in identity_fields)].append(row)
    for group_rows in grouped.values():
        top, runner, margin = winner_for_rows(group_rows)
        for row in group_rows:
            row["top_tendency"] = top
            row["runner_up_tendency"] = runner
            row["top_margin_pp"] = margin


def winner_map(rows: Sequence[Mapping[str, Any]]) -> dict[tuple[int, int], str]:
    grouped: dict[tuple[int, int], list[Mapping[str, Any]]] = defaultdict(list)
    for row in rows:
        grouped[(int(row["x"]), int(row["y"]))].append(row)
    return {key: winner_for_rows(group)[0] for key, group in grouped.items()}


def attach_tendencies(
    combined: list[dict[str, Any]],
    seed_rows: Sequence[Mapping[str, Any]],
    size_rows: Sequence[Mapping[str, Any]],
) -> None:
    if not combined:
        raise ValueError("cannot attach tendencies to an empty surface")
    datasets = {str(row["dataset"]) for row in combined}
    if len(datasets) != 1:
        raise ValueError(f"combined surface mixes datasets: {sorted(datasets)}")
    dataset = next(iter(datasets))
    annotate_tendency_groups(combined, ("x", "y"))
    by_coordinate: dict[tuple[int, int], list[dict[str, Any]]] = defaultdict(list)
    for row in combined:
        by_coordinate[(int(row["x"]), int(row["y"]))].append(row)
    component_winners: dict[
        tuple[int, int], list[tuple[str, str, str]]
    ] = defaultdict(list)
    for component in (seed_rows, size_rows):
        scopes: dict[tuple[Any, int, int], list[Mapping[str, Any]]] = defaultdict(list)
        scope_field = "seed" if component is seed_rows else "map_size"
        for row in component:
            scopes[(row[scope_field], int(row["x"]), int(row["y"]))].append(row)
        for (scope_value, x, y), group_rows in scopes.items():
            winner, _, margin = winner_for_rows(group_rows)
            component_winners[(x, y)].append((
                scope_field, str(scope_value),
                winner if margin > EPSILON else "mixed_tie"))
    observed_seed_count = len({int(row["seed"]) for row in seed_rows})
    observed_size_count = len({str(row["map_size"]) for row in size_rows})
    if dataset == "full":
        required_seed_count = len(CALIBRATION_SEEDS) + len(HOLDOUT_SEEDS)
        required_size_count = len(MAP_SIZES)
        stability_field = "cross_stable"
        stability_scope = "full_six_seeds_and_four_size_surfaces"
    else:
        required_seed_count = observed_seed_count
        required_size_count = observed_size_count
        stability_field = "dataset_relative_stable"
        stability_scope = (
            f"{dataset}_{required_seed_count}_seed_and_"
            f"{required_size_count}_size_surfaces")
    for coordinate, rows in by_coordinate.items():
        top = str(rows[0]["top_tendency"])
        runner = str(rows[0]["runner_up_tendency"])
        margin = float(rows[0]["top_margin_pp"])
        observations = component_winners.get(coordinate, [])
        observed = [winner for _, _, winner in observations]
        stable = (
            observed_seed_count == required_seed_count and
            observed_size_count == required_size_count and
            len(observations) == required_seed_count + required_size_count and
            top != "unsupported" and margin > EPSILON and
            all(winner == top for winner in observed))
        exact_winners = [
            f"{scope}:{value}={winner}"
            for scope, value, winner in observations]
        disagreements = [
            f"{scope}:{value}={winner}"
            for scope, value, winner in observations if winner != top]
        for row in rows:
            row["top_tendency"] = top
            row["runner_up_tendency"] = runner
            row["top_margin_pp"] = margin
            row[stability_field] = stable
            row["stability_scope"] = stability_scope
            row["stability_seed_component_count"] = observed_seed_count
            row["stability_size_component_count"] = observed_size_count
            row["component_winners"] = exact_winners
            row["component_winner_disagreements"] = disagreements


def surface_fields(rows: Sequence[Mapping[str, Any]]) -> list[str]:
    identity = [
        field for field in (
            "dataset", "scope", "seed", "seed_kind", "map_size",
            "x", "y", "group")
        if any(field in row for row in rows)
    ]
    metrics = list(SURFACE_METRICS) + [
        "component_min_land_share", "component_max_land_share",
        "component_range_land_share", "component_stddev_land_share",
        "top_tendency", "runner_up_tendency", "top_margin_pp",
        "cross_stable", "dataset_relative_stable", "stability_scope",
        "stability_seed_component_count", "stability_size_component_count",
        "component_winners",
        "component_winner_disagreements",
    ]
    return identity + [field for field in metrics if any(field in row for row in rows)]


def average_continuous(
    rows: Sequence[Mapping[str, Any]], dataset: str
) -> list[dict[str, Any]]:
    grouped: dict[tuple[int, int], list[Mapping[str, Any]]] = defaultdict(list)
    for row in rows:
        grouped[(int(row["x"]), int(row["y"]))].append(row)
    return [{
        "dataset": dataset, "x": x, "y": y,
        **{
            name: math.fsum(float(row[name]) for row in group) / len(group)
            for name in CONTINUOUS
        },
    } for (x, y), group in sorted(grouped.items())]


def seed_size_surface(
    rows: Sequence[Mapping[str, Any]], dataset: str,
    seed_kind: Mapping[int, str],
) -> list[dict[str, Any]]:
    order = {size: index for index, size in enumerate(MAP_SIZES)}
    result = []
    for source in rows:
        row = dict(source)
        row["seed_kind"] = seed_kind[int(row["seed"])]
        row["dataset"] = dataset
        row["scope"] = "per_seed_size_weighted_configs"
        result.append(row)
    result = sorted(result, key=lambda row: (
        str(row["dataset"]), int(row["seed"]),
        order[str(row["map_size"])], int(row["x"]), int(row["y"]),
        GROUPS.index(str(row["group"]))))
    annotate_tendency_groups(result, ("seed", "map_size", "x", "y"))
    return result


def linear_slope(xs: Sequence[float], ys: Sequence[float]) -> float:
    x_mean = math.fsum(xs) / len(xs)
    y_mean = math.fsum(ys) / len(ys)
    denominator = math.fsum((x - x_mean) ** 2 for x in xs)
    if denominator == 0:
        return 0.0
    return math.fsum(
        (x - x_mean) * (y - y_mean) for x, y in zip(xs, ys)
    ) / denominator


def rank_values(values: Sequence[float]) -> list[float]:
    order = sorted(range(len(values)), key=lambda index: values[index])
    ranks = [0.0] * len(values)
    start = 0
    while start < len(order):
        end = start + 1
        while end < len(order) and values[order[end]] == values[order[start]]:
            end += 1
        rank = (start + 1 + end) / 2.0
        for offset in range(start, end):
            ranks[order[offset]] = rank
        start = end
    return ranks


def spearman(left: Sequence[float], right: Sequence[float]) -> float:
    if len(left) != len(right) or not left:
        return 0.0
    left_rank = rank_values(left)
    right_rank = rank_values(right)
    left_mean = math.fsum(left_rank) / len(left_rank)
    right_mean = math.fsum(right_rank) / len(right_rank)
    numerator = math.fsum(
        (a - left_mean) * (b - right_mean)
        for a, b in zip(left_rank, right_rank))
    denominator = math.sqrt(
        math.fsum((a - left_mean) ** 2 for a in left_rank) *
        math.fsum((b - right_mean) ** 2 for b in right_rank))
    return numerator / denominator if denominator else 1.0


def axis_profile(
    surface: Sequence[Mapping[str, Any]], metric: str, axis: str,
    expected_sign: int,
) -> dict[str, Any]:
    by_coordinate = {
        (int(row["x"]), int(row["y"])): row for row in surface}
    values: list[float] = []
    for coordinate in GRID:
        if axis == "x":
            samples = [
                float(by_coordinate[(coordinate, other)][metric])
                for other in GRID]
        else:
            samples = [
                float(by_coordinate[(other, coordinate)][metric])
                for other in GRID]
        values.append(math.fsum(samples) / len(samples))
    deltas = [
        values[index + 1] - values[index]
        for index in range(len(values) - 1)]
    reversals = sum(delta * expected_sign < -EPSILON for delta in deltas)
    endpoint = values[-1] - values[0]
    return {
        "values": values,
        "deltas": deltas,
        "endpoint_difference": endpoint,
        "linear_slope": linear_slope(GRID, values),
        "rank_correlation": spearman(GRID, values),
        "reversal_count": reversals,
        "directionally_supported": (
            endpoint * expected_sign > 0 and reversals == 0),
        "zero_effect": abs(endpoint) <= EPSILON and all(
            abs(delta) <= EPSILON for delta in deltas),
    }


def build_axis_response(
    continuous_by_dataset: Mapping[str, Sequence[Mapping[str, Any]]],
    component_rows: Mapping[str, Sequence[Mapping[str, Any]]],
) -> list[dict[str, Any]]:
    output: list[dict[str, Any]] = []
    definitions = (
        ("temperature", "x", 1),
        ("moisture", "y", 1),
        ("precipitation", "y", 1),
    )
    for dataset, surface in continuous_by_dataset.items():
        raw_components = component_rows[dataset]
        by_component: dict[tuple[int, str], list[Mapping[str, Any]]] = (
            defaultdict(list))
        for row in raw_components:
            by_component[(
                int(row["seed"]), str(row["map_size"])
            )].append(row)
        for metric, axis, expected_sign in definitions:
            profile = axis_profile(surface, metric, axis, expected_sign)
            component_profiles = [
                axis_profile(rows, metric, axis, expected_sign)
                for _, rows in sorted(by_component.items())
            ]
            endpoints = [
                float(item["endpoint_difference"]) for item in component_profiles]
            slopes = [
                float(item["linear_slope"]) for item in component_profiles]
            correlations = [
                float(item["rank_correlation"]) for item in component_profiles]
            max_reversals = max(
                (int(item["reversal_count"]) for item in component_profiles),
                default=0)
            supported_fraction = (
                math.fsum(float(item["directionally_supported"])
                          for item in component_profiles) /
                len(component_profiles) if component_profiles else 0.0)
            exact_zero_fraction = (
                math.fsum(float(item["zero_effect"])
                          for item in component_profiles) /
                len(component_profiles) if component_profiles else 0.0)
            exact_zero = (
                bool(profile["zero_effect"]) and
                exact_zero_fraction == 1.0)
            robust_directional_support = (
                bool(profile["directionally_supported"]) and
                supported_fraction == 1.0)
            response_class = (
                "exact_zero" if exact_zero else
                "robust_directional_support"
                if robust_directional_support else
                "inconsistent_response")
            values = profile["values"]
            deltas = profile["deltas"]
            for index, (coordinate, value) in enumerate(zip(GRID, values)):
                output.append({
                    "dataset": dataset,
                    "metric": metric,
                    "axis": axis.upper(),
                    "coordinate": coordinate,
                    "mean": value,
                    "adjacent_delta": "" if index == 0 else deltas[index - 1],
                    "endpoint_difference": profile["endpoint_difference"],
                    "linear_slope": profile["linear_slope"],
                    "rank_correlation": profile["rank_correlation"],
                    "reversal_count": profile["reversal_count"],
                    "directionally_supported":
                        profile["directionally_supported"],
                    "zero_effect": profile["zero_effect"],
                    "exact_zero_response": exact_zero,
                    "robust_directional_support":
                        robust_directional_support,
                    "response_class": response_class,
                    "magnitude_materiality_status": (
                        "not_applicable_exact_zero" if exact_zero else
                        "requires_user_supplied_threshold"),
                    "component_count": len(component_profiles),
                    "component_endpoint_min": min(endpoints, default=0.0),
                    "component_endpoint_max": max(endpoints, default=0.0),
                    "component_endpoint_range": (
                        max(endpoints, default=0.0) -
                        min(endpoints, default=0.0)),
                    "component_slope_min": min(slopes, default=0.0),
                    "component_slope_max": max(slopes, default=0.0),
                    "component_rank_correlation_min":
                        min(correlations, default=0.0),
                    "component_rank_correlation_max":
                        max(correlations, default=0.0),
                    "component_max_reversals": max_reversals,
                    "component_directionally_supported_fraction":
                        supported_fraction,
                    "component_exact_zero_fraction": exact_zero_fraction,
                })
    return output


def holdout_validation(
    calibration: Sequence[Mapping[str, Any]],
    holdout: Sequence[Mapping[str, Any]],
) -> list[dict[str, Any]]:
    cal = {
        (int(row["x"]), int(row["y"]), str(row["group"])): row
        for row in calibration}
    hold = {
        (int(row["x"]), int(row["y"]), str(row["group"])): row
        for row in holdout}
    if set(cal) != set(hold):
        raise ValueError("calibration and holdout surfaces have different keys")
    cal_winners = winner_map(calibration)
    hold_winners = winner_map(holdout)
    rank_by_coordinate: dict[tuple[int, int], float] = {}
    for y in GRID:
        for x in GRID:
            rank_by_coordinate[(x, y)] = spearman(
                [float(cal[(x, y, group)]["lift_pp"]) for group in GROUPS],
                [float(hold[(x, y, group)]["lift_pp"]) for group in GROUPS],
            )
    coordinate_rows: list[dict[str, Any]] = []
    by_group: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for key in sorted(cal):
        left = cal[key]
        right = hold[key]
        row = {
            "record_type": "coordinate",
            "group": key[2], "x": key[0], "y": key[1],
            "mean_share_abs_error": abs(
                float(left["mean_land_share"]) - float(right["mean_land_share"])),
            "occurrence_abs_error": abs(
                float(left["occurrence_probability"]) -
                float(right["occurrence_probability"])),
            "prob_ge_1pct_abs_error": abs(
                float(left["probability_ge_1pct"]) -
                float(right["probability_ge_1pct"])),
            "prob_ge_5pct_abs_error": abs(
                float(left["probability_ge_5pct"]) -
                float(right["probability_ge_5pct"])),
            "prob_ge_10pct_abs_error": abs(
                float(left["probability_ge_10pct"]) -
                float(right["probability_ge_10pct"])),
            "lift_pp_abs_error": abs(
                float(left["lift_pp"]) - float(right["lift_pp"])),
            "calibration_top_tendency":
                cal_winners[(key[0], key[1])],
            "holdout_top_tendency":
                hold_winners[(key[0], key[1])],
            "top_tendency_agreement": (
                cal_winners[(key[0], key[1])] ==
                hold_winners[(key[0], key[1])]),
            "coordinate_rank_agreement":
                rank_by_coordinate[(key[0], key[1])],
        }
        coordinate_rows.append(row)
        by_group[key[2]].append(row)
    summary_rows: list[dict[str, Any]] = []
    error_fields = (
        "mean_share_abs_error", "occurrence_abs_error",
        "prob_ge_1pct_abs_error", "prob_ge_5pct_abs_error",
        "prob_ge_10pct_abs_error", "lift_pp_abs_error")
    for group in GROUPS:
        rows = by_group[group]
        summary: dict[str, Any] = {
            "record_type": "group_summary", "group": group, "x": "", "y": "",
            "top_tendency_agreement": math.fsum(
                float(row["top_tendency_agreement"]) for row in rows) / len(rows),
            "coordinate_rank_agreement_mean": math.fsum(
                float(row["coordinate_rank_agreement"]) for row in rows
            ) / len(rows),
        }
        for field in error_fields:
            values = [float(row[field]) for row in rows]
            summary[f"{field}_mae"] = math.fsum(values) / len(values)
            summary[f"{field}_max"] = max(values)
        summary_rows.append(summary)
    coordinate_rank_values = list(rank_by_coordinate.values())
    overall_summary = {
        "record_type": "overall_summary", "group": "", "x": "", "y": "",
        "top_tendency_agreement": math.fsum(
            float(cal_winners[key] == hold_winners[key])
            for key in sorted(cal_winners)) / len(cal_winners),
        "coordinate_rank_agreement_mean":
            math.fsum(coordinate_rank_values) / len(coordinate_rank_values),
        "coordinate_rank_agreement_min": min(coordinate_rank_values),
        "global_lift_rank_agreement": spearman(
            [float(cal[key]["lift_pp"]) for key in sorted(cal)],
            [float(hold[key]["lift_pp"]) for key in sorted(hold)]),
    }
    return coordinate_rows + summary_rows + [overall_summary]


def size_differences(
    rows: Sequence[Mapping[str, Any]], dataset: str
) -> list[dict[str, Any]]:
    indexed = {
        (str(row["map_size"]), int(row["x"]), int(row["y"]), str(row["group"])): row
        for row in rows}
    output: list[dict[str, Any]] = []
    for left_index, left_size in enumerate(MAP_SIZES):
        for right_size in MAP_SIZES[left_index + 1:]:
            pair_rows: list[dict[str, Any]] = []
            for y in GRID:
                for x in GRID:
                    for group in GROUPS:
                        left = indexed[(left_size, x, y, group)]
                        right = indexed[(right_size, x, y, group)]
                        pair_rows.append({
                            "record_type": "coordinate",
                            "dataset": dataset,
                            "left_size": left_size,
                            "right_size": right_size,
                            "x": x, "y": y, "group": group,
                            "mean_share_abs_difference": abs(
                                float(left["mean_land_share"]) -
                                float(right["mean_land_share"])),
                            "lift_pp_abs_difference": abs(
                                float(left["lift_pp"]) -
                                float(right["lift_pp"])),
                            "occurrence_abs_difference": abs(
                                float(left["occurrence_probability"]) -
                                float(right["occurrence_probability"])),
                        })
            output.extend(pair_rows)
            for group in GROUPS:
                group_rows = [
                    row for row in pair_rows if row["group"] == group]
                output.append({
                    "record_type": "pair_group_summary",
                    "dataset": dataset, "left_size": left_size,
                    "right_size": right_size, "x": "", "y": "",
                    "group": group,
                    "mean_share_pairwise_mae": math.fsum(
                        float(row["mean_share_abs_difference"])
                        for row in group_rows) / len(group_rows),
                    "lift_pp_pairwise_mae": math.fsum(
                        float(row["lift_pp_abs_difference"])
                        for row in group_rows) / len(group_rows),
                    "occurrence_pairwise_mae": math.fsum(
                        float(row["occurrence_abs_difference"])
                        for row in group_rows) / len(group_rows),
                    "mean_share_pairwise_max": max(
                        float(row["mean_share_abs_difference"])
                        for row in group_rows),
                })
            output.append({
                "record_type": "pair_summary",
                "dataset": dataset, "left_size": left_size,
                "right_size": right_size, "x": "", "y": "", "group": "",
                "mean_share_pairwise_mae": math.fsum(
                    float(row["mean_share_abs_difference"])
                    for row in pair_rows) / len(pair_rows),
                "lift_pp_pairwise_mae": math.fsum(
                    float(row["lift_pp_abs_difference"])
                    for row in pair_rows) / len(pair_rows),
                "occurrence_pairwise_mae": math.fsum(
                    float(row["occurrence_abs_difference"])
                    for row in pair_rows) / len(pair_rows),
                "mean_share_pairwise_max": max(
                    float(row["mean_share_abs_difference"])
                    for row in pair_rows),
            })
    return output


def mixed_record_fields(
    rows: Sequence[Mapping[str, Any]], preferred: Sequence[str],
) -> list[str]:
    available = {field for row in rows for field in row}
    return (
        [field for field in preferred if field in available] +
        sorted(available.difference(preferred)))


def model_payload(
    calibration: Sequence[Mapping[str, Any]], raw_digest: str
) -> dict[str, Any]:
    grouped: dict[tuple[int, int], list[Mapping[str, Any]]] = defaultdict(list)
    for row in calibration:
        grouped[(int(row["x"]), int(row["y"]))].append(row)
    cells = []
    for (x, y), rows in sorted(grouped.items()):
        top, runner, margin = winner_for_rows(rows)
        cells.append({
            "x": x, "y": y,
            "top_tendency": top,
            "runner_up_tendency": runner,
            "top_margin_pp": float(f"{margin:.12f}"),
            "groups": {
                str(row["group"]): {
                    metric: float(f"{float(row[metric]):.12f}")
                    for metric in SURFACE_METRICS
                } for row in sorted(rows, key=lambda item: GROUPS.index(
                    str(item["group"])))
            },
        })
    return {
        "model_version": 1,
        "fit_scope": "calibration-only",
        "calibration_seeds": list(CALIBRATION_SEEDS),
        "map_sizes": list(MAP_SIZES),
        "coordinate_grid": list(GRID),
        "raw_manifest_digest": raw_digest,
        "cells": cells,
    }


def raw_digest(shards: Sequence[Shard]) -> str:
    digest = hashlib.sha256()
    for shard in sorted(shards, key=lambda item: item.logical_path):
        digest.update(shard.dataset.encode("ascii"))
        digest.update(b"\0")
        digest.update(str(shard.seed).encode("ascii"))
        digest.update(b"\0")
        digest.update(shard.sha256.encode("ascii"))
        digest.update(b"\n")
    return digest.hexdigest().upper()


def write_calibration_freeze(
    output_dir: Path,
    surface_path: Path,
    model_path: Path,
    calibration_raw_digest: str,
) -> tuple[Path, Path, str, str]:
    surface_hash = sha256_file(surface_path)
    model_hash = sha256_file(model_path)
    surface_hash_path = output_dir / "calibration_surface.sha256"
    write_text_new(surface_hash_path, surface_hash + "\n")
    payload = {
        "freeze_version": 1,
        "fit_scope": "calibration-only",
        "calibration_seeds": list(CALIBRATION_SEEDS),
        "map_sizes": list(MAP_SIZES),
        "coordinate_grid": list(GRID),
        "raw_manifest_digest": calibration_raw_digest,
        "surface": {
            "path": surface_path.name,
            "sha256": surface_hash,
            "bytes": surface_path.stat().st_size,
            "row_count": len(GRID) * len(GRID) * len(GROUPS),
        },
        "model": {
            "path": model_path.name,
            "sha256": model_hash,
            "bytes": model_path.stat().st_size,
        },
    }
    manifest = {
        **payload,
        "manifest_payload_sha256": hashlib.sha256(
            binding_json_bytes(payload)).hexdigest().upper(),
    }
    manifest_path = output_dir / "calibration_freeze_manifest.json"
    write_json_new(manifest_path, manifest)
    return surface_hash_path, manifest_path, surface_hash, model_hash


def read_sha256(path: Path) -> str:
    digest = path.read_text(encoding="ascii").strip().upper()
    if len(digest) != 64:
        raise RuntimeError(f"invalid SHA-256 text in {path}")
    try:
        int(digest, 16)
    except ValueError as exc:
        raise RuntimeError(f"invalid SHA-256 text in {path}") from exc
    return digest


def verify_calibration_freeze(
    calibration_dir: Path,
) -> dict[str, Any]:
    model_path = calibration_dir / "calibration_model.json"
    model_hash_path = calibration_dir / "calibration_model.sha256"
    surface_path = calibration_dir / "calibration_surface.csv"
    surface_hash_path = calibration_dir / "calibration_surface.sha256"
    manifest_path = calibration_dir / "calibration_freeze_manifest.json"
    required = (
        model_path, model_hash_path, surface_path, surface_hash_path,
        manifest_path)
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise RuntimeError(
            f"calibration freeze is incomplete; missing={missing}")
    model_hash = read_sha256(model_hash_path)
    surface_hash = read_sha256(surface_hash_path)
    if sha256_file(model_path) != model_hash:
        raise RuntimeError("frozen calibration model hash mismatch")
    if sha256_file(surface_path) != surface_hash:
        raise RuntimeError("frozen calibration surface hash mismatch")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    recorded_manifest_hash = manifest.get("manifest_payload_sha256")
    payload = dict(manifest)
    payload.pop("manifest_payload_sha256", None)
    observed_manifest_hash = hashlib.sha256(
        binding_json_bytes(payload)).hexdigest().upper()
    if not isinstance(recorded_manifest_hash, str) or (
            recorded_manifest_hash.upper() != observed_manifest_hash):
        raise RuntimeError("calibration freeze manifest payload hash mismatch")
    if (
        int(manifest.get("freeze_version", -1)) != 1 or
        manifest.get("fit_scope") != "calibration-only" or
        manifest.get("calibration_seeds") != list(CALIBRATION_SEEDS) or
        manifest.get("map_sizes") != list(MAP_SIZES) or
        manifest.get("coordinate_grid") != list(GRID)
    ):
        raise RuntimeError("calibration freeze contract metadata mismatch")
    surface_metadata = manifest.get("surface")
    model_metadata = manifest.get("model")
    if not isinstance(surface_metadata, dict) or not isinstance(
            model_metadata, dict):
        raise RuntimeError("calibration freeze file metadata is invalid")
    if (
        surface_metadata.get("path") != surface_path.name or
        str(surface_metadata.get("sha256", "")).upper() != surface_hash or
        int(surface_metadata.get("bytes", -1)) != surface_path.stat().st_size or
        int(surface_metadata.get("row_count", -1)) !=
            len(GRID) * len(GRID) * len(GROUPS) or
        model_metadata.get("path") != model_path.name or
        str(model_metadata.get("sha256", "")).upper() != model_hash or
        int(model_metadata.get("bytes", -1)) != model_path.stat().st_size
    ):
        raise RuntimeError("calibration freeze file metadata mismatch")
    calibration = read_surface(surface_path)
    expected_keys = {
        (x, y, group) for y in GRID for x in GRID for group in GROUPS}
    observed_keys = {
        (int(row["x"]), int(row["y"]), str(row["group"]))
        for row in calibration}
    if len(calibration) != len(expected_keys) or observed_keys != expected_keys:
        raise RuntimeError("frozen calibration surface lattice is incomplete")
    if any(str(row.get("dataset")) != "calibration" for row in calibration):
        raise RuntimeError("frozen calibration surface dataset mismatch")
    model_bytes = model_path.read_bytes()
    model = json.loads(model_bytes.decode("utf-8"))
    raw_manifest_digest = str(manifest.get("raw_manifest_digest", ""))
    if model.get("raw_manifest_digest") != raw_manifest_digest:
        raise RuntimeError("calibration model/raw-manifest binding mismatch")
    reconstructed = model_payload(calibration, raw_manifest_digest)
    if canonical_json_bytes(reconstructed) != model_bytes:
        raise RuntimeError(
            "frozen calibration model does not reconstruct from its surface")
    return {
        "model_sha256": model_hash,
        "surface_sha256": surface_hash,
        "manifest_sha256": sha256_file(manifest_path),
        "surface": calibration,
    }


def value_map(
    rows: Sequence[Mapping[str, Any]], metric: str, group: str
) -> dict[tuple[int, int], float]:
    return {
        (int(row["x"]), int(row["y"])): float(row[metric])
        for row in rows if str(row["group"]) == group}


def color_for(value: float, low: float, high: float, diverging: bool) -> str:
    if high <= low + EPSILON:
        return "#D7DED8"
    ratio = max(0.0, min(1.0, (value - low) / (high - low)))
    if diverging:
        if ratio < 0.5:
            local = ratio * 2
            start, end = (55, 105, 165), (242, 240, 226)
        else:
            local = (ratio - 0.5) * 2
            start, end = (242, 240, 226), (190, 94, 62)
    else:
        local = ratio
        start, end = (235, 239, 224), (38, 124, 117)
    rgb = tuple(round(a + (b - a) * local) for a, b in zip(start, end))
    return f"#{rgb[0]:02X}{rgb[1]:02X}{rgb[2]:02X}"


def render_grid_svg(
    path: Path, title: str, rows: Sequence[Mapping[str, Any]], metric: str,
    diverging: bool = False,
) -> None:
    panel_width, panel_height = 250, 250
    margin_x, margin_y = 24, 58
    columns = 4
    width = columns * panel_width
    height = 2 * panel_height + 54
    all_values = [float(row[metric]) for row in rows]
    if diverging:
        magnitude = max((abs(value) for value in all_values), default=1.0)
        low, high = -magnitude, magnitude
    else:
        low, high = min(all_values, default=0.0), max(all_values, default=1.0)
    chunks = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" '
        f'height="{height}" viewBox="0 0 {width} {height}" '
        f'data-source="measured-lattice" data-interpolation="none">',
        f"<title>{escape_xml(title)} — measured lattice; no interpolation</title>",
        '<rect width="100%" height="100%" fill="#F6F3E8"/>',
        f'<text x="18" y="25" font-family="sans-serif" font-size="18" '
        f'font-weight="bold">{escape_xml(title)}</text>',
        '<text x="18" y="44" font-family="sans-serif" font-size="12">'
        'Measured lattice; no interpolation. X: Cold → Hot. Y: Dry → Wet.</text>',
    ]
    for group_index, group in enumerate(GROUPS):
        column = group_index % columns
        row_index = group_index // columns
        origin_x = column * panel_width + margin_x
        origin_y = row_index * panel_height + margin_y
        cell = 20
        values = value_map(rows, metric, group)
        chunks.append(
            f'<text x="{origin_x}" y="{origin_y - 9}" '
            f'font-family="sans-serif" font-size="13" font-weight="bold">'
            f'{escape_xml(GROUP_LABELS[group])}</text>')
        for y_index, y in enumerate(reversed(GRID)):
            for x_index, x in enumerate(GRID):
                value = values[(x, y)]
                fill = color_for(value, low, high, diverging)
                px = origin_x + x_index * cell
                py = origin_y + y_index * cell
                chunks.append(
                    f'<rect x="{px}" y="{py}" width="{cell}" height="{cell}" '
                    f'fill="{fill}" stroke="#FFFFFF" stroke-width="0.5">'
                    f'<title>x={x}, y={y}, {metric}={value:.6f}</title></rect>')
        chunks.append(
            f'<text x="{origin_x}" y="{origin_y + 9 * cell + 15}" '
            f'font-family="sans-serif" font-size="10">-50 … 0 … +50</text>')
    chunks.append("</svg>\n")
    write_text_new(path, "\n".join(chunks))


def escape_xml(value: str) -> str:
    return (value.replace("&", "&amp;").replace("<", "&lt;")
            .replace(">", "&gt;").replace('"', "&quot;"))


def largest(rows: Sequence[Mapping[str, Any]], field: str) -> Mapping[str, Any]:
    return max(rows, key=lambda row: float(row.get(field, 0.0)))


def measured_coordinate_ranges(
    coordinates: Iterable[tuple[int, int]],
) -> list[dict[str, Any]]:
    order = {value: index for index, value in enumerate(GRID)}
    by_y: dict[int, set[int]] = defaultdict(set)
    for x, y in coordinates:
        by_y[int(y)].add(int(x))
    output: list[dict[str, Any]] = []
    for y in sorted(by_y, key=order.__getitem__):
        xs = sorted(by_y[y], key=order.__getitem__)
        runs: list[list[int]] = []
        for x in xs:
            if not runs or order[x] != order[runs[-1][1]] + 1:
                runs.append([x, x])
            else:
                runs[-1][1] = x
        output.append({"y": y, "x_runs": runs})
    return output


def format_coordinate_ranges(ranges: Sequence[Mapping[str, Any]]) -> str:
    parts = []
    for row in ranges:
        run_text = ", ".join(
            f"x={run[0]}" if int(run[0]) == int(run[1])
            else f"x={run[0]}..{run[1]}"
            for run in row["x_runs"])
        parts.append(f"y={row['y']}: {run_text}")
    return "; ".join(parts) if parts else "none"


def tendency_label(tendency: str, chinese: bool) -> str:
    if tendency in GROUP_LABELS:
        return GROUP_LABELS_ZH[tendency] if chinese else GROUP_LABELS[tendency]
    aliases = {
        "unsupported": ("证据不足", "unsupported"),
        "mixed_tie": ("并列混合", "mixed tie"),
    }
    return aliases.get(tendency, (tendency, tendency))[0 if chinese else 1]


def map_size_winner_changes(
    size_rows: Sequence[Mapping[str, Any]],
) -> list[dict[str, Any]]:
    grouped: dict[tuple[str, int, int], list[Mapping[str, Any]]] = (
        defaultdict(list))
    for row in size_rows:
        grouped[(
            str(row["map_size"]), int(row["x"]), int(row["y"])
        )].append(row)
    output = []
    for y in GRID:
        for x in GRID:
            winners = []
            for size in MAP_SIZES:
                top, runner, margin = winner_for_rows(grouped[(size, x, y)])
                winner = (
                    "mixed_tie" if top != "unsupported" and margin <= EPSILON
                    else top)
                winners.append({
                    "map_size": size,
                    "top_tendency": winner,
                    "runner_up_tendency": runner,
                    "top_margin_pp": margin,
                })
            if len({row["top_tendency"] for row in winners}) > 1:
                output.append({"x": x, "y": y, "winners": winners})
    return output


def unique_coordinate_error_leaders(
    rows: Sequence[Mapping[str, Any]], field: str, limit: int = 10,
) -> list[dict[str, Any]]:
    by_coordinate: dict[tuple[int, int], Mapping[str, Any]] = {}
    for row in rows:
        coordinate = (int(row["x"]), int(row["y"]))
        existing = by_coordinate.get(coordinate)
        if existing is None or (
            float(row[field]), -GROUPS.index(str(row["group"]))
        ) > (
            float(existing[field]), -GROUPS.index(str(existing["group"]))
        ):
            by_coordinate[coordinate] = row
    ordered = sorted(
        by_coordinate.values(),
        key=lambda row: (
            -float(row[field]), int(row["y"]), int(row["x"]),
            GROUPS.index(str(row["group"]))),
    )[:limit]
    return [{
        "x": int(row["x"]), "y": int(row["y"]),
        "group": str(row["group"]), "absolute_error": float(row[field]),
    } for row in ordered]


def holdout_disagreement_leaders(
    coordinate_rows: Sequence[Mapping[str, Any]],
) -> dict[str, Any]:
    metrics = {
        "share": "mean_share_abs_error",
        "occurrence": "occurrence_abs_error",
        "threshold_ge_1pct": "prob_ge_1pct_abs_error",
        "threshold_ge_5pct": "prob_ge_5pct_abs_error",
        "threshold_ge_10pct": "prob_ge_10pct_abs_error",
        "lift": "lift_pp_abs_error",
    }
    result = {
        name: unique_coordinate_error_leaders(coordinate_rows, field)
        for name, field in metrics.items()}
    by_coordinate: dict[tuple[int, int], Mapping[str, Any]] = {}
    for row in coordinate_rows:
        by_coordinate.setdefault((int(row["x"]), int(row["y"])), row)
    ranked = sorted(
        by_coordinate.values(),
        key=lambda row: (
            float(row["coordinate_rank_agreement"]),
            int(row["y"]), int(row["x"])))[:10]
    result["rank"] = [{
        "x": int(row["x"]), "y": int(row["y"]),
        "rank_agreement": float(row["coordinate_rank_agreement"]),
        "rank_disagreement": 1.0 - float(row["coordinate_rank_agreement"]),
    } for row in ranked]
    result["top_tendency"] = [{
        "x": int(row["x"]), "y": int(row["y"]),
        "calibration_top_tendency": str(row["calibration_top_tendency"]),
        "holdout_top_tendency": str(row["holdout_top_tendency"]),
    } for row in sorted(
        (row for row in by_coordinate.values()
         if not bool(row["top_tendency_agreement"])),
        key=lambda row: (int(row["y"]), int(row["x"])))]
    return result


def stability_findings(
    seed_rows: Sequence[Mapping[str, Any]],
    size_rows: Sequence[Mapping[str, Any]],
) -> dict[str, Any]:
    def ranges(
        rows: Sequence[Mapping[str, Any]], component_field: str,
    ) -> list[dict[str, Any]]:
        grouped: dict[
            tuple[int, int, str], list[Mapping[str, Any]]
        ] = defaultdict(list)
        for row in rows:
            grouped[(
                int(row["x"]), int(row["y"]), str(row["group"])
            )].append(row)
        output = []
        for (x, y, group), group_rows in sorted(grouped.items()):
            values = [
                float(row["mean_land_share"]) for row in group_rows]
            output.append({
                "x": x, "y": y, "group": group,
                "component": component_field,
                "component_count": len(group_rows),
                "range": max(values) - min(values),
                "stddev": (
                    statistics.pstdev(values) if len(values) > 1 else 0.0),
            })
        return output

    seed_ranges = ranges(seed_rows, "seed")
    size_ranges = ranges(size_rows, "map_size")
    largest_seed = largest(seed_ranges, "range")
    largest_size = largest(size_ranges, "range")
    return {
        "largest_seed_sensitivity": largest_seed,
        "largest_map_size_sensitivity": largest_size,
        "mean_seed_range": math.fsum(
            float(row["range"]) for row in seed_ranges) / len(seed_ranges),
        "mean_map_size_range": math.fsum(
            float(row["range"]) for row in size_ranges) / len(size_ranges),
    }


def substantive_summary(
    dataset: str,
    surface: Sequence[Mapping[str, Any]],
    holdout_rows: Sequence[Mapping[str, Any]] | None,
    size_difference_rows: Sequence[Mapping[str, Any]],
    per_size_rows: Sequence[Mapping[str, Any]],
    axis_rows: Sequence[Mapping[str, Any]],
) -> tuple[dict[str, Any], str, str]:
    coordinates = {(int(row["x"]), int(row["y"])) for row in surface}
    is_full = dataset == "full"
    stability_field = (
        "cross_stable" if is_full else "dataset_relative_stable")
    stable_coordinates = {
        (int(row["x"]), int(row["y"]))
        for row in surface if bool(row.get(stability_field))
    }
    top_by_coordinate = winner_map(surface)
    representative_by_coordinate: dict[
        tuple[int, int], Mapping[str, Any]
    ] = {}
    for row in surface:
        representative_by_coordinate.setdefault(
            (int(row["x"]), int(row["y"])), row)
    group_findings: dict[str, Any] = {}
    for group in GROUPS[:-1]:
        favored = sorted(
            [coordinate for coordinate, winner in top_by_coordinate.items()
             if winner == group],
            key=lambda coordinate: (
                GRID.index(coordinate[1]), GRID.index(coordinate[0])))
        group_rows = [
            row for row in surface
            if str(row["group"]) == group and
            (int(row["x"]), int(row["y"])) in favored]
        anchor = LABEL_ANCHORS[group]
        anchor_row = next(
            row for row in surface
            if str(row["group"]) == group and
            (int(row["x"]), int(row["y"])) == anchor)
        strongest = max(
            group_rows, key=lambda row: float(row["lift_pp"]),
            default=anchor_row)
        finding = {
            "favored_coordinate_count": len(favored),
            "stable_favored_count": sum(
                coordinate in stable_coordinates for coordinate in favored),
            "favored_coordinates": favored,
            "favored_coordinate_ranges": measured_coordinate_ranges(favored),
            "maximum_lift_pp": max(
                (float(row["lift_pp"]) for row in group_rows), default=0.0),
            "maximum_lift_coordinate": [
                int(strongest["x"]), int(strongest["y"])],
            "mean_occurrence_when_favored": (
                math.fsum(float(row["occurrence_probability"]) for row in group_rows) /
                len(group_rows) if group_rows else 0.0),
            "minimum_occurrence_when_favored": min(
                (float(row["occurrence_probability"]) for row in group_rows),
                default=0.0),
            "maximum_occurrence_when_favored": max(
                (float(row["occurrence_probability"]) for row in group_rows),
                default=0.0),
            "mean_share_when_favored": (
                math.fsum(float(row["mean_land_share"]) for row in group_rows) /
                len(group_rows) if group_rows else 0.0),
            "mean_probability_ge_1pct_when_favored": (
                math.fsum(float(row["probability_ge_1pct"]) for row in group_rows) /
                len(group_rows) if group_rows else 0.0),
            "mean_probability_ge_5pct_when_favored": (
                math.fsum(float(row["probability_ge_5pct"]) for row in group_rows) /
                len(group_rows) if group_rows else 0.0),
            "mean_probability_ge_10pct_when_favored": (
                math.fsum(float(row["probability_ge_10pct"]) for row in group_rows) /
                len(group_rows) if group_rows else 0.0),
            "label_anchor": list(anchor),
            "label_anchor_occurrence": float(anchor_row["occurrence_probability"]),
            "label_anchor_share": float(anchor_row["mean_land_share"]),
            "label_anchor_lift_pp": float(anchor_row["lift_pp"]),
            "label_anchor_probability_ge_1pct": float(
                anchor_row["probability_ge_1pct"]),
            "label_anchor_probability_ge_5pct": float(
                anchor_row["probability_ge_5pct"]),
            "label_anchor_probability_ge_10pct": float(
                anchor_row["probability_ge_10pct"]),
        }
        if is_full:
            finding["cross_stable_favored_count"] = finding[
                "stable_favored_count"]
        else:
            finding["dataset_relative_stable_favored_count"] = finding[
                "stable_favored_count"]
        group_findings[group] = finding
    axis_findings = []
    for metric, axis in (
            ("temperature", "X"), ("moisture", "Y"), ("precipitation", "Y")):
        candidates = [
            row for row in axis_rows
            if row["dataset"] == dataset and row["metric"] == metric and
            row["axis"] == axis]
        if candidates:
            first = candidates[0]
            axis_findings.append({
                "metric": metric, "axis": axis,
                "endpoint_difference": float(first["endpoint_difference"]),
                "linear_slope": float(first["linear_slope"]),
                "rank_correlation": float(first["rank_correlation"]),
                "reversal_count": int(first["reversal_count"]),
                "directionally_supported": bool(first["directionally_supported"]),
                "zero_effect": bool(first["zero_effect"]),
                "exact_zero_response": bool(first["exact_zero_response"]),
                "robust_directional_support": bool(
                    first["robust_directional_support"]),
                "response_class": str(first["response_class"]),
                "component_endpoint_min": float(
                    first["component_endpoint_min"]),
                "component_endpoint_max": float(
                    first["component_endpoint_max"]),
                "component_max_reversals": int(
                    first["component_max_reversals"]),
                "component_directionally_supported_fraction": float(
                    first["component_directionally_supported_fraction"]),
                "component_exact_zero_fraction": float(
                    first["component_exact_zero_fraction"]),
            })
    axis_by_metric = {
        str(item["metric"]): item for item in axis_findings}
    temperature_axis = axis_by_metric.get("temperature", {})
    moisture_axis = axis_by_metric.get("moisture", {})
    precipitation_axis = axis_by_metric.get("precipitation", {})
    temperature_class = str(
        temperature_axis.get("response_class", "inconsistent_response"))
    moisture_class = str(
        moisture_axis.get("response_class", "inconsistent_response"))
    precipitation_class = str(
        precipitation_axis.get("response_class", "inconsistent_response"))
    dry_wet_class = (
        "robust_directional_support"
        if moisture_class == precipitation_class ==
        "robust_directional_support" else
        "exact_zero"
        if moisture_class == precipitation_class == "exact_zero" else
        "inconsistent_response")
    temperature_control_conclusion = (
        "no_measured_x_temperature_control"
        if temperature_class == "exact_zero" else
        "robust_directional_x_temperature_control_observed"
        if temperature_class == "robust_directional_support" else
        "x_temperature_control_not_robustly_supported")
    axis_label_findings = {
        "cold_hot_directionally_supported":
            temperature_class == "robust_directional_support",
        "dry_wet_directionally_supported":
            dry_wet_class == "robust_directional_support",
        "temperature_x_response_class": temperature_class,
        "dry_wet_response_class": dry_wet_class,
        "x_temperature_control_conclusion": temperature_control_conclusion,
        "temperature_x_exact_zero_response": bool(
            temperature_axis.get("exact_zero_response")),
        "materiality_note": (
            "Magnitude materiality, including whether a tiny nonzero response "
            "is meaningful, cannot be judged without a user-supplied "
            "threshold. No materiality threshold was invented."),
    }
    holdout_summary = None
    if holdout_rows:
        summaries = [
            row for row in holdout_rows if row["record_type"] == "group_summary"]
        overall = next(
            row for row in holdout_rows
            if row["record_type"] == "overall_summary")
        worst = largest(summaries, "mean_share_abs_error_mae")
        coordinate_only = [
            row for row in holdout_rows if row["record_type"] == "coordinate"]
        largest_cell = largest(coordinate_only, "mean_share_abs_error")
        unique_coordinates: dict[tuple[int, int], Mapping[str, Any]] = {}
        for row in coordinate_only:
            unique_coordinates.setdefault((int(row["x"]), int(row["y"])), row)
        disagreement_leaders = holdout_disagreement_leaders(coordinate_only)
        holdout_summary = {
            "worst_group_mae": worst["group"],
            "worst_group_mae_value": float(worst["mean_share_abs_error_mae"]),
            "largest_coordinate_error": {
                "group": largest_cell["group"],
                "x": largest_cell["x"], "y": largest_cell["y"],
                "absolute_error": float(largest_cell["mean_share_abs_error"]),
            },
            "top_tendency_agreement_fraction": math.fsum(
                float(row["top_tendency_agreement"])
                for row in unique_coordinates.values()
            ) / len(unique_coordinates),
            "coordinate_rank_agreement_mean": float(
                overall["coordinate_rank_agreement_mean"]),
            "global_lift_rank_agreement": float(
                overall["global_lift_rank_agreement"]),
            "unique_coordinate_leaders": disagreement_leaders,
            "largest_disagreements": disagreement_leaders["share"],
        }
    coordinate_size_rows = [
        row for row in size_difference_rows
        if row.get("record_type", "coordinate") == "coordinate"]
    pair_size_rows = [
        row for row in size_difference_rows
        if row.get("record_type") == "pair_summary"]
    largest_size = largest(
        coordinate_size_rows, "mean_share_abs_difference")
    size_winner_changes = map_size_winner_changes(per_size_rows)
    stable_cells = []
    for group in GROUPS:
        group_coordinates = sorted(
            (coordinate for coordinate in stable_coordinates
             if top_by_coordinate[coordinate] == group),
            key=lambda coordinate: (
                GRID.index(coordinate[1]), GRID.index(coordinate[0])))
        stable_cells.append({
            "group": group,
            "coordinate_count": len(group_coordinates),
            "coordinates": group_coordinates,
            "coordinate_ranges":
                measured_coordinate_ranges(group_coordinates),
        })
    mixed_coordinates = [{
        "x": x, "y": y,
        "top_tendency": representative_by_coordinate[(x, y)].get(
            "top_tendency", "unsupported"),
        "runner_up_tendency": representative_by_coordinate[(x, y)].get(
            "runner_up_tendency", "unsupported"),
        "top_margin_pp": float(
            representative_by_coordinate[(x, y)].get(
                "top_margin_pp", 0.0)),
        "component_winners":
            representative_by_coordinate[(x, y)].get(
                "component_winners", []),
        "component_disagreements":
            representative_by_coordinate[(x, y)].get(
                "component_winner_disagreements", []),
    } for x, y in sorted(
        coordinates - stable_coordinates,
        key=lambda coordinate: (
            GRID.index(coordinate[1]), GRID.index(coordinate[0])))]
    summary = {
        "dataset": dataset,
        "coordinate_count": len(coordinates),
        "stability_kind": (
            "cross_stable" if is_full else "dataset_relative"),
        "stable_coordinate_count": len(stable_coordinates),
        "mixed_or_unsupported_coordinate_count": (
            len(coordinates) - len(stable_coordinates)),
        "mixed_or_unsupported_coordinates": mixed_coordinates,
        "group_findings": group_findings,
        "axis_findings": axis_findings,
        "axis_label_findings": axis_label_findings,
        "holdout_validation": holdout_summary,
        "map_size_winner_change_count": len(size_winner_changes),
        "map_size_winner_changes": size_winner_changes,
        "largest_map_size_difference": {
            "left_size": largest_size["left_size"],
            "right_size": largest_size["right_size"],
            "group": largest_size["group"],
            "x": largest_size["x"], "y": largest_size["y"],
            "absolute_share_difference": float(
                largest_size["mean_share_abs_difference"]),
        },
        "pairwise_map_size_mae": [{
            "left_size": row["left_size"],
            "right_size": row["right_size"],
            "mean_share_mae": float(row["mean_share_pairwise_mae"]),
            "lift_pp_mae": float(row["lift_pp_pairwise_mae"]),
            "occurrence_mae": float(row["occurrence_pairwise_mae"]),
        } for row in pair_size_rows],
        "certainty_finding": (
            "A categorical background overstates certainty in every mixed or "
            "unsupported measured cell. Full six-seed/four-size cross-stable "
            "cells are direct redraw evidence."
            if is_full else
            "A categorical background overstates certainty in every mixed or "
            "unsupported measured cell. Dataset-relative stability is "
            "provisional and is not a final six-seed stability judgment."),
    }
    if is_full:
        summary["cross_stable_coordinate_count"] = len(stable_coordinates)
        summary["cross_stable_redraw_cells"] = stable_cells
    else:
        summary["dataset_relative_stable_coordinate_count"] = len(
            stable_coordinates)
        summary["dataset_relative_stable_cells"] = stable_cells
    english = markdown_summary(summary, chinese=False)
    chinese = markdown_summary(summary, chinese=True)
    return summary, english, chinese


def markdown_summary(summary: Mapping[str, Any], chinese: bool) -> str:
    if "stability_kind" in summary:
        return expanded_markdown_summary(summary, chinese)
    stable = int(summary["cross_stable_coordinate_count"])
    mixed = int(summary["mixed_or_unsupported_coordinate_count"])
    if chinese:
        lines = [
            "# 气候校准统计结论",
            "",
            f"- 81 个实测格点中，{stable} 个满足跨种子、跨尺寸稳定性；"
            f"{mixed} 个为混合或证据不足。",
            "- 分类背景在混合或证据不足的格点会夸大确定性；只有跨稳定格点"
            "足以直接指导类别边界重绘。",
            "",
            "## 生物群系倾向",
            "",
        ]
    else:
        lines = [
            "# Climate calibration statistical conclusions",
            "",
            f"- {stable} of 81 measured coordinates are cross-stable; "
            f"{mixed} are mixed or unsupported.",
            "- A categorical background overstates certainty in mixed or "
            "unsupported cells. Only cross-stable cells are direct evidence "
            "for categorical redraw boundaries.",
            "",
            "## Biome tendencies",
            "",
        ]
    if "generated_world_count" in summary:
        count_line = (
            f"- 证据包含 {int(summary['generated_world_count']):,} 个生成世界，"
            f"按原始四边形重数代表 {int(summary['weighted_raw_case_count']):,} 个案例。"
            if chinese else
            f"- Evidence contains {int(summary['generated_world_count']):,} "
            f"generated worlds representing "
            f"{int(summary['weighted_raw_case_count']):,} "
            "raw-quadrilateral-weighted cases.")
        lines.insert(2, count_line)
    for group, finding in summary["group_findings"].items():
        label = GROUP_LABELS_ZH[group] if chinese else GROUP_LABELS[group]
        favored = finding["favored_coordinate_count"]
        stable_count = finding["cross_stable_favored_count"]
        occurrence = 100 * finding["label_anchor_occurrence"]
        share = 100 * finding["label_anchor_share"]
        lift = finding["label_anchor_lift_pp"]
        anchor = tuple(finding["label_anchor"])
        if chinese:
            lines.append(
                f"- **{label}**：在 {favored} 个格点为最高正提升，其中 "
                f"{stable_count} 个跨稳定。当前标签锚点 {anchor} 的出现概率为 "
                f"{occurrence:.2f}%，平均陆地占比为 {share:.2f}%，"
                f"相对无条件基线提升 {lift:.3f} 个百分点。")
        else:
            lines.append(
                f"- **{label}** is the highest positive-lift tendency at "
                f"{favored} coordinates, {stable_count} cross-stable. At the "
                f"nearest measured current-label anchor {anchor}, occurrence "
                f"is {occurrence:.2f}%, mean terrestrial share is {share:.2f}%, "
                f"and lift is {lift:.3f} percentage points.")
    lines.extend(["", "## " + ("坐标轴响应" if chinese else "Axis response"), ""])
    for finding in summary["axis_findings"]:
        supported = (
            "方向一致" if chinese and finding["directionally_supported"] else
            "存在反转" if chinese else
            "directionally consistent" if finding["directionally_supported"] else
            "contains reversals")
        if chinese:
            lines.append(
                f"- {finding['metric']} 对 {finding['axis']} 轴的端点变化为 "
                f"{finding['endpoint_difference']:.3f}，斜率 "
                f"{finding['linear_slope']:.5f}，反转 "
                f"{finding['reversal_count']} 次（{supported}）。")
        else:
            lines.append(
                f"- {finding['metric']} versus {finding['axis']}: endpoint "
                f"change {finding['endpoint_difference']:.3f}, slope "
                f"{finding['linear_slope']:.5f}, "
                f"{finding['reversal_count']} reversals ({supported}); "
                f"seed×size endpoint range "
                f"[{finding['component_endpoint_min']:.3f}, "
                f"{finding['component_endpoint_max']:.3f}], with "
                f"{100 * finding['component_directionally_supported_fraction']:.1f}% "
                f"directionally consistent components.")
    label_findings = summary["axis_label_findings"]
    if chinese:
        lines.append(
            f"- Cold/Hot 标签的方向性证据"
            f"{'一致' if label_findings['cold_hot_directionally_supported'] else '不完全一致'}；"
            f"Dry/Wet 标签的方向性证据"
            f"{'一致' if label_findings['dry_wet_directionally_supported'] else '不完全一致'}。"
            + (
                " X 轴对温度没有可测的格点响应。"
                if label_findings["temperature_x_exact_zero_response"] else
                " 未另行虚构“实质性”阈值；报告中保留实际效应量和反转。"))
    else:
        lines.append(
            "- Cold/Hot directional evidence is "
            f"{'consistent' if label_findings['cold_hot_directionally_supported'] else 'not fully consistent'}; "
            "Dry/Wet directional evidence is "
            f"{'consistent' if label_findings['dry_wet_directionally_supported'] else 'not fully consistent'}. "
            + (
                "X has no measurable lattice response in temperature."
                if label_findings["temperature_x_exact_zero_response"] else
                "No additional materiality threshold was invented; the "
                "measured effect size and reversals are reported directly."))
    size = summary["largest_map_size_difference"]
    lines.extend(["", "## " + (
        "尺寸与留出验证" if chinese else "Map size and holdout"), ""])
    if chinese:
        lines.append(
            f"- 最大尺寸差异出现在 {size['left_size']} 与 "
            f"{size['right_size']}、{GROUP_LABELS_ZH[size['group']]}、"
            f"格点 ({size['x']}, {size['y']})："
            f"{100 * size['absolute_share_difference']:.3f} 个百分点。")
    else:
        lines.append(
            f"- Largest map-size difference: {size['left_size']} versus "
            f"{size['right_size']}, {GROUP_LABELS[size['group']]}, "
            f"({size['x']}, {size['y']}): "
            f"{100 * size['absolute_share_difference']:.3f} percentage points.")
    stability = summary.get("stability")
    if stability:
        seed_item = stability["largest_seed_sensitivity"]
        size_item = stability["largest_map_size_sensitivity"]
        if chinese:
            lines.append(
                f"- 最大种子敏感性位于 {GROUP_LABELS_ZH[seed_item['group']]} "
                f"格点 ({seed_item['x']}, {seed_item['y']})："
                f"{100 * seed_item['range']:.3f} 个百分点；"
                f"最大尺寸敏感性位于 {GROUP_LABELS_ZH[size_item['group']]} "
                f"格点 ({size_item['x']}, {size_item['y']})："
                f"{100 * size_item['range']:.3f} 个百分点。")
        else:
            lines.append(
                f"- Largest seed sensitivity: "
                f"{GROUP_LABELS[seed_item['group']]} at "
                f"({seed_item['x']}, {seed_item['y']}), "
                f"{100 * seed_item['range']:.3f} percentage points. "
                f"Largest map-size sensitivity: "
                f"{GROUP_LABELS[size_item['group']]} at "
                f"({size_item['x']}, {size_item['y']}), "
                f"{100 * size_item['range']:.3f} percentage points.")
    holdout = summary.get("holdout_validation")
    if holdout:
        if chinese:
            lines.append(
                f"- 留出集最差群组 MAE 为 "
                f"{GROUP_LABELS_ZH[holdout['worst_group_mae']]} "
                f"({100 * holdout['worst_group_mae_value']:.3f} 个百分点)；"
                f"顶层倾向一致率为 "
                f"{100 * holdout['top_tendency_agreement_fraction']:.2f}%。")
        else:
            lines.append(
                f"- Worst holdout group MAE: "
                f"{GROUP_LABELS[holdout['worst_group_mae']]} "
                f"({100 * holdout['worst_group_mae_value']:.3f} percentage "
                f"points). Top-tendency agreement is "
                f"{100 * holdout['top_tendency_agreement_fraction']:.2f}%; "
                f"mean coordinate rank agreement is "
                f"{holdout['coordinate_rank_agreement_mean']:.4f} and global "
                f"lift-rank agreement is "
                f"{holdout['global_lift_rank_agreement']:.4f}.")
    lines.extend([
        "",
        ("所有数值仅来自实测 9×9 格点；SVG 未使用插值。"
         if chinese else
         "All quantitative claims use measured 9×9 lattice points only; "
         "the SVGs use no interpolation."),
        "",
    ])
    return "\n".join(lines)


def expanded_markdown_summary(
    summary: Mapping[str, Any], chinese: bool,
) -> str:
    is_full = summary["stability_kind"] == "cross_stable"
    dataset = str(summary["dataset"])
    stable = int(summary["stable_coordinate_count"])
    mixed = int(summary["mixed_or_unsupported_coordinate_count"])
    if chinese:
        lines = [
            "# 气候校准统计结论",
            "",
            (
                f"- 81 个实测格点中，{stable} 个满足完整六种子、四尺寸稳定性；"
                f"{mixed} 个为混合或证据不足。"
                if is_full else
                f"- 81 个实测格点中，{stable} 个仅在 {dataset} 数据集内部"
                f"稳定；{mixed} 个为混合或证据不足；这不是最终六种子判断。"),
            (
                "- 分类背景在混合或证据不足的格点会夸大确定性；完整稳定格点"
                "才是直接重绘证据。"
                if is_full else
                "- 分类背景在混合或证据不足的格点会夸大确定性；当前内部稳定"
                "格点仅为阶段性证据。"),
        ]
    else:
        lines = [
            "# Climate calibration statistical conclusions",
            "",
            (
                f"- {stable} of 81 measured coordinates are cross-stable "
                f"across all six seeds and four size-specific surfaces; "
                f"{mixed} are mixed or unsupported."
                if is_full else
                f"- {stable} of 81 measured coordinates are stable only "
                f"within the {dataset} dataset components; {mixed} are mixed "
                "or unsupported. This is not a final six-seed judgment."),
            (
                "- A categorical background overstates certainty at mixed or "
                "unsupported cells; full stability is direct redraw evidence."
                if is_full else
                "- A categorical background overstates certainty at mixed or "
                "unsupported cells; dataset-relative stability is provisional."),
        ]
    if "generated_world_count" in summary:
        lines.append(
            f"- 证据包含 {int(summary['generated_world_count']):,} 个生成世界，"
            f"按原始四边形重数代表 {int(summary['weighted_raw_case_count']):,} "
            "个案例。"
            if chinese else
            f"- Evidence contains {int(summary['generated_world_count']):,} "
            f"generated worlds representing "
            f"{int(summary['weighted_raw_case_count']):,} "
            "raw-quadrilateral-weighted cases.")
    lines.extend(["", "## " + (
        "生物群系倾向" if chinese else "Biome tendencies"), ""])
    for group, finding in summary["group_findings"].items():
        label = tendency_label(group, chinese)
        ranges = format_coordinate_ranges(finding["favored_coordinate_ranges"])
        stable_text = (
            "完整稳定" if chinese and is_full else
            "数据集内部稳定" if chinese else
            "full cross-stable" if is_full else
            "dataset-relative stable")
        occurrence = 100 * float(finding["mean_occurrence_when_favored"])
        occurrence_min = 100 * float(
            finding["minimum_occurrence_when_favored"])
        occurrence_max = 100 * float(
            finding["maximum_occurrence_when_favored"])
        share = 100 * float(finding["mean_share_when_favored"])
        ge_1 = 100 * float(
            finding["mean_probability_ge_1pct_when_favored"])
        ge_5 = 100 * float(
            finding["mean_probability_ge_5pct_when_favored"])
        ge_10 = 100 * float(
            finding["mean_probability_ge_10pct_when_favored"])
        anchor = tuple(finding["label_anchor"])
        if chinese:
            lines.append(
                f"- **{label}**：最高正提升区域为 {ranges}（"
                f"{finding['favored_coordinate_count']} 个格点，其中 "
                f"{finding['stable_favored_count']} 个{stable_text}）。倾向区"
                f"平均出现率 {occurrence:.2f}%（{occurrence_min:.2f}%–"
                f"{occurrence_max:.2f}%），平均占比 {share:.2f}%，"
                f"达到 ≥1%/≥5%/≥10% 占比的概率为 "
                f"{ge_1:.2f}%/{ge_5:.2f}%/{ge_10:.2f}%。锚点 {anchor}："
                f"出现率 {100 * finding['label_anchor_occurrence']:.2f}%，"
                f"占比 {100 * finding['label_anchor_share']:.2f}%，提升 "
                f"{finding['label_anchor_lift_pp']:.3f} 个百分点，"
                f"≥1%/≥5%/≥10% 概率 "
                f"{100 * finding['label_anchor_probability_ge_1pct']:.2f}%/"
                f"{100 * finding['label_anchor_probability_ge_5pct']:.2f}%/"
                f"{100 * finding['label_anchor_probability_ge_10pct']:.2f}%。")
        else:
            lines.append(
                f"- **{label}** has the highest positive lift at {ranges} "
                f"({finding['favored_coordinate_count']} cells; "
                f"{finding['stable_favored_count']} {stable_text}). In that "
                f"favored zone, mean occurrence is {occurrence:.2f}% "
                f"({occurrence_min:.2f}%–{occurrence_max:.2f}%), mean share is "
                f"{share:.2f}%, and ≥1%/≥5%/≥10% probabilities are "
                f"{ge_1:.2f}%/{ge_5:.2f}%/{ge_10:.2f}%. At anchor {anchor}, "
                f"occurrence is "
                f"{100 * finding['label_anchor_occurrence']:.2f}%, share is "
                f"{100 * finding['label_anchor_share']:.2f}%, lift is "
                f"{finding['label_anchor_lift_pp']:.3f} pp, and "
                f"≥1%/≥5%/≥10% probabilities are "
                f"{100 * finding['label_anchor_probability_ge_1pct']:.2f}%/"
                f"{100 * finding['label_anchor_probability_ge_5pct']:.2f}%/"
                f"{100 * finding['label_anchor_probability_ge_10pct']:.2f}%.")

    stable_cells = (
        summary["cross_stable_redraw_cells"] if is_full else
        summary["dataset_relative_stable_cells"])
    lines.extend(["", "## " + (
        "完整稳定重绘格点" if chinese and is_full else
        "数据集内部稳定格点" if chinese else
        "Full cross-stable redraw cells" if is_full else
        "Dataset-relative stable measured cells"), ""])
    if not is_full:
        lines.append(
            "- 以下仅为阶段性数据集内部证据，不能替代最终六种子判断。"
            if chinese else
            "- These are provisional within-dataset cells, not a final "
            "six-seed judgment.")
    for cell in stable_cells:
        lines.append(
            f"- **{tendency_label(str(cell['group']), chinese)}**："
            f"{cell['coordinate_count']} 个；"
            f"{format_coordinate_ranges(cell['coordinate_ranges'])}。"
            if chinese else
            f"- **{tendency_label(str(cell['group']), chinese)}**: "
            f"{cell['coordinate_count']} cells; "
            f"{format_coordinate_ranges(cell['coordinate_ranges'])}.")

    lines.extend(["", "## " + (
        "混合或证据不足格点" if chinese else
        "Mixed or unsupported measured cells"), ""])
    mixed_rows = summary["mixed_or_unsupported_coordinates"]
    if not mixed_rows:
        lines.append("- 无。" if chinese else "- None.")
    for row in mixed_rows:
        evidence = (
            row["component_disagreements"] or row["component_winners"])
        evidence_text = ", ".join(str(item) for item in evidence) or "none"
        lines.append(
            f"- ({row['x']},{row['y']})：最高="
            f"{tendency_label(str(row['top_tendency']), chinese)}，次高="
            f"{tendency_label(str(row['runner_up_tendency']), chinese)}，差值="
            f"{row['top_margin_pp']:.6f} 个百分点；{evidence_text}。"
            if chinese else
            f"- ({row['x']},{row['y']}): top="
            f"{tendency_label(str(row['top_tendency']), chinese)}, runner="
            f"{tendency_label(str(row['runner_up_tendency']), chinese)}, "
            f"margin={row['top_margin_pp']:.6f} pp; {evidence_text}.")

    lines.extend(["", "## " + (
        "坐标轴响应" if chinese else "Axis response"), ""])
    class_labels = {
        "exact_zero": ("精确零响应", "exact-zero response"),
        "robust_directional_support": ("稳健方向支持", "robust directional support"),
        "inconsistent_response": ("响应不一致", "inconsistent response"),
    }
    for finding in summary["axis_findings"]:
        classification = class_labels[str(finding["response_class"])][
            0 if chinese else 1]
        lines.append(
            f"- {finding['metric']} 对 {finding['axis']}：端点变化 "
            f"{finding['endpoint_difference']:.6f}，斜率 "
            f"{finding['linear_slope']:.8f}，反转 "
            f"{finding['reversal_count']} 次，分类={classification}，"
            f"种子×尺寸端点范围 [{finding['component_endpoint_min']:.6f}, "
            f"{finding['component_endpoint_max']:.6f}]。"
            if chinese else
            f"- {finding['metric']} versus {finding['axis']}: endpoint "
            f"{finding['endpoint_difference']:.6f}, slope "
            f"{finding['linear_slope']:.8f}, "
            f"{finding['reversal_count']} reversals, "
            f"classification={classification}, seed×size endpoint range "
            f"[{finding['component_endpoint_min']:.6f}, "
            f"{finding['component_endpoint_max']:.6f}].")
    axis_labels = summary["axis_label_findings"]
    control = str(axis_labels["x_temperature_control_conclusion"])
    if chinese:
        control_text = {
            "no_measured_x_temperature_control":
                "X 轴在实测格点上不控制温度（精确零响应）。",
            "robust_directional_x_temperature_control_observed":
                "X 轴对温度有稳健方向控制，但幅度是否足够大仍未判定。",
            "x_temperature_control_not_robustly_supported":
                "X 轴对温度的控制未获稳健支持。",
        }[control]
        lines.extend([
            f"- Cold/Hot 分类={axis_labels['temperature_x_response_class']}；"
            f"Dry/Wet 分类={axis_labels['dry_wet_response_class']}。"
            f"{control_text}",
            "- 数值幅度的实质性（包括微小非零响应是否有意义）必须由用户提供"
            "阈值后才能判断；本处理器没有虚构阈值。",
        ])
    else:
        control_text = {
            "no_measured_x_temperature_control":
                "X does not control temperature on the measured lattice "
                "(exact-zero response).",
            "robust_directional_x_temperature_control_observed":
                "X has robust directional control of temperature, but its "
                "magnitude has not been judged material.",
            "x_temperature_control_not_robustly_supported":
                "Robust X control of temperature is not supported.",
        }[control]
        lines.extend([
            "- Cold/Hot classification="
            f"{axis_labels['temperature_x_response_class']}; Dry/Wet "
            f"classification={axis_labels['dry_wet_response_class']}. "
            f"{control_text}",
            "- Magnitude materiality—including whether a tiny nonzero response "
            "is meaningful—cannot be judged without a user-supplied threshold; "
            "no threshold was invented.",
        ])

    lines.extend(["", "## " + (
        "地图尺寸与留出验证" if chinese else
        "Map size and holdout validation"), ""])
    size = summary["largest_map_size_difference"]
    lines.append(
        f"- 最大尺寸占比差异：{size['left_size']} 对 {size['right_size']}，"
        f"{tendency_label(str(size['group']), chinese)}，"
        f"({size['x']},{size['y']})，"
        f"{100 * size['absolute_share_difference']:.6f} 个百分点。"
        if chinese else
        f"- Largest map-size share difference: {size['left_size']} versus "
        f"{size['right_size']}, "
        f"{tendency_label(str(size['group']), chinese)}, "
        f"({size['x']},{size['y']}), "
        f"{100 * size['absolute_share_difference']:.6f} pp.")
    size_changes = summary["map_size_winner_changes"]
    lines.append(
        f"- {len(size_changes)} 个格点的最高倾向随地图尺寸变化。"
        if chinese else
        f"- The highest tendency changes by map size at "
        f"{len(size_changes)} measured coordinates.")
    for change in size_changes:
        components = "; ".join(
            f"{item['map_size']}="
            f"{tendency_label(str(item['top_tendency']), chinese)}"
            f"[{float(item['top_margin_pp']):.6f}pp]"
            for item in change["winners"])
        lines.append(
            f"- ({change['x']},{change['y']})：{components}。"
            if chinese else
            f"- ({change['x']},{change['y']}): {components}.")
    stability = summary.get("stability")
    if stability:
        seed_item = stability["largest_seed_sensitivity"]
        size_item = stability["largest_map_size_sensitivity"]
        lines.append(
            f"- 最大种子敏感性："
            f"{tendency_label(str(seed_item['group']), chinese)} "
            f"({seed_item['x']},{seed_item['y']}) "
            f"{100 * seed_item['range']:.6f} 个百分点；最大尺寸敏感性："
            f"{tendency_label(str(size_item['group']), chinese)} "
            f"({size_item['x']},{size_item['y']}) "
            f"{100 * size_item['range']:.6f} 个百分点。"
            if chinese else
            f"- Largest seed sensitivity: "
            f"{tendency_label(str(seed_item['group']), chinese)} "
            f"({seed_item['x']},{seed_item['y']}) "
            f"{100 * seed_item['range']:.6f} pp; largest size sensitivity: "
            f"{tendency_label(str(size_item['group']), chinese)} "
            f"({size_item['x']},{size_item['y']}) "
            f"{100 * size_item['range']:.6f} pp.")

    holdout = summary.get("holdout_validation")
    if holdout:
        lines.append(
            f"- 留出集最差群组 MAE："
            f"{tendency_label(str(holdout['worst_group_mae']), chinese)} "
            f"{100 * holdout['worst_group_mae_value']:.6f} 个百分点；"
            f"最高倾向一致率 "
            f"{100 * holdout['top_tendency_agreement_fraction']:.2f}%；"
            f"坐标秩一致性 {holdout['coordinate_rank_agreement_mean']:.6f}，"
            f"全局提升秩一致性 {holdout['global_lift_rank_agreement']:.6f}。"
            if chinese else
            f"- Worst holdout group MAE: "
            f"{tendency_label(str(holdout['worst_group_mae']), chinese)} "
            f"{100 * holdout['worst_group_mae_value']:.6f} pp; "
            f"top-tendency agreement "
            f"{100 * holdout['top_tendency_agreement_fraction']:.2f}%; "
            f"coordinate rank agreement "
            f"{holdout['coordinate_rank_agreement_mean']:.6f}; global lift "
            f"rank agreement {holdout['global_lift_rank_agreement']:.6f}.")
        leader_labels = {
            "share": ("占比", "share"),
            "occurrence": ("出现率", "occurrence"),
            "threshold_ge_1pct": ("≥1% 概率", "≥1% probability"),
            "threshold_ge_5pct": ("≥5% 概率", "≥5% probability"),
            "threshold_ge_10pct": ("≥10% 概率", "≥10% probability"),
            "lift": ("提升值", "lift"),
        }
        leaders = holdout["unique_coordinate_leaders"]
        for metric, labels in leader_labels.items():
            entries = "; ".join(
                f"({item['x']},{item['y']})/"
                f"{tendency_label(str(item['group']), chinese)}="
                f"{(item['absolute_error'] if metric == 'lift' else 100 * item['absolute_error']):.6f}pp"
                for item in leaders[metric])
            lines.append(
                f"- 唯一格点{labels[0]}差异前列：{entries}。"
                if chinese else
                f"- Unique-coordinate {labels[1]} disagreement leaders: "
                f"{entries}.")
        rank_entries = "; ".join(
            f"({item['x']},{item['y']})/rho={item['rank_agreement']:.6f}"
            for item in leaders["rank"])
        top_entries = "; ".join(
            f"({item['x']},{item['y']})/"
            f"{tendency_label(str(item['calibration_top_tendency']), chinese)}"
            f"→{tendency_label(str(item['holdout_top_tendency']), chinese)}"
            for item in leaders["top_tendency"])
        lines.extend([
            f"- 唯一格点秩差异前列：{rank_entries}。"
            if chinese else
            f"- Unique-coordinate rank-disagreement leaders: {rank_entries}.",
            f"- 最高倾向变化格点：{top_entries or '无'}。"
            if chinese else
            f"- Top-tendency disagreement coordinates: {top_entries or 'none'}.",
        ])
    lines.extend([
        "",
        "所有数值仅来自实测 9×9 格点；SVG 未使用插值。"
        if chinese else
        "All quantitative claims use measured 9×9 lattice points only; "
        "the SVGs use no interpolation.",
        "",
    ])
    return "\n".join(lines)


def write_lineage(
    run_root: Path,
    output_dir: Path,
    published_output_dir: Path,
    shards: Sequence[Shard],
    output_paths: Sequence[Path],
    model_hash: str = "",
) -> None:
    rows: list[dict[str, Any]] = []
    origin = load_raw_origin(run_root)
    for shard in sorted(shards, key=lambda item: item.logical_path):
        raw_path = origin.resolve(shard.logical_path)
        if raw_path != shard.path.resolve():
            raise RuntimeError(
                f"raw shard logical/physical path mismatch: {shard.path}")
        rows.append({
            "kind": "raw_shard",
            "path": shard.logical_path,
            "sha256": shard.sha256, "bytes": shard.path.stat().st_size,
            "rows": shard.row_count, "model_sha256": "",
        })
    for path in sorted(output_paths, key=lambda item: str(item)):
        if path.is_file():
            staged_path = path.resolve()
            if not staged_path.is_relative_to(output_dir):
                raise RuntimeError(
                    f"processed output is outside phase output: {staged_path}")
            published_path = (
                published_output_dir /
                staged_path.relative_to(output_dir)
            )
            if not published_path.is_relative_to(run_root):
                raise RuntimeError(
                    f"published output is outside run root: {published_path}")
            rows.append({
                "kind": "processed_output",
                "path": published_path.relative_to(run_root).as_posix(),
                "sha256": sha256_file(path), "bytes": path.stat().st_size,
                "rows": "", "model_sha256": model_hash,
            })
    write_csv_new(
        output_dir / "summary_lineage.csv", rows,
        ("kind", "path", "sha256", "bytes", "rows", "model_sha256"))


def process_raw(
    run_root: Path, datasets: Mapping[str, Sequence[int]],
    contract: WeightContract, manifest_phase: str,
) -> tuple[
    list[dict[str, Any]], list[dict[str, Any]], list[dict[str, Any]], list[Shard]
]:
    all_surface: list[dict[str, Any]] = []
    all_continuous: list[dict[str, Any]] = []
    failures: list[dict[str, Any]] = []
    all_shards: list[Shard] = []
    discovered: dict[str, list[Shard]] = {}
    origin = load_raw_origin(run_root)
    for dataset, seeds in datasets.items():
        shards = discover_shards(run_root, dataset, seeds, origin)
        discovered[dataset] = shards
        all_shards.extend(shards)
    verify_phase_manifest(run_root, manifest_phase, all_shards, origin)
    for dataset, seeds in datasets.items():
        shards = discovered[dataset]
        for seed in seeds:
            records_by_size, seed_failures = read_seed_records(
                shards, seed, contract, origin)
            failures.extend(seed_failures)
            if seed_failures:
                continue
            for size in MAP_SIZES:
                surface, continuous = compute_seed_size(
                    dataset, seed, size, records_by_size[size], contract)
                all_surface.extend(surface)
                all_continuous.extend(continuous)
    return all_surface, all_continuous, failures, all_shards


def write_common_outputs(
    output_dir: Path,
    surface_name: str,
    combined: list[dict[str, Any]],
    seed_rows: list[dict[str, Any]],
    size_rows: list[dict[str, Any]],
    seed_size_rows: list[dict[str, Any]],
    failures: Sequence[Mapping[str, Any]],
) -> list[Path]:
    output_dir.mkdir(parents=True, exist_ok=False)
    paths: list[Path] = []
    surface_path = output_dir / surface_name
    write_csv_new(surface_path, combined, surface_fields(combined))
    paths.append(surface_path)
    seed_path = output_dir / "per_seed_surface.csv"
    write_csv_new(seed_path, seed_rows, surface_fields(seed_rows))
    paths.append(seed_path)
    size_path = output_dir / "per_size_surface.csv"
    write_csv_new(size_path, size_rows, surface_fields(size_rows))
    paths.append(size_path)
    seed_size_path = output_dir / "per_seed_size_surface.csv"
    write_csv_new(
        seed_size_path, seed_size_rows, surface_fields(seed_size_rows))
    paths.append(seed_size_path)
    failure_path = output_dir / "failures.csv"
    failure_fields = (
        "dataset", "seed", "map_size", "config_index", "failure_stage",
        "failure_reason", "shard", "row_number")
    write_csv_new(failure_path, list(failures), failure_fields)
    paths.append(failure_path)
    return paths


def process_qualification(
    run_root: Path, output_root: Path | None = None
) -> Path:
    contract = load_weight_contract(run_root)
    surface, continuous, failures, shards = process_raw(
        run_root, {"calibration": QUALIFICATION_SEEDS}, contract,
        "qualification")
    output_base = run_root if output_root is None else output_root
    output_dir = output_base / "processed" / "qualification"
    published_output_dir = run_root / "processed" / "qualification"
    if failures:
        output_dir.mkdir(parents=True, exist_ok=False)
        failure_path = output_dir / "failures.csv"
        write_csv_new(failure_path, failures, tuple(failures[0]))
        raise RuntimeError(
            f"qualification contains {len(failures)} generation failures")
    seed_kind = {QUALIFICATION_SEEDS[0]: "qualification"}
    seed_rows = per_seed_surface(surface, seed_kind)
    size_rows = per_size_surface(surface, "qualification")
    seed_size_rows = seed_size_surface(surface, "qualification", seed_kind)
    combined = combine_surface(surface, "qualification", "equal_seed_size")
    attach_tendencies(combined, seed_rows, size_rows)
    paths = write_common_outputs(
        output_dir, "qualification_surface.csv", combined,
        seed_rows, size_rows, seed_size_rows, failures)
    continuous_combined = average_continuous(continuous, "qualification")
    axis_rows = build_axis_response(
        {"qualification": continuous_combined},
        {"qualification": continuous})
    axis_path = output_dir / "axis_response.csv"
    write_csv_new(axis_path, axis_rows, tuple(axis_rows[0]))
    paths.append(axis_path)
    svg_dir = output_dir / "svg"
    svg_dir.mkdir()
    for filename, metric, diverging in (
        ("absolute_share.svg", "mean_land_share", False),
        ("lift.svg", "lift_pp", True),
        ("occurrence.svg", "occurrence_probability", False),
        ("uncertainty.svg", "other_corner_p90_p10", False),
    ):
        path = svg_dir / filename
        render_grid_svg(path, f"Qualification {metric}", combined, metric, diverging)
        paths.append(path)
    size_diff = size_differences(size_rows, "qualification")
    size_path = output_dir / "size_differences.csv"
    write_csv_new(
        size_path, size_diff,
        mixed_record_fields(
            size_diff, ("record_type", "dataset", "left_size", "right_size",
                        "x", "y", "group")))
    paths.append(size_path)
    summary, english, chinese = substantive_summary(
        "qualification", combined, None, size_diff, size_rows, axis_rows)
    summary.update({
        "seed_count": 1,
        "map_size_count": 4,
        "effective_configuration_count": 28561,
        "generated_world_count": 114244,
        "weighted_raw_case_count": 1562500,
        "stability": stability_findings(seed_rows, size_rows),
    })
    english = markdown_summary(summary, chinese=False)
    chinese = markdown_summary(summary, chinese=True)
    summary_path = output_dir / "summary.json"
    write_json_new(summary_path, summary)
    paths.append(summary_path)
    en_path = output_dir / "summary_en.md"
    zh_path = output_dir / "summary_zh.md"
    write_text_new(en_path, english)
    write_text_new(zh_path, chinese)
    paths.extend((en_path, zh_path))
    write_lineage(
        run_root, output_dir, published_output_dir, shards, paths)
    return output_dir


def process_calibration(
    run_root: Path, output_root: Path | None = None
) -> Path:
    contract = load_weight_contract(run_root)
    surface, continuous, failures, shards = process_raw(
        run_root, {"calibration": CALIBRATION_SEEDS}, contract,
        "calibration")
    output_base = run_root if output_root is None else output_root
    output_dir = output_base / "processed" / "calibration"
    published_output_dir = run_root / "processed" / "calibration"
    if failures:
        output_dir.mkdir(parents=True, exist_ok=False)
        write_csv_new(output_dir / "failures.csv", failures, tuple(failures[0]))
        raise RuntimeError(
            f"calibration contains {len(failures)} generation failures")
    seed_kind = {seed: "calibration" for seed in CALIBRATION_SEEDS}
    seed_rows = per_seed_surface(surface, seed_kind)
    size_rows = per_size_surface(surface, "calibration")
    seed_size_rows = seed_size_surface(surface, "calibration", seed_kind)
    combined = combine_surface(surface, "calibration", "equal_seed_size")
    attach_tendencies(combined, seed_rows, size_rows)
    paths = write_common_outputs(
        output_dir, "calibration_surface.csv", combined,
        seed_rows, size_rows, seed_size_rows, failures)
    continuous_combined = average_continuous(continuous, "calibration")
    axis_rows = build_axis_response(
        {"calibration": continuous_combined}, {"calibration": continuous})
    axis_path = output_dir / "axis_response.csv"
    write_csv_new(axis_path, axis_rows, tuple(axis_rows[0]))
    paths.append(axis_path)
    size_diff = size_differences(size_rows, "calibration")
    size_path = output_dir / "size_differences.csv"
    write_csv_new(
        size_path, size_diff,
        mixed_record_fields(
            size_diff, ("record_type", "dataset", "left_size", "right_size",
                        "x", "y", "group")))
    paths.append(size_path)
    calibration_raw_digest = raw_digest(shards)
    serialized_surface = read_surface(
        output_dir / "calibration_surface.csv")
    model = model_payload(serialized_surface, calibration_raw_digest)
    model_path = output_dir / "calibration_model.json"
    write_json_new(model_path, model)
    model_hash = sha256_file(model_path)
    model_hash_path = output_dir / "calibration_model.sha256"
    write_text_new(model_hash_path, model_hash + "\n")
    paths.extend((model_path, model_hash_path))
    surface_hash_path, freeze_manifest_path, surface_hash, frozen_model_hash = (
        write_calibration_freeze(
            output_dir, output_dir / "calibration_surface.csv", model_path,
            calibration_raw_digest))
    if frozen_model_hash != model_hash:
        raise RuntimeError("calibration model changed while freezing evidence")
    verified_freeze = verify_calibration_freeze(output_dir)
    if (
        verified_freeze["model_sha256"] != model_hash
        or verified_freeze["surface_sha256"] != surface_hash
    ):
        raise RuntimeError("calibration freeze changed during immediate verify")
    paths.extend((surface_hash_path, freeze_manifest_path))
    svg_dir = output_dir / "svg"
    svg_dir.mkdir()
    for filename, metric, diverging in (
        ("absolute_share.svg", "mean_land_share", False),
        ("lift.svg", "lift_pp", True),
        ("occurrence.svg", "occurrence_probability", False),
        ("uncertainty.svg", "other_corner_p90_p10", False),
    ):
        path = svg_dir / filename
        render_grid_svg(path, f"Calibration {metric}", combined, metric, diverging)
        paths.append(path)
    summary, english, chinese = substantive_summary(
        "calibration", combined, None, size_diff, size_rows, axis_rows)
    summary["calibration_model_sha256"] = model_hash
    summary["calibration_surface_sha256"] = surface_hash
    summary["calibration_freeze_manifest_sha256"] = sha256_file(
        freeze_manifest_path)
    summary.update({
        "seed_count": 4,
        "map_size_count": 4,
        "effective_configuration_count": 28561,
        "generated_world_count": 456976,
        "weighted_raw_case_count": 6250000,
        "stability": stability_findings(seed_rows, size_rows),
    })
    english = markdown_summary(summary, chinese=False)
    chinese = markdown_summary(summary, chinese=True)
    summary_path = output_dir / "summary.json"
    write_json_new(summary_path, summary)
    paths.append(summary_path)
    en_path = output_dir / "summary_en.md"
    zh_path = output_dir / "summary_zh.md"
    write_text_new(en_path, english)
    write_text_new(zh_path, chinese)
    paths.extend((en_path, zh_path))
    write_lineage(
        run_root, output_dir, published_output_dir,
        shards, paths, model_hash)
    return output_dir


def read_surface(path: Path) -> list[dict[str, Any]]:
    with path.open("r", encoding="utf-8", newline="") as source:
        rows = list(csv.DictReader(source))
    numeric = set(SURFACE_METRICS) | {
        "component_min_land_share", "component_max_land_share",
        "component_range_land_share", "component_stddev_land_share",
        "top_margin_pp",
    }
    for row in rows:
        for field in numeric:
            if field in row and row[field] != "":
                row[field] = float(row[field])
        for field in (
            "x", "y", "seed", "stability_seed_component_count",
            "stability_size_component_count"):
            if field in row and row[field] != "":
                row[field] = int(row[field])
        if "cross_stable" in row:
            row["cross_stable"] = parse_truth(str(row["cross_stable"]))
        if "dataset_relative_stable" in row:
            row["dataset_relative_stable"] = parse_truth(
                str(row["dataset_relative_stable"]))
    return rows


def process_full(run_root: Path, output_root: Path | None = None) -> Path:
    output_base = run_root if output_root is None else output_root
    calibration_dir = run_root / "processed" / "calibration"
    model_path = calibration_dir / "calibration_model.json"
    if not model_path.is_file():
        raise RuntimeError(
            "full processing requires a published frozen calibration")
    frozen = verify_calibration_freeze(calibration_dir)
    frozen_hash = str(frozen["model_sha256"])
    frozen_surface_hash = str(frozen["surface_sha256"])
    calibration = list(frozen["surface"])

    contract = load_weight_contract(run_root)
    hold_surface, hold_continuous, failures, hold_shards = process_raw(
        run_root, {"holdout": HOLDOUT_SEEDS}, contract, "holdout")
    if failures:
        output_dir = output_base / "processed" / "holdout"
        output_dir.mkdir(parents=True, exist_ok=False)
        write_csv_new(output_dir / "failures.csv", failures, tuple(failures[0]))
        raise RuntimeError(f"holdout contains {len(failures)} generation failures")
    hold_seed_kind = {seed: "holdout" for seed in HOLDOUT_SEEDS}
    hold_seed_rows = per_seed_surface(hold_surface, hold_seed_kind)
    hold_size_rows = per_size_surface(hold_surface, "holdout")
    hold_seed_size_rows = seed_size_surface(
        hold_surface, "holdout", hold_seed_kind)
    hold_combined = combine_surface(hold_surface, "holdout", "equal_seed_size")
    attach_tendencies(hold_combined, hold_seed_rows, hold_size_rows)
    holdout_dir = output_base / "processed" / "holdout"
    published_holdout_dir = run_root / "processed" / "holdout"
    hold_paths = write_common_outputs(
        holdout_dir, "holdout_surface.csv", hold_combined,
        hold_seed_rows, hold_size_rows, hold_seed_size_rows, failures)
    validation = holdout_validation(calibration, hold_combined)
    validation_path = holdout_dir / "holdout_validation.csv"
    validation_fields = sorted({
        field for row in validation for field in row},
        key=lambda field: (
            ("record_type", "group", "x", "y").index(field)
            if field in ("record_type", "group", "x", "y") else 99, field))
    write_csv_new(validation_path, validation, validation_fields)
    hold_paths.append(validation_path)
    hold_size_diff = size_differences(hold_size_rows, "holdout")
    hold_size_path = holdout_dir / "size_differences.csv"
    write_csv_new(
        hold_size_path, hold_size_diff,
        mixed_record_fields(
            hold_size_diff,
            ("record_type", "dataset", "left_size", "right_size",
             "x", "y", "group")))
    hold_paths.append(hold_size_path)
    hold_continuous_combined = average_continuous(
        hold_continuous, "holdout")
    hold_axis_rows = build_axis_response(
        {"holdout": hold_continuous_combined},
        {"holdout": hold_continuous})
    hold_axis_path = holdout_dir / "axis_response.csv"
    write_csv_new(hold_axis_path, hold_axis_rows, tuple(hold_axis_rows[0]))
    hold_paths.append(hold_axis_path)
    hold_svg_dir = holdout_dir / "svg"
    hold_svg_dir.mkdir()
    for filename, metric, diverging in (
        ("absolute_share.svg", "mean_land_share", False),
        ("lift.svg", "lift_pp", True),
        ("occurrence.svg", "occurrence_probability", False),
        ("uncertainty.svg", "other_corner_p90_p10", False),
    ):
        path = hold_svg_dir / filename
        render_grid_svg(path, f"Holdout {metric}",
                        hold_combined, metric, diverging)
        hold_paths.append(path)
    hold_summary, hold_english, hold_chinese = substantive_summary(
        "holdout", hold_combined, validation,
        hold_size_diff, hold_size_rows, hold_axis_rows)
    hold_summary["calibration_model_sha256"] = frozen_hash
    hold_summary["calibration_surface_sha256"] = frozen_surface_hash
    hold_summary["calibration_freeze_manifest_sha256"] = str(
        frozen["manifest_sha256"])
    hold_summary.update({
        "seed_count": 2,
        "map_size_count": 4,
        "effective_configuration_count": 28561,
        "generated_world_count": 228488,
        "weighted_raw_case_count": 3125000,
        "stability": stability_findings(
            hold_seed_rows, hold_size_rows),
    })
    hold_english = markdown_summary(hold_summary, chinese=False)
    hold_chinese = markdown_summary(hold_summary, chinese=True)
    hold_summary_path = holdout_dir / "summary.json"
    write_json_new(hold_summary_path, hold_summary)
    hold_paths.append(hold_summary_path)
    hold_en_path = holdout_dir / "summary_en.md"
    hold_zh_path = holdout_dir / "summary_zh.md"
    write_text_new(hold_en_path, hold_english)
    write_text_new(hold_zh_path, hold_chinese)
    hold_paths.extend((hold_en_path, hold_zh_path))
    write_lineage(
        run_root, holdout_dir, published_holdout_dir,
        hold_shards, hold_paths, frozen_hash)

    calibration_raw_surface, calibration_continuous, cal_failures, cal_shards = (
        process_raw(
            run_root, {"calibration": CALIBRATION_SEEDS}, contract,
            "calibration"))
    if cal_failures:
        raise RuntimeError("calibration raw data changed to include failures")
    full_surface_components = calibration_raw_surface + hold_surface
    full_continuous_components = calibration_continuous + hold_continuous
    full_seed_kind = {
        **{seed: "calibration" for seed in CALIBRATION_SEEDS},
        **{seed: "holdout" for seed in HOLDOUT_SEEDS},
    }
    full_seed_rows = per_seed_surface(full_surface_components, full_seed_kind)
    full_size_rows = per_size_surface(full_surface_components, "full")
    full_seed_size_rows = seed_size_surface(
        full_surface_components, "full", full_seed_kind)
    full_combined = combine_surface(
        full_surface_components, "full", "equal_seed_size")
    attach_tendencies(full_combined, full_seed_rows, full_size_rows)
    full_dir = output_base / "processed" / "full"
    published_full_dir = run_root / "processed" / "full"
    full_paths = write_common_outputs(
        full_dir, "full_surface.csv", full_combined,
        full_seed_rows, full_size_rows, full_seed_size_rows, ())
    size_rows = (
        size_differences(full_size_rows, "full") +
        size_differences(hold_size_rows, "holdout"))
    size_path = full_dir / "size_differences.csv"
    write_csv_new(
        size_path, size_rows,
        mixed_record_fields(
            size_rows, ("record_type", "dataset", "left_size", "right_size",
                        "x", "y", "group")))
    full_paths.append(size_path)
    continuous_surfaces = {
        "calibration": average_continuous(calibration_continuous, "calibration"),
        "holdout": average_continuous(hold_continuous, "holdout"),
        "full": average_continuous(full_continuous_components, "full"),
    }
    axis_rows = build_axis_response(
        continuous_surfaces,
        {
            "calibration": calibration_continuous,
            "holdout": hold_continuous,
            "full": full_continuous_components,
        })
    axis_path = full_dir / "axis_response.csv"
    write_csv_new(axis_path, axis_rows, tuple(axis_rows[0]))
    full_paths.append(axis_path)
    copy_validation_path = full_dir / "holdout_validation.csv"
    write_bytes_new(copy_validation_path, validation_path.read_bytes())
    full_paths.append(copy_validation_path)

    svg_dir = full_dir / "svg"
    svg_dir.mkdir()
    svg_specs = (
        ("absolute_share.svg", full_combined, "mean_land_share", False),
        ("lift.svg", full_combined, "lift_pp", True),
        ("occurrence.svg", full_combined, "occurrence_probability", False),
        ("uncertainty.svg", full_combined, "other_corner_p90_p10", False),
    )
    for filename, rows, metric, diverging in svg_specs:
        path = svg_dir / filename
        render_grid_svg(path, f"Full {metric}", rows, metric, diverging)
        full_paths.append(path)
    size_range_rows = []
    by_key: dict[tuple[int, int, str], list[Mapping[str, Any]]] = defaultdict(list)
    for row in full_size_rows:
        by_key[(int(row["x"]), int(row["y"]), str(row["group"]))].append(row)
    for (x, y, group), rows in sorted(by_key.items()):
        values = [float(row["mean_land_share"]) for row in rows]
        size_range_rows.append({
            "x": x, "y": y, "group": group,
            "size_range": max(values) - min(values)})
    size_svg = svg_dir / "size_differences.svg"
    render_grid_svg(
        size_svg, "Full map-size share range", size_range_rows, "size_range")
    full_paths.append(size_svg)
    cal_hold_difference = []
    cal_index = {
        (int(row["x"]), int(row["y"]), str(row["group"])): row
        for row in calibration}
    hold_index = {
        (int(row["x"]), int(row["y"]), str(row["group"])): row
        for row in hold_combined}
    for key in sorted(cal_index):
        cal_hold_difference.append({
            "x": key[0], "y": key[1], "group": key[2],
            "calibration_minus_holdout": (
                float(cal_index[key]["mean_land_share"]) -
                float(hold_index[key]["mean_land_share"])),
        })
    difference_svg = svg_dir / "calibration_vs_holdout.svg"
    render_grid_svg(
        difference_svg, "Calibration minus holdout share",
        cal_hold_difference, "calibration_minus_holdout", True)
    full_paths.append(difference_svg)

    summary, english, chinese = substantive_summary(
        "full", full_combined, validation,
        [row for row in size_rows if row["dataset"] == "full"],
        full_size_rows, axis_rows)
    summary["calibration_model_sha256"] = frozen_hash
    summary["calibration_surface_sha256"] = frozen_surface_hash
    final_freeze = verify_calibration_freeze(calibration_dir)
    summary["calibration_freeze_manifest_sha256"] = str(
        final_freeze["manifest_sha256"])
    summary["calibration_model_unchanged_after_holdout"] = (
        final_freeze["model_sha256"] == frozen_hash)
    summary["calibration_surface_unchanged_after_holdout"] = (
        final_freeze["surface_sha256"] == frozen_surface_hash)
    summary["weighted_raw_case_count"] = 9375000
    summary["generated_world_count"] = 685464
    summary["seed_count"] = 6
    summary["map_size_count"] = 4
    summary["effective_configuration_count"] = 28561
    summary["stability"] = stability_findings(
        full_seed_rows, full_size_rows)
    english = markdown_summary(summary, chinese=False)
    chinese = markdown_summary(summary, chinese=True)
    summary_path = full_dir / "summary.json"
    write_json_new(summary_path, summary)
    full_paths.append(summary_path)
    en_path = full_dir / "summary_en.md"
    zh_path = full_dir / "summary_zh.md"
    write_text_new(en_path, english)
    write_text_new(zh_path, chinese)
    full_paths.extend((en_path, zh_path))
    all_shards = cal_shards + hold_shards
    write_lineage(
        run_root, full_dir, published_full_dir,
        all_shards, full_paths, frozen_hash)
    if (
        final_freeze["model_sha256"] != frozen_hash or
        final_freeze["surface_sha256"] != frozen_surface_hash
    ):
        raise RuntimeError(
            "holdout processing altered frozen calibration evidence")
    return full_dir


def run_self_tests() -> None:
    assert roles_for_coordinate(-12, 12) == ("TL",)
    assert roles_for_coordinate(0, 12) == ("TL", "TR")
    assert roles_for_coordinate(0, 0) == ROLE_ORDER
    samples = [(0.0, 1), (0.5, 2), (1.0, 1)]
    assert weighted_quantile(samples, 0.10) == 0.0
    assert weighted_quantile(samples, 0.50) == 0.5
    assert weighted_quantile(samples, 0.90) == 1.0
    records = {
        0: RawRecord(0, 1, 100, (100, 0, 0, 0, 0, 0, 0, 0),
                     (10.0, 20.0, 30.0)),
        1: RawRecord(1, 3, 100, (0, 100, 0, 0, 0, 0, 0, 0),
                     (30.0, 40.0, 50.0)),
    }
    stats = weighted_group_stats(records, ((0, 1), (1, 3)), 0)
    assert abs(stats["mean_land_share"] - 0.25) < EPSILON
    assert abs(stats["occurrence_probability"] - 0.25) < EPSILON
    assert abs(weighted_continuous_mean(
        records, ((0, 1), (1, 3)), 0) - 25.0) < EPSILON
    averaged = mean_rows([
        {metric: 0.0 for metric in SURFACE_METRICS},
        {metric: 2.0 for metric in SURFACE_METRICS},
    ], {"x": 0, "y": 0, "group": "icefield"})
    assert all(abs(float(averaged[metric]) - 1.0) < EPSILON
               for metric in SURFACE_METRICS)
    assert spearman([1, 2, 3], [2, 4, 6]) == 1.0
    assert spearman([1, 2, 3], [6, 4, 2]) == -1.0
    assert measured_coordinate_ranges(
        [(-50, 12), (-38, 12), (-12, 12), (25, 25)]
    ) == [
        {"y": 12, "x_runs": [[-50, -38], [-12, -12]]},
        {"y": 25, "x_runs": [[25, 25]]},
    ]

    def synthetic_surface(
        seeds: Sequence[int],
    ) -> list[dict[str, Any]]:
        output = []
        for seed in seeds:
            for size in MAP_SIZES:
                for y in GRID:
                    for x in GRID:
                        for group_index, group in enumerate(GROUPS):
                            metrics = {
                                metric: (len(GROUPS) - group_index) / 100.0
                                for metric in SURFACE_METRICS}
                            metrics["lift_pp"] = float(
                                len(GROUPS) - group_index)
                            output.append({
                                "dataset": "synthetic",
                                "seed": seed,
                                "map_size": size,
                                "x": x, "y": y, "group": group,
                                **metrics,
                            })
        return output

    partial_components = synthetic_surface((101,))
    partial_kind = {101: "qualification"}
    partial_seed_rows = per_seed_surface(partial_components, partial_kind)
    partial_size_rows = per_size_surface(
        partial_components, "qualification")
    partial_seed_size_rows = seed_size_surface(
        partial_components, "qualification", partial_kind)
    for grouped_rows in (
            partial_seed_rows, partial_size_rows, partial_seed_size_rows):
        assert all(
            {"top_tendency", "runner_up_tendency", "top_margin_pp"} <= set(row)
            for row in grouped_rows)
        assert all(row["top_tendency"] == "icefield" for row in grouped_rows)
        assert all(float(row["top_margin_pp"]) == 1.0 for row in grouped_rows)
    partial_combined = combine_surface(
        partial_components, "qualification", "equal_seed_size")
    attach_tendencies(
        partial_combined, partial_seed_rows, partial_size_rows)
    assert all(
        row["dataset_relative_stable"] for row in partial_combined)
    assert all("cross_stable" not in row for row in partial_combined)

    full_seeds = CALIBRATION_SEEDS + HOLDOUT_SEEDS
    full_components = synthetic_surface(full_seeds)
    full_kind = {
        **{seed: "calibration" for seed in CALIBRATION_SEEDS},
        **{seed: "holdout" for seed in HOLDOUT_SEEDS},
    }
    full_seed_rows = per_seed_surface(full_components, full_kind)
    full_size_rows = per_size_surface(full_components, "full")
    full_seed_size_rows = seed_size_surface(
        full_components, "full", full_kind)
    full_combined = combine_surface(
        full_components, "full", "equal_seed_size")
    attach_tendencies(full_combined, full_seed_rows, full_size_rows)
    assert all(row["cross_stable"] for row in full_combined)
    assert all(
        "dataset_relative_stable" not in row for row in full_combined)
    assert all(
        row["stability_seed_component_count"] == 6 and
        row["stability_size_component_count"] == 4
        for row in full_combined)
    assert all(
        row["top_tendency"] == "icefield"
        for row in full_seed_size_rows)

    changed_sizes = [dict(row) for row in partial_size_rows]
    for row in changed_sizes:
        if row["map_size"] == "Small" and row["x"] == 0 and row["y"] == 0:
            if row["group"] == "icefield":
                row["lift_pp"] = 0.0
            elif row["group"] == "tundra":
                row["lift_pp"] = 100.0
    size_winner_evidence = map_size_winner_changes(changed_sizes)
    assert any(
        row["x"] == 0 and row["y"] == 0
        for row in size_winner_evidence)

    comparison_base = []
    comparison_holdout = []
    for y in GRID:
        for x in GRID:
            for group_index, group in enumerate(GROUPS):
                row = {
                    "x": x, "y": y, "group": group,
                    "mean_land_share": group_index / 100,
                    "occurrence_probability": group_index / 10,
                    "probability_ge_1pct": group_index / 10,
                    "probability_ge_5pct": group_index / 20,
                    "probability_ge_10pct": group_index / 40,
                    "lift_pp": float(group_index),
                }
                comparison_base.append(row)
                comparison_holdout.append(dict(row))
    validation = holdout_validation(comparison_base, comparison_holdout)
    overall = next(
        row for row in validation if row["record_type"] == "overall_summary")
    assert overall["top_tendency_agreement"] == 1.0
    assert overall["global_lift_rank_agreement"] == 1.0
    changed_holdout = [dict(row) for row in comparison_holdout]
    changed = next(
        row for row in changed_holdout
        if row["x"] == -50 and row["y"] == -50 and
        row["group"] == "icefield")
    changed.update({
        "mean_land_share": 0.5,
        "occurrence_probability": 1.0,
        "probability_ge_1pct": 1.0,
        "probability_ge_5pct": 1.0,
        "probability_ge_10pct": 1.0,
        "lift_pp": 99.0,
    })
    changed_validation = holdout_validation(
        comparison_base, changed_holdout)
    changed_coordinates = [
        row for row in changed_validation
        if row["record_type"] == "coordinate"]
    leaders = holdout_disagreement_leaders(changed_coordinates)
    assert leaders["share"][0]["x"] == -50
    assert leaders["share"][0]["y"] == -50
    assert leaders["threshold_ge_10pct"][0]["x"] == -50
    assert leaders["top_tendency"] == [{
        "x": -50, "y": -50,
        "calibration_top_tendency": "other_transition",
        "holdout_top_tendency": "icefield",
    }]
    size_rows = []
    for size_index, size in enumerate(MAP_SIZES):
        for row in comparison_base:
            size_rows.append({
                **row, "map_size": size,
                "mean_land_share": float(row["mean_land_share"]) +
                    size_index / 1000,
            })
    size_evidence = size_differences(size_rows, "synthetic")
    assert sum(
        row["record_type"] == "pair_summary" for row in size_evidence) == 6
    continuous_components = []
    for seed in (1, 2):
        for size in MAP_SIZES:
            for y in GRID:
                for x in GRID:
                    continuous_components.append({
                        "seed": seed, "map_size": size, "x": x, "y": y,
                        "temperature": float(x), "moisture": float(y),
                        "precipitation": float(y) * 2,
                    })
    continuous_combined = average_continuous(
        continuous_components, "synthetic")
    axis = build_axis_response(
        {"synthetic": continuous_combined},
        {"synthetic": continuous_components})
    temperature_x = next(
        row for row in axis
        if row["metric"] == "temperature" and row["coordinate"] == -50)
    assert temperature_x["endpoint_difference"] == 100.0
    assert temperature_x["component_count"] == 8
    assert temperature_x["component_directionally_supported_fraction"] == 1.0
    assert temperature_x["response_class"] == "robust_directional_support"
    zero_components = [
        {**row, "temperature": 0.0} for row in continuous_components]
    zero_axis = build_axis_response(
        {"zero": average_continuous(zero_components, "zero")},
        {"zero": zero_components})
    zero_temperature = next(
        row for row in zero_axis
        if row["metric"] == "temperature" and row["coordinate"] == -50)
    assert zero_temperature["response_class"] == "exact_zero"
    inconsistent_components = [dict(row) for row in continuous_components]
    for row in inconsistent_components:
        if row["seed"] == 1 and row["map_size"] == "Small":
            row["temperature"] = -float(row["x"])
    inconsistent_axis = build_axis_response(
        {"inconsistent": average_continuous(
            inconsistent_components, "inconsistent")},
        {"inconsistent": inconsistent_components})
    inconsistent_temperature = next(
        row for row in inconsistent_axis
        if row["metric"] == "temperature" and row["coordinate"] == -50)
    assert inconsistent_temperature["response_class"] == "inconsistent_response"
    assert inconsistent_temperature[
        "magnitude_materiality_status"] == "requires_user_supplied_threshold"

    def synthetic_continuous(
        seeds: Sequence[int],
    ) -> list[dict[str, Any]]:
        return [{
            "seed": seed, "map_size": size, "x": x, "y": y,
            "temperature": float(x), "moisture": float(y),
            "precipitation": float(y) * 2,
        } for seed in seeds for size in MAP_SIZES
            for y in GRID for x in GRID]

    partial_continuous = synthetic_continuous((101,))
    partial_axis = build_axis_response(
        {"qualification": average_continuous(
            partial_continuous, "qualification")},
        {"qualification": partial_continuous})
    partial_size_diff = size_differences(
        partial_size_rows, "qualification")
    partial_summary, partial_en, partial_zh = substantive_summary(
        "qualification", partial_combined, None, partial_size_diff,
        partial_size_rows, partial_axis)
    assert partial_summary["dataset_relative_stable_coordinate_count"] == 81
    assert "cross-stable" not in partial_en.lower()
    assert "dataset-relative" in partial_en.lower()
    assert "最终六种子" in partial_zh

    full_continuous = synthetic_continuous(full_seeds)
    full_axis = build_axis_response(
        {"full": average_continuous(full_continuous, "full")},
        {"full": full_continuous})
    full_size_diff = size_differences(full_size_rows, "full")
    full_summary, full_en, full_zh = substantive_summary(
        "full", full_combined, None, full_size_diff,
        full_size_rows, full_axis)
    assert full_summary["cross_stable_coordinate_count"] == 81
    assert full_summary["mixed_or_unsupported_coordinate_count"] == 0
    assert full_summary["cross_stable_redraw_cells"][0][
        "coordinate_count"] == 81
    assert "full cross-stable redraw cells" in full_en.lower()
    assert "完整稳定重绘格点" in full_zh
    with tempfile.TemporaryDirectory(prefix="climate_processor_selftest_") as name:
        root = Path(name)
        path = root / "test.svg"
        rows = [
            {"x": x, "y": y, "group": group, "metric": (x + y + 100) / 200}
            for group in GROUPS for y in GRID for x in GRID]
        render_grid_svg(path, "Synthetic", rows, "metric")
        text = path.read_text(encoding="utf-8")
        assert 'data-interpolation="none"' in text
        assert text.count("<rect ") == 1 + len(GROUPS) * 81
        second_svg = root / "test_repeat.svg"
        render_grid_svg(second_svg, "Synthetic", rows, "metric")
        assert path.read_bytes() == second_svg.read_bytes()
        calibration_dir = root / "processed" / "calibration"
        calibration_dir.mkdir(parents=True)
        calibration_surface = [
            {**row, "dataset": "calibration"} for row in partial_combined]
        calibration_surface_path = (
            calibration_dir / "calibration_surface.csv")
        write_csv_new(
            calibration_surface_path, calibration_surface,
            surface_fields(calibration_surface))
        synthetic_raw_digest = "A" * 64
        calibration_model_path = (
            calibration_dir / "calibration_model.json")
        write_json_new(
            calibration_model_path,
            model_payload(calibration_surface, synthetic_raw_digest))
        calibration_model_hash = sha256_file(calibration_model_path)
        write_text_new(
            calibration_dir / "calibration_model.sha256",
            calibration_model_hash + "\n")
        _, freeze_manifest_path, calibration_surface_hash, _ = (
            write_calibration_freeze(
                calibration_dir, calibration_surface_path,
                calibration_model_path, synthetic_raw_digest))
        frozen = verify_calibration_freeze(calibration_dir)
        assert frozen["model_sha256"] == calibration_model_hash
        assert frozen["surface_sha256"] == calibration_surface_hash
        assert frozen["manifest_sha256"] == sha256_file(freeze_manifest_path)
        rounding_dir = root / "rounding" / "processed" / "calibration"
        rounding_dir.mkdir(parents=True)
        rounding_surface = [dict(row) for row in calibration_surface]
        coordinate_rows = [
            row for row in rounding_surface
            if int(row["x"]) == GRID[0] and int(row["y"]) == GRID[0]
        ]
        for row in coordinate_rows:
            row["lift_pp"] = -100.0
        coordinate_rows[0]["lift_pp"] = 3.2985015748894
        coordinate_rows[1]["lift_pp"] = 1.4491884061386
        rounding_surface_path = rounding_dir / "calibration_surface.csv"
        write_csv_new(
            rounding_surface_path,
            rounding_surface,
            surface_fields(rounding_surface),
        )
        pre_serialized_model = model_payload(
            rounding_surface, synthetic_raw_digest)
        serialized_rounding_surface = read_surface(rounding_surface_path)
        serialized_model = model_payload(
            serialized_rounding_surface, synthetic_raw_digest)
        assert canonical_json_bytes(pre_serialized_model) != canonical_json_bytes(
            serialized_model)
        rounding_cell = next(
            cell for cell in serialized_model["cells"]
            if cell["x"] == GRID[0] and cell["y"] == GRID[0]
        )
        assert rounding_cell["top_margin_pp"] == 1.84931316875
        rounding_model_path = rounding_dir / "calibration_model.json"
        write_json_new(rounding_model_path, serialized_model)
        rounding_model_hash = sha256_file(rounding_model_path)
        write_text_new(
            rounding_dir / "calibration_model.sha256",
            rounding_model_hash + "\n",
        )
        write_calibration_freeze(
            rounding_dir,
            rounding_surface_path,
            rounding_model_path,
            synthetic_raw_digest,
        )
        rounding_frozen = verify_calibration_freeze(rounding_dir)
        assert rounding_frozen["model_sha256"] == rounding_model_hash
        with calibration_surface_path.open("ab") as output:
            output.write(b"\n")
        try:
            verify_calibration_freeze(calibration_dir)
        except RuntimeError as exc:
            assert "surface hash mismatch" in str(exc)
        else:
            raise AssertionError("tampered calibration surface was accepted")
        raw = root / "raw" / "calibration" / "seed_1" / "shard_0000.csv"
        raw.parent.mkdir(parents=True)
        raw.write_bytes(b"header\nrow\n")
        done = raw.with_suffix(".done")
        done.write_bytes(b"{}\n")
        shard = Shard(
            raw, done, raw.relative_to(root).as_posix(),
            "calibration", 1, sha256_file(raw), 1)
        staged_output = (
            root / "stage" / "processed" / "qualification")
        staged_artifact = staged_output / "qualification_surface.csv"
        write_bytes_new(staged_artifact, b"x,y\n0,0\n")
        write_lineage(
            root,
            staged_output,
            root / "processed" / "qualification",
            [shard],
            [staged_artifact],
        )
        with (
            staged_output / "summary_lineage.csv"
        ).open("r", encoding="utf-8", newline="") as source:
            staged_lineage = list(csv.DictReader(source))
        assert {row["path"] for row in staged_lineage} == {
            raw.relative_to(root).as_posix(),
            "processed/qualification/qualification_surface.csv",
        }
        assert all(
            not Path(row["path"]).is_absolute()
            for row in staged_lineage)
        manifest_payload = {
            "phase": "synthetic", "binding_run_id": "test",
            "row_schema_version": "test", "header_sha256": "test",
            "shard_count": 1, "row_count": 1, "failure_count": 0,
            "shards": [{
                "seed_kind": "calibration", "seed": 1,
                "csv_path": raw.relative_to(root).as_posix(),
                "csv_sha256": shard.sha256, "row_count": 1,
            }],
        }
        manifest = {
            **manifest_payload,
            "manifest_payload_sha256": hashlib.sha256(
                binding_json_bytes(manifest_payload)).hexdigest().upper(),
        }
        manifest_path = root / "manifests" / "raw_manifest_synthetic.json"
        manifest_path.parent.mkdir()
        manifest_path.write_bytes(canonical_json_bytes(manifest))
        verify_phase_manifest(
            root, "synthetic", [shard], load_raw_origin(root))
        import_source = root / "attempt_14"
        import_destination = root / "attempt_16"
        source_binding = {
            "run_id": "RAW-BINDING",
            "row_schema_version": "test",
            "base_head": "head",
            "source_manifest_hash": "source",
            "executable_hash": "exe",
            "scripts_manifest_hash": "scripts",
            "config_manifest_hash": "config",
        }
        processing_binding = {"run_id": "PROCESSING-BINDING"}
        source_binding_path = import_source / "binding" / "binding.json"
        processing_binding_path = (
            import_destination / "binding" / "binding.json")
        write_json_new(source_binding_path, source_binding)
        write_json_new(processing_binding_path, processing_binding)
        imported_raw = (
            import_source / "raw" / "calibration" /
            "seed_1" / "shard_0000.csv")
        write_bytes_new(imported_raw, b"header\nrow\n")
        imported_done = imported_raw.with_suffix(".done")
        write_json_new(imported_done, {
            "status": "PASS",
            "binding_run_id": "RAW-BINDING",
            "csv_sha256": sha256_file(imported_raw),
            "row_count": 1,
            "shard": {
                "seed_kind": "calibration", "seed": 1,
                "world_count": 1,
            },
        })
        imported_shard = Shard(
            imported_raw,
            imported_done,
            imported_raw.relative_to(import_source).as_posix(),
            "calibration",
            1,
            sha256_file(imported_raw),
            1,
        )
        imported_manifest_payload = {
            "phase": "synthetic",
            "binding_run_id": "RAW-BINDING",
            "row_schema_version": "test",
            "header_sha256": "test",
            "shard_count": 1,
            "row_count": 1,
            "failure_count": 0,
            "shards": [{
                "seed_kind": "calibration",
                "seed": 1,
                "csv_path": imported_shard.logical_path,
                "csv_sha256": imported_shard.sha256,
                "row_count": 1,
            }],
        }
        imported_manifest = {
            **imported_manifest_payload,
            "manifest_payload_sha256": hashlib.sha256(
                binding_json_bytes(imported_manifest_payload)
            ).hexdigest().upper(),
        }
        write_json_new(
            import_source / "manifests" / "raw_manifest_synthetic.json",
            imported_manifest,
        )
        import_payload = {
            "schema_version": RAW_IMPORT_SCHEMA_VERSION,
            "status": "PASS",
            "source_attempt_path": str(import_source.resolve()),
            "source_binding_path": "binding/binding.json",
            "source_binding_sha256": sha256_file(source_binding_path),
            "source_binding_run_id": "RAW-BINDING",
            "processing_binding_sha256": sha256_file(
                processing_binding_path),
        }
        write_json_new(import_destination / "raw_import.json", {
            **import_payload,
            "manifest_payload_sha256": hashlib.sha256(
                canonical_json_bytes(import_payload)).hexdigest().upper(),
        })
        imported_origin = load_raw_origin(import_destination)
        verify_phase_manifest(
            import_destination,
            "synthetic",
            [imported_shard],
            imported_origin,
        )
        imported_stage = (
            import_destination / "stage" / "processed" / "qualification")
        imported_artifact = imported_stage / "qualification_surface.csv"
        write_bytes_new(imported_artifact, b"x,y\n0,0\n")
        write_lineage(
            import_destination,
            imported_stage,
            import_destination / "processed" / "qualification",
            [imported_shard],
            [imported_artifact],
        )
        with (
            imported_stage / "summary_lineage.csv"
        ).open("r", encoding="utf-8", newline="") as source:
            imported_lineage = list(csv.DictReader(source))
        assert imported_lineage[0]["path"] == imported_shard.logical_path
        assert not (import_destination / "raw").exists()
    print("worldgen climate calibration processor self-tests: PASS")


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true",
                        help="run deterministic synthetic tests and exit")
    subparsers = parser.add_subparsers(dest="mode")
    for mode in ("qualification", "calibration", "full"):
        subparser = subparsers.add_parser(mode)
        subparser.add_argument("--run-root", required=True, type=Path)
        subparser.add_argument(
            "--output-root", type=Path,
            help="write processed evidence below this staging root")
    args = parser.parse_args(argv)
    if not args.self_test and args.mode is None:
        parser.error("a processing mode is required unless --self-test is used")
    return args


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    if args.self_test:
        run_self_tests()
        return 0
    run_root = args.run_root.resolve()
    if not run_root.is_dir():
        raise FileNotFoundError(run_root)
    output_root = (
        args.output_root.resolve()
        if args.output_root is not None else run_root
    )
    if args.mode == "qualification":
        output = process_qualification(run_root, output_root)
    elif args.mode == "calibration":
        output = process_calibration(run_root, output_root)
    else:
        output = process_full(run_root, output_root)
    print(f"processed climate calibration evidence: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
