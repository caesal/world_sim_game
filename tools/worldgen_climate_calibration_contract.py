#!/usr/bin/env python3
"""Frozen enumeration contract for offline worldgen climate calibration."""

from __future__ import annotations

import argparse
import csv
import gzip
import hashlib
import io
import itertools
import json
import os
from pathlib import Path
import shutil
import sys
import uuid


CONTRACT_SCHEMA_VERSION = "worldgen-climate-calibration-contract-v1"
ROW_SCHEMA_VERSION = "worldgen-climate-calibration-v1"
LEVELS = (0, 12, 25, 38, 50)
ORDERED_SUMS = (0, 12, 24, 25, 37, 38, 50, 62, 63, 75, 76, 88, 100)
ORDERED_SUM_MULTIPLICITIES = (1, 2, 1, 2, 2, 2, 5, 2, 2, 2, 1, 2, 1)
CALIBRATION_SEEDS = (2026072301, 2026072302, 2026072303, 2026072304)
HOLDOUT_SEEDS = (2026072391, 2026072392)
MAP_SIZES = (
    ("Small", 576, 400),
    ("Medium", 720, 500),
    ("Large", 864, 600),
    ("Extreme", 1152, 800),
)
FIXED_PARAMETERS = {
    "ocean": 50,
    "continent": 50,
    "relief": 50,
    "vegetation": 50,
    "bias_mountain": 50,
    "bias_wetland": 50,
    "random_seed": 0,
}
CORNER_ROLES = ("TL", "TR", "BL", "BR")
CORNER_SIGNS = {
    "TL": (-1, 1),
    "TR": (1, 1),
    "BL": (-1, -1),
    "BR": (1, -1),
}
SHARD_CONFIG_COUNT = 128
RAW_ARRANGEMENT_COUNT = 25**4
EFFECTIVE_CONFIG_COUNT = len(ORDERED_SUMS) ** 4
CONDITIONAL_RAW_COUNT = 25**3
SPARSE_CORNER_CONDITION_ROW_COUNT = 422_500
ONE_SEED_WORLD_COUNT = EFFECTIVE_CONFIG_COUNT * len(MAP_SIZES)
CALIBRATION_WORLD_COUNT = ONE_SEED_WORLD_COUNT * len(CALIBRATION_SEEDS)
HOLDOUT_WORLD_COUNT = ONE_SEED_WORLD_COUNT * len(HOLDOUT_SEEDS)
FULL_WORLD_COUNT = CALIBRATION_WORLD_COUNT + HOLDOUT_WORLD_COUNT
CONCEPTUAL_WEIGHTED_WORLD_COUNT = RAW_ARRANGEMENT_COUNT * len(MAP_SIZES) * (
    len(CALIBRATION_SEEDS) + len(HOLDOUT_SEEDS)
)

EFFECTIVE_CONFIG_FIELDS = (
    "config_index",
    "bias_forest",
    "bias_desert",
    "moisture",
    "drought",
)
MULTIPLICITY_FIELDS = EFFECTIVE_CONFIG_FIELDS + ("config_multiplicity",)
CORNER_WEIGHT_FIELDS = (
    "corner_role",
    "position_index",
    "x",
    "y",
    "x_magnitude",
    "y_magnitude",
    "config_index",
    "weight",
)
CONTRACT_OUTPUT_NAMES = (
    "effective_configs.csv",
    "effective_config_multiplicities.csv",
    "corner_condition_weights.csv.gz",
    "enumeration_proofs.json",
)


class ContractError(RuntimeError):
    """Raised when frozen enumeration evidence is incomplete or inconsistent."""


def canonical_json_bytes(value: object) -> bytes:
    return (
        json.dumps(value, ensure_ascii=True, sort_keys=True, separators=(",", ":"))
        + "\n"
    ).encode("ascii")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def _fsync_directory(path: Path) -> None:
    if os.name == "nt":
        return
    descriptor = os.open(path, os.O_RDONLY)
    try:
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def _atomic_write_bytes(path: Path, data: bytes) -> None:
    temporary = path.parent / f".t_{uuid.uuid4().hex[:12]}"
    with temporary.open("xb") as handle:
        handle.write(data)
        handle.flush()
        os.fsync(handle.fileno())
    if path.exists():
        raise ContractError(f"refusing to overwrite existing contract file: {path}")
    os.replace(temporary, path)
    _fsync_directory(path.parent)


