#!/usr/bin/env python3
"""Orchestrate the bounded Ver0.3.7.a aridity recalibration evidence."""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import os
import struct
import subprocess
import sys
import time
from decimal import Decimal, localcontext
from fractions import Fraction
from pathlib import Path
from typing import Any, Sequence


ACK = "VER037A_ARIDITY_RECALIBRATION"
EXPECTED_HISTORICAL_CSV_SHA256 = (
    "7877D20B28294949D112C22EFFDBA6364E901A1415503CC6E26893A74490F8D7"
)
EXPECTED_HISTORICAL_MATRIX_HASH = "4e676bf614ff7377"
DROUGHT_DIVISORS = (16, 20, 24, 32)
DESERT_BASES = (8, 10, 12, 14)
BIAS_SPANS = (2, 4, 6, 8)
SEMI_ARID_WIDTHS = (8, 10, 12, 14)
OASIS_MARGINS = (0, 5, 10, 15, 20, 25, 30)
QUALIFICATION_COEFFICIENTS = (4, 14, 22, 16, 0)
PILOT_COEFFICIENTS = (24, 10, 4, 10, 0)
MAX_ADVANCED_CANDIDATES = 12
MAX_SUCCESSFUL_PATH_WORLDS = 7824
MAX_PROJECTED_RUNTIME_SECONDS = 4 * 60 * 60
QUALIFICATION_SEEDS = (2026072301, 2026072302)
SCREENING_SEEDS = (2026072301,)
CALIBRATION_SEEDS = (2026072302, 2026072303, 2026072304, 2026072305)
HOLDOUT_SEEDS = (2026082201, 2026082202, 2026082203, 2026082204)
FINAL_SEEDS = SCREENING_SEEDS + CALIBRATION_SEEDS + HOLDOUT_SEEDS
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
PER_WORLD_LIMITS = {
    "B": Fraction(55, 100),
    "C": Fraction(55, 100),
    "D": Fraction(65, 100),
    "E": Fraction(75, 100),
    "F": Fraction(60, 100),
}
MEAN_LIMITS = {
    "B": Fraction(40, 100),
    "C": Fraction(40, 100),
    "D": Fraction(50, 100),
    "E": Fraction(60, 100),
    "F": Fraction(45, 100),
}
MONOTONIC = (
    ("A", "B", "A<=B"),
    ("A", "C", "A<=C"),
    ("B", "D", "B<=D"),
    ("C", "D", "C<=D"),
    ("D", "E", "E>=D"),
    ("F", "D", "D>=F"),
)
LEGACY_CSV_COLUMNS = (
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
)
CALIBRATION_CSV_COLUMNS = (
    *LEGACY_CSV_COLUMNS,
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
COMPARISON_COLUMNS = tuple(
    column for column in CALIBRATION_CSV_COLUMNS if column != "row"
)
ARTIFACT_MANIFEST_COLUMNS = (
    "seed", "map_size", "map_name", "case_id", "width", "height",
    "physical_hash", "oasis_count", "geography_file",
    "geography_pixel_hash", "climate_file", "climate_pixel_hash",
)


class CalibrationError(RuntimeError):
    pass


def _sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def _write_bytes_new(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("xb") as handle:
        handle.write(data)


def _write_text_new(path: Path, text: str) -> None:
    _write_bytes_new(path, text.encode("utf-8"))


def _json_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n").encode(
        "utf-8"
    )


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


def _candidate_id(divisor: int, desert_base: int, span: int,
                  semi_arid_width: int) -> str:
    return f"d{divisor}_a{desert_base}_b{span}_s{semi_arid_width}"


def _coefficient_payload(divisor: int, desert_base: int, span: int,
                         semi_arid_width: int, oasis_margin: int) -> dict[str, int]:
    return {
        "drought_divisor": divisor,
        "desert_base": desert_base,
        "desert_bias_span": span,
        "semi_arid_width": semi_arid_width,
        "oasis_transition_margin": oasis_margin,
    }


def _case_name(row: dict[str, str]) -> str:
    values = (int(row["moisture"]), int(row["drought"]), int(row["bias_desert"]))
    try:
        return CASE_BY_VALUES[values]
    except KeyError as error:
        raise CalibrationError(f"unexpected configuration {values}") from error


def _row_key(row: dict[str, str]) -> tuple[int, int, str]:
    return int(row["seed"]), int(row["map_size"]), _case_name(row)


def _share(row: dict[str, str], count_column: str) -> Fraction:
    land = int(row["land_count"])
    if land <= 0:
        return Fraction(0, 1)
    return Fraction(int(row[count_column]), land)


def _combined_share(row: dict[str, str]) -> Fraction:
    return _share(row, "combined_arid_count")


def _mean(values: Sequence[Fraction]) -> Fraction:
    if not values:
        raise CalibrationError("cannot calculate an empty mean")
    return sum(values, Fraction(0, 1)) / len(values)


def _artifact_manifest(root: Path, output: Path) -> None:
    records: list[dict[str, Any]] = []
    output_resolved = output.resolve()
    for path in sorted(item for item in root.rglob("*") if item.is_file()):
        if path.resolve() == output_resolved:
            continue
        records.append(
            {
                "path": path.relative_to(root).as_posix(),
                "size": path.stat().st_size,
                "sha256": _sha256_file(path),
            }
        )
    _write_json_new(output, {"root": str(root.resolve()), "files": records})


def _find_single_output(directory: Path, names: Sequence[str], suffix: str) -> Path:
    for name in names:
        candidate = directory / name
        if candidate.is_file():
            return candidate
    matches = sorted(directory.glob(f"*{suffix}"))
    if len(matches) == 1:
        return matches[0]
    raise CalibrationError(
        f"expected one {suffix} output in {directory}, found {[str(path) for path in matches]}"
    )


def _load_csv(path: Path, columns: Sequence[str] = CALIBRATION_CSV_COLUMNS
              ) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        if tuple(reader.fieldnames or ()) != tuple(columns):
            raise CalibrationError(
                f"unexpected CSV columns in {path}: {reader.fieldnames}; "
                f"expected {len(columns)}-column schema"
            )
        rows = list(reader)
    if not rows:
        raise CalibrationError(f"CSV contains no rows: {path}")
    return rows


def _legacy_projection_bytes(rows: Sequence[dict[str, str]]) -> bytes:
    text = io.StringIO(newline="")
    writer = csv.DictWriter(
        text, fieldnames=LEGACY_CSV_COLUMNS, lineterminator="\n",
    )
    writer.writeheader()
    for row in rows:
        writer.writerow({column: row[column] for column in LEGACY_CSV_COLUMNS})
    return text.getvalue().encode("utf-8")


def _validate_shape(rows: Sequence[dict[str, str]], seeds: Sequence[int]) -> None:
    expected_keys = [
        (seed, map_index, case_name)
        for seed in seeds
        for map_index, _, _, _ in MAPS
        for case_name, _, _, _ in CASES
    ]
    actual_keys = [_row_key(row) for row in rows]
    if actual_keys != expected_keys:
        raise CalibrationError(
            f"matrix row order/shape mismatch: expected {len(expected_keys)} rows, "
            f"received {len(actual_keys)}"
        )
    map_by_index = {item[0]: item for item in MAPS}
    for row in rows:
        map_index = int(row["map_size"])
        _, name, width, height = map_by_index[map_index]
        if (row["map_name"], int(row["width"]), int(row["height"])) != (
            name,
            width,
            height,
        ):
            raise CalibrationError(f"map identity mismatch in row {_row_key(row)}")


def _validate_coefficients(rows: Sequence[dict[str, str]],
                           coefficients: Sequence[int]) -> None:
    expected = tuple(int(value) for value in coefficients)
    fields = (
        "drought_divisor", "desert_base", "desert_bias_span",
        "semi_arid_width", "oasis_transition_margin",
    )
    for row in rows:
        actual = tuple(int(row[field]) for field in fields)
        if actual != expected:
            raise CalibrationError(
                f"effective coefficient mismatch in row {_row_key(row)}: "
                f"expected {expected}, got {actual}"
            )


def _projected_oasis(row: dict[str, str], margin: int) -> int:
    if margin not in OASIS_MARGINS:
        raise CalibrationError(f"unsupported oasis transition margin {margin}")
    return int(row[f"projected_visible_oasis_margin_{margin}"])


def _zero_oasis_worlds(rows: Sequence[dict[str, str]]) -> list[dict[str, Any]]:
    return [
        {
            "seed": int(row["seed"]),
            "map_size": int(row["map_size"]),
            "map_name": row["map_name"],
            "configuration": _case_name(row),
        }
        for row in rows
        if int(row["drought"]) == 100 and int(row["oasis_count"]) == 0
    ]


def _oasis_group_evaluation(
    rows: Sequence[dict[str, str]], seeds: Sequence[int], margin: int,
    projected: bool,
) -> dict[str, Any]:
    _validate_shape(rows, seeds)
    groups: list[dict[str, Any]] = []
    failures: list[dict[str, Any]] = []
    for map_index, map_name, _, _ in MAPS:
        for case_name in ("C", "D", "E", "F"):
            members = [
                row for row in rows
                if int(row["map_size"]) == map_index and
                _case_name(row) == case_name
            ]
            if len(members) != len(seeds):
                raise CalibrationError(
                    f"oasis group shape mismatch for {map_name}/{case_name}"
                )
            values = [
                _projected_oasis(row, margin) if projected
                else int(row["oasis_count"])
                for row in members
            ]
            entry = {
                "map_size": map_index,
                "map_name": map_name,
                "configuration": case_name,
                "margin": margin,
                "source": "projected" if projected else "actual",
                "seed_count": len(members),
                "seed_values": [
                    {"seed": int(row["seed"]), "oasis_count": value}
                    for row, value in zip(members, values)
                ],
                "aggregate_oasis_count": sum(values),
                "ok": sum(values) > 0,
            }
            groups.append(entry)
            if not entry["ok"]:
                failures.append({
                    "map_size": map_index,
                    "map_name": map_name,
                    "configuration": case_name,
                    "reason": "aggregate_oasis_count_is_zero",
                })
    return {
        "ok": not failures,
        "margin": margin,
        "source": "projected" if projected else "actual",
        "group_count": len(groups),
        "groups": groups,
        "failures": failures,
    }


def _scrubbed_child_environment(settings: dict[str, str]) -> dict[str, str]:
    child = {
        key: value
        for key, value in os.environ.items()
        if not key.upper().startswith("WORLD_SIM_ARIDITY_")
    }
    child.update(settings)
    return child


def _run_probe(
    exe: Path,
    run_directory: Path,
    stage: str,
    mode: str,
    seeds: Sequence[int],
    divisor: int | None = None,
    desert_base: int | None = None,
    span: int | None = None,
    semi_arid_width: int | None = None,
    oasis_margin: int | None = None,
    require_success: bool = True,
) -> dict[str, Any]:
    if run_directory.exists():
        raise CalibrationError(f"refusing to overwrite run directory {run_directory}")
    run_directory.mkdir(parents=True)
    settings = {
        "WORLD_SIM_ARIDITY_PROBE_DIR": str(run_directory.resolve()),
        "WORLD_SIM_ARIDITY_CALIBRATION_ACK": ACK,
        "WORLD_SIM_ARIDITY_CALIBRATION_STAGE": stage,
        "WORLD_SIM_ARIDITY_CALIBRATION_MODE": mode,
    }
    if mode == "override":
        if any(value is None for value in (
            divisor, desert_base, span, semi_arid_width, oasis_margin,
        )):
            raise CalibrationError("override mode requires all five coefficients")
        settings["WORLD_SIM_ARIDITY_CALIBRATION_DROUGHT_DIVISOR"] = str(divisor)
        settings["WORLD_SIM_ARIDITY_CALIBRATION_DESERT_BASE"] = str(desert_base)
        settings["WORLD_SIM_ARIDITY_CALIBRATION_DESERT_BIAS_SPAN"] = str(span)
        settings["WORLD_SIM_ARIDITY_CALIBRATION_SEMI_ARID_WIDTH"] = str(
            semi_arid_width
        )
        settings["WORLD_SIM_ARIDITY_CALIBRATION_OASIS_TRANSITION_MARGIN"] = str(
            oasis_margin
        )
    elif any(value is not None for value in (
        divisor, desert_base, span, semi_arid_width, oasis_margin,
    )):
        raise CalibrationError("production mode must not receive override coefficients")
    command = [str(exe.resolve()), "--probe-worldgen-aridity-matrix"]
    _write_json_new(
        run_directory / "command.json",
        {
            "argv": command,
            "cwd": str(exe.resolve().parent),
            "exe_sha256": _sha256_file(exe),
            "environment": settings,
            "scrubbed_prefix": "WORLD_SIM_ARIDITY_",
        },
    )
    started = time.perf_counter()
    completed = subprocess.run(
        command,
        cwd=exe.resolve().parent,
        env=_scrubbed_child_environment(settings),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    elapsed_seconds = time.perf_counter() - started
    _write_bytes_new(run_directory / "stdout.txt", completed.stdout)
    _write_bytes_new(run_directory / "stderr.txt", completed.stderr)
    _write_text_new(run_directory / "exit_code.txt", f"{completed.returncode}\n")
    csv_path: Path | None = None
    summary_path: Path | None = None
    rows: list[dict[str, str]] | None = None
    try:
        csv_path = _find_single_output(
            run_directory, ("aridity_calibration.csv", "aridity_matrix.csv"), ".csv"
        )
        summary_path = _find_single_output(
            run_directory,
            ("aridity_calibration_summary.txt", "aridity_matrix_summary.txt"),
            "_summary.txt",
        )
        rows = _load_csv(
            csv_path,
            LEGACY_CSV_COLUMNS
            if stage == "qualification" else CALIBRATION_CSV_COLUMNS,
        )
        _validate_shape(rows, seeds)
    except CalibrationError:
        if require_success:
            _artifact_manifest(run_directory, run_directory / "artifact_manifest.json")
            raise
    result = {
        "directory": str(run_directory.resolve()),
        "stage": stage,
        "mode": mode,
        "candidate_id": _candidate_id(divisor, desert_base, span, semi_arid_width)
        if all(value is not None for value in (
            divisor, desert_base, span, semi_arid_width,
        ))
        else None,
        "drought_divisor": divisor,
        "desert_base": desert_base,
        "desert_bias_span": span,
        "semi_arid_width": semi_arid_width,
        "oasis_transition_margin": oasis_margin,
        "exit_code": completed.returncode,
        "elapsed_seconds": elapsed_seconds,
        "seconds_per_world": elapsed_seconds / len(rows) if rows else None,
        "csv_path": str(csv_path.resolve()) if csv_path else None,
        "csv_sha256": _sha256_file(csv_path) if csv_path else None,
        "summary_path": str(summary_path.resolve()) if summary_path else None,
        "summary_sha256": _sha256_file(summary_path) if summary_path else None,
        "row_count": len(rows) if rows is not None else 0,
        "rows": rows,
    }
    _write_json_new(
        run_directory / "run_result.json",
        {key: value for key, value in result.items() if key != "rows"},
    )
    _artifact_manifest(run_directory, run_directory / "artifact_manifest.json")
    if require_success and completed.returncode != 0:
        raise CalibrationError(
            f"probe failed with exit code {completed.returncode}: {run_directory}"
        )
    return result


def _group_statistics(rows: Sequence[dict[str, str]]) -> list[dict[str, Any]]:
    groups: dict[tuple[int, str], list[dict[str, str]]] = {}
    for row in rows:
        groups.setdefault((int(row["map_size"]), _case_name(row)), []).append(row)
    output: list[dict[str, Any]] = []
    map_names = {index: name for index, name, _, _ in MAPS}
    for (map_index, case_name), members in sorted(groups.items()):
        entry: dict[str, Any] = {
            "map_size": map_index,
            "map_name": map_names[map_index],
            "configuration": case_name,
            "sample_count": len(members),
            "oasis_min": min(int(row["oasis_count"]) for row in members),
            "oasis_max": max(int(row["oasis_count"]) for row in members),
            "oasis_total": sum(int(row["oasis_count"]) for row in members),
        }
        for label, column in (
            ("desert", "desert_count"),
            ("semi_arid", "semi_arid_count"),
            ("combined_arid", "combined_arid_count"),
            ("non_arid", "non_arid_count"),
            ("oasis", "oasis_count"),
            ("wetland", "wetland_count"),
            ("river_channel", "river_channel_count"),
        ):
            if column not in members[0]:
                continue
            shares = [_share(row, column) for row in members]
            entry[f"{label}_mean_share"] = _fraction_payload(_mean(shares))
            entry[f"{label}_max_share"] = _fraction_payload(max(shares))
            entry[f"{label}_min_share"] = _fraction_payload(min(shares))
        output.append(entry)
    return output


def _evaluate(
    rows: Sequence[dict[str, str]],
    seeds: Sequence[int],
    enforce_means: bool,
) -> dict[str, Any]:
    _validate_shape(rows, seeds)
    failures: list[dict[str, Any]] = []
    by_world: dict[tuple[int, int], dict[str, dict[str, str]]] = {}
    for row in rows:
        key = _row_key(row)
        seed, map_size, case_name = key
        by_world.setdefault((seed, map_size), {})[case_name] = row
        structural_reasons: list[str] = []
        if int(row["generated"]) != 1:
            structural_reasons.append("generation_failed")
        if int(row["physical_hash"], 16) == 0:
            structural_reasons.append("zero_physical_hash")
        if int(row["land_count"]) <= 0:
            structural_reasons.append("zero_land")
        if int(row["oasis_semantic_errors"]) != 0:
            structural_reasons.append("oasis_semantic_errors")
        if int(row["oasis_count"]) != int(row["oasis_reachable_count"]):
            structural_reasons.append("oasis_reachable_mismatch")
        active_margin = int(row["oasis_transition_margin"])
        if active_margin not in OASIS_MARGINS:
            structural_reasons.append("unsupported_effective_oasis_margin")
        elif int(row["oasis_count"]) != _projected_oasis(row, active_margin):
            structural_reasons.append("active_margin_projection_mismatch")
        projected_values = [_projected_oasis(row, margin) for margin in OASIS_MARGINS]
        if any(value < 0 for value in projected_values):
            structural_reasons.append("negative_projected_oasis_count")
        if projected_values != sorted(projected_values):
            structural_reasons.append("nonmonotonic_projected_oasis_counts")
        if int(row["river_channel_count"]) < 0:
            structural_reasons.append("negative_river_channel_count")
        if int(row["ok"]) != 1:
            structural_reasons.append("structural_row_not_ok")
        limit = PER_WORLD_LIMITS.get(case_name)
        if limit is not None and _combined_share(row) > limit:
            structural_reasons.append("per_world_combined_arid_ceiling")
        if structural_reasons:
            failures.append(
                {
                    "type": "row",
                    "seed": seed,
                    "map_size": map_size,
                    "configuration": case_name,
                    "reasons": structural_reasons,
                    "combined_arid_share": _fraction_payload(_combined_share(row)),
                }
            )
    for (seed, map_size), cases in sorted(by_world.items()):
        shares = {name: _combined_share(cases[name]) for name, _, _, _ in CASES}
        for left, right, label in MONOTONIC:
            if shares[left] > shares[right]:
                failures.append(
                    {
                        "type": "monotonic",
                        "seed": seed,
                        "map_size": map_size,
                        "relationship": label,
                        "left": _fraction_payload(shares[left]),
                        "right": _fraction_payload(shares[right]),
                    }
                )
    statistics = _group_statistics(rows)
    if enforce_means:
        for item in statistics:
            case_name = item["configuration"]
            limit = MEAN_LIMITS.get(case_name)
            if limit is None:
                continue
            payload = item["combined_arid_mean_share"]
            mean_share = Fraction(payload["numerator"], payload["denominator"])
            if mean_share > limit:
                failures.append(
                    {
                        "type": "per_size_mean",
                        "map_size": item["map_size"],
                        "map_name": item["map_name"],
                        "configuration": case_name,
                        "value": payload,
                        "limit": _fraction_payload(limit),
                    }
                )
    return {
        "ok": not failures,
        "seed_count": len(seeds),
        "world_count": len(rows),
        "mean_limits_enforced": enforce_means,
        "failures": failures,
        "statistics": statistics,
    }


def _ranking_metrics(rows: Sequence[dict[str, str]]) -> dict[str, Any]:
    d_rows = [row for row in rows if _case_name(row) == "D"]
    grand_d = _mean([_combined_share(row) for row in d_rows])
    maximum = max(_combined_share(row) for row in rows)
    size_means = []
    for map_index, _, _, _ in MAPS:
        members = [row for row in d_rows if int(row["map_size"]) == map_index]
        size_means.append(_mean([_combined_share(row) for row in members]))
    cross_size_range = max(size_means) - min(size_means)
    return {
        "configuration_d_grand_mean": _fraction_payload(grand_d),
        "maximum_sampled_combined_arid_share": _fraction_payload(maximum),
        "configuration_d_cross_size_mean_range": _fraction_payload(cross_size_range),
        "configuration_d_size_means": [_fraction_payload(value) for value in size_means],
    }


def _ranking_key(item: dict[str, Any]) -> tuple[Fraction, Fraction, Fraction, str]:
    metrics = item["ranking_metrics"]
    grand = metrics["configuration_d_grand_mean"]
    maximum = metrics["maximum_sampled_combined_arid_share"]
    spread = metrics["configuration_d_cross_size_mean_range"]
    return (
        -Fraction(grand["numerator"], grand["denominator"]),
        Fraction(maximum["numerator"], maximum["denominator"]),
        Fraction(spread["numerator"], spread["denominator"]),
        item["candidate_id"],
    )


def _write_flat_csv(path: Path, rows: Sequence[dict[str, Any]], fields: Sequence[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        for row in rows:
            writer.writerow({field: row.get(field, "") for field in fields})


def _write_statistics_csv(path: Path, statistics: Sequence[dict[str, Any]]) -> None:
    flattened: list[dict[str, Any]] = []
    for item in statistics:
        row: dict[str, Any] = {
            "map_size": item["map_size"],
            "map_name": item["map_name"],
            "configuration": item["configuration"],
            "sample_count": item["sample_count"],
            "oasis_min": item["oasis_min"],
            "oasis_max": item["oasis_max"],
            "oasis_total": item["oasis_total"],
        }
        for metric in (
            "desert", "semi_arid", "combined_arid", "non_arid", "oasis",
            "wetland", "river_channel",
        ):
            for aggregate in ("mean", "max", "min"):
                key = f"{metric}_{aggregate}_share"
                if key not in item:
                    continue
                payload = item[key]
                row[f"{metric}_{aggregate}_share"] = payload["decimal"]
        flattened.append(row)
    fields = list(flattened[0]) if flattened else []
    _write_flat_csv(path, flattened, fields)


def _selection_hash_argument(value: str) -> str:
    path = Path(value)
    if path.is_file():
        text = path.read_text(encoding="utf-8").strip().split()[0]
    else:
        text = value.strip()
    if len(text) != 64 or any(character not in "0123456789abcdefABCDEF" for character in text):
        raise CalibrationError("selection SHA-256 must be a 64-character hexadecimal digest")
    return text.upper()


def _load_selected_record(path: Path, expected_hash_argument: str) -> dict[str, Any]:
    expected = _selection_hash_argument(expected_hash_argument)
    actual = _sha256_file(path)
    if actual != expected:
        raise CalibrationError(f"selected record hash mismatch: expected {expected}, got {actual}")
    with path.open("r", encoding="utf-8") as handle:
        record = json.load(handle)
    if record.get("status") != "SELECTED_FROM_CALIBRATION_ONLY":
        raise CalibrationError("selected record does not carry the frozen selection status")
    if int(record.get("schema_version", 0)) != 2:
        raise CalibrationError("selected record has the wrong schema version")
    required = (
        "candidate_id", "drought_divisor", "desert_base",
        "desert_bias_span", "semi_arid_width", "oasis_transition_margin",
        "calibration_confirmation_csv_sha256",
    )
    if any(field not in record for field in required):
        raise CalibrationError("selected record is missing expanded coefficient evidence")
    if int(record["oasis_transition_margin"]) not in OASIS_MARGINS:
        raise CalibrationError("selected record has an invalid oasis margin")
    return record


def _require_executable(path: Path) -> Path:
    resolved = path.resolve()
    if not resolved.is_file():
        raise CalibrationError(f"executable does not exist: {resolved}")
    return resolved


def _stage_ack_audit(args: argparse.Namespace, output: Path) -> None:
    exe = _require_executable(args.exe)
    cases = (
        ("unset", None, 0),
        ("empty", "", 0),
        ("wrong_case", "ver037a_aridity_recalibration", 0),
        ("prefix", "XVER037A_ARIDITY_RECALIBRATION", 0),
        ("suffix", "VER037A_ARIDITY_RECALIBRATION_SUFFIX", 0),
        ("arbitrary", "arbitrary-nonempty", 0),
        ("exact", ACK, 1),
    )
    results: list[dict[str, Any]] = []
    for name, acknowledgement, expected in cases:
        case_directory = output / name
        case_directory.mkdir(parents=True)
        settings = {
            "WORLD_SIM_ARIDITY_CALIBRATION_STAGE": "pilot",
            "WORLD_SIM_ARIDITY_CALIBRATION_MODE": "override",
            "WORLD_SIM_ARIDITY_CALIBRATION_DROUGHT_DIVISOR": "24",
            "WORLD_SIM_ARIDITY_CALIBRATION_DESERT_BASE": "10",
            "WORLD_SIM_ARIDITY_CALIBRATION_DESERT_BIAS_SPAN": "4",
            "WORLD_SIM_ARIDITY_CALIBRATION_SEMI_ARID_WIDTH": "10",
            "WORLD_SIM_ARIDITY_CALIBRATION_OASIS_TRANSITION_MARGIN": "0",
        }
        if acknowledgement is not None:
            settings["WORLD_SIM_ARIDITY_CALIBRATION_ACK"] = acknowledgement
        command = [str(exe), "--probe-worldgen-aridity-formula"]
        _write_json_new(case_directory / "command.json", {
            "argv": command,
            "cwd": str(exe.parent),
            "exe_sha256": _sha256_file(exe),
            "environment": settings,
            "expected_requested": expected,
            "expected_override_activated": expected,
        })
        completed = subprocess.run(
            command, cwd=exe.parent,
            env=_scrubbed_child_environment(settings),
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
        )
        _write_bytes_new(case_directory / "stdout.txt", completed.stdout)
        _write_bytes_new(case_directory / "stderr.txt", completed.stderr)
        _write_text_new(
            case_directory / "exit_code.txt", f"{completed.returncode}\n"
        )
        text = completed.stdout.decode("utf-8", errors="replace")
        gates = {
            "exit_zero": completed.returncode == 0,
            "requested_exact": f"requested={expected}" in text,
            "activation_exact": f"override_activated={expected}" in text,
            "inactive_before": "active_before=0" in text,
            "inactive_after": "active_after=0" in text,
            "formula_ok": "formula_ok=1" in text,
            "lifecycle_ok": "lifecycle_ok=1" in text,
            "overall_row_ok": "ok=1" in text,
        }
        result = {
            "case": name,
            "acknowledgement": acknowledgement,
            "expected_requested": expected,
            "exit_code": completed.returncode,
            "stdout_sha256": _sha256_bytes(completed.stdout),
            "stderr_sha256": _sha256_bytes(completed.stderr),
            "gates": gates,
            "ok": all(gates.values()),
        }
        results.append(result)
        _write_json_new(case_directory / "result.json", result)
        _artifact_manifest(
            case_directory, case_directory / "artifact_manifest.json"
        )
    payload = {
        "ok": len(results) == 7 and all(item["ok"] for item in results),
        "case_count": len(results),
        "worlds_generated": 0,
        "results": results,
    }
    _write_json_new(output / "ack_audit_result.json", payload)
    if not payload["ok"]:
        raise CalibrationError("exact environment ACK audit failed")


def _stage_qualify(args: argparse.Namespace, output: Path) -> None:
    exe = _require_executable(args.exe)
    historical = args.historical_csv.resolve()
    if not historical.is_file():
        raise CalibrationError(f"historical CSV does not exist: {historical}")
    historical_sha = _sha256_file(historical)
    if historical_sha != EXPECTED_HISTORICAL_CSV_SHA256:
        raise CalibrationError(
            f"historical CSV SHA mismatch: expected {EXPECTED_HISTORICAL_CSV_SHA256}, "
            f"got {historical_sha}"
        )
    historical_rows = _load_csv(historical, LEGACY_CSV_COLUMNS)
    _validate_shape(historical_rows, QUALIFICATION_SEEDS)
    historical_summary = historical.with_name("aridity_matrix_summary.txt")
    if not historical_summary.is_file():
        raise CalibrationError(
            f"historical summary does not exist: {historical_summary}"
        )
    historical_summary_text = historical_summary.read_text(
        encoding="utf-8", errors="replace"
    )
    historical_matrix_hash_ok = (
        EXPECTED_HISTORICAL_MATRIX_HASH in historical_summary_text.lower()
    )
    divisor, desert_base, span, semi_arid_width, oasis_margin = (
        QUALIFICATION_COEFFICIENTS
    )
    override = _run_probe(
        exe,
        output / "q" / "o",
        "qualification",
        "override",
        QUALIFICATION_SEEDS,
        divisor,
        desert_base,
        span,
        semi_arid_width,
        oasis_margin,
    )
    if override["rows"] is None:
        raise CalibrationError("qualification override produced no parseable CSV")
    historical_bytes = historical.read_bytes()
    override_legacy_bytes = _legacy_projection_bytes(override["rows"])
    override_exact = override_legacy_bytes == historical_bytes
    allowed_transition_fields = {
        "physical_hash",
        "oasis_count",
        "oasis_predicate_count",
        "oasis_reachable_count",
    }
    unexpected_differences: list[dict[str, Any]] = []
    intended_transition_differences: list[dict[str, Any]] = []
    historical_by_key = {_row_key(row): row for row in historical_rows}
    override_by_key = {_row_key(row): row for row in override["rows"]}
    if list(historical_by_key) != list(override_by_key):
        unexpected_differences.append({
            "type": "row_key_or_order_mismatch",
            "historical_keys": list(historical_by_key),
            "override_keys": list(override_by_key),
        })
    for key in historical_by_key.keys() & override_by_key.keys():
        old_row = historical_by_key[key]
        new_row = override_by_key[key]
        row_differences = {
            column: {"historical": old_row[column], "override": new_row[column]}
            for column in LEGACY_CSV_COLUMNS
            if old_row[column] != new_row[column]
        }
        if not row_differences:
            continue
        invalid_fields = sorted(set(row_differences) - allowed_transition_fields)
        drought = int(old_row["drought"])
        oasis_fields = (
            "oasis_count", "oasis_predicate_count", "oasis_reachable_count",
        )
        decreases = [
            field for field in oasis_fields
            if int(new_row[field]) < int(old_row[field])
        ]
        final_oasis_changed = old_row["oasis_count"] != new_row["oasis_count"]
        physical_hash_changed = old_row["physical_hash"] != new_row["physical_hash"]
        difference = {
            "seed": key[0],
            "map_size": key[1],
            "configuration": key[2],
            "drought": drought,
            "fields": row_differences,
        }
        if (
            drought <= 0
            or invalid_fields
            or decreases
            or final_oasis_changed != physical_hash_changed
        ):
            difference["invalid_fields"] = invalid_fields
            difference["decreasing_oasis_fields"] = decreases
            difference["physical_hash_matches_final_oasis_change"] = (
                final_oasis_changed == physical_hash_changed
            )
            unexpected_differences.append(difference)
        else:
            intended_transition_differences.append(difference)
    summary_text = Path(override["summary_path"]).read_text(
        encoding="utf-8", errors="replace"
    )
    override_structural_ok = "structural_ok=1" in summary_text
    intended_delta_ok = not unexpected_differences
    result = {
        "ok": override["exit_code"] == 0
        and historical_matrix_hash_ok
        and override_structural_ok
        and intended_delta_ok,
        "qualification_mode": (
            "historical_hash_reproduction_plus_intended_transition_delta_audit"
        ),
        "historical_csv": str(historical),
        "historical_csv_sha256": historical_sha,
        "historical_summary": str(historical_summary.resolve()),
        "historical_summary_sha256": _sha256_file(historical_summary),
        "expected_matrix_hash": EXPECTED_HISTORICAL_MATRIX_HASH,
        "historical_matrix_hash_matches": historical_matrix_hash_ok,
        "historical_production_worlds_reused": 48,
        "override_worlds_launched": 48,
        "successful_path_worlds_through_qualification": 96,
        "override_4_22_csv_sha256": override["csv_sha256"],
        "override_legacy_projection_sha256": _sha256_bytes(
            override_legacy_bytes
        ),
        "override_legacy_projection_bytes_equal_historical": override_exact,
        "whole_csv_identity_expected": False,
        "whole_csv_identity_exemption_reason": (
            "the approved local dry transition oasis branch is active at "
            "margin zero for drought-positive rows"
        ),
        "override_structural_ok": override_structural_ok,
        "intended_transition_difference_count": len(
            intended_transition_differences
        ),
        "intended_transition_differences": intended_transition_differences,
        "unexpected_difference_count": len(unexpected_differences),
        "unexpected_differences": unexpected_differences,
        "coefficients": _coefficient_payload(*QUALIFICATION_COEFFICIENTS),
    }
    _write_json_new(output / "qualification_result.json", result)
    if not result["ok"]:
        raise CalibrationError("4/22/14/16/0 harness qualification failed")


def _stage_pilot(args: argparse.Namespace, output: Path) -> None:
    exe = _require_executable(args.exe)
    divisor, desert_base, span, semi_arid_width, oasis_margin = PILOT_COEFFICIENTS
    run = _run_probe(
        exe, output / "p", "pilot", "override", SCREENING_SEEDS,
        divisor, desert_base, span, semi_arid_width, oasis_margin,
        require_success=False,
    )
    evaluation = None
    if run["rows"] is not None:
        _validate_coefficients(run["rows"], PILOT_COEFFICIENTS)
        evaluation = _evaluate(
            run["rows"], SCREENING_SEEDS, enforce_means=False,
        )
    elapsed = float(run["elapsed_seconds"])
    projected = elapsed * MAX_SUCCESSFUL_PATH_WORLDS / 24.0
    result = {
        "ok": run["exit_code"] == 0 and run["row_count"] == 24 and
        bool(evaluation and evaluation["ok"]) and
        projected <= MAX_PROJECTED_RUNTIME_SECONDS,
        "candidate_id": _candidate_id(
            divisor, desert_base, span, semi_arid_width,
        ),
        "coefficients": _coefficient_payload(*PILOT_COEFFICIENTS),
        "row_count": run["row_count"],
        "csv_path": run["csv_path"],
        "csv_sha256": run["csv_sha256"],
        "elapsed_seconds": elapsed,
        "seconds_per_world": elapsed / 24.0,
        "projected_successful_path_worlds": MAX_SUCCESSFUL_PATH_WORLDS,
        "projected_total_seconds": projected,
        "projected_total_hours": projected / 3600.0,
        "maximum_seconds": MAX_PROJECTED_RUNTIME_SECONDS,
        "runtime_gate_ok": projected <= MAX_PROJECTED_RUNTIME_SECONDS,
        "evaluation": evaluation,
    }
    _write_json_new(output / "pilot_result.json", result)
    if not result["ok"]:
        if projected > MAX_PROJECTED_RUNTIME_SECONDS:
            raise CalibrationError("pilot projects more than four hours; stop before screening")
        raise CalibrationError("pilot qualification failed")


def _candidate_summary(
    candidate_id: str,
    divisor: int,
    desert_base: int,
    span: int,
    semi_arid_width: int,
    run: dict[str, Any] | None,
    evaluation: dict[str, Any] | None,
    error: str | None,
) -> dict[str, Any]:
    return {
        "candidate_id": candidate_id,
        "drought_divisor": divisor,
        "desert_base": desert_base,
        "desert_bias_span": span,
        "semi_arid_width": semi_arid_width,
        "process_ok": run is not None and run["exit_code"] == 0,
        "matrix_ok": bool(evaluation and evaluation["ok"]),
        "ok": run is not None and run["exit_code"] == 0 and bool(evaluation and evaluation["ok"]),
        "csv_path": run["csv_path"] if run else None,
        "csv_sha256": run["csv_sha256"] if run else None,
        "row_count": run["row_count"] if run else 0,
        "elapsed_seconds": run["elapsed_seconds"] if run else None,
        "failure_count": len(evaluation["failures"]) if evaluation else 1,
        "error": error,
        "evaluation": evaluation,
    }


def _require_pilot(path: Path, expected_hash_argument: str) -> dict[str, Any]:
    expected = _selection_hash_argument(expected_hash_argument)
    actual = _sha256_file(path)
    if actual != expected:
        raise CalibrationError(
            f"pilot result hash mismatch: expected {expected}, got {actual}"
        )
    result = json.loads(path.read_text(encoding="utf-8"))
    if not result.get("ok") or not result.get("runtime_gate_ok"):
        raise CalibrationError("select requires a passing pilot runtime result")
    if result.get("candidate_id") != "d24_a10_b4_s10":
        raise CalibrationError("pilot result carries the wrong candidate")
    if int(result.get("row_count", 0)) != 24:
        raise CalibrationError("pilot result does not bind exactly 24 worlds")
    if float(result.get("projected_total_seconds", 0.0)) > (
        MAX_PROJECTED_RUNTIME_SECONDS
    ):
        raise CalibrationError("pilot result projects more than four hours")
    return result


def _smallest_passing_margin(
    rows: Sequence[dict[str, str]], seeds: Sequence[int],
) -> tuple[int | None, list[dict[str, Any]]]:
    evaluations: list[dict[str, Any]] = []
    selected: int | None = None
    for margin in OASIS_MARGINS:
        evaluation = _oasis_group_evaluation(
            rows, seeds, margin, projected=True,
        )
        evaluations.append(evaluation)
        if selected is None and evaluation["ok"]:
            selected = margin
    return selected, evaluations


def _projection_confirmation(
    projected_rows: Sequence[dict[str, str]],
    actual_rows: Sequence[dict[str, str]], margin: int,
) -> dict[str, Any]:
    projected_by_key = {_row_key(row): row for row in projected_rows}
    actual_by_key = {_row_key(row): row for row in actual_rows}
    mismatches: list[dict[str, Any]] = []
    if list(projected_by_key) != list(actual_by_key):
        mismatches.append({"type": "row_key_or_order_mismatch"})
    for key in projected_by_key.keys() & actual_by_key.keys():
        expected = _projected_oasis(projected_by_key[key], margin)
        actual = int(actual_by_key[key]["oasis_count"])
        if expected != actual:
            mismatches.append({
                "type": "projected_actual_oasis_mismatch",
                "seed": key[0],
                "map_size": key[1],
                "configuration": key[2],
                "projected": expected,
                "actual": actual,
            })
    return {
        "ok": not mismatches and len(projected_rows) == len(actual_rows),
        "margin": margin,
        "projected_row_count": len(projected_rows),
        "actual_row_count": len(actual_rows),
        "mismatches": mismatches,
    }


def _stage_select(args: argparse.Namespace, output: Path) -> None:
    exe = _require_executable(args.exe)
    historical = args.historical_csv.resolve()
    if not historical.is_file() or _sha256_file(historical) != EXPECTED_HISTORICAL_CSV_SHA256:
        raise CalibrationError("select requires the exact qualified historical 4/22 CSV")
    pilot_path = args.pilot_result.resolve()
    pilot = _require_pilot(pilot_path, args.pilot_sha256)
    screening: list[dict[str, Any]] = []
    for divisor in DROUGHT_DIVISORS:
        for desert_base in DESERT_BASES:
            for span in BIAS_SPANS:
                for semi_arid_width in SEMI_ARID_WIDTHS:
                    candidate_id = _candidate_id(
                        divisor, desert_base, span, semi_arid_width,
                    )
                    run = None
                    evaluation = None
                    error = None
                    coefficients = (
                        divisor, desert_base, span, semi_arid_width, 0,
                    )
                    try:
                        run = _run_probe(
                            exe, output / "s" / candidate_id,
                            "screening", "override", SCREENING_SEEDS,
                            *coefficients, require_success=False,
                        )
                        if run["rows"] is None:
                            raise CalibrationError(
                                "screening probe produced no parseable CSV"
                            )
                        _validate_coefficients(run["rows"], coefficients)
                        evaluation = _evaluate(
                            run["rows"], SCREENING_SEEDS,
                            enforce_means=False,
                        )
                    except Exception as exception:
                        error = f"{type(exception).__name__}: {exception}"
                    item = _candidate_summary(
                        candidate_id, divisor, desert_base, span,
                        semi_arid_width, run, evaluation, error,
                    )
                    if item["ok"] and run is not None:
                        item["ranking_metrics"] = _ranking_metrics(run["rows"])
                    screening.append(item)
    survivors = [item for item in screening if item["ok"]]
    screening_ranking = sorted(survivors, key=_ranking_key)
    advanced = screening_ranking[:MAX_ADVANCED_CANDIDATES]
    screening_payload = {
        "candidate_count": len(screening),
        "expected_candidate_count": 256,
        "pilot_result": str(pilot_path),
        "pilot_result_sha256": _sha256_file(pilot_path),
        "pilot": pilot,
        "candidates": screening,
    }
    _write_json_new(output / "screening_candidates.json", screening_payload)
    _write_flat_csv(
        output / "screening_candidates.csv",
        screening,
        (
            "candidate_id",
            "drought_divisor",
            "desert_base",
            "desert_bias_span",
            "semi_arid_width",
            "process_ok",
            "matrix_ok",
            "ok",
            "failure_count",
            "csv_sha256",
            "error",
        ),
    )
    _write_json_new(
        output / "screening_survivors.json",
        {
            "count": len(survivors),
            "candidate_ids": [item["candidate_id"] for item in survivors],
            "ranking_rule": [
                "highest configuration-D grand mean combined-arid share",
                "lowest maximum sampled combined-arid share",
                "lowest cross-size range of configuration-D means",
                "lexicographically smallest candidate ID",
            ],
            "ranking": screening_ranking,
            "advanced_count": len(advanced),
            "advanced_candidate_ids": [item["candidate_id"] for item in advanced],
        },
    )
    screening_ranking_rows: list[dict[str, Any]] = []
    for position, item in enumerate(screening_ranking, 1):
        metrics = item["ranking_metrics"]
        screening_ranking_rows.append({
            "rank": position,
            "advanced": position <= MAX_ADVANCED_CANDIDATES,
            "candidate_id": item["candidate_id"],
            "drought_divisor": item["drought_divisor"],
            "desert_base": item["desert_base"],
            "desert_bias_span": item["desert_bias_span"],
            "semi_arid_width": item["semi_arid_width"],
            "configuration_d_grand_mean": metrics[
                "configuration_d_grand_mean"
            ]["decimal"],
            "maximum_sampled_combined_arid_share": metrics[
                "maximum_sampled_combined_arid_share"
            ]["decimal"],
            "configuration_d_cross_size_mean_range": metrics[
                "configuration_d_cross_size_mean_range"
            ]["decimal"],
        })
    _write_flat_csv(
        output / "screening_ranking.csv", screening_ranking_rows,
        (
            "rank", "advanced", "candidate_id", "drought_divisor",
            "desert_base", "desert_bias_span", "semi_arid_width",
            "configuration_d_grand_mean",
            "maximum_sampled_combined_arid_share",
            "configuration_d_cross_size_mean_range",
        ),
    )
    if len(screening) != 256:
        raise CalibrationError("screening did not enumerate exactly 256 candidates")
    if not survivors:
        raise CalibrationError("no candidate survived screening")

    calibration: list[dict[str, Any]] = []
    for survivor in advanced:
        divisor = int(survivor["drought_divisor"])
        desert_base = int(survivor["desert_base"])
        span = int(survivor["desert_bias_span"])
        semi_arid_width = int(survivor["semi_arid_width"])
        candidate_id = survivor["candidate_id"]
        run = None
        evaluation = None
        margin_evaluations = None
        selected_margin = None
        error = None
        try:
            run = _run_probe(
                exe,
                output / "c" / candidate_id,
                "calibration",
                "override",
                CALIBRATION_SEEDS,
                divisor,
                desert_base,
                span,
                semi_arid_width,
                0,
                require_success=False,
            )
            if run["rows"] is None:
                raise CalibrationError("calibration probe produced no parseable CSV")
            _validate_coefficients(
                run["rows"], (divisor, desert_base, span, semi_arid_width, 0),
            )
            evaluation = _evaluate(run["rows"], CALIBRATION_SEEDS, enforce_means=True)
            selected_margin, margin_evaluations = _smallest_passing_margin(
                run["rows"], CALIBRATION_SEEDS,
            )
        except Exception as exception:
            error = f"{type(exception).__name__}: {exception}"
        item = _candidate_summary(
            candidate_id, divisor, desert_base, span, semi_arid_width,
            run, evaluation, error,
        )
        item["oasis_margin_evaluations"] = margin_evaluations
        item["selected_oasis_transition_margin"] = selected_margin
        item["oasis_statistical_ok"] = selected_margin is not None
        item["ok"] = bool(item["ok"] and selected_margin is not None)
        if item["ok"] and run is not None:
            item["ranking_metrics"] = _ranking_metrics(run["rows"])
        calibration.append(item)
    passing = [item for item in calibration if item["ok"]]
    ranking = sorted(passing, key=_ranking_key)
    _write_json_new(
        output / "calibration_candidates.json",
        {
            "screening_survivor_count": len(survivors),
            "advanced_count": len(advanced),
            "candidates": calibration,
        },
    )
    ranking_payload = {
        "passing_count": len(ranking),
        "ranking_rule": [
            "highest configuration-D grand mean combined-arid share",
            "lowest maximum sampled combined-arid share",
            "lowest cross-size range of configuration-D means",
            "lexicographically smallest candidate ID",
        ],
        "candidates": ranking,
    }
    _write_json_new(output / "calibration_ranking.json", ranking_payload)
    ranking_csv: list[dict[str, Any]] = []
    for position, item in enumerate(ranking, 1):
        metrics = item["ranking_metrics"]
        ranking_csv.append(
            {
                "rank": position,
                "candidate_id": item["candidate_id"],
                "drought_divisor": item["drought_divisor"],
                "desert_base": item["desert_base"],
                "desert_bias_span": item["desert_bias_span"],
                "semi_arid_width": item["semi_arid_width"],
                "oasis_transition_margin": item[
                    "selected_oasis_transition_margin"
                ],
                "configuration_d_grand_mean": metrics["configuration_d_grand_mean"]["decimal"],
                "maximum_sampled_combined_arid_share": metrics[
                    "maximum_sampled_combined_arid_share"
                ]["decimal"],
                "configuration_d_cross_size_mean_range": metrics[
                    "configuration_d_cross_size_mean_range"
                ]["decimal"],
            }
        )
    _write_flat_csv(
        output / "calibration_ranking.csv",
        ranking_csv,
        (
            "rank",
            "candidate_id",
            "drought_divisor",
            "desert_base",
            "desert_bias_span",
            "semi_arid_width",
            "oasis_transition_margin",
            "configuration_d_grand_mean",
            "maximum_sampled_combined_arid_share",
            "configuration_d_cross_size_mean_range",
        ),
    )
    if not ranking:
        raise CalibrationError("no screening survivor passed calibration")
    selected = ranking[0]
    selected_screening = next(item for item in screening if item["candidate_id"] == selected["candidate_id"])
    selected_coefficients = (
        int(selected["drought_divisor"]), int(selected["desert_base"]),
        int(selected["desert_bias_span"]), int(selected["semi_arid_width"]),
        int(selected["selected_oasis_transition_margin"]),
    )
    confirmation = _run_probe(
        exe, output / "x" / selected["candidate_id"], "confirmation",
        "override", CALIBRATION_SEEDS, *selected_coefficients,
        require_success=False,
    )
    if confirmation["rows"] is None:
        raise CalibrationError("selected confirmation produced no parseable CSV")
    _validate_coefficients(confirmation["rows"], selected_coefficients)
    confirmation_evaluation = _evaluate(
        confirmation["rows"], CALIBRATION_SEEDS, enforce_means=True,
    )
    confirmation_oasis = _oasis_group_evaluation(
        confirmation["rows"], CALIBRATION_SEEDS,
        selected_coefficients[4], projected=False,
    )
    selected_calibration_rows = _load_csv(
        Path(selected["csv_path"]), CALIBRATION_CSV_COLUMNS,
    )
    projection_confirmation = _projection_confirmation(
        selected_calibration_rows, confirmation["rows"],
        selected_coefficients[4],
    )
    confirmation_result = {
        "ok": confirmation["exit_code"] == 0 and
        confirmation_evaluation["ok"] and confirmation_oasis["ok"] and
        projection_confirmation["ok"],
        "candidate_id": selected["candidate_id"],
        "coefficients": _coefficient_payload(*selected_coefficients),
        "csv_path": confirmation["csv_path"],
        "csv_sha256": confirmation["csv_sha256"],
        "evaluation": confirmation_evaluation,
        "oasis_statistical_evaluation": confirmation_oasis,
        "projection_confirmation": projection_confirmation,
        "individual_zero_oasis_worlds": _zero_oasis_worlds(
            confirmation["rows"]
        ),
    }
    _write_json_new(output / "selected_confirmation.json", confirmation_result)
    if not confirmation_result["ok"]:
        raise CalibrationError(
            "selected candidate actual calibration confirmation failed"
        )
    historical_rows = _load_csv(historical, LEGACY_CSV_COLUMNS)
    _validate_shape(historical_rows, QUALIFICATION_SEEDS)
    selected_screening_rows = _load_csv(
        Path(selected_screening["csv_path"]), CALIBRATION_CSV_COLUMNS,
    )
    selected_overlap_rows = selected_screening_rows + [
        row for row in confirmation["rows"]
        if int(row["seed"]) == QUALIFICATION_SEEDS[1]
    ]
    _validate_shape(selected_overlap_rows, QUALIFICATION_SEEDS)
    old_statistics = _group_statistics(historical_rows)
    selected_overlap_statistics = _group_statistics(selected_overlap_rows)
    old_by_key = _statistics_by_key(old_statistics)
    selected_by_key = _statistics_by_key(selected_overlap_statistics)
    old_vs_selected_rows: list[dict[str, Any]] = []
    for key in sorted(old_by_key):
        old_item = old_by_key[key]
        new_item = selected_by_key[key]
        row: dict[str, Any] = {
            "map_size": key[0],
            "map_name": old_item["map_name"],
            "configuration": key[1],
            "old_oasis_min": old_item["oasis_min"],
            "selected_oasis_min": new_item["oasis_min"],
        }
        for metric in ("desert", "semi_arid", "combined_arid", "non_arid"):
            old_payload = old_item[f"{metric}_mean_share"]
            new_payload = new_item[f"{metric}_mean_share"]
            old_fraction = Fraction(old_payload["numerator"], old_payload["denominator"])
            new_fraction = Fraction(new_payload["numerator"], new_payload["denominator"])
            row[f"old_{metric}_mean"] = old_payload["decimal"]
            row[f"selected_{metric}_mean"] = new_payload["decimal"]
            row[f"selected_minus_old_{metric}_mean"] = _fraction_payload(
                new_fraction - old_fraction
            )["decimal"]
            row[f"old_{metric}_max"] = old_item[f"{metric}_max_share"]["decimal"]
            row[f"selected_{metric}_max"] = new_item[f"{metric}_max_share"]["decimal"]
        old_vs_selected_rows.append(row)
    old_vs_selected_payload = {
        "historical_csv": str(historical),
        "historical_csv_sha256": _sha256_file(historical),
        "overlap_seeds": list(QUALIFICATION_SEEDS),
        "old_statistics": old_statistics,
        "selected_statistics": selected_overlap_statistics,
        "rows": old_vs_selected_rows,
    }
    _write_json_new(output / "old_4_22_vs_selected.json", old_vs_selected_payload)
    _write_flat_csv(
        output / "old_4_22_vs_selected.csv",
        old_vs_selected_rows,
        list(old_vs_selected_rows[0]) if old_vs_selected_rows else [],
    )
    selected_record = {
        "schema_version": 2,
        "status": "SELECTED_FROM_CALIBRATION_ONLY",
        "candidate_id": selected["candidate_id"],
        "drought_divisor": selected["drought_divisor"],
        "desert_base": selected["desert_base"],
        "desert_bias_span": selected["desert_bias_span"],
        "semi_arid_width": selected["semi_arid_width"],
        "oasis_transition_margin": selected[
            "selected_oasis_transition_margin"
        ],
        "selection_rule": ranking_payload["ranking_rule"],
        "ranking_metrics": selected["ranking_metrics"],
        "pilot_result": str(pilot_path),
        "pilot_result_sha256": _sha256_file(pilot_path),
        "screening_csv_relative": str(
            Path(selected_screening["csv_path"]).resolve().relative_to(output.resolve())
        ).replace("\\", "/"),
        "screening_csv_sha256": selected_screening["csv_sha256"],
        "calibration_projection_csv_relative": str(
            Path(selected["csv_path"]).resolve().relative_to(output.resolve())
        ).replace("\\", "/"),
        "calibration_projection_csv_sha256": selected["csv_sha256"],
        "calibration_confirmation_csv_relative": str(
            Path(confirmation["csv_path"]).resolve().relative_to(output.resolve())
        ).replace("\\", "/"),
        "calibration_confirmation_csv_sha256": confirmation["csv_sha256"],
        "calibration_statistics": confirmation_evaluation["statistics"],
        "calibration_oasis_statistical_evaluation": confirmation_oasis,
        "projection_confirmation": projection_confirmation,
        "screening_survivor_count": len(survivors),
        "screening_advanced_count": len(advanced),
        "calibration_passing_count": len(ranking),
        "world_counts": {
            "historical_and_override_qualification": 96,
            "pilot": 24,
            "screening": sum(int(item["row_count"]) for item in screening),
            "calibration": sum(int(item["row_count"]) for item in calibration),
            "selected_confirmation": confirmation["row_count"],
            "through_selection": 96 + 24 +
            sum(int(item["row_count"]) for item in screening) +
            sum(int(item["row_count"]) for item in calibration) +
            int(confirmation["row_count"]),
            "maximum_successful_path": MAX_SUCCESSFUL_PATH_WORLDS,
        },
        "holdout_data_used": False,
        "old_vs_selected": {
            "available": True,
            "historical_csv_sha256": _sha256_file(historical),
            "overlap_seeds": list(QUALIFICATION_SEEDS),
            "result_relative": "old_4_22_vs_selected.json",
        },
    }
    selected_path = output / "selected_candidate.json"
    _write_json_new(selected_path, selected_record)
    selected_sha = _sha256_file(selected_path)
    _write_text_new(output / "selected_candidate.sha256", f"{selected_sha}  selected_candidate.json\n")
    _write_statistics_csv(
        output / "selected_calibration_per_size_configuration.csv",
        confirmation_evaluation["statistics"],
    )


def _load_bound_json(path: Path, expected_hash_argument: str,
                     label: str) -> tuple[dict[str, Any], str]:
    expected = _selection_hash_argument(expected_hash_argument)
    actual = _sha256_file(path)
    if actual != expected:
        raise CalibrationError(
            f"{label} hash mismatch: expected {expected}, got {actual}"
        )
    return json.loads(path.read_text(encoding="utf-8")), actual


def _stage_bind_selection(args: argparse.Namespace, output: Path) -> None:
    exe = _require_executable(args.exe)
    source_output = args.selection_output.resolve()
    source_path = args.selection_record.resolve()
    source = _load_selected_record(source_path, args.selection_sha256)
    expected_source_path = source_output / "selected_candidate.json"
    if source_path != expected_source_path or not expected_source_path.is_file():
        raise CalibrationError(
            "selection record is not the immutable record in selection output"
        )
    qualification_path = args.qualification_result.resolve()
    qualification, qualification_sha = _load_bound_json(
        qualification_path, args.qualification_sha256, "qualification result",
    )
    ack_path = args.ack_result.resolve()
    ack, ack_sha = _load_bound_json(
        ack_path, args.ack_sha256, "ACK audit result",
    )
    qualification_ok = (
        qualification.get("ok") is True
        and qualification.get("historical_csv_sha256") ==
            EXPECTED_HISTORICAL_CSV_SHA256
        and qualification.get("historical_matrix_hash_matches") is True
        and int(qualification.get("unexpected_difference_count", -1)) == 0
        and int(qualification.get("historical_production_worlds_reused", 0)) == 48
        and int(qualification.get("override_worlds_launched", 0)) == 48
    )
    ack_ok = (
        ack.get("ok") is True
        and int(ack.get("case_count", 0)) == 7
        and int(ack.get("worlds_generated", -1)) == 0
        and len(ack.get("results", ())) == 7
        and all(item.get("ok") for item in ack.get("results", ()))
    )
    if not qualification_ok or not ack_ok:
        raise CalibrationError(
            "selection binding requires passing qualification and ACK evidence"
        )
    bound = dict(source)
    bound["evidence_binding_status"] = (
        "SUPERSEDING_SELECTION_EVIDENCE_BOUND_BEFORE_HOLDOUT"
    )
    bound["source_selection_output"] = str(source_output)
    bound["source_selection_record"] = str(source_path)
    bound["source_selection_record_sha256"] = _sha256_file(source_path)
    bound["selection_executable"] = str(exe)
    bound["selection_executable_sha256"] = _sha256_file(exe)
    bound["qualification_evidence"] = {
        "path": str(qualification_path),
        "sha256": qualification_sha,
        "historical_csv_sha256": qualification["historical_csv_sha256"],
        "historical_matrix_hash": EXPECTED_HISTORICAL_MATRIX_HASH,
        "mode": qualification.get("qualification_mode"),
        "unexpected_difference_count": qualification[
            "unexpected_difference_count"
        ],
    }
    bound["ack_audit_evidence"] = {
        "path": str(ack_path),
        "sha256": ack_sha,
        "case_count": ack["case_count"],
        "worlds_generated": ack["worlds_generated"],
    }
    through_selection = int(bound["world_counts"]["through_selection"])
    if through_selection > MAX_SUCCESSFUL_PATH_WORLDS:
        raise CalibrationError("selection world accounting exceeds maximum")
    bound["world_counts"] = {
        **bound["world_counts"],
        "through_bound_selection": through_selection,
        "binding_worlds": 0,
        "maximum_successful_path": MAX_SUCCESSFUL_PATH_WORLDS,
    }
    bound_path = output / "selected_candidate.json"
    _write_json_new(bound_path, bound)
    bound_sha = _sha256_file(bound_path)
    _write_text_new(
        output / "selected_candidate.sha256",
        f"{bound_sha}  selected_candidate.json\n",
    )
    _write_json_new(output / "selection_binding_result.json", {
        "ok": True,
        "selected_candidate": str(bound_path.resolve()),
        "selected_candidate_sha256": bound_sha,
        "source_selection_record_sha256": _sha256_file(source_path),
        "selection_executable_sha256": _sha256_file(exe),
        "qualification_result_sha256": qualification_sha,
        "ack_audit_result_sha256": ack_sha,
        "worlds_generated": 0,
        "through_selection_worlds": through_selection,
        "maximum_successful_path_worlds": MAX_SUCCESSFUL_PATH_WORLDS,
    })


def _statistics_by_key(statistics: Sequence[dict[str, Any]]) -> dict[tuple[int, str], dict[str, Any]]:
    return {(int(item["map_size"]), item["configuration"]): item for item in statistics}


def _calibration_holdout_differences(
    calibration: Sequence[dict[str, Any]], holdout: Sequence[dict[str, Any]]
) -> list[dict[str, Any]]:
    calibration_by_key = _statistics_by_key(calibration)
    holdout_by_key = _statistics_by_key(holdout)
    differences: list[dict[str, Any]] = []
    for key in sorted(calibration_by_key):
        cal = calibration_by_key[key]["combined_arid_mean_share"]
        out = holdout_by_key[key]["combined_arid_mean_share"]
        cal_fraction = Fraction(cal["numerator"], cal["denominator"])
        out_fraction = Fraction(out["numerator"], out["denominator"])
        differences.append(
            {
                "map_size": key[0],
                "configuration": key[1],
                "calibration_mean": cal,
                "holdout_mean": out,
                "holdout_minus_calibration": _fraction_payload(out_fraction - cal_fraction),
            }
        )
    return differences


def _stage_holdout(args: argparse.Namespace, output: Path) -> None:
    exe = _require_executable(args.exe)
    selected = _load_selected_record(args.selected_record.resolve(), args.selection_sha256)
    divisor = int(selected["drought_divisor"])
    desert_base = int(selected["desert_base"])
    span = int(selected["desert_bias_span"])
    semi_arid_width = int(selected["semi_arid_width"])
    oasis_margin = int(selected["oasis_transition_margin"])
    candidate_id = selected["candidate_id"]
    if candidate_id != _candidate_id(
        divisor, desert_base, span, semi_arid_width,
    ):
        raise CalibrationError("selected candidate ID disagrees with its coefficients")
    run = _run_probe(
        exe,
        output / "h" / candidate_id,
        "holdout",
        "override",
        HOLDOUT_SEEDS,
        divisor,
        desert_base,
        span,
        semi_arid_width,
        oasis_margin,
        require_success=False,
    )
    if run["rows"] is None:
        raise CalibrationError("holdout probe produced no parseable CSV")
    _validate_coefficients(
        run["rows"],
        (divisor, desert_base, span, semi_arid_width, oasis_margin),
    )
    evaluation = _evaluate(run["rows"], HOLDOUT_SEEDS, enforce_means=True)
    oasis_evaluation = _oasis_group_evaluation(
        run["rows"], HOLDOUT_SEEDS, oasis_margin, projected=False,
    )
    differences = _calibration_holdout_differences(
        selected["calibration_statistics"], evaluation["statistics"]
    )
    holdout_ok = run["exit_code"] == 0 and evaluation["ok"] and (
        oasis_evaluation["ok"]
    )
    through_selection = int(
        selected["world_counts"].get(
            "through_bound_selection",
            selected["world_counts"]["through_selection"],
        )
    )
    through_holdout = through_selection + int(run["row_count"])
    holdout_ok = holdout_ok and through_holdout <= MAX_SUCCESSFUL_PATH_WORLDS
    result = {
        "ok": holdout_ok,
        "status": "HOLDOUT_PASS_NO_RESELECTION"
        if holdout_ok
        else "HOLDOUT_FAIL_STOP",
        "selected_record": str(args.selected_record.resolve()),
        "selected_record_sha256": _sha256_file(args.selected_record.resolve()),
        "candidate_id": candidate_id,
        "drought_divisor": divisor,
        "desert_base": desert_base,
        "desert_bias_span": span,
        "semi_arid_width": semi_arid_width,
        "oasis_transition_margin": oasis_margin,
        "csv_path": run["csv_path"],
        "csv_sha256": run["csv_sha256"],
        "row_count": run["row_count"],
        "elapsed_seconds": run["elapsed_seconds"],
        "evaluation": evaluation,
        "oasis_statistical_evaluation": oasis_evaluation,
        "individual_zero_oasis_worlds": _zero_oasis_worlds(run["rows"]),
        "replacement_selection_attempted": False,
        "world_counts": {
            "through_selection": through_selection,
            "holdout": int(run["row_count"]),
            "through_holdout": through_holdout,
            "maximum_successful_path": MAX_SUCCESSFUL_PATH_WORLDS,
            "within_maximum": through_holdout <= MAX_SUCCESSFUL_PATH_WORLDS,
        },
    }
    _write_json_new(output / "holdout_result.json", result)
    _write_json_new(output / "calibration_vs_holdout.json", {"differences": differences})
    flat_differences = [
        {
            "map_size": item["map_size"],
            "configuration": item["configuration"],
            "calibration_mean": item["calibration_mean"]["decimal"],
            "holdout_mean": item["holdout_mean"]["decimal"],
            "holdout_minus_calibration": item["holdout_minus_calibration"]["decimal"],
        }
        for item in differences
    ]
    _write_flat_csv(
        output / "calibration_vs_holdout.csv",
        flat_differences,
        (
            "map_size",
            "configuration",
            "calibration_mean",
            "holdout_mean",
            "holdout_minus_calibration",
        ),
    )
    _write_statistics_csv(output / "holdout_per_size_configuration.csv", evaluation["statistics"])
    if not result["ok"]:
        raise CalibrationError("frozen selected candidate failed holdout; no replacement selected")


def _load_stage_rows(directory: Path) -> tuple[Path, list[dict[str, str]]]:
    csv_path = _find_single_output(
        directory, ("aridity_calibration.csv", "aridity_matrix.csv"), ".csv"
    )
    return csv_path, _load_csv(csv_path)


def _compare_rows(
    production: Sequence[dict[str, str]],
    override: Sequence[dict[str, str]],
) -> dict[str, Any]:
    production_keys = [_row_key(row) for row in production]
    override_keys = [_row_key(row) for row in override]
    mismatches: list[dict[str, Any]] = []
    if production_keys != override_keys:
        mismatches.append(
            {
                "type": "row_order_or_key_set",
                "production_keys": production_keys,
                "override_keys": override_keys,
            }
        )
    for production_row, override_row in zip(production, override):
        differences = {
            column: {"production": production_row[column], "override": override_row[column]}
            for column in COMPARISON_COLUMNS
            if production_row[column] != override_row[column]
        }
        if differences:
            mismatches.append(
                {"type": "row_values", "key": _row_key(production_row), "differences": differences}
            )
    return {
        "ok": not mismatches and len(production) == len(override),
        "production_row_count": len(production),
        "override_row_count": len(override),
        "mismatches": mismatches,
    }


def _compare_screening_projection(
    production: Sequence[dict[str, str]],
    screening: Sequence[dict[str, str]],
    selected_margin: int,
) -> dict[str, Any]:
    production_keys = [_row_key(row) for row in production]
    screening_keys = [_row_key(row) for row in screening]
    mismatches: list[dict[str, Any]] = []
    allowed_active_margin_fields = {
        "physical_hash",
        "oasis_count",
        "oasis_predicate_count",
        "oasis_reachable_count",
        "oasis_transition_margin",
        "oasis_transition_limit",
    }
    if production_keys != screening_keys:
        mismatches.append({
            "type": "row_order_or_key_set",
            "production_keys": production_keys,
            "screening_keys": screening_keys,
        })
    for production_row, screening_row in zip(production, screening):
        differences = {
            column: {
                "production": production_row[column],
                "screening": screening_row[column],
            }
            for column in COMPARISON_COLUMNS
            if column not in allowed_active_margin_fields
            and production_row[column] != screening_row[column]
        }
        projected = _projected_oasis(screening_row, selected_margin)
        actual = int(production_row["oasis_count"])
        if differences or actual != projected or (
            int(production_row["oasis_transition_margin"]) != selected_margin
        ):
            mismatches.append({
                "type": "screening_macro_or_projection_identity",
                "key": _row_key(production_row),
                "unexpected_field_differences": differences,
                "projected_visible_oasis": projected,
                "actual_visible_oasis": actual,
                "production_margin": int(
                    production_row["oasis_transition_margin"]
                ),
                "selected_margin": selected_margin,
            })
    return {
        "ok": not mismatches and len(production) == len(screening),
        "comparison_mode": (
            "exact_macro_fields_plus_selected_margin_oasis_projection"
        ),
        "selected_margin": selected_margin,
        "production_row_count": len(production),
        "screening_row_count": len(screening),
        "intentionally_margin_dependent_fields": sorted(
            allowed_active_margin_fields
        ),
        "mismatches": mismatches,
    }


def _audit_bmp(path: Path, expected_width: int,
               expected_height: int) -> dict[str, Any]:
    data = path.read_bytes()
    ok = False
    details: dict[str, Any] = {}
    if len(data) >= 54 and data[:2] == b"BM":
        declared_size = struct.unpack_from("<I", data, 2)[0]
        pixel_offset = struct.unpack_from("<I", data, 10)[0]
        dib_size = struct.unpack_from("<I", data, 14)[0]
        width = struct.unpack_from("<i", data, 18)[0]
        height = struct.unpack_from("<i", data, 22)[0]
        planes = struct.unpack_from("<H", data, 26)[0]
        bits_per_pixel = struct.unpack_from("<H", data, 28)[0]
        row_bytes = ((abs(width) * bits_per_pixel + 31) // 32) * 4
        pixels_fit = pixel_offset + row_bytes * abs(height) <= len(data)
        pixel_hash = 1469598103934665603
        if pixels_fit and bits_per_pixel in (24, 32):
            bytes_per_pixel = bits_per_pixel // 8
            for y in range(abs(height)):
                row_offset = pixel_offset + y * row_bytes
                for x in range(abs(width)):
                    offset = row_offset + x * bytes_per_pixel
                    blue, green, red = data[offset:offset + 3]
                    pixel_hash ^= blue | (green << 8) | (red << 16)
                    pixel_hash = (pixel_hash * 1099511628211) & 0xffffffffffffffff
        ok = (
            declared_size == len(data) and dib_size >= 40 and
            width == expected_width and abs(height) == expected_height and
            planes == 1 and bits_per_pixel in (24, 32) and pixels_fit
        )
        details = {
            "declared_size": declared_size,
            "pixel_offset": pixel_offset,
            "dib_size": dib_size,
            "width": width,
            "height": height,
            "planes": planes,
            "bits_per_pixel": bits_per_pixel,
            "pixels_fit": pixels_fit,
            "pixel_hash": f"{pixel_hash:016x}" if pixels_fit else None,
        }
    return {
        "path": str(path.resolve()),
        "size": len(data),
        "sha256": _sha256_bytes(data),
        "expected_width": expected_width,
        "expected_height": expected_height,
        "decode_ok": ok,
        **details,
    }


def _audit_final_artifacts(run_directory: Path) -> dict[str, Any]:
    manifest_path = run_directory / "aridity_artifacts_manifest.csv"
    if not manifest_path.is_file():
        return {"ok": False, "reason": "missing artifact manifest"}
    with manifest_path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        if tuple(reader.fieldnames or ()) != ARTIFACT_MANIFEST_COLUMNS:
            return {
                "ok": False,
                "reason": "artifact manifest schema mismatch",
                "columns": reader.fieldnames,
            }
        rows = list(reader)
    expected = [
        (2026082201, map_index, chr(ord("A") + case_index))
        for map_index, _, _, _ in MAPS
        for case_index in range(len(CASES))
    ]
    actual = [
        (int(row["seed"]), int(row["map_size"]), row["case_id"])
        for row in rows
    ]
    failures: list[dict[str, Any]] = []
    images: list[dict[str, Any]] = []
    seen: set[Path] = set()
    if actual != expected:
        failures.append({"type": "manifest_shape_or_order", "actual": actual})
    map_by_index = {item[0]: item for item in MAPS}
    root = run_directory.resolve()
    for row in rows:
        map_index = int(row["map_size"])
        if map_index not in map_by_index:
            failures.append({"type": "invalid_map_size", "row": row})
            continue
        _, map_name, width, height = map_by_index[map_index]
        if row["map_name"] != map_name or int(row["width"]) != width or (
            int(row["height"]) != height
        ):
            failures.append({"type": "map_identity", "row": row})
        for mode in ("geography", "climate"):
            path = (run_directory / row[f"{mode}_file"]).resolve()
            try:
                path.relative_to(root)
            except ValueError:
                failures.append({"type": "path_escape", "path": str(path)})
                continue
            if path in seen:
                failures.append({"type": "duplicate_image", "path": str(path)})
            seen.add(path)
            if not path.is_file():
                failures.append({"type": "missing_image", "path": str(path)})
                continue
            audit = _audit_bmp(path, width, height)
            audit["mode"] = mode
            audit["seed"] = int(row["seed"])
            audit["map_size"] = map_index
            audit["configuration"] = row["case_id"]
            audit["expected_pixel_hash"] = row[f"{mode}_pixel_hash"].lower()
            images.append(audit)
            if not audit["decode_ok"]:
                failures.append({"type": "bmp_decode", "path": str(path)})
            elif audit["pixel_hash"] != audit["expected_pixel_hash"]:
                failures.append({
                    "type": "pixel_hash_mismatch",
                    "path": str(path),
                    "expected": audit["expected_pixel_hash"],
                    "actual": audit["pixel_hash"],
                })
        for field in (
            "physical_hash", "geography_pixel_hash", "climate_pixel_hash",
        ):
            if int(row[field], 16) == 0:
                failures.append({"type": "zero_hash", "field": field, "row": row})
    return {
        "ok": len(rows) == 24 and len(images) == 48 and not failures,
        "manifest": str(manifest_path.resolve()),
        "manifest_sha256": _sha256_file(manifest_path),
        "row_count": len(rows),
        "image_count": len(images),
        "images": images,
        "failures": failures,
    }


def _stage_verify_production(args: argparse.Namespace, output: Path) -> None:
    exe = _require_executable(args.exe)
    selected_path = args.selected_record.resolve()
    selected = _load_selected_record(selected_path, args.selection_sha256)
    selection_output = args.selection_output.resolve()
    holdout_output = args.holdout_output.resolve()
    selection_copy = selection_output / "selected_candidate.json"
    expected_source_selection_sha = selected.get(
        "source_selection_record_sha256", _sha256_file(selected_path)
    )
    if (
        not selection_copy.is_file()
        or _sha256_file(selection_copy) != expected_source_selection_sha
    ):
        raise CalibrationError(
            "selection output does not contain the bound source selection record"
        )
    holdout_record_path = holdout_output / "holdout_result.json"
    if not holdout_record_path.is_file():
        raise CalibrationError("holdout output lacks holdout_result.json")
    holdout_record = json.loads(holdout_record_path.read_text(encoding="utf-8"))
    if (
        not holdout_record.get("ok")
        or holdout_record.get("candidate_id") != selected["candidate_id"]
        or holdout_record.get("selected_record_sha256") != _sha256_file(selected_path)
    ):
        raise CalibrationError("holdout output is not a PASS for the frozen candidate")
    candidate_id = selected["candidate_id"]
    selected_coefficients = (
        int(selected["drought_divisor"]), int(selected["desert_base"]),
        int(selected["desert_bias_span"]), int(selected["semi_arid_width"]),
        int(selected["oasis_transition_margin"]),
    )
    if candidate_id != _candidate_id(*selected_coefficients[:4]):
        raise CalibrationError("selected candidate ID disagrees with its coefficients")
    run = _run_probe(
        exe,
        output / "f",
        "final",
        "production",
        FINAL_SEEDS,
        require_success=False,
    )
    if run["rows"] is None:
        raise CalibrationError("final production probe produced no parseable CSV")
    final_rows = run["rows"]
    _validate_coefficients(final_rows, selected_coefficients)
    screening_eval = _evaluate(
        [row for row in final_rows if int(row["seed"]) in SCREENING_SEEDS],
        SCREENING_SEEDS,
        enforce_means=False,
    )
    calibration_eval = _evaluate(
        [row for row in final_rows if int(row["seed"]) in CALIBRATION_SEEDS],
        CALIBRATION_SEEDS,
        enforce_means=True,
    )
    holdout_eval = _evaluate(
        [row for row in final_rows if int(row["seed"]) in HOLDOUT_SEEDS],
        HOLDOUT_SEEDS,
        enforce_means=True,
    )
    calibration_oasis = _oasis_group_evaluation(
        [row for row in final_rows if int(row["seed"]) in CALIBRATION_SEEDS],
        CALIBRATION_SEEDS, selected_coefficients[4], projected=False,
    )
    holdout_oasis = _oasis_group_evaluation(
        [row for row in final_rows if int(row["seed"]) in HOLDOUT_SEEDS],
        HOLDOUT_SEEDS, selected_coefficients[4], projected=False,
    )
    confirmation_csv, confirmation_rows = _load_stage_rows(
        selection_output / "x" / candidate_id
    )
    screening_csv, screening_rows = _load_stage_rows(
        selection_output / "s" / candidate_id
    )
    holdout_csv, holdout_rows = _load_stage_rows(holdout_output / "h" / candidate_id)
    final_screening_rows = [
        row for row in final_rows if int(row["seed"]) in SCREENING_SEEDS
    ]
    final_calibration_rows = [
        row for row in final_rows if int(row["seed"]) in CALIBRATION_SEEDS
    ]
    final_holdout_rows = [
        row for row in final_rows if int(row["seed"]) in HOLDOUT_SEEDS
    ]
    screening_comparison = _compare_screening_projection(
        final_screening_rows, screening_rows, selected_coefficients[4],
    )
    calibration_comparison = _compare_rows(
        final_calibration_rows, confirmation_rows,
    )
    holdout_comparison = _compare_rows(final_holdout_rows, holdout_rows)
    source_hashes_ok = (
        _sha256_file(screening_csv) == selected["screening_csv_sha256"]
        and
        _sha256_file(confirmation_csv) ==
        selected["calibration_confirmation_csv_sha256"]
        and _sha256_file(holdout_csv) == holdout_record["csv_sha256"]
    )
    artifact_audit = _audit_final_artifacts(output / "f")
    through_holdout = int(
        holdout_record.get("world_counts", {}).get("through_holdout", -1)
    )
    through_final = through_holdout + int(run["row_count"])
    world_count_ok = (
        through_holdout >= 0
        and int(run["row_count"]) == 216
        and through_final <= MAX_SUCCESSFUL_PATH_WORLDS
    )
    result = {
        "ok": run["exit_code"] == 0
        and screening_eval["ok"]
        and calibration_eval["ok"]
        and holdout_eval["ok"]
        and calibration_oasis["ok"]
        and holdout_oasis["ok"]
        and screening_comparison["ok"]
        and calibration_comparison["ok"]
        and holdout_comparison["ok"]
        and source_hashes_ok
        and world_count_ok
        and artifact_audit["ok"],
        "status": "FINAL_PRODUCTION_IDENTITY_PASS",
        "selected_record": str(selected_path),
        "selected_record_sha256": _sha256_file(selected_path),
        "candidate_id": candidate_id,
        "drought_divisor": selected["drought_divisor"],
        "desert_base": selected["desert_base"],
        "desert_bias_span": selected["desert_bias_span"],
        "semi_arid_width": selected["semi_arid_width"],
        "oasis_transition_margin": selected["oasis_transition_margin"],
        "final_csv": run["csv_path"],
        "final_csv_sha256": run["csv_sha256"],
        "final_row_count": run["row_count"],
        "elapsed_seconds": run["elapsed_seconds"],
        "maximum_successful_path_worlds": MAX_SUCCESSFUL_PATH_WORLDS,
        "world_counts": {
            "through_holdout": through_holdout,
            "final": int(run["row_count"]),
            "through_final": through_final,
            "within_maximum": world_count_ok,
        },
        "override_source_hashes_ok": source_hashes_ok,
        "screening_evaluation": screening_eval,
        "calibration_evaluation": calibration_eval,
        "holdout_evaluation": holdout_eval,
        "calibration_oasis_statistical_evaluation": calibration_oasis,
        "holdout_oasis_statistical_evaluation": holdout_oasis,
        "screening_row_comparison": screening_comparison,
        "calibration_row_comparison": calibration_comparison,
        "holdout_row_comparison": holdout_comparison,
        "individual_zero_oasis_worlds": _zero_oasis_worlds(final_rows),
        "artifact_audit": artifact_audit,
    }
    if not result["ok"]:
        result["status"] = "FINAL_PRODUCTION_IDENTITY_FAIL"
    _write_json_new(output / "production_verification.json", result)
    _write_json_new(
        output / "row_comparison.json",
        {
            "screening": screening_comparison,
            "calibration": calibration_comparison,
            "holdout": holdout_comparison,
        },
    )
    _write_json_new(output / "final_artifact_audit.json", artifact_audit)
    _write_statistics_csv(
        output / "production_calibration_per_size_configuration.csv",
        calibration_eval["statistics"],
    )
    _write_statistics_csv(
        output / "production_holdout_per_size_configuration.csv",
        holdout_eval["statistics"],
    )
    if not result["ok"]:
        raise CalibrationError("final production differs from the frozen selected override evidence")


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="stage", required=True)
    for stage in (
        "ack-audit", "qualify", "pilot", "select", "bind-selection",
        "holdout", "verify-production",
    ):
        child = subparsers.add_parser(stage)
        child.add_argument("--exe", required=True, type=Path)
        child.add_argument("--output", required=True, type=Path)
        if stage in ("qualify", "select"):
            child.add_argument("--historical-csv", required=True, type=Path)
        if stage == "select":
            child.add_argument("--pilot-result", required=True, type=Path)
            child.add_argument("--pilot-sha256", required=True)
        if stage == "bind-selection":
            child.add_argument("--selection-output", required=True, type=Path)
            child.add_argument("--selection-record", required=True, type=Path)
            child.add_argument("--selection-sha256", required=True)
            child.add_argument("--qualification-result", required=True, type=Path)
            child.add_argument("--qualification-sha256", required=True)
            child.add_argument("--ack-result", required=True, type=Path)
            child.add_argument("--ack-sha256", required=True)
        if stage in ("holdout", "verify-production"):
            child.add_argument("--selected-record", required=True, type=Path)
            child.add_argument("--selection-sha256", required=True)
        if stage == "verify-production":
            child.add_argument("--selection-output", required=True, type=Path)
            child.add_argument("--holdout-output", required=True, type=Path)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    output = args.output.resolve()
    if output.exists():
        print(f"refusing to overwrite output directory: {output}", file=sys.stderr)
        return 2
    output.mkdir(parents=True)
    handlers = {
        "ack-audit": _stage_ack_audit,
        "qualify": _stage_qualify,
        "pilot": _stage_pilot,
        "select": _stage_select,
        "bind-selection": _stage_bind_selection,
        "holdout": _stage_holdout,
        "verify-production": _stage_verify_production,
    }
    try:
        handlers[args.stage](args, output)
        _write_json_new(output / "stage_result.json", {"stage": args.stage, "ok": True})
        _artifact_manifest(output, output / "artifact_manifest.json")
        return 0
    except Exception as exception:
        failure = {
            "stage": args.stage,
            "ok": False,
            "exception_type": type(exception).__name__,
            "message": str(exception),
        }
        try:
            _write_json_new(output / "failure.json", failure)
            _artifact_manifest(output, output / "artifact_manifest.json")
        except Exception as evidence_exception:
            print(f"failed to preserve failure evidence: {evidence_exception}", file=sys.stderr)
        print(f"{type(exception).__name__}: {exception}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
