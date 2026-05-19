// Issue #112 event engine tests.
//
// Replaces the issue #110 test file. Asserts the binding strict-RFC
// semantics from the issue #112 corrective spec:
//   - per-country / per-category weighted-random draw using state.rng
//   - state-based option chooser (NOT options[0]) + author-controlled
//     option_effect_mode (OptionOnly / BaseThenOption / OptionThenBase)
//   - conditional followup chain: each followup must satisfy its own
//     triggers POST-parent-apply; cycle guard + depth guard run
//   - player-country option events defer effects until ChooseEventOption
//
// Each test seeds state directly and drives `event_engine::tick_events`.

#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/event_engine.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::EventDefinition;
using leviathan::core::EventOption;
using leviathan::core::EventOptionEffectMode;
using leviathan::core::EventTrigger;
using leviathan::core::GameState;
using leviathan::core::InterestGroupKind;
using leviathan::core::InterestGroupState;
using leviathan::core::PolicyEffect;
using leviathan::core::WeightModifier;
namespace eng = leviathan::systems::event_engine;

namespace {

CountryState make_country(int id, std::string code,
                          double stability  = 0.50,
                          double legitimacy = 0.50) {
    CountryState c;
    c.id         = CountryId{id};
    c.id_code    = std::move(code);
    c.name       = c.id_code;
    c.stability  = stability;
    c.legitimacy = legitimacy;
    return c;
}

EventTrigger trig(std::string target, std::string op, double value) {
    return EventTrigger{std::move(target), std::move(op), value};
}

PolicyEffect peff(std::string target, std::string op, double value) {
    return PolicyEffect{std::move(target), std::move(op), value};
}

WeightModifier wmod(std::string target, std::string op,
                    double value, double delta) {
    WeightModifier m;
    m.target = std::move(target);
    m.op = std::move(op);
    m.value = value;
    m.weight_delta = delta;
    return m;
}

EventDefinition make_event(std::string id_code,
                           std::string category,
                           std::vector<EventTrigger>   triggers,
                           std::vector<PolicyEffect>   effects = {},
                           std::vector<WeightModifier> weights = {},
                           std::vector<EventOption>    options = {},
                           std::vector<std::string>    followups = {},
                           EventOptionEffectMode       mode = EventOptionEffectMode::OptionOnly) {
    EventDefinition d;
    d.id_code            = std::move(id_code);
    d.name               = d.id_code;
    d.visible_report     = "test report";
    d.true_cause         = "test true cause";
    d.category           = std::move(category);
    d.triggers           = std::move(triggers);
    d.effects            = std::move(effects);
    d.weight_modifiers   = std::move(weights);
    d.options            = std::move(options);
    d.followup_event_ids = std::move(followups);
    d.option_effect_mode = mode;
    return d;
}

}  // namespace

// =====================================================================
// §1 + §3 — per-country / per-category weighted-random draw
// =====================================================================

TEST_CASE("Issue #112: tick_events fires ONE event per (country, category) bucket") {
    // Two events in the SAME category that both match this
    // country. Issue #112 §1: one draw per category → only ONE
    // fires, not both.
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.10));
    s.events.push_back(make_event(
        "event_a", /*category=*/"unrest",
        { trig("country.stability", "lt", 0.20) },
        { peff("country.stability", "add", -0.005) }));
    s.events.push_back(make_event(
        "event_b", /*category=*/"unrest",
        { trig("country.stability", "lt", 0.20) },
        { peff("country.stability", "add", -0.005) }));

    const auto r = eng::tick_events(s);
    REQUIRE(r);
    CHECK(r.value().events_matched == 2);
    CHECK(r.value().events_drawn   == 1);
    CHECK(s.event_history.size()   == 1u);
}

TEST_CASE("Issue #112: tick_events fires ONE event per DIFFERENT category") {
    // Same country, two events in DIFFERENT categories → TWO
    // distinct buckets → TWO draws.
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.10, /*leg*/0.10));
    s.events.push_back(make_event(
        "stab_event", "stability",
        { trig("country.stability", "lt", 0.20) },
        { peff("country.stability", "add", -0.01) }));
    s.events.push_back(make_event(
        "leg_event", "legitimacy",
        { trig("country.legitimacy", "lt", 0.20) },
        { peff("country.legitimacy", "add", -0.01) }));

    const auto r = eng::tick_events(s);
    REQUIRE(r);
    CHECK(r.value().events_matched      == 2);
    CHECK(r.value().events_drawn        == 2);
    CHECK(r.value().categories_processed == 2);
    CHECK(s.event_history.size()        == 2u);
}