def _write_csv(path: Path, fields: tuple[str, ...], rows: object) -> None:
    with path.open("x", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle, fieldnames=fields, extrasaction="raise", lineterminator="\n"
        )
        writer.writeheader()
        writer.writerows(rows)
        handle.flush()
        os.fsync(handle.fileno())


def _write_corner_weights_gzip(path: Path, condition_weights: list[list[dict[int, int]]]) -> None:
    with path.open("xb") as raw_handle:
        with gzip.GzipFile(
            filename="", mode="wb", fileobj=raw_handle, mtime=0
        ) as compressed:
            with io.TextIOWrapper(compressed, encoding="utf-8", newline="") as text:
                writer = csv.DictWriter(
                    text,
                    fieldnames=CORNER_WEIGHT_FIELDS,
                    extrasaction="raise",
                    lineterminator="\n",
                )
                writer.writeheader()
                for role_index, role in enumerate(CORNER_ROLES):
                    sign_x, sign_y = CORNER_SIGNS[role]
                    for position_index, weights in enumerate(
                        condition_weights[role_index]
                    ):
                        x_index, y_index = divmod(position_index, len(LEVELS))
                        x_magnitude = LEVELS[x_index]
                        y_magnitude = LEVELS[y_index]
                        for config_index in sorted(weights):
                            writer.writerow(
                                {
                                    "corner_role": role,
                                    "position_index": position_index,
                                    "x": sign_x * x_magnitude,
                                    "y": sign_y * y_magnitude,
                                    "x_magnitude": x_magnitude,
                                    "y_magnitude": y_magnitude,
                                    "config_index": config_index,
                                    "weight": weights[config_index],
                                }
                            )
                text.flush()
        raw_handle.flush()
        os.fsync(raw_handle.fileno())


def position_index(x_index: int, y_index: int) -> int:
    if not (0 <= x_index < len(LEVELS) and 0 <= y_index < len(LEVELS)):
        raise ContractError("corner position index is outside the frozen 5x5 grid")
    return x_index * len(LEVELS) + y_index


def config_index(
    bias_forest: int, bias_desert: int, moisture: int, drought: int
) -> int:
    try:
        forest_index = ORDERED_SUMS.index(bias_forest)
        desert_index = ORDERED_SUMS.index(bias_desert)
        moisture_index = ORDERED_SUMS.index(moisture)
        drought_index = ORDERED_SUMS.index(drought)
    except ValueError as exc:
        raise ContractError(f"value outside frozen ordered-sum set: {exc}") from exc
    side = len(ORDERED_SUMS)
    return (
        ((forest_index * side + desert_index) * side + moisture_index) * side
        + drought_index
    )


def decode_config_index(index: int) -> tuple[int, int, int, int]:
    if not 0 <= index < EFFECTIVE_CONFIG_COUNT:
        raise ContractError(f"config index outside 0..{EFFECTIVE_CONFIG_COUNT - 1}")
    side = len(ORDERED_SUMS)
    drought_index = index % side
    index //= side
    moisture_index = index % side
    index //= side
    desert_index = index % side
    forest_index = index // side
    return (
        ORDERED_SUMS[forest_index],
        ORDERED_SUMS[desert_index],
        ORDERED_SUMS[moisture_index],
        ORDERED_SUMS[drought_index],
    )


def expected_config_multiplicity(index: int) -> int:
    values = decode_config_index(index)
    result = 1
    for value in values:
        result *= ORDERED_SUM_MULTIPLICITIES[ORDERED_SUMS.index(value)]
    return result


def effective_config_row(index: int) -> dict[str, int]:
    forest, desert, moisture, drought = decode_config_index(index)
    return {
        "config_index": index,
        "bias_forest": forest,
        "bias_desert": desert,
        "moisture": moisture,
        "drought": drought,
    }


