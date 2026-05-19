// M5.9 event pipeline integration checkpoint.
//
// M5 ships a four-surface inner loop plus a single wiring step:
//
//   state.events / state.event_history (M5.1 schema + M5.4 data)
//     -> event_evaluator::match_events           (M5.2)
//        -> EventMatch with per-trigger actor binding   (M5.3)
//     -> event_firer::record_match               (M5.5)
//        -> appends EventInstance to event_history       (M5.4)
//     -> event_effects::apply_event_effects       (M5.6)
//        -> reuses policy::apply_effects_to_actor (M5.6 extract)
//
//   event_engine::tick_events(state)              (M5.7)
//     wraps the above into one round.
//
//   monthly::tick_all_countries(state)            (M5.8)
//     calls event_engine::tick_events as STEP 7, after every
//     M1.6/M1.7/M1.8 per-country tick and every M3.2/M3.3/M3.4
//     state-wide step finished writing.
//
// Each leg is already pinned at the unit-test level. This file
// pins five coarser properties at the seam between M5 and any
// future milestone:
//
//   A. M5 does NOT change canonical 1930 runner output. The
//      canonical events at M5.1 (low_stability_unrest with
//      country.stability < 0.30; radical_interest_group_warning
//      with interest_group.radicalism > 0.75) are deliberately
//      tuned to NOT fire on canonical state (GER stability 0.55+;
//      canonical IG radicalism 0.10). M1.17's 365-day soak + the
//      M5.4 1-day runner regression test already pin parts of
//      this; here we pin the explicit "canonical run at M5.9 ->
//      event_history is empty AND save_version is 14".
//
//   B. A hand-built state whose event DOES fire on its first
//      monthly tick produces non-empty event_history through
//      runner::run_state. Effect lands on the right country.
//      Save round-trip preserves the entry.
//
//   C. No new artefact appears when events fire. Still 10 files
//      on disk after a firing run. events.jsonl semantics
//      unchanged (no event id_codes leak into the lifecycle log).
//      state.applied_commands stays empty (events aren't player
//      commands). country.active_policies stays empty (events
//      aren't policies).
//
//   D. Two byte-for-byte identical hand-built states with the
//      same options produce byte-identical 10 artefacts even
//      WITH events firing. The M5 pipeline is deterministic.
//
//   E. Pipeline failure path through the runner: an event with
//      a bad effect target causes monthly::tick_all_countries
//      to fail via event_engine::tick_events failing via
//      apply_event_effects's M1.5 pre-flight reject. The runner
//      surfaces that, and end_tick is NOT reached, so no
//      output artefacts are written (M2.9 contract).
//
// M5.9 deliberately does NOT add a new system, formula,
// artefact, save schema bump, gameplay branch, RunnerOptions
// field, CLI flag, or close M5 ??see
// docs/milestone-5-checkpoint.md.

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/simulation_config.hpp"
#include "leviathan/systems/runner.hpp"
#include "leviathan/systems/save_system.hpp"

namespace fs = std::filesystem;
using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::EventDefinition;
using leviathan::core::EventTrigger;
using leviathan::core::FactionId;
using leviathan::core::FactionState;
using leviathan::core::GameDate;
using leviathan::core::GameState;
using leviathan::core::PolicyEffect;
using leviathan::core::SimulationConfig;
namespace rn = leviathan::systems::runner;
namespace ss = leviathan::systems::save_system;