TEST_CASE("Issue #112: same event template fires for multiple countries independently") {
    // Two countries both above threshold for the same event template.
    // Per-country draw: each country gets its own independent fire.
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.10));
    s.countries.push_back(make_country(1, "FRA", /*stab*/0.10));
    s.events.push_back(make_event(
        "unrest", "stability",
        { trig("country.stability", "lt", 0.20) },
        { peff("country.stability", "add", -0.01) }));

    const auto r = eng::tick_events(s);
    REQUIRE(r);
    // Each country matched once → 2 matches; each fires its own
    // draw → 2 fires.
    CHECK(r.value().events_matched         == 2);
    CHECK(r.value().events_drawn           == 2);
    CHECK(r.value().countries_with_matches == 2);
    REQUIRE(s.event_history.size() == 2u);
}

TEST_CASE("Issue #112: IG in another country does NOT contribute to this country's match pool") {
    // IG owning country = FRA (CountryId{1}); evaluator must NOT
    // surface it for GER's draw.
    GameState s;
    s.countries.push_back(make_country(0, "GER"));
    s.countries.push_back(make_country(1, "FRA"));
    // High-radicalism IG OWNED BY FRA.
    InterestGroupState ig;
    ig.id_code = "fra_radicals";
    ig.kind    = InterestGroupKind::Workers;
    ig.country = CountryId{1};   // FRA
    ig.radicalism = 0.95;
    s.interest_groups.push_back(ig);
    s.events.push_back(make_event(
        "radical_warning", "unrest",
        { trig("interest_group.radicalism", "gt", 0.75) },
        { peff("country.legitimacy", "add", -0.01) }));

    const auto r = eng::tick_events(s);
    REQUIRE(r);
    // FRA's IG matches FRA's event pool only (1 match, 1 fire);
    // GER's pool is empty (0 matches).
    CHECK(r.value().events_matched         == 1);
    CHECK(r.value().events_drawn           == 1);
    REQUIRE(s.event_history.size() == 1u);
    REQUIRE(s.event_history[0].actors.size() >= 1u);
    CHECK(s.event_history[0].actors[0].country_id_code == "FRA");
}

TEST_CASE("Issue #112: no matched events → state.rng.counter unchanged") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.80));  // above threshold
    s.events.push_back(make_event(
        "low_stab", "unrest",
        { trig("country.stability", "lt", 0.20) },
        { peff("country.stability", "add", -0.01) }));
    s.rng.seed    = 42;
    s.rng.counter = 7;

    const auto r = eng::tick_events(s);
    REQUIRE(r);
    CHECK(r.value().events_matched == 0);
    CHECK(r.value().events_drawn   == 0);
    CHECK(s.rng.counter            == 7u);   // unchanged
}

TEST_CASE("Issue #112: single-candidate bucket still consumes one RNG draw "
          "(random::weighted_choice always advances counter)") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.10));
    s.events.push_back(make_event(
        "only_match", "unrest",
        { trig("country.stability", "lt", 0.20) },
        { peff("country.stability", "add", -0.01) }));
    s.rng.seed    = 42;
    s.rng.counter = 0;

    const auto r = eng::tick_events(s);
    REQUIRE(r);
    CHECK(s.rng.counter == 1u);   // exactly one draw consumed
}