def _enumerate_raw_arrangements() -> tuple[list[int], list[list[dict[int, int]]], int]:
    config_weights = [0] * EFFECTIVE_CONFIG_COUNT
    condition_weights: list[list[dict[int, int]]] = [
        [dict() for _ in range(25)] for _ in CORNER_ROLES
    ]
    sum_to_index = {value: index for index, value in enumerate(ORDERED_SUMS)}
    side = len(ORDERED_SUMS)
    raw_count = 0

    # This is the single exhaustive traversal of all 25^4 raw quadrilaterals.
    for tl, tr, bl, br in itertools.product(range(25), repeat=4):
        tl_x, tl_y = divmod(tl, len(LEVELS))
        tr_x, tr_y = divmod(tr, len(LEVELS))
        bl_x, bl_y = divmod(bl, len(LEVELS))
        br_x, br_y = divmod(br, len(LEVELS))
        forest_index = sum_to_index[LEVELS[tl_x] + LEVELS[bl_x]]
        desert_index = sum_to_index[LEVELS[tr_x] + LEVELS[br_x]]
        moisture_index = sum_to_index[LEVELS[tl_y] + LEVELS[tr_y]]
        drought_index = sum_to_index[LEVELS[bl_y] + LEVELS[br_y]]
        index = (
            ((forest_index * side + desert_index) * side + moisture_index) * side
            + drought_index
        )
        config_weights[index] += 1
        for role_index, role_position in enumerate((tl, tr, bl, br)):
            role_weights = condition_weights[role_index][role_position]
            role_weights[index] = role_weights.get(index, 0) + 1
        raw_count += 1
    return config_weights, condition_weights, raw_count


def _proofs(
    config_weights: list[int],
    condition_weights: list[list[dict[int, int]]],
    raw_count: int,
) -> dict[str, object]:
    pair_sum_counts: dict[int, int] = {}
    for left, right in itertools.product(LEVELS, repeat=2):
        value = left + right
        pair_sum_counts[value] = pair_sum_counts.get(value, 0) + 1
    calculated_sums = tuple(sorted(pair_sum_counts))
    calculated_multiplicities = tuple(pair_sum_counts[value] for value in calculated_sums)
    if calculated_sums != ORDERED_SUMS:
        raise ContractError("ordered pair-sum values differ from frozen contract")
    if calculated_multiplicities != ORDERED_SUM_MULTIPLICITIES:
        raise ContractError("ordered pair-sum multiplicities differ from frozen contract")
    if raw_count != RAW_ARRANGEMENT_COUNT:
        raise ContractError(f"raw arrangement count mismatch: {raw_count}")
    nonzero_count = sum(weight > 0 for weight in config_weights)
    if nonzero_count != EFFECTIVE_CONFIG_COUNT:
        raise ContractError(f"effective configuration count mismatch: {nonzero_count}")
    if sum(config_weights) != RAW_ARRANGEMENT_COUNT:
        raise ContractError("effective multiplicities do not sum to raw arrangements")

    formula_mismatches = []
    for index, observed in enumerate(config_weights):
        expected = expected_config_multiplicity(index)
        if observed != expected:
            formula_mismatches.append(
                {"config_index": index, "observed": observed, "expected": expected}
            )
    if formula_mismatches:
        raise ContractError(
            f"config multiplicity formula mismatches: {len(formula_mismatches)}"
        )

    conditional_sums: dict[str, list[int]] = {}
    sparse_row_count = 0
    inverse_config_weight_proven = True
    for role_index, role in enumerate(CORNER_ROLES):
        sums = [sum(weights.values()) for weights in condition_weights[role_index]]
        conditional_sums[role] = sums
        sparse_row_count += sum(len(weights) for weights in condition_weights[role_index])
        if any(value != CONDITIONAL_RAW_COUNT for value in sums):
            raise ContractError(f"{role} fixed-position conditional sum mismatch")
        inverse_sums = [0] * EFFECTIVE_CONFIG_COUNT
        for weights in condition_weights[role_index]:
            for index, weight in weights.items():
                inverse_sums[index] += weight
        if inverse_sums != config_weights:
            inverse_config_weight_proven = False
            raise ContractError(
                f"{role} inverse conditional weights differ from config weights"
            )
    if sparse_row_count != SPARSE_CORNER_CONDITION_ROW_COUNT:
        raise ContractError(
            "sparse corner-condition row count mismatch: "
            f"{sparse_row_count} != {SPARSE_CORNER_CONDITION_ROW_COUNT}"
        )

    lexicographic_ok = all(
        decode_config_index(index)
        <= decode_config_index(index + 1)
        for index in range(EFFECTIVE_CONFIG_COUNT - 1)
    )
    if not lexicographic_ok:
        raise ContractError("config indices are not lexicographic")

    return {
        "contract_schema_version": CONTRACT_SCHEMA_VERSION,
        "raw_arrangements": raw_count,
        "effective_configurations": nonzero_count,
        "effective_multiplicity_sum": sum(config_weights),
        "ordered_pair_sums": list(calculated_sums),
        "ordered_pair_multiplicities": list(calculated_multiplicities),
        "conditional_expected_sum": CONDITIONAL_RAW_COUNT,
        "conditional_sums_by_role_and_position": conditional_sums,
        "sparse_corner_condition_rows": sparse_row_count,
        "sparse_corner_condition_rows_expected": (
            SPARSE_CORNER_CONDITION_ROW_COUNT
        ),
        "inverse_config_weight_by_role_proven": inverse_config_weight_proven,
        "config_index_order": ["bias_forest", "bias_desert", "moisture", "drought"],
        "config_index_base": 0,
        "lexicographic_order_proven": lexicographic_ok,
        "multiplicity_formula_proven": not formula_mismatches,
        "one_seed_worlds": ONE_SEED_WORLD_COUNT,
        "calibration_worlds": CALIBRATION_WORLD_COUNT,
        "holdout_worlds": HOLDOUT_WORLD_COUNT,
        "full_worlds": FULL_WORLD_COUNT,
        "conceptual_multiplicity_weighted_raw_cases": CONCEPTUAL_WEIGHTED_WORLD_COUNT,
    }


