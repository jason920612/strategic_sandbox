#!/usr/bin/env python3
"""Random-player intervention smoke for Project Leviathan.

What this does
==============
1. Runs a long-horizon simulation (default: 70 years on
   data/scenarios/1930_rfc_compliance.json) with a chosen player
   country and a deterministic seed -- the "observation" stage.
2. Reads the resulting save.json, extracts any
   `pending_player_events` the simulation deferred for the player
   country during the day-loop, and randomly picks ONE pending
   event (= randomly intervening "on the day that event fired").
3. Randomly picks one of the event's options.
4. Writes a `commands.json` script containing a single
   `ChooseEventOption` entry and re-runs the *same scenario, same
   seed, same player* with `--commands commands.json` -- the
   "intervention" stage. This flows through the production
   `commands::dispatch_one` pipeline, the production
   `event_effects::apply_option_effects_with_mode` applicator,
   and the normal save / annual stats / events.jsonl emit path.
5. Asserts both stages report `Sanity issues : 0` and writes a
   short report summarising key numeric deltas between the two
   stages (stability / legitimacy / gdp / corruption per year).

Why this exists
===============
The user's request (2026-05-19): "GitHub Action that runs a
70-year sim, randomly intervenes on some day with a player
option, and observes the numbers -- through the normal gameplay
path." This script is the cross-platform orchestrator the action
calls.

The "normal gameplay path" constraint:
- `ChooseEventOption` is the production command kind the M2.20
  / issue-#112 surface uses.
- `--commands PATH` is the production CLI flag that funnels JSON
  commands through `commands::dispatch_one`.
- Resolution of a `PendingPlayerEvent` via this path triggers
  the same `event_effects::apply_option_effects_with_mode` call
  that an AI country would make at the moment its options-event
  fired.
- No mocks, no test-only code paths.

Failure modes
=============
- Build/scenario invalid -> script exits non-zero with the
  binary's stderr.
- Either stage reports non-zero `Sanity issues` -> script exits
  non-zero.
- `pending_player_events` empty -> script exits SUCCESS with a
  "no intervention available" note. (Some player choices are
  rare; an empty pending list is not a regression.)
- ChooseEventOption rejected by the order_execution gate ->
  script exits non-zero. (We expect player-options events to be
  resolvable by the player country itself.)
"""

from __future__ import annotations

import argparse
import csv
import dataclasses
import json
import os
import pathlib
import random
import subprocess
import sys
from typing import Iterable, Optional


# Countries on the compliance scenario with realistic odds of
# triggering the player-options events (legitimacy_crisis needs
# legitimacy < 0.20; corruption_scandal needs IG loyalty < 0.10).
# CZE starts at legitimacy 0.15 so it triggers immediately --
# making it a reliable default for CI smoke. The rest are
# higher-stress countries whose stats drift the right way over
# decades.
STRESS_PRONE_PLAYERS = ["CZE", "CHN", "POL", "SOV", "IND", "ITA"]

# Default scenario + scenario length. 70 years = 25 567 days
# (covers 1930-01-01 through 1999-12-31 plus extra; matches the
# integration test horizon).
DEFAULT_SCENARIO = "data/scenarios/1930_rfc_compliance.json"
DEFAULT_DAYS = 25_567


@dataclasses.dataclass
class StageResult:
    output_dir: pathlib.Path
    stdout: str
    save_path: pathlib.Path
    annual_csv: pathlib.Path
    sanity_issues: int

    @property
    def save(self) -> dict:
        with open(self.save_path, "r", encoding="utf-8") as fh:
            return json.load(fh)


def parse_sanity_issues(stdout: str) -> int:
    """Pull the `Sanity issues : N` line out of the runner's stdout."""
    for line in stdout.splitlines():
        if "Sanity issues" in line and ":" in line:
            try:
                return int(line.split(":", 1)[1].strip())
            except ValueError:
                continue
    # The runner always prints the line on a successful end_tick;
    # absence means the run failed before end_tick.
    return -1