TEST_CASE("Issue #112: different seeds can produce different selected events "
          "from the same matched pool") {
    auto build = [](std::uint64_t seed) {
        GameState s;
        s.countries.push_back(make_country(0, "GER", /*stab*/0.10));
        s.events.push_back(make_event(
            "a", "unrest", { trig("country.stability", "lt", 0.20) },
            { peff("country.stability", "add", -0.005) }));
        s.events.push_back(make_event(
            "b", "unrest", { trig("country.stability", "lt", 0.20) },
            { peff("country.stability", "add", -0.005) }));
        s.rng.seed    = seed;
        s.rng.counter = 0;
        return s;
    };

    // Sample multiple seeds; record which event fired each time.
    int a_count = 0;
    int b_count = 0;
    for (std::uint64_t seed = 1; seed <= 200; ++seed) {
        GameState s = build(seed);
        REQUIRE(eng::tick_events(s));
        REQUIRE(s.event_history.size() == 1u);
        if (s.event_history[0].event_id_code == "a") { ++a_count; }
        else if (s.event_history[0].event_id_code == "b") { ++b_count; }
    }
    // Both events should be selected at least once across 200 seeds
    // (proves RNG actually picks; not stuck on one).
    CHECK(a_count > 0);
    CHECK(b_count > 0);
}

// =====================================================================
// §5 — event options
// =====================================================================

TEST_CASE("Issue #112: AI option chooser picks state-best option, NOT options[0]") {
    GameState s;
    // Country with high corruption — should prefer the anti-
    // corruption option, NOT the do-nothing options[0].
    auto c = make_country(0, "GER");
    c.corruption = 0.80;
    s.countries.push_back(c);

    EventOption do_nothing;
    do_nothing.id_code = "do_nothing";
    do_nothing.effects = { peff("country.stability", "add", -0.01) };
    EventOption anti_corruption;
    anti_corruption.id_code = "anti_corruption";
    anti_corruption.effects = { peff("country.corruption", "add", -0.05) };

    s.events.push_back(make_event(
        "scandal", "political",
        { trig("country.stability", "lt", 0.60) },  // matches: c.stability=0.5
        /*effects=*/{},
        /*weights=*/{},
        /*options=*/{do_nothing, anti_corruption},
        /*followups=*/{},
        EventOptionEffectMode::OptionOnly));

    REQUIRE(eng::tick_events(s));
    // anti_corruption picked: corruption-axis desire is -0.80, effect
    // value -0.05; score = -0.05 * -0.80 = +0.04 wins over do_nothing.
    // Mechanical asymptotic-add: 0.80 + (-0.05) * 0.80 = 0.76.
    CHECK(s.countries[0].corruption == doctest::Approx(0.76));
}

TEST_CASE("Issue #112: option_effect_mode = OptionOnly applies only option effects") {
    GameState s;
    auto c = make_country(0, "GER", /*stab*/0.50, /*leg*/0.50);
    s.countries.push_back(c);
    EventOption opt;
    opt.id_code = "act";
    opt.effects = { peff("country.legitimacy", "add", 0.10) };
    s.events.push_back(make_event(
        "x", "test",
        { trig("country.stability", "lt", 0.60) },
        /*base effects:*/ { peff("country.legitimacy", "add", -0.99) },
        /*weights=*/{},
        /*options=*/{opt},
        /*followups=*/{},
        EventOptionEffectMode::OptionOnly));

    REQUIRE(eng::tick_events(s));
    // Option only ran (base -0.99 ignored). Mechanical asymptotic-add:
    //   0.50 + 0.10 * (1 - 0.50) = 0.55
    CHECK(s.countries[0].legitimacy == doctest::Approx(0.55));
}

TEST_CASE("Issue #112: option_effect_mode = BaseThenOption applies both, base first") {
    GameState s;
    auto c = make_country(0, "GER", /*stab*/0.50, /*leg*/0.50);
    s.countries.push_back(c);
    EventOption opt;
    opt.id_code = "act";
    opt.effects = { peff("country.legitimacy", "add", 0.20) };
    s.events.push_back(make_event(
        "x", "test",
        { trig("country.stability", "lt", 0.60) },
        /*base effects:*/ { peff("country.legitimacy", "add", -0.10) },
        /*weights=*/{},
        /*options=*/{opt},
        /*followups=*/{},
        EventOptionEffectMode::BaseThenOption));

    REQUIRE(eng::tick_events(s));
    // Mechanical asymptotic-add (base then option):
    //   base   -0.10 on 0.50:        0.50 + (-0.10) * 0.50 = 0.45
    //   option +0.20 on 0.45:        0.45 + 0.20 * (1 - 0.45) = 0.56
    CHECK(s.countries[0].legitimacy == doctest::Approx(0.56));
}

