#!/usr/bin/env python3
"""Sequential, fixture-free remaster correctness gate (Python standard library)."""

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
import time


ROOT = Path(__file__).resolve().parents[2]
PROFILE = "remaster/profiles/ALttP/alttp-profile.toml"
# Explicit curation, not discovery: new probes/capture-dependent tests must not
# silently become part of the gate. All assertions remain enabled, including O2.
QUICK = (
    ("dungeon_transition_attributes", "Read-only pre-scroll complete collision, stairs, objects and doors"),
    ("dungeon_departure_geometry", "EndFrame outgoing snapshot, both BGs, local ramps, retirement/reset"),
    ("dungeon_transition_cache", "EndFrame partial construction, source retention, independent planes"),
    ("dungeon_geometry_cache", "EndFrame animation reuse versus full solve/maps/serialized packet"),
    ("dungeon_entry_lifecycle", "EndFrame signed entry continuity; ROM/profile/defrost and warp rejection"),
    ("dungeon_scroll_ownership", "Cardinal scrolling, 16-bit wrap, uploaded producer and flip rejection"),
    ("dungeon_background_ownership", "Independent BG/plane seams, authored transforms and effective offsets"),
    ("dungeon_mixed_floor", "Effective mixed-foot heights, H/V flips, slopes and ambiguous support"),
    ("dungeon_disconnected_plane", "Unobserved overlapping planes stay unresolved; BG character bases"),
    ("offscreen_continuity", "Exact signed sources across camera edges, HUD recovery and hidden clusters"),
    ("held_lamp", "Lamp anchor/state and actual HUD ink versus transparent/world pixels"),
    ("sprite_obstacle", "Progressive OBJ layers, support isolation, sparse interior-contact rejection"),
    ("sprite_obstacle_clipping", "Exact actor/latched-OAM clipped children, signed bases and ownership rejection"),
    ("scene_subscreen_wall", "Physical authored subscreen floor/wall/stair receivers, transforms, priority and mask rejection"),
    ("obstacle_group_editor", "Repeated OBJ occurrences, reviewed footprint completion and H/V/Z preservation"),
    ("paused_obstacle", "EndFrame paused handoff, retained support, effective pixels and exact bulk Z refresh"),
)
FULL_EXTRA = (
    ("npc_transition_floor", "Proven transition-marker floor support without movement ownership, transforms and unsafe-source rejection"),
    ("npc_floor_support", "Follower producer-body contacts, delayed gameplay plane and clipped/contradictory support rejection"),
    ("dungeon_doorway_column", "Surviving directional doorway columns, transforms, per-plane support and cache invalidation"),
    ("dungeon_doorway_propagation", "Doorway frame bands, non-support members, flips, split-BG relations and ownership rejection"),
    ("doorway_metadata", "Doorway/Door versioned migration, directional support metadata and capture roundtrip"),
    ("dungeon_door_foot", "Explicit doorway support directions, flips, signed bases and conflicting assembly contacts"),
    ("dungeon_stair_assembly_support", "Complete stair ownership, signed landing datums, transforms and ambiguous-source/plane rejection"),
    ("dungeon_authored_stair", "Owned stair producer offsets preserve authored arrays; ordinary unowned stairs retain zero-offset policy"),
    ("dungeon_corner_cap_region", "Ordered high-corner cap/face contacts, transforms, authored preservation and ownership rejection"),
    ("layer_view", "Background inspection masks, subscreen-only exposure, OBJ exclusion and frame-boundary latching"),
    ("context_location_publication", "EndFrame preserves selectable BG locations before contexts exist and across capture roundtrip"),
    ("dungeon_wall_obstacle_seam", "Exact authored wall tangent seams to supported assemblies, flips, overwrite and ambiguous/underpass rejection"),
    ("dungeon_wall_floor_contact", "Direct authored wall feet on occupied Floor, transformed relief and independent overlap/ownership rejection"),
    ("dungeon_wall_stack", "Complete ROM wall-stack ownership, sculpted tangent edges, special bands, transformations and ambiguity/plane conflicts"),
    ("tile_type_overlay", "Authored metadata colors, unknown tiles, OBJ/HUD ownership and GI view cycle"),
    ("dungeon_oriented_obstacle", "Horizontal footprint support direction and mirrored/V-symmetric ownership"),
    ("link_stair_height", "Continuous Link assembly stair support, both orientations/planes and ambiguous landing rejection"),
    ("dungeon_floor_continuity", "Cardinal same-BG floor relief bases, narrow strips, barriers and plane/level conflicts"),
    ("door_opening_height", "Doorframe versus connected landing bases, signed heights, flips and plane ownership"),
    ("dungeon_frame_recognition", "Complete ordinary/stair frame ownership, all directions/flips/split BGs, opening independence and ambiguity rejection"),
    ("dungeon_placement_reuse", "EndFrame relation transport and placement-cache invalidation"),
    ("dungeon_semantic_cache", "EndFrame semantic activation, facing, context and door-mask invalidation"),
    ("dungeon_terrain_lifecycle", "Pickup/carried/thrown/landed support, OAM ownership and ambiguity"),
    ("dungeon_persistent_support_rejection", "Dual-owner body rejection without component/constraint corruption"),
    ("emitter_patches", "Production visible source clustering, area, transforms and animation links"),
    ("sphere_samples", "Production distance-based sphere sample budgets and invalid-input defaults"),
)
BASE_FLAGS = ["-std=c++17", "-Wall", "-Wextra", "-pedantic", "-O0", "-g", "-UNDEBUG"]
SANITIZER_FLAGS = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                   "-fno-sanitize-recover=all"]
# These activate optional external fixtures or shader variants in existing tests.
# Pin the synthetic/default variant instead of allowing shell state to change scope.
OPTIONAL_ENV = ("ALTTP_TERRAIN_TEST_EVIDENCE", "S9X_HUD_WRITE_REPLAY",
                "S9X_REMASTER_TEST_ADAPTIVE_DEPTH", "S9X_REMASTER_TEST_BATCHED",
                "S9X_REMASTER_EDGE_SWEEP", "S9X_REMASTER_EDGE_CLUSTERED",
                "S9X_REMASTER_EDGE_ASSERT_CONTINUITY")