def contract_identity() -> dict[str, object]:
    return {
        "contract_schema_version": CONTRACT_SCHEMA_VERSION,
        "row_schema_version": ROW_SCHEMA_VERSION,
        "levels": list(LEVELS),
        "ordered_pair_sums": list(ORDERED_SUMS),
        "ordered_pair_multiplicities": list(ORDERED_SUM_MULTIPLICITIES),
        "corner_roles": list(CORNER_ROLES),
        "corner_signs": {key: list(value) for key, value in CORNER_SIGNS.items()},
        "adapter": {
            "bias_forest": "abs(TL.x) + abs(BL.x)",
            "bias_desert": "TR.x + BR.x",
            "moisture": "TL.y + TR.y",
            "drought": "abs(BL.y) + abs(BR.y)",
        },
        "fixed_parameters": FIXED_PARAMETERS,
        "map_sizes": [
            {"name": name, "width": width, "height": height}
            for name, width, height in MAP_SIZES
        ],
        "calibration_seeds": list(CALIBRATION_SEEDS),
        "holdout_seeds": list(HOLDOUT_SEEDS),
        "shard_config_count": SHARD_CONFIG_COUNT,
        "raw_arrangement_count": RAW_ARRANGEMENT_COUNT,
        "effective_config_count": EFFECTIVE_CONFIG_COUNT,
        "conditional_raw_count": CONDITIONAL_RAW_COUNT,
    }


def _verify_manifest(output_dir: Path) -> dict[str, object]:
    manifest_path = output_dir / "config_manifest.json"
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ContractError(f"cannot read config manifest: {exc}") from exc
    if manifest.get("identity") != contract_identity():
        raise ContractError("existing config manifest identity differs from frozen contract")
    files = manifest.get("files")
    if not isinstance(files, dict) or set(files) != set(CONTRACT_OUTPUT_NAMES):
        raise ContractError("config manifest file inventory is incomplete")
    for name in CONTRACT_OUTPUT_NAMES:
        path = output_dir / name
        metadata = files[name]
        if not path.is_file():
            raise ContractError(f"missing contract artifact: {path}")
        if path.stat().st_size != metadata.get("bytes"):
            raise ContractError(f"contract artifact size mismatch: {path}")
        if sha256_file(path) != metadata.get("sha256"):
            raise ContractError(f"contract artifact hash mismatch: {path}")
    if manifest.get("manifest_payload_sha256") != sha256_bytes(
        canonical_json_bytes(
            {
                "identity": manifest["identity"],
                "files": manifest["files"],
                "proofs_sha256": manifest["proofs_sha256"],
            }
        )
    ):
        raise ContractError("config manifest payload hash mismatch")
    return manifest