def run_stage(
    binary: pathlib.Path,
    scenario: pathlib.Path,
    seed: int,
    days: int,
    player: str,
    output_dir: pathlib.Path,
    commands: Optional[pathlib.Path] = None,
) -> StageResult:
    args = [
        str(binary),
        "--scenario", str(scenario),
        "--player", player,
        "--seed", str(seed),
        "--days", str(days),
        "--output", str(output_dir),
    ]
    if commands is not None:
        args.extend(["--commands", str(commands)])

    print(f"[stage] $ {' '.join(args)}", flush=True)
    proc = subprocess.run(
        args,
        capture_output=True,
        text=True,
        check=False,
    )
    stdout = proc.stdout + ("\n" + proc.stderr if proc.stderr else "")
    if proc.returncode != 0:
        sys.stderr.write(stdout)
        raise SystemExit(
            f"[stage] runner failed with exit code {proc.returncode} "
            f"(output_dir={output_dir})"
        )
    sanity = parse_sanity_issues(stdout)
    if sanity != 0:
        sys.stderr.write(stdout)
        raise SystemExit(
            f"[stage] runner reported Sanity issues = {sanity} "
            f"(output_dir={output_dir})"
        )
    return StageResult(
        output_dir=output_dir,
        stdout=stdout,
        save_path=output_dir / "save.json",
        annual_csv=output_dir / "annual_world_stats.csv",
        sanity_issues=sanity,
    )


def find_event_definition(
    save: dict,
    event_id_code: str,
) -> Optional[dict]:
    """Return the event definition matching `event_id_code` from the save."""
    for ev in save.get("events", []):
        if ev.get("id_code") == event_id_code:
            return ev
    return None


def event_history_date(save: dict, history_index: int) -> str:
    """Return the `fired_on` date for an event_history entry, or '?'."""
    hist = save.get("event_history", [])
    if 0 <= history_index < len(hist):
        return hist[history_index].get("fired_on", "?")
    return "?"


def pick_intervention(
    save: dict,
    rng: random.Random,
) -> Optional[tuple[int, str, str, str]]:
    """Pick a random pending player event + option.

    Returns (event_history_index, event_id_code, option_id_code,
    fired_on_date) or None if there are no pending events.
    """
    pending = save.get("pending_player_events", [])
    if not pending:
        return None

    entry = rng.choice(pending)
    history_index = entry["event_history_index"]
    event_id_code = entry["event_id_code"]
    fired_on = event_history_date(save, history_index)

    definition = find_event_definition(save, event_id_code)
    if definition is None:
        raise SystemExit(
            f"[intervention] pending event '{event_id_code}' has no "
            "matching definition in save.events (corrupt save?)"
        )
    options = definition.get("options", [])
    if not options:
        raise SystemExit(
            f"[intervention] event '{event_id_code}' has no options "
            "but appeared in pending_player_events (corrupt save?)"
        )
    chosen_option = rng.choice(options)
    return (
        history_index,
        event_id_code,
        chosen_option["id_code"],
        fired_on,
    )