def test_specs(args):
    specs = [dict(name=n, source=f"remaster/tests/{n}_test.cpp", description=d,
                  flags=[], arguments=[], kind="portable") for n, d in QUICK]
    if args.suite == "full":
        specs += [dict(name=n, source=f"remaster/tests/{n}_test.cpp", description=d,
                       flags=["-I."] if n == "tile_type_overlay" else [], arguments=[], kind="portable") for n, d in FULL_EXTRA]
        for mode, flags in (("debug", ["-pthread"]),
                            ("optimized", ["-pthread", "-O2"])):
            specs.append(dict(name=f"dungeon_small_stack_{mode}",
                              source="remaster/tests/dungeon_small_stack_test.cpp",
                              description="Solver/apply/EndFrame on 512/544 KiB pthread stacks",
                              flags=flags, arguments=[PROFILE], kind="portable"))
    if args.legacy_smoke:
        specs.append(dict(name="legacy_remaster_smoke", source="remaster/tests/remaster_test.cpp",
                          description="Opt-in stale monolithic fixture; failure is a real gate failure",
                          flags=[], arguments=[], kind="legacy"))
    if args.metal:
        specs.append(dict(name="metal_lighting", source="macosx/remaster-lighting-test.mm",
                          description="Production shaders, synthetic GPU transport/visibility comparisons",
                          flags=["-fobjc-arc", "-framework", "Foundation", "-framework", "Metal"],
                          arguments=[], kind="metal"))
    return specs


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_snapshot(spec):
    # Conservative dependency superset: local remaster headers are header-based
    # production code. Include untracked/dirty headers, not just git's HEAD.
    paths = set(ROOT.joinpath("remaster").rglob("*.h"))
    paths.update((ROOT / spec["source"], Path(__file__).resolve()))
    if spec["arguments"]:
        paths.add(ROOT / PROFILE)
    if spec["kind"] == "metal":
        paths.add(ROOT / "macosx/shaders.metal")
        paths.add(ROOT / "macosx/remaster-debug-pixels-test.h")
    files = {str(p.relative_to(ROOT)): sha256(p) for p in sorted(paths)}
    digest = hashlib.sha256(json.dumps(files, sort_keys=True).encode()).hexdigest()
    return {"sha256": digest, "files": files,
            "scope": "test + runner + all remaster/**/*.h + explicit runtime inputs"}