TEST_CASE("Issue #112: option_effect_mode = OptionThenBase applies both, option first") {
    GameState s;
    auto c = make_country(0, "GER", /*stab*/0.50, /*leg*/0.50);
    s.countries.push_back(c);
    EventOption opt;
    opt.id_code = "act";
    opt.effects = { peff("country.legitimacy", "add", 0.20) };
    s.events.push_back(make_event(
        "x", "test",
        { trig("country.stability", "lt", 0.60) },
        /*base effects:*/ { peff("country.legitimacy", "add", -0.10) },
        /*weights=*/{},
        /*options=*/{opt},
        /*followups=*/{},
        EventOptionEffectMode::OptionThenBase));

    REQUIRE(eng::tick_events(s));
    // Mechanical asymptotic-add (option then base):
    //   option +0.20 on 0.50:        0.50 + 0.20 * (1 - 0.50) = 0.60
    //   base   -0.10 on 0.60:        0.60 + (-0.10) * 0.60   = 0.54
    // The ordering pin is the load-bearing thing; under asymptotic-add
    // the result IS sensitive to order (was commutative under linear).
    CHECK(s.countries[0].legitimacy == doctest::Approx(0.54));
    CHECK(s.event_history.size() == 1u);
}

// =====================================================================
// §6 — conditional followup chain
// =====================================================================

TEST_CASE("Issue #112: followup whose triggers fail post-parent-apply is NOT recorded") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.10, /*leg*/0.50));

    // Followup requires stability < 0.05 — does NOT match
    // post-parent-apply (parent drops stability to 0.09).
    s.events.push_back(make_event(
        "followup", "post",
        { trig("country.stability", "lt", 0.05) },
        { peff("country.legitimacy", "add", 0.03) }));
    // Parent matches the initial state.
    s.events.push_back(make_event(
        "parent", "stability",
        { trig("country.stability", "lt", 0.20) },
        { peff("country.stability", "add", -0.01) },
        /*weights=*/{},
        /*options=*/{},
        /*followups=*/{"followup"}));

    const auto r = eng::tick_events(s);
    REQUIRE(r);
    REQUIRE(s.event_history.size() == 1u);   // ONLY parent
    CHECK(s.event_history[0].event_id_code == "parent");
    CHECK(r.value().followups_recorded == 0);
    // Followup's effect did NOT apply.
    CHECK(s.countries[0].legitimacy == doctest::Approx(0.50));
}

TEST_CASE("Issue #112: followup whose triggers DO match post-parent-apply records + applies") {
    // To prove the chain runs CONDITIONALLY (followup must satisfy
    // its own triggers after parent applies), pin the followup
    // trigger to a threshold that ONLY matches AFTER parent's
    // effect lands. country.legitimacy = 0.50; parent drops
    // legitimacy by 0.05 to 0.45; followup trigger fires at
    // legitimacy < 0.48 → matches post-apply, NOT pre-apply.
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.10, /*leg*/0.50));

    s.events.push_back(make_event(
        "followup", "post",
        { trig("country.legitimacy", "lt", 0.48) },
        { peff("country.legitimacy", "add", 0.03) }));
    s.events.push_back(make_event(
        "parent", "stability",
        { trig("country.stability", "lt", 0.20) },
        { peff("country.legitimacy", "add", -0.05) },   // 0.50 → 0.45
        /*weights=*/{},
        /*options=*/{},
        /*followups=*/{"followup"}));

    REQUIRE(eng::tick_events(s));
    REQUIRE(s.event_history.size() == 2u);
    CHECK(s.event_history[0].event_id_code == "parent");
    CHECK(s.event_history[1].event_id_code == "followup");
    // Mechanical asymptotic-add (sequential):
    //   parent   -0.05 on 0.50:  0.50 + (-0.05) * 0.50 = 0.475
    //   followup +0.03 on 0.475: 0.475 + 0.03 * (1 - 0.475) = 0.49075
    CHECK(s.countries[0].legitimacy == doctest::Approx(0.49075));
}