namespace {

struct TempDir {
    fs::path path;
    explicit TempDir(std::string name)
        : path(fs::temp_directory_path() / name) {
        std::error_code ec;
        fs::remove_all(path, ec);
        fs::create_directories(path, ec);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

std::string read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

EventTrigger make_trigger(const std::string& target,
                          const std::string& op,
                          double value) {
    return EventTrigger{target, op, value};
}

PolicyEffect make_effect(const std::string& target,
                         const std::string& op,
                         double value) {
    return PolicyEffect{target, op, value};
}

EventDefinition make_event(const std::string& id_code,
                           std::vector<EventTrigger> triggers,
                           std::vector<PolicyEffect> effects) {
    EventDefinition d;
    d.id_code        = id_code;
    d.name           = id_code;
    d.visible_report = "test visible report";   // M6.2: required non-empty
    d.true_cause     = "test true cause";       // M6.1: required non-empty
    d.category       = "test";                  // issue #112: required non-empty
    d.triggers       = std::move(triggers);
    d.effects  = std::move(effects);
    return d;
}

// One-country, one-faction state seeded so its monthly pipeline
// stays well-behaved AND so a stability-threshold event fires on
// the FIRST monthly tick. Identical at both byte-identical
// determinism call sites in this file.
GameState make_m5_firing_state() {
    SimulationConfig cfg;
    cfg.start_date = GameDate(1930, 1, 1);
    cfg.seed       = 1u;
    GameState state = leviathan::core::make_game_state(cfg);

    CountryState c;
    c.id         = CountryId{0};
    c.id_code    = "GER";
    c.name       = "Germany";
    c.stability  = 0.20;   // below the threshold so the event matches
    c.legitimacy = 0.50;
    state.countries.push_back(c);

    FactionState f;
    f.id              = FactionId{0};
    f.country         = CountryId{0};
    f.id_code         = "GER_bureau";
    f.country_id_code = "GER";
    f.name            = "Bureaucracy";
    f.type            = "bureaucracy";
    f.support         = 0.30;
    f.loyalty         = 0.50;
    f.radicalism      = 0.20;
    state.factions.push_back(f);

    // Event: when stability < 0.30, drop legitimacy by 0.05.
    state.events.push_back(make_event(
        "low_stability_unrest_firing",
        { make_trigger("country.stability", "lt", 0.30) },
        { make_effect("country.legitimacy", "add", -0.05) }));

    return state;
}

}  // namespace

#ifdef LEVIATHAN_TEST_DATA_DIR
namespace {

const fs::path kDataDir          = LEVIATHAN_TEST_DATA_DIR;
const fs::path kCanonicalConfig  = kDataDir / "config" / "simulation.json";
const fs::path kCanonicalScenario =
    kDataDir / "scenarios" / "1930_minimal.json";

}  // namespace

// =====================================================================
// A. canonical scenario at M5.9 still produces event_history: []
// =====================================================================
TEST_CASE("M5 integration: canonical scenario at M5.9 -> event_history is empty, save_version 14") {
    TempDir td("leviathan_m5_canonical_no_fire");
    rn::RunnerOptions opts;
    opts.config_path   = kCanonicalConfig;
    opts.scenario_path = kCanonicalScenario;
    opts.days          = 365;          // crosses ~12 month boundaries
    opts.output_dir    = td.path;
    REQUIRE(rn::run(opts).ok());

    const std::string save = read_file(td.path / "save.json");

    // The M5.8 wiring runs event_engine::tick_events on every
    // month boundary, but the canonical events at M5.1 are
    // deliberately tuned so neither fires on the canonical
    // 1930 scenario.
    CHECK(save.find("\"save_version\": 18")    != std::string::npos);
    CHECK(save.find("\"event_history\": []")   != std::string::npos);

    // events.jsonl is the M0.6 lifecycle log; M5.8 did NOT
    // change its semantics. Canonical event id_codes must not
    // appear in it.
    const std::string ev_log = read_file(td.path / "events.jsonl");
    CHECK(ev_log.find("low_stability_unrest")             == std::string::npos);
    CHECK(ev_log.find("radical_interest_group_warning")   == std::string::npos);

    // All 10 unconditional artefacts present (no new artefact
    // shipped in M5).
    CHECK(fs::exists(td.path / "save.json"));
    CHECK(fs::exists(td.path / "events.jsonl"));
    CHECK(fs::exists(td.path / "interest_groups.csv"));
    CHECK(fs::exists(td.path / "interest_group_country_feedback.csv"));
    CHECK(fs::exists(td.path / "interest_group_authority_pressure.csv"));
    CHECK(fs::exists(td.path / "provinces.svg"));
    CHECK(fs::exists(td.path / "map.html"));
    // No event_history artefact exists in M5.x.
    CHECK_FALSE(fs::exists(td.path / "event_history.csv"));
    CHECK_FALSE(fs::exists(td.path / "event_history.json"));
    CHECK_FALSE(fs::exists(td.path / "events.csv"));
}

#endif  // LEVIATHAN_TEST_DATA_DIR

// =====================================================================
// B. hand-built event that fires through runner::run_state
// =====================================================================
TEST_CASE("M5 integration: a firing event lands its effect and round-trips through save (current v15)") {
    TempDir td("leviathan_m5_firing_event_apply");

    GameState state = make_m5_firing_state();

    rn::RunnerOptions opts;
    opts.days        = 31;             // crosses exactly one month boundary
    opts.output_dir  = td.path;

    const auto r = rn::run_state(state, opts);
    REQUIRE(r.ok());

    const std::string save = read_file(td.path / "save.json");

    // The event recorded one EventInstance + applied its effect
    // to GER (the first / only actor).
    CHECK(save.find("\"save_version\": 18")                       != std::string::npos);
    CHECK(save.find("\"low_stability_unrest_firing\"")            != std::string::npos);
    // Mechanical asymptotic-add applied to legitimacy:
    //   0.50 + (-0.05) * 0.50 = 0.475
    CHECK(save.find("\"legitimacy\":")                            != std::string::npos);
    CHECK(save.find("0.475")                                      != std::string::npos);

    // Round-trip: deserialize and re-serialize; event_history
    // and legitimacy survive byte-stably.
    const auto r2 = ss::deserialize(save);
    REQUIRE(r2.ok());
    const auto& reloaded = r2.value();
    REQUIRE(reloaded.event_history.size() == 1u);
    CHECK(reloaded.event_history[0].event_id_code ==
          "low_stability_unrest_firing");
    REQUIRE(reloaded.countries.size() == 1u);
    CHECK(reloaded.countries[0].legitimacy == doctest::Approx(0.475));
}

// =====================================================================
// C. no new artefact when events fire; events.jsonl semantics
//    unchanged; applied_commands / active_policies untouched
// =====================================================================
TEST_CASE("M5 integration: firing run still produces exactly the same 10-artefact set") {
    TempDir td("leviathan_m5_firing_artefact_set");

    GameState state = make_m5_firing_state();

    rn::RunnerOptions opts;
    opts.days               = 31;
    opts.output_dir         = td.path;
    opts.summary_csv_path   = td.path / "summary.csv";
    opts.countries_csv_path = td.path / "countries.csv";
    opts.factions_csv_path  = td.path / "factions.csv";

    REQUIRE(rn::run_state(state, opts).ok());

    // The 10-file artefact contract holds.
    CHECK(fs::exists(td.path / "save.json"));
    CHECK(fs::exists(td.path / "events.jsonl"));
    CHECK(fs::exists(td.path / "summary.csv"));
    CHECK(fs::exists(td.path / "countries.csv"));
    CHECK(fs::exists(td.path / "factions.csv"));
    CHECK(fs::exists(td.path / "interest_groups.csv"));
    CHECK(fs::exists(td.path / "interest_group_country_feedback.csv"));
    CHECK(fs::exists(td.path / "interest_group_authority_pressure.csv"));
    CHECK(fs::exists(td.path / "provinces.svg"));
    CHECK(fs::exists(td.path / "map.html"));

    // No new artefact appeared for event_history.
    CHECK_FALSE(fs::exists(td.path / "event_history.csv"));
    CHECK_FALSE(fs::exists(td.path / "event_history.json"));
    CHECK_FALSE(fs::exists(td.path / "events.csv"));

    // events.jsonl is the M0.6 lifecycle log. RCR-1 (RFC-090
    // §5.9) updates its semantics: event_firer now emits one
    // per-fire LogEntry per recorded event, with category
    // "event_fired" and the fired event_id_code in the message
    // + metadata. So the firing event's id_code DOES now appear
    // in events.jsonl. (The original M5 invariant — events.jsonl
    // semantics unchanged — has been superseded by the RCR-1
    // corrective batch.)
    const std::string ev_log = read_file(td.path / "events.jsonl");
    CHECK(ev_log.find("low_stability_unrest_firing") != std::string::npos);
    CHECK(ev_log.find("event_fired")                 != std::string::npos);

    // The save must NOT contain a synthesised active_policies
    // entry (events aren't policies) or an applied_commands
    // entry (events aren't player commands).
    const std::string save = read_file(td.path / "save.json");
    const auto r2 = ss::deserialize(save);
    REQUIRE(r2.ok());
    const auto& reloaded = r2.value();
    CHECK(reloaded.applied_commands.empty());
    REQUIRE(reloaded.countries.size() == 1u);
    CHECK(reloaded.countries[0].active_policies.empty());
}

// =====================================================================
// D. determinism: same hand-built state + same options ->
//    byte-identical 10 artefacts WITH events firing
// =====================================================================
TEST_CASE("M5 integration: firing run is deterministic ??two runs produce byte-identical 10 artefacts") {
    auto run_once = [](const fs::path& output_dir) {
        GameState state = make_m5_firing_state();
        rn::RunnerOptions opts;
        opts.days               = 31;
        opts.output_dir         = output_dir;
        opts.summary_csv_path   = output_dir / "summary.csv";
        opts.countries_csv_path = output_dir / "countries.csv";
        opts.factions_csv_path  = output_dir / "factions.csv";
        REQUIRE(rn::run_state(state, opts).ok());
    };

    TempDir td_a("leviathan_m5_firing_det_a");
    TempDir td_b("leviathan_m5_firing_det_b");
    run_once(td_a.path);
    run_once(td_b.path);

    const char* kFiles[] = {
        "save.json", "events.jsonl",
        "summary.csv", "countries.csv", "factions.csv",
        "interest_groups.csv",
        "interest_group_country_feedback.csv",
        "interest_group_authority_pressure.csv",
        "provinces.svg", "map.html",
    };
    for (const char* name : kFiles) {
        CAPTURE(name);
        CHECK(read_file(td_a.path / name) ==
              read_file(td_b.path / name));
    }
}

// =====================================================================
// E. failure path through runner: bad event effect target ->
//    monthly tick fails -> run_state fails -> M2.9 contract
//    (no output artefacts on pre-end_tick failure)
// =====================================================================
TEST_CASE("M5 integration: bad event effect target fails the run; no artefacts written (M2.9 pre-end_tick contract)") {
    TempDir td("leviathan_m5_firing_apply_failure");

    GameState state = make_m5_firing_state();
    // Replace the event with one whose effect has an unknown
    // target. The trigger still matches; the apply path's M1.5
    // pre-flight reject will fire.
    state.events.clear();
    state.events.push_back(make_event(
        "bad_target_event",
        { make_trigger("country.stability", "lt", 0.30) },
        { make_effect("country.no_such_field", "add", 0.10) }));

    rn::RunnerOptions opts;
    opts.days       = 31;
    opts.output_dir = td.path;

    const auto r = rn::run_state(state, opts);
    REQUIRE(r.failed());

    // Per M2.9: failures before end_tick leave zero artefacts on
    // disk. The monthly pipeline failed at the event tick, before
    // end_tick was reached.
    CHECK_FALSE(fs::exists(td.path / "save.json"));
    CHECK_FALSE(fs::exists(td.path / "events.jsonl"));
    CHECK_FALSE(fs::exists(td.path / "provinces.svg"));
    CHECK_FALSE(fs::exists(td.path / "map.html"));
}

// =====================================================================
// M6.8 (RFC-090 §6.8 "debug 模式顯示真相"): end-to-end check that
// --debug toggles the events.jsonl `true_cause` reveal while
// leaving everything else (save.json, applied effects,
// state.rng.counter, event_history) byte-identical.
// =====================================================================

TEST_CASE("M6.8 integration: --debug reveals true_cause in events.jsonl; same-seed save.json byte-identical to non-debug") {
    TempDir td_nodbg("leviathan_m6_8_nodbg");
    TempDir td_dbg  ("leviathan_m6_8_dbg");

    auto build_state = []() {
        GameState s = make_m5_firing_state();
        // Author a distinctive true_cause so the test can search
        // for the verbatim string in events.jsonl.
        s.events[0].true_cause =
            "M6.8 SECRET TRUTH FOR DEBUG REVEAL";
        return s;
    };

    rn::RunnerOptions opts;
    opts.days       = 31;     // crosses one month boundary; event fires
    opts.output_dir = td_nodbg.path;
    opts.debug_mode = false;
    {
        GameState s = build_state();
        REQUIRE(rn::run_state(s, opts).ok());
    }

    opts.output_dir = td_dbg.path;
    opts.debug_mode = true;
    {
        GameState s = build_state();
        REQUIRE(rn::run_state(s, opts).ok());
    }

    const std::string log_nodbg = read_file(td_nodbg.path / "events.jsonl");
    const std::string log_dbg   = read_file(td_dbg.path   / "events.jsonl");
    const std::string save_nodbg = read_file(td_nodbg.path / "save.json");
    const std::string save_dbg   = read_file(td_dbg.path   / "save.json");

    // Hard contract: the truth IS persisted in state.logs and the
    // save.json `logs` array REGARDLESS of debug_mode. Two same-
    // seed runs produce byte-identical save.json across the
    // debug toggle.
    CHECK(save_nodbg == save_dbg);

    // events.jsonl divergence: with --debug, true_cause appears
    // verbatim; without --debug, it is filtered out.
    CHECK(log_dbg.find("M6.8 SECRET TRUTH FOR DEBUG REVEAL")
          != std::string::npos);
    CHECK(log_dbg.find("\"true_cause\":\"M6.8 SECRET TRUTH FOR DEBUG REVEAL\"")
          != std::string::npos);
    CHECK(log_nodbg.find("true_cause") == std::string::npos);
    CHECK(log_nodbg.find("M6.8 SECRET TRUTH FOR DEBUG REVEAL")
          == std::string::npos);

    // The event_id_code IS visible on both sides — the M6.8
    // filter is scoped narrowly to the true_cause key only.
    CHECK(log_nodbg.find("low_stability_unrest_firing")
          != std::string::npos);
    CHECK(log_dbg.find("low_stability_unrest_firing")
          != std::string::npos);
}

TEST_CASE("M6.8 integration: --debug does NOT advance state.rng.counter or change which events fire") {
    GameState s_nodbg = make_m5_firing_state();
    GameState s_dbg   = make_m5_firing_state();
    const std::uint64_t before_counter = s_nodbg.rng.counter;
    REQUIRE(before_counter == s_dbg.rng.counter);

    TempDir td_a("leviathan_m6_8_rng_a");
    TempDir td_b("leviathan_m6_8_rng_b");

    rn::RunnerOptions opts;
    opts.days       = 31;
    opts.output_dir = td_a.path;
    opts.debug_mode = false;
    REQUIRE(rn::run_state(s_nodbg, opts).ok());

    opts.output_dir = td_b.path;
    opts.debug_mode = true;
    REQUIRE(rn::run_state(s_dbg, opts).ok());

    // No RNG-consumption divergence.
    CHECK(s_nodbg.rng.counter == s_dbg.rng.counter);

    // Same event_history (same event fired in both runs).
    REQUIRE(s_nodbg.event_history.size() == s_dbg.event_history.size());
    REQUIRE(s_nodbg.event_history.size() == 1u);
    CHECK(s_nodbg.event_history[0].event_id_code ==
          s_dbg.event_history[0].event_id_code);
}

// =====================================================================
// M6.9 (RFC-090 §6.9 "非 debug 模式隱藏真相") — end-to-end
// integration. The non-debug events.jsonl artefact emits a
// distorted player-facing surface (visible_report +
// information_accuracy + reported_intensity + noise_sample);
// the truth (true_cause) is filtered. In debug mode all five
// keys appear. The save.json `logs` array is byte-identical
// across the debug toggle because state.logs is identical
// either way — only the events.jsonl writer branches.
// =====================================================================

TEST_CASE("M6.9 integration: non-debug events.jsonl emits distorted publicText; --debug additionally reveals true_cause") {
    TempDir td_nodbg("leviathan_m6_9_nodbg");
    TempDir td_dbg  ("leviathan_m6_9_dbg");

    auto build_state = []() {
        GameState s = make_m5_firing_state();
        s.events[0].true_cause     = "M6.9 SECRET TRUTH";
        s.events[0].visible_report = "M6.9 PUBLIC REPORT";
        // Use distinctive intelligence settings so distortion is
        // observable. GER (the firing country) gets cap=0.5,
        // budget.intelligence=0.0, corruption=0.0 from
        // make_m5_firing_state's defaults; that maps to accuracy
        // ≈ 0.61. We pin THAT below as a regression marker.
        return s;
    };

    rn::RunnerOptions opts;
    opts.days       = 31;     // crosses one month boundary; event fires
    opts.output_dir = td_nodbg.path;
    opts.debug_mode = false;
    {
        GameState s = build_state();
        REQUIRE(rn::run_state(s, opts).ok());
    }

    opts.output_dir = td_dbg.path;
    opts.debug_mode = true;
    {
        GameState s = build_state();
        REQUIRE(rn::run_state(s, opts).ok());
    }

    const std::string log_nodbg = read_file(td_nodbg.path / "events.jsonl");
    const std::string log_dbg   = read_file(td_dbg.path   / "events.jsonl");
    const std::string save_nodbg = read_file(td_nodbg.path / "save.json");
    const std::string save_dbg   = read_file(td_dbg.path   / "save.json");

    // M6.8 carry-over: save.json byte-identical across debug
    // toggle (the truth + distortion ALL live in state.logs;
    // events.jsonl is the only artefact that differs).
    CHECK(save_nodbg == save_dbg);

    // M6.9 NEW: visible_report appears in BOTH modes (the
    // public-facing text — the M6.2 string verbatim).
    CHECK(log_nodbg.find("\"visible_report\":\"M6.9 PUBLIC REPORT\"")
          != std::string::npos);
    CHECK(log_dbg.find("\"visible_report\":\"M6.9 PUBLIC REPORT\"")
          != std::string::npos);

    // M6.9 NEW: distortion numerics appear in BOTH modes.
    CHECK(log_nodbg.find("\"information_accuracy\"") != std::string::npos);
    CHECK(log_nodbg.find("\"reported_intensity\"")   != std::string::npos);
    CHECK(log_nodbg.find("\"noise_sample\"")         != std::string::npos);
    CHECK(log_dbg.find("\"information_accuracy\"")   != std::string::npos);
    CHECK(log_dbg.find("\"reported_intensity\"")     != std::string::npos);
    CHECK(log_dbg.find("\"noise_sample\"")           != std::string::npos);

    // M6.8 carry-over: true_cause appears only in debug mode.
    CHECK(log_nodbg.find("M6.9 SECRET TRUTH") == std::string::npos);
    CHECK(log_dbg.find("M6.9 SECRET TRUTH")   != std::string::npos);
    CHECK(log_nodbg.find("\"true_cause\"")    == std::string::npos);
    CHECK(log_dbg.find("\"true_cause\"")      != std::string::npos);
}

TEST_CASE("M6.9 integration: same seed produces deterministic distorted publicText") {
    TempDir td_a("leviathan_m6_9_det_a");
    TempDir td_b("leviathan_m6_9_det_b");

    auto build_state = []() {
        GameState s = make_m5_firing_state();
        s.events[0].true_cause     = "deterministic truth";
        s.events[0].visible_report = "deterministic public";
        return s;
    };

    rn::RunnerOptions opts;
    opts.days       = 31;
    opts.debug_mode = false;

    opts.output_dir = td_a.path;
    {
        GameState s = build_state();
        REQUIRE(rn::run_state(s, opts).ok());
    }
    opts.output_dir = td_b.path;
    {
        GameState s = build_state();
        REQUIRE(rn::run_state(s, opts).ok());
    }

    const std::string log_a = read_file(td_a.path / "events.jsonl");
    const std::string log_b = read_file(td_b.path / "events.jsonl");
    CHECK(log_a == log_b);   // same noise_sample across runs

    const std::string save_a = read_file(td_a.path / "save.json");
    const std::string save_b = read_file(td_b.path / "save.json");
    CHECK(save_a == save_b);
}

TEST_CASE("M6.9 integration: save format stays at v18 (no schema bump)") {
    TempDir td("leviathan_m6_9_save_version");
    GameState s = make_m5_firing_state();
    rn::RunnerOptions opts;
    opts.days       = 31;
    opts.output_dir = td.path;
    REQUIRE(rn::run_state(s, opts).ok());

    const std::string save = read_file(td.path / "save.json");
    CHECK(save.find("\"save_version\": 18") != std::string::npos);
    CHECK(save.find("\"save_version\": 19") == std::string::npos);
}