def command_to_log(command, log_path, timeout, env):
    start = time.monotonic()
    result = {"command": command, "cwd": str(ROOT), "log": str(log_path),
              "returncode": None, "status": "error"}
    with log_path.open("w", encoding="utf-8") as log:
        log.write("$ " + shlex.join(command) + "\n")
        log.flush()
        try:
            process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=log,
                                       stderr=subprocess.STDOUT, start_new_session=True)
            try:
                result["returncode"] = process.wait(timeout=timeout)
                result["status"] = "passed" if process.returncode == 0 else "failed"
            except (subprocess.TimeoutExpired, KeyboardInterrupt) as exc:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                result["returncode"] = process.returncode
                result["status"] = "timeout" if isinstance(exc, subprocess.TimeoutExpired) else "interrupted"
                log.write(f"\nRunner: {result['status']} (limit {timeout}s)\n")
        except OSError as exc:
            result["error"] = str(exc)
            log.write(f"\nRunner: {exc}\n")
    result["seconds"] = round(time.monotonic() - start, 6)
    return result


def git_text(*arguments):
    try:
        p = subprocess.run(["git", *arguments], cwd=ROOT, capture_output=True,
                           text=True, timeout=10)
        return p.stdout.strip() if p.returncode == 0 else None
    except (OSError, subprocess.TimeoutExpired):
        return None