TEST_CASE("Issue #112: followup chain immediate-predecessor `followup_of` metadata") {
    // Chain A → B → C. The triggers stage so that each downstream
    // step only matches AFTER the prior step applies — proves the
    // chain ran via recursion (not direct draw of each event in
    // its own bucket). C.followup_of must point to B (immediate
    // predecessor), NOT A (root).
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.10, /*leg*/0.50));

    // Asymptotic-add trajectory under repeated -0.10 from leg=0.50:
    //   after a: 0.50 + (-0.10) * 0.50  = 0.45
    //   after b: 0.45 + (-0.10) * 0.45  = 0.405
    // Trigger thresholds chosen above the post-apply value but
    // below the predecessor's post-apply value, so the conditional
    // followup chain proceeds.
    s.events.push_back(make_event(
        "c", "tail",
        { trig("country.legitimacy", "lt", 0.42) },   // matches after b applies (0.405)
        { peff("country.legitimacy", "add", -0.10) }));
    s.events.push_back(make_event(
        "b", "mid",
        { trig("country.legitimacy", "lt", 0.48) },   // matches after a applies (0.45)
        { peff("country.legitimacy", "add", -0.10) },
        /*weights=*/{},
        /*options=*/{},
        /*followups=*/{"c"}));
    s.events.push_back(make_event(
        "a", "head",
        { trig("country.stability", "lt", 0.20) },    // matches initially
        { peff("country.legitimacy", "add", -0.10) },
        /*weights=*/{},
        /*options=*/{},
        /*followups=*/{"b"}));

    REQUIRE(eng::tick_events(s));
    REQUIRE(s.event_history.size() == 3u);
    CHECK(s.event_history[0].event_id_code == "a");
    CHECK(s.event_history[1].event_id_code == "b");
    CHECK(s.event_history[2].event_id_code == "c");
    // Walk state.logs for followup_of metadata. The implementation
    // emits one `event_fired` LogEntry per fire; followups carry
    // a `followup_of` metadata entry naming the IMMEDIATE
    // predecessor.
    int b_pred_a = 0;
    int c_pred_b = 0;
    for (const auto& log : s.logs) {
        if (log.category != "event_fired") { continue; }
        std::string e_id, fp;
        for (const auto& kv : log.metadata) {
            if (kv.first == "event_id_code") { e_id = kv.second; }
            if (kv.first == "followup_of")   { fp   = kv.second; }
        }
        if (e_id == "b" && fp == "a") { ++b_pred_a; }
        if (e_id == "c" && fp == "b") { ++c_pred_b; }
    }
    CHECK(b_pred_a == 1);
    CHECK(c_pred_b == 1);
}

TEST_CASE("Issue #112: followup cycle guard stops A → B → A") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.10, /*leg*/0.50));

    s.events.push_back(make_event(
        "a", "head",
        { trig("country.stability", "lt", 0.20) },
        { peff("country.stability", "add", -0.005) },
        /*weights=*/{}, /*options=*/{}, /*followups=*/{"b"}));
    s.events.push_back(make_event(
        "b", "mid",
        { trig("country.stability", "lt", 0.20) },
        { peff("country.legitimacy", "add", 0.01) },
        /*weights=*/{}, /*options=*/{}, /*followups=*/{"a"}));

    REQUIRE(eng::tick_events(s));
    // Chain: A fires → B fires (a in visited set) → trying to go
    // back to A is blocked by the visited guard → recursion ends.
    // So event_history = [A, B], not [A, B, A, B, ...].
    REQUIRE(s.event_history.size() == 2u);
    CHECK(s.event_history[0].event_id_code == "a");
    CHECK(s.event_history[1].event_id_code == "b");
}