def materialize_contract(output_dir: Path) -> dict[str, object]:
    output_dir = output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    manifest_path = output_dir / "config_manifest.json"
    if manifest_path.exists():
        return _verify_manifest(output_dir)

    occupied = [name for name in CONTRACT_OUTPUT_NAMES if (output_dir / name).exists()]
    if occupied:
        raise ContractError(
            "partial contract evidence exists; preserve this attempt and use a new "
            f"attempt instead of overwriting: {occupied}"
        )

    staging = output_dir / f".ct_{uuid.uuid4().hex[:12]}"
    staging.mkdir()
    try:
        config_weights, condition_weights, raw_count = _enumerate_raw_arrangements()
        proofs = _proofs(config_weights, condition_weights, raw_count)

        _write_csv(
            staging / "effective_configs.csv",
            EFFECTIVE_CONFIG_FIELDS,
            (
                effective_config_row(index)
                for index in range(EFFECTIVE_CONFIG_COUNT)
            ),
        )
        _write_csv(
            staging / "effective_config_multiplicities.csv",
            MULTIPLICITY_FIELDS,
            (
                {
                    **effective_config_row(index),
                    "config_multiplicity": config_weights[index],
                }
                for index in range(EFFECTIVE_CONFIG_COUNT)
            ),
        )
        _write_corner_weights_gzip(
            staging / "corner_condition_weights.csv.gz", condition_weights
        )
        _atomic_write_bytes(
            staging / "enumeration_proofs.json", canonical_json_bytes(proofs)
        )

        files = {}
        for name in CONTRACT_OUTPUT_NAMES:
            path = staging / name
            files[name] = {
                "bytes": path.stat().st_size,
                "sha256": sha256_file(path),
            }
        payload = {
            "identity": contract_identity(),
            "files": files,
            "proofs_sha256": files["enumeration_proofs.json"]["sha256"],
        }
        manifest = {
            **payload,
            "manifest_payload_sha256": sha256_bytes(canonical_json_bytes(payload)),
        }
        _atomic_write_bytes(
            staging / "config_manifest.json", canonical_json_bytes(manifest)
        )

        for name in (*CONTRACT_OUTPUT_NAMES, "config_manifest.json"):
            destination = output_dir / name
            if destination.exists():
                raise ContractError(f"refusing to overwrite contract output: {destination}")
            os.replace(staging / name, destination)
        _fsync_directory(output_dir)
        staging.rmdir()
    except Exception:
        # A failed staging directory is intentionally retained as evidence.
        raise
    return _verify_manifest(output_dir)


def _parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        required=True,
        type=Path,
        help="Numbered calibration attempt directory.",
    )
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Require existing evidence and verify it without enumerating.",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    arguments = _parse_args(sys.argv[1:] if argv is None else argv)
    try:
        if arguments.verify:
            manifest = _verify_manifest(arguments.output_dir.resolve())
        else:
            manifest = materialize_contract(arguments.output_dir)
    except ContractError as exc:
        print(f"contract error: {exc}", file=sys.stderr)
        return 2
    print(
        json.dumps(
            {
                "status": "PASS",
                "output_dir": str(arguments.output_dir.resolve()),
                "config_manifest_sha256": sha256_file(
                    arguments.output_dir.resolve() / "config_manifest.json"
                ),
                "manifest_payload_sha256": manifest["manifest_payload_sha256"],
                "raw_arrangements": RAW_ARRANGEMENT_COUNT,
                "effective_configurations": EFFECTIVE_CONFIG_COUNT,
            },
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