def write_commands_script(
    path: pathlib.Path,
    history_index: int,
    option_id_code: str,
) -> None:
    payload = {
        "commands": [
            {
                "kind": "ChooseEventOption",
                "event_history_index": history_index,
                "option_id_code": option_id_code,
            }
        ]
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(payload, fh, indent=2)


def read_annual_csv(path: pathlib.Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with open(path, "r", newline="", encoding="utf-8") as fh:
        reader = csv.DictReader(fh)
        return list(reader)


def summarise_trajectory(
    rows: list[dict[str, str]],
    label: str,
    key_columns: Iterable[str] = (
        "year",
        "avg_stability",
        "avg_legitimacy",
        "avg_corruption",
        "avg_gdp",
        "total_gdp",
        "event_history_count",
    ),
) -> str:
    if not rows:
        return f"  [{label}] annual_world_stats.csv missing or empty\n"
    header = rows[0]
    available = [c for c in key_columns if c in header]
    out_lines = [f"  [{label}] {len(rows)} year-rows, columns={available}"]
    if rows:
        first = rows[0]
        mid = rows[len(rows) // 2]
        last = rows[-1]
        for tag, row in (("first", first), ("mid", mid), ("last", last)):
            parts = []
            for col in available:
                parts.append(f"{col}={row.get(col, '?')}")
            out_lines.append(f"    {tag}: " + " ".join(parts))
    return "\n".join(out_lines) + "\n"


def write_report(
    report_path: pathlib.Path,
    *,
    scenario: pathlib.Path,
    player: str,
    seed: int,
    days: int,
    stage1: StageResult,
    stage2: Optional[StageResult],
    intervention: Optional[tuple[int, str, str, str]],
) -> None:
    report_path.parent.mkdir(parents=True, exist_ok=True)
    lines = []
    lines.append("# Random-player intervention smoke report\n")
    lines.append("## Configuration\n")
    lines.append(f"- Scenario : `{scenario}`")
    lines.append(f"- Player   : `{player}`")
    lines.append(f"- Seed     : `{seed}`")
    lines.append(f"- Days     : `{days}` (~{days // 365} years)")
    lines.append("")
    lines.append("## Stage 1 (observation, no intervention)\n")
    lines.append(f"- Sanity issues: `{stage1.sanity_issues}`")
    lines.append(f"- save.json    : `{stage1.save_path}`")
    lines.append(f"- annual stats : `{stage1.annual_csv}`")
    stage1_rows = read_annual_csv(stage1.annual_csv)
    lines.append("```")
    lines.append(summarise_trajectory(stage1_rows, "stage1").rstrip())
    lines.append("```")
    lines.append("")

    if intervention is None:
        lines.append("## Intervention\n")
        lines.append(
            "No `pending_player_events` were produced for this "
            "player + seed combination over the simulated horizon. "
            "Stage 2 was skipped. This is a legitimate outcome (the "
            "player country never had its legitimacy / IG loyalty "
            "drop into the player-options event range).\n"
        )
    else:
        hist_idx, event_id, option_id, fired_on = intervention
        lines.append("## Intervention (Stage 2, with --commands)\n")
        lines.append(f"- Picked event_history_index : `{hist_idx}`")
        lines.append(f"- Event id_code              : `{event_id}`")
        lines.append(f"- Event fired on             : `{fired_on}`")
        lines.append(f"- Option id_code             : `{option_id}`")
        if stage2 is not None:
            lines.append(f"- Sanity issues              : `{stage2.sanity_issues}`")
            stage2_rows = read_annual_csv(stage2.annual_csv)
            lines.append("```")
            lines.append(summarise_trajectory(stage2_rows, "stage2").rstrip())
            lines.append("```")
        lines.append("")

    lines.append("## Path verification\n")
    lines.append(
        "The intervention runs through the production command path:\n"
        "  - CLI flag `--commands` parsed in `runner.cpp::parse_args`.\n"
        "  - JSON script applied via "
        "`commands::apply_command_script` -> `commands::dispatch_one` -> "
        "`order_execution::evaluate` -> "
        "`event_effects::apply_option_effects_with_mode`.\n"
        "  - No test-only or fake code paths are exercised.\n"
    )

    with open(report_path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines).rstrip() + "\n")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--binary",
        type=pathlib.Path,
        required=True,
        help="Path to the leviathan executable.",
    )
    parser.add_argument(
        "--scenario",
        type=pathlib.Path,
        default=pathlib.Path(DEFAULT_SCENARIO),
        help=(
            "Path to the scenario JSON. Defaults to the 20-country "
            "compliance scenario."
        ),
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=None,
        help=(
            "Deterministic seed. If omitted, a random seed in "
            "[1, 2^31) is drawn from os.urandom."
        ),
    )
    parser.add_argument(
        "--days",
        type=int,
        default=DEFAULT_DAYS,
        help=(
            "Number of days to simulate. Default 25 567 "
            "(~70 years; 1930-01-01 through end of 1999)."
        ),
    )
    parser.add_argument(
        "--player",
        type=str,
        default=None,
        help=(
            "Player country id_code. If omitted, one is chosen at "
            "random from a stress-prone pool that's likely to "
            "trigger the player-options events."
        ),
    )
    parser.add_argument(
        "--output-dir",
        type=pathlib.Path,
        default=pathlib.Path("out/random_player_intervention"),
        help="Where to write stage1/, stage2/, commands.json, report.md.",
    )
    args = parser.parse_args(argv)

    if not args.binary.exists():
        print(f"[error] binary not found: {args.binary}", file=sys.stderr)
        return 2
    if not args.scenario.exists():
        print(f"[error] scenario not found: {args.scenario}", file=sys.stderr)
        return 2

    seed = (
        args.seed
        if args.seed is not None
        else int.from_bytes(os.urandom(4), "big") & 0x7FFF_FFFF
    )
    rng = random.Random(seed)

    player = args.player if args.player else rng.choice(STRESS_PRONE_PLAYERS)
    print(f"[setup] player={player} seed={seed} days={args.days}", flush=True)

    args.output_dir.mkdir(parents=True, exist_ok=True)
    stage1_dir = args.output_dir / "stage1"
    stage2_dir = args.output_dir / "stage2"
    commands_path = args.output_dir / "commands.json"
    report_path = args.output_dir / "report.md"

    stage1 = run_stage(
        binary=args.binary,
        scenario=args.scenario,
        seed=seed,
        days=args.days,
        player=player,
        output_dir=stage1_dir,
    )

    stage1_save = stage1.save
    intervention = pick_intervention(stage1_save, rng)

    stage2: Optional[StageResult] = None
    if intervention is None:
        print(
            "[stage1] pending_player_events is empty -- no intervention "
            "available for this player+seed. Skipping stage 2.",
            flush=True,
        )
    else:
        hist_idx, event_id, option_id, fired_on = intervention
        print(
            f"[intervention] picked event_history[{hist_idx}] "
            f"'{event_id}' fired_on={fired_on} -> option '{option_id}'",
            flush=True,
        )
        write_commands_script(commands_path, hist_idx, option_id)
        stage2 = run_stage(
            binary=args.binary,
            scenario=args.scenario,
            seed=seed,
            days=args.days,
            player=player,
            output_dir=stage2_dir,
            commands=commands_path,
        )

        # Verify the choice actually applied: stage2.save.applied_commands
        # must contain a ChooseEventOption entry with the same indices.
        stage2_save = stage2.save
        applied = stage2_save.get("applied_commands", [])
        # save_system serialises each entry as
        # {"applied_on": ..., "command": {"kind": ...}}.
        def _command_of(entry: dict) -> dict:
            cmd = entry.get("command")
            return cmd if isinstance(cmd, dict) else entry
        matched = any(
            _command_of(entry).get("kind") == "ChooseEventOption"
            and _command_of(entry).get("event_history_index") == hist_idx
            and _command_of(entry).get("option_id_code") == option_id
            for entry in applied
        )
        if not matched:
            kinds = sorted({_command_of(e).get("kind", "?") for e in applied})
            raise SystemExit(
                "[verify] ChooseEventOption never reached "
                "state.applied_commands -- the production path did not "
                "execute the intervention. Dumping applied_commands "
                "kinds: " + ", ".join(kinds)
            )
        print(
            f"[verify] applied_commands contains the ChooseEventOption "
            f"entry (history_index={hist_idx}, option_id_code={option_id}).",
            flush=True,
        )

    write_report(
        report_path,
        scenario=args.scenario,
        player=player,
        seed=seed,
        days=args.days,
        stage1=stage1,
        stage2=stage2,
        intervention=intervention,
    )
    print(f"[report] wrote {report_path}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