TEST_CASE("Issue #112: followup max-depth guard stops at kMaxFollowupDepth = 5") {
    // 7-step chain (a → b → c → d → e → f → g). a's trigger
    // matches initially; each downstream step's trigger ONLY
    // matches after its immediate predecessor applies (so the
    // recursion's F1+G1 conditional check actually runs and
    // doesn't short-circuit). All in the SAME category so the
    // direct-draw bucket only chooses ONE event (with one
    // candidate matching initially, that's `a`); the rest are
    // reached purely via chain recursion.
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.10,
                                       /*leg*/0.50));

    // Asymptotic-add trajectory under repeated -0.05 from leg=0.50:
    //   after a: 0.50 + (-0.05) * 0.50  ≈ 0.475
    //   after b: 0.475 + (-0.05) * 0.475 ≈ 0.45125
    //   after c: 0.45125 + (-0.05) * 0.45125 ≈ 0.4287
    //   after d: 0.4287 + (-0.05) * 0.4287 ≈ 0.4072
    //   after e: 0.4072 + (-0.05) * 0.4072 ≈ 0.3869
    //   after f: 0.3869 + (-0.05) * 0.3869 ≈ 0.3676
    // Each chain step's trigger threshold is set above the
    // post-apply value but below the predecessor's post-apply
    // value, so the conditional followup chain proceeds AND
    // only 'a' matches the initial-snapshot per-category bucket.
    auto chain_event = [](const std::string& id,
                          double leg_trig,
                          const std::string& followup_id) {
        return make_event(
            id, "chain",
            { trig("country.legitimacy", "lt", leg_trig) },
            { peff("country.legitimacy", "add", -0.05) },
            /*weights=*/{}, /*options=*/{},
            followup_id.empty() ? std::vector<std::string>{}
                                : std::vector<std::string>{followup_id});
    };
    // First event uses a stability-trigger so it matches the initial
    // state (legitimacy=0.50 does NOT satisfy any of the chain
    // legit-triggers below).
    s.events.push_back(make_event(
        "a", "chain",
        { trig("country.stability", "lt", 0.20) },
        { peff("country.legitimacy", "add", -0.05) },
        /*weights=*/{}, /*options=*/{},
        /*followups=*/{"b"}));
    s.events.push_back(chain_event("b", 0.48, "c"));
    s.events.push_back(chain_event("c", 0.46, "d"));
    s.events.push_back(chain_event("d", 0.44, "e"));
    s.events.push_back(chain_event("e", 0.42, "f"));
    s.events.push_back(chain_event("f", 0.40, "g"));
    s.events.push_back(chain_event("g", 0.38, ""));

    REQUIRE(eng::tick_events(s));
    // a + 5 followups before max-depth guard stops = 6 entries.
    REQUIRE(s.event_history.size() == 6u);
    CHECK(s.event_history[0].event_id_code == "a");
    CHECK(s.event_history[5].event_id_code == "f");  // depth 5
    // g never recorded (depth 6 blocked by guard).
    for (const auto& inst : s.event_history) {
        CHECK(inst.event_id_code != "g");
    }
}

// =====================================================================
// §5 + player-country deferral
// =====================================================================

TEST_CASE("Issue #112: player-country event with options creates pending entry, "
          "applies NO effects, processes NO followups") {
    GameState s;
    auto c = make_country(0, "GER", /*stab*/0.50, /*leg*/0.10);
    s.countries.push_back(c);
    s.player_country = CountryId{0};

    EventOption pick;
    pick.id_code = "act";
    pick.effects = { peff("country.legitimacy", "add", 0.20) };

    // `followup`'s trigger DOES NOT match initially (legitimacy=0.10
    // is not > 0.50) — so it doesn't fire from its own category
    // bucket. Only path to fire would be via `crisis`'s recursion,
    // which the player-country deferral path skips.
    s.events.push_back(make_event(
        "followup", "post",
        { trig("country.legitimacy", "gt", 0.50) },
        { peff("country.stability", "add", -0.05) }));
    s.events.push_back(make_event(
        "crisis", "political",
        { trig("country.legitimacy", "lt", 0.20) },
        /*base effects:*/{ peff("country.legitimacy", "add", -0.99) },
        /*weights=*/{},
        /*options=*/{pick},
        /*followups=*/{"followup"},
        EventOptionEffectMode::OptionOnly));

    const auto r = eng::tick_events(s);
    REQUIRE(r);
    // Parent recorded (events.jsonl shows what was offered) but
    // NO effects applied and NO followup recorded.
    REQUIRE(s.event_history.size() == 1u);
    CHECK(s.event_history[0].event_id_code == "crisis");
    CHECK(r.value().events_pending_player_choice == 1);
    CHECK(s.countries[0].legitimacy == doctest::Approx(0.10));  // unchanged
    REQUIRE(s.pending_player_events.size() == 1u);
    CHECK(s.pending_player_events[0].event_id_code   == "crisis");
    CHECK(s.pending_player_events[0].country_id_code == "GER");
}