def write_report(path, report):
    temporary = path.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", choices=("quick", "full"), default="quick")
    parser.add_argument("--metal", action="store_true", help="Append macOS Metal GPU regressions; unavailable is failure")
    parser.add_argument("--sanitize", action="store_true", help="Compile with ASan + UBSan, fatal on findings")
    parser.add_argument("--output", type=Path, default=ROOT / "build/remaster-tests",
                        help="Result parent directory (a unique run subdirectory is created)")
    parser.add_argument("--list", action="store_true", help="List selected tests without compiling or writing results")
    parser.add_argument("--legacy-smoke", action="store_true", help="Also run known-stale remaster_test (not expected-pass)")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "c++"), help="Compiler command (default CXX or c++)")
    parser.add_argument("--timeout", type=int, default=180, help="Seconds per compile or test process (default 180)")
    args = parser.parse_args(argv)
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    compiler = shlex.split(args.cxx)
    if not compiler:
        parser.error("--cxx must be a nonempty compiler command")
    specs = test_specs(args)
    if args.list:
        for spec in specs:
            print(f"{spec['name']:38} {spec['source']}\n  {spec['description']}")
        print(f"{len(specs)} selected; no tests executed")
        return 0

    start = time.monotonic()
    args.output.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix=datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ-"),
                               dir=args.output.resolve()))
    report_path = run / "results.json"
    env = os.environ.copy()
    removed = {key: env.pop(key) for key in OPTIONAL_ENV if key in env}
    if args.sanitize:
        for key in ("ASAN_OPTIONS", "UBSAN_OPTIONS"):
            env[key] = env.get(key, "") + ":halt_on_error=1:abort_on_error=1"
    report = {"schema_version": 1, "status": "running", "suite": args.suite,
              "metal": args.metal, "sanitize": args.sanitize, "legacy_smoke": args.legacy_smoke,
              "started_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "root": str(ROOT), "result_directory": str(run),
              "platform": platform.platform(), "machine": platform.machine(),
              "python": sys.version, "git_head": git_text("rev-parse", "HEAD"),
              "git_status_before": git_text("status", "--short"),
              "environment": {"removed_optional_test_variables": removed,
                              "sanitizer_options": {k: env[k] for k in ("ASAN_OPTIONS", "UBSAN_OPTIONS") if k in env}},
              "selected": [s["name"] for s in specs], "tests": [], "errors": [],
              "limitations": ["Synthetic inputs, not a captured transition corpus",
                              "Not complete-room validation or room-height approval",
                              "Wall-clock timings are diagnostics, not benchmark thresholds"]}
    version = command_to_log(compiler + ["--version"], run / "compiler.log", args.timeout, env)
    resolved = shutil.which(compiler[0])
    version_text = (run / "compiler.log").read_text()
    report["compiler"] = {**version, "resolved_executable": resolved,
                          "version_text": version_text,
                          "version_sha256": hashlib.sha256(version_text.encode()).hexdigest(),
                          "executable_sha256": sha256(Path(resolved)) if resolved else None}
    if version["status"] != "passed":
        report["errors"].append("Compiler fingerprint command failed; see compiler.log")
    write_report(report_path, report)
    initial_inputs = {}
    for spec in specs:
        try:
            initial_inputs[spec["name"]] = source_snapshot(spec)
        except OSError as exc:
            report["errors"].append(f"{spec['name']}: cannot fingerprint initial inputs: {exc}")
    for spec in specs:
        print(f"[{len(report['tests']) + 1}/{len(specs)}] {spec['name']}", flush=True)
        directory = run / spec["name"]
        directory.mkdir()
        result = {**spec, "status": "error"}
        try:
            result["source_fingerprint"] = source_snapshot(spec)
            if result["source_fingerprint"] != initial_inputs.get(spec["name"]):
                raise RuntimeError("Sources changed since suite start or initial fingerprint unavailable; rerun")
            if spec["kind"] == "metal" and sys.platform != "darwin":
                raise RuntimeError("--metal requires macOS, Foundation/Metal SDK and a Metal device; not skipped")
            binary = directory / "test"
            flags = BASE_FLAGS + spec["flags"] + (SANITIZER_FLAGS if args.sanitize else [])
            compile_command = compiler + flags + [spec["source"], "-o", str(binary)]
            result["compile"] = command_to_log(compile_command, directory / "compile.log", args.timeout, env)
            result["status"] = "compile_failed"
            if result["compile"]["status"] == "passed":
                result["binary_sha256"] = sha256(binary)
                result["run"] = command_to_log([str(binary), *spec["arguments"]], directory / "run.log", args.timeout, env)
                result["status"] = "passed" if result["run"]["status"] == "passed" else "run_failed"
            after = source_snapshot(spec)
            if after != result["source_fingerprint"]:
                result["source_fingerprint_after"] = after
                result["status"] = "inputs_changed"
                result["error"] = "Sources changed during compile/run; rerun on stable inputs"
        except (OSError, RuntimeError) as exc:
            result["error"] = str(exc)
            (directory / "runner.log").write_text(str(exc) + "\n")
        report["tests"].append(result)
        write_report(report_path, report)
        print(f"  {result['status']}", flush=True)
        if any(result.get(phase, {}).get("status") == "interrupted" for phase in ("compile", "run")):
            report["errors"].append("Interrupted; remaining selected tests were not executed")
            break
    report["counts"] = {"selected": len(specs), "executed": len(report["tests"]),
                        "passed": sum(t["status"] == "passed" for t in report["tests"])}
    report["counts"]["failed"] = len(report["tests"]) - report["counts"]["passed"]
    report["counts"]["not_executed"] = len(specs) - len(report["tests"])
    for spec in specs:
        try:
            if source_snapshot(spec) != initial_inputs.get(spec["name"]):
                report["errors"].append(f"{spec['name']}: inputs changed during suite; results are not a stable-source gate")
        except OSError as exc:
            report["errors"].append(f"{spec['name']}: final fingerprint failed: {exc}")
    report["status"] = "passed" if (not report["errors"] and report["counts"]["passed"] == len(specs)) else "failed"
    report["seconds"] = round(time.monotonic() - start, 6)
    report["git_status_after"] = git_text("status", "--short")
    write_report(report_path, report)
    print(f"{report['status']}: {report['counts']['passed']}/{len(specs)} tests, {report['seconds']:.2f}s\n{report_path}")
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    sys.exit(main())
