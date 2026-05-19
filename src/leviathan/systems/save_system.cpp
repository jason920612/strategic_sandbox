#include "leviathan/systems/save_system.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include "internal/json_helpers.hpp"
#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/interest_group_kind.hpp"
#include "leviathan/core/log_entry.hpp"
#include "leviathan/systems/faction_demands.hpp"

namespace leviathan::systems::save_system {

namespace {

// Shared JSON helpers live in internal/json_helpers.hpp. The detail
// namespace's json type is nlohmann::ordered_json (chosen for the
// M0.6 / M0.8 byte-stable-metadata invariant), so save_system gets
// the right behaviour automatically.
using leviathan::systems::detail::fmt_err;
using leviathan::systems::detail::navigate;
using leviathan::systems::detail::require_string;
using leviathan::systems::detail::require_number;
using leviathan::systems::detail::require_ratio;
using leviathan::systems::detail::require_u64;
using leviathan::systems::detail::json;

// ----- severity <-> string -------------------------------------------------

std::string severity_to_string(core::LogSeverity s) {
    switch (s) {
        case core::LogSeverity::Debug: return "debug";
        case core::LogSeverity::Info:  return "info";
        case core::LogSeverity::Warn:  return "warn";
        case core::LogSeverity::Error: return "error";
    }
    return "info";
}

core::Result<core::LogSeverity> severity_from_string(std::string_view s) {
    if (s == "debug") return core::Result<core::LogSeverity>::success(core::LogSeverity::Debug);
    if (s == "info")  return core::Result<core::LogSeverity>::success(core::LogSeverity::Info);
    if (s == "warn")  return core::Result<core::LogSeverity>::success(core::LogSeverity::Warn);
    if (s == "error") return core::Result<core::LogSeverity>::success(core::LogSeverity::Error);
    std::string msg = "unknown severity '";
    msg.append(s.data(), s.size());
    msg += "' (expected debug|info|warn|error)";
    return core::Result<core::LogSeverity>::failure(std::move(msg));
}

// ----- PlayerCommandKind <-> string ---------------------------------------

std::string player_command_kind_to_string(core::PlayerCommandKind k) {
    switch (k) {
        case core::PlayerCommandKind::EnactPolicy:       return "EnactPolicy";
        case core::PlayerCommandKind::AdjustBudget:      return "AdjustBudget";
        case core::PlayerCommandKind::ChooseEventOption: return "ChooseEventOption";
    }
    // Unreachable for any valid enum value. The fallback returns a
    // sentinel rather than re-using a real kind's string so a bug
    // (e.g. a memory-corruption-produced bad value) shows up loudly
    // in any save it touches. The compiler's -Wswitch warning is the
    // primary defense; this is the runtime backstop. (PR #32 nit.)
    return "UnknownPlayerCommandKind";
}

core::Result<core::PlayerCommandKind> player_command_kind_from_string(
        std::string_view s) {
    if (s == "EnactPolicy") {
        return core::Result<core::PlayerCommandKind>::success(
            core::PlayerCommandKind::EnactPolicy);
    }
    if (s == "AdjustBudget") {
        return core::Result<core::PlayerCommandKind>::success(
            core::PlayerCommandKind::AdjustBudget);
    }
    if (s == "ChooseEventOption") {
        return core::Result<core::PlayerCommandKind>::success(
            core::PlayerCommandKind::ChooseEventOption);
    }
    std::string msg = "unknown player command kind '";
    msg.append(s.data(), s.size());
    msg += "' (expected EnactPolicy|AdjustBudget|ChooseEventOption)";
    return core::Result<core::PlayerCommandKind>::failure(std::move(msg));
}

// ----- InterestGroupKind <-> string (M3.1; shared helper in M3.5) ---------
//
// The mapping itself lives in `leviathan/core/interest_group_kind.hpp` so
// save_system, scenario_loader, and diagnostics all spell the same names.
// We re-export the unqualified function names into this anonymous
// namespace to keep the existing call sites below readable.

using core::interest_group_kind_to_string;
using core::interest_group_kind_from_string;

// ----- serialise -----------------------------------------------------------

json country_to_json(const core::CountryState& c) {
    // M1.1 schema. Field order is fixed; tests pin the on-disk shape.
    // The save format separates runtime state (gdp / stability /
    // tax_revenue / budget_balance) from the JSON config's "initial_*"
    // fields - saves always store runtime values.
    json j = json::object();
    j["id"]                        = c.id.value();
    j["id_code"]                   = c.id_code;
    j["name"]                      = c.name;
    j["display_name"]              = c.display_name;
    j["gdp"]                       = c.gdp;
    j["tax_revenue"]               = c.tax_revenue;
    j["budget_balance"]            = c.budget_balance;
    j["legal_tax_burden"]          = c.legal_tax_burden;
    j["fiscal_capacity"]           = c.fiscal_capacity;
    j["administrative_efficiency"] = c.administrative_efficiency;
    j["central_control"]           = c.central_control;
    j["corruption"]                = c.corruption;
    j["stability"]                 = c.stability;
    j["legitimacy"]                = c.legitimacy;
    j["military_power"]            = c.military_power;
    j["threat_perception"]         = c.threat_perception;
    j["military_strength"]         = c.military_strength;     // RCR-1 (RFC-090 §3.8)
    j["last_gdp_growth_rate"]      = c.last_gdp_growth_rate;  // M1.12

    // M1.3 budget block - nested object, fixed key order.
    json budget = json::object();
    budget["administration"]  = c.budget.administration;
    budget["military"]        = c.budget.military;
    budget["education"]       = c.budget.education;
    budget["welfare"]         = c.budget.welfare;
    budget["intelligence"]    = c.budget.intelligence;
    budget["infrastructure"]  = c.budget.infrastructure;
    budget["industry"]        = c.budget.industry;
    j["budget"] = std::move(budget);

    // M2.16 government_authority block - nested object, fixed key
    // order. Four [0, 1] ratio fields. Save format v10 requires the
    // block; DataLoader treats it as optional in raw country JSON.
    json authority = json::object();
    authority["bureaucratic_compliance"] = c.government_authority.bureaucratic_compliance;
    authority["military_loyalty"]        = c.government_authority.military_loyalty;
    authority["intelligence_capability"] = c.government_authority.intelligence_capability;
    authority["media_control"]           = c.government_authority.media_control;
    j["government_authority"] = std::move(authority);

    // M1.15 active_policies block - array of {policy_id_code,
    // expires_on}. May be empty. Insertion order matches the order
    // in which apply_policy_effects appended entries.
    json active = json::array();
    for (const auto& ap : c.active_policies) {
        json entry = json::object();
        entry["policy_id_code"] = ap.policy_id_code;
        entry["expires_on"]     = ap.expires_on.to_string();
        active.push_back(std::move(entry));
    }
    j["active_policies"] = std::move(active);
    return j;
}

json faction_to_json(const core::FactionState& f) {
    // M1.2 schema. Field order is fixed and pinned by tests.
    json j = json::object();
    j["id"]              = f.id.value();
    j["country"]         = f.country.value();
    j["id_code"]         = f.id_code;
    j["country_id_code"] = f.country_id_code;
    j["name"]            = f.name;
    j["type"]            = f.type;
    j["support"]         = f.support;
    j["influence"]       = f.influence;
    j["radicalism"]      = f.radicalism;
    j["loyalty"]         = f.loyalty;
    j["resources"]       = f.resources;

    json pref = json::array();
    for (const auto& s : f.preferred_policies) {
        pref.push_back(s);
    }
    j["preferred_policies"] = std::move(pref);
    return j;
}

json policy_to_json(const core::PolicyData& p) {
    // M1.4 schema. Field order is fixed and pinned by tests.
    json j = json::object();
    j["id"]            = p.id.value();
    j["id_code"]       = p.id_code;
    j["name"]          = p.name;
    j["category"]      = p.category;
    j["duration_days"] = p.duration_days;
    j["admin_cost"]    = p.admin_cost;

    json effects = json::array();
    for (const auto& e : p.effects) {
        json eff = json::object();
        eff["target"] = e.target;
        eff["op"]     = e.op;
        eff["value"]  = e.value;
        effects.push_back(std::move(eff));
    }
    j["effects"] = std::move(effects);
    return j;
}

json log_entry_to_json(const core::LogEntry& e) {
    json j = json::object();
    j["date"]     = e.date.to_string();
    j["category"] = e.category;
    j["severity"] = severity_to_string(e.severity);
    j["source"]   = e.source;
    j["message"]  = e.message;
    // `json` is ordered_json here, so insertion order survives dump.
    json md = json::object();
    for (const auto& [k, v] : e.metadata) {
        md[k] = v;
    }
    j["metadata"] = std::move(md);
    return j;
}

// ----- deserialise helpers -------------------------------------------------

// SaveSystem-specific date helper (DataLoader has its own copy because
// only it uses simulation.start_date / simulation.end_date). The shared
// require_date is a candidate for extraction if it ever ends up in
// three places; for now SaveSystem reads dates only in current_date
// and in log entries, both via this helper.
core::Result<core::GameDate> require_date(const json& root,
                                          std::string_view path,
                                          std::string_view source) {
    auto s = require_string(root, path, source);
    if (!s) return core::Result<core::GameDate>::failure(std::move(s.error()));
    auto d = core::GameDate::parse(s.value());
    if (!d) {
        std::string msg = "'";
        msg.append(path.data(), path.size());
        msg += "' = \"";
        msg += s.value();
        msg += "\" is not a real Gregorian date";
        return core::Result<core::GameDate>::failure(fmt_err(source, msg));
    }
    return core::Result<core::GameDate>::success(d.value());
}

core::Result<core::CountryState> country_from_json(const json& j,
                                                   std::size_t array_index,
                                                   std::string_view source) {
    // Build a contextualised source label so every child error names
    // the right array element. Sub-helpers see this as the source.
    const std::string ctx =
        std::string(source) + ": countries[" + std::to_string(array_index) + "]";

    if (!j.is_object()) {
        return core::Result<core::CountryState>::failure(
            ctx + ": expected JSON object");
    }

    core::CountryState c;

    auto id_value = require_u64(j, "id", ctx);
    if (!id_value) {
        return core::Result<core::CountryState>::failure(std::move(id_value.error()));
    }
    // CountryId::underlying_type is int. A save with an id larger than
    // INT_MAX would silently truncate under static_cast, so reject it
    // up front rather than producing a corrupted CountryId.
    using under = core::CountryId::underlying_type;
    constexpr auto kMaxId =
        static_cast<std::uint64_t>(std::numeric_limits<under>::max());
    if (id_value.value() > kMaxId) {
        return core::Result<core::CountryState>::failure(
            ctx + ": 'id' is out of range for CountryId (max " +
            std::to_string(kMaxId) + ")");
    }
    c.id = core::CountryId{static_cast<under>(id_value.value())};

    auto id_code = require_string(j, "id_code", ctx);
    if (!id_code) {
        return core::Result<core::CountryState>::failure(std::move(id_code.error()));
    }
    c.id_code = id_code.value();

    auto name = require_string(j, "name", ctx);
    if (!name) {
        return core::Result<core::CountryState>::failure(std::move(name.error()));
    }
    c.name = name.value();

    auto display = require_string(j, "display_name", ctx);
    if (!display) {
        return core::Result<core::CountryState>::failure(std::move(display.error()));
    }
    c.display_name = display.value();

    // M1.1 schema: read every numeric field. SaveSystem does NOT
    // enforce [0,1] ratio ranges - if the in-memory state held an
    // out-of-range value, that's the loader's job (DataLoader) to
    // reject at config time. Saves of valid in-memory state will
    // always be in range.
    auto load_num = [&](const char* key, double& dst)
        -> core::Result<core::CountryState> {
        auto r = require_number(j, key, ctx);
        if (!r) {
            return core::Result<core::CountryState>::failure(std::move(r.error()));
        }
        dst = r.value();
        return core::Result<core::CountryState>::success(core::CountryState{});
    };

    if (auto r = load_num("gdp",                       c.gdp);            !r) return r;
    if (auto r = load_num("tax_revenue",               c.tax_revenue);    !r) return r;
    if (auto r = load_num("budget_balance",            c.budget_balance); !r) return r;
    if (auto r = load_num("legal_tax_burden",          c.legal_tax_burden);          !r) return r;
    if (auto r = load_num("fiscal_capacity",           c.fiscal_capacity);           !r) return r;
    if (auto r = load_num("administrative_efficiency", c.administrative_efficiency); !r) return r;
    if (auto r = load_num("central_control",           c.central_control);           !r) return r;
    if (auto r = load_num("corruption",                c.corruption);                !r) return r;
    if (auto r = load_num("stability",                 c.stability);                 !r) return r;
    if (auto r = load_num("legitimacy",                c.legitimacy);                !r) return r;
    if (auto r = load_num("military_power",            c.military_power);            !r) return r;
    if (auto r = load_num("threat_perception",         c.threat_perception);         !r) return r;
    if (auto r = load_num("military_strength",         c.military_strength);         !r) return r;  // RCR-1 (RFC-090 §3.8)
    if (auto r = load_num("last_gdp_growth_rate",      c.last_gdp_growth_rate);      !r) return r;  // M1.12

    // M1.3 budget block.
    if (!j.contains("budget")) {
        return core::Result<core::CountryState>::failure(
            ctx + ": missing required field 'budget'");
    }
    const auto& bj = j.at("budget");
    if (!bj.is_object()) {
        return core::Result<core::CountryState>::failure(
            ctx + ": 'budget' has wrong type (expected JSON object)");
    }
    const std::string budget_ctx = ctx + ": budget";
    auto load_budget = [&](const char* key, double& dst)
        -> core::Result<core::CountryState> {
        auto r = require_number(bj, key, budget_ctx);
        if (!r) {
            return core::Result<core::CountryState>::failure(std::move(r.error()));
        }
        dst = r.value();
        return core::Result<core::CountryState>::success(core::CountryState{});
    };
    if (auto r = load_budget("administration",  c.budget.administration);  !r) return r;
    if (auto r = load_budget("military",        c.budget.military);        !r) return r;
    if (auto r = load_budget("education",       c.budget.education);       !r) return r;
    if (auto r = load_budget("welfare",         c.budget.welfare);         !r) return r;
    if (auto r = load_budget("intelligence",    c.budget.intelligence);    !r) return r;
    if (auto r = load_budget("infrastructure",  c.budget.infrastructure);  !r) return r;
    if (auto r = load_budget("industry",        c.budget.industry);        !r) return r;

    // M2.16 government_authority block. Required as of save format
    // v10 (the version-history note in save_system.hpp covers the
    // bump rationale). Strict in the save layer: every sub-key is
    // required, finite, and in [0, 1] via require_ratio. The
    // DataLoader is more permissive — a country JSON without the
    // block defaults each field to 0.5 — but a save written from a
    // valid in-memory state will always include and round-trip the
    // block.
    if (!j.contains("government_authority")) {
        return core::Result<core::CountryState>::failure(
            ctx + ": missing required field 'government_authority'");
    }
    const auto& gaj = j.at("government_authority");
    if (!gaj.is_object()) {
        return core::Result<core::CountryState>::failure(
            ctx +
            ": 'government_authority' has wrong type (expected JSON object)");
    }
    const std::string ga_ctx = ctx + ": government_authority";
    auto load_authority = [&](const char* key, double& dst)
        -> core::Result<core::CountryState> {
        auto r = require_ratio(gaj, key, ga_ctx);
        if (!r) {
            return core::Result<core::CountryState>::failure(std::move(r.error()));
        }
        dst = r.value();
        return core::Result<core::CountryState>::success(core::CountryState{});
    };
    if (auto r = load_authority("bureaucratic_compliance",
                                c.government_authority.bureaucratic_compliance);
        !r) return r;
    if (auto r = load_authority("military_loyalty",
                                c.government_authority.military_loyalty);
        !r) return r;
    if (auto r = load_authority("intelligence_capability",
                                c.government_authority.intelligence_capability);
        !r) return r;
    if (auto r = load_authority("media_control",
                                c.government_authority.media_control);
        !r) return r;

    // M1.15 active_policies block. Required as of save format v7.
    // Empty array is fine; missing field is a hard failure (v6 saves
    // dropped here per the v7 history note in save_system.hpp).
    if (!j.contains("active_policies")) {
        return core::Result<core::CountryState>::failure(
            ctx + ": missing required field 'active_policies'");
    }
    const auto& ap_arr = j.at("active_policies");
    if (!ap_arr.is_array()) {
        return core::Result<core::CountryState>::failure(
            ctx + ": 'active_policies' has wrong type (expected JSON array)");
    }
    c.active_policies.reserve(ap_arr.size());
    for (std::size_t i = 0; i < ap_arr.size(); ++i) {
        const std::string ap_ctx =
            ctx + ": active_policies[" + std::to_string(i) + "]";
        const auto& entry = ap_arr[i];
        if (!entry.is_object()) {
            return core::Result<core::CountryState>::failure(
                ap_ctx + ": expected JSON object");
        }
        auto id_code_r = require_string(entry, "policy_id_code", ap_ctx);
        if (!id_code_r) {
            return core::Result<core::CountryState>::failure(
                std::move(id_code_r.error()));
        }
        auto expires_r = require_date(entry, "expires_on", ap_ctx);
        if (!expires_r) {
            return core::Result<core::CountryState>::failure(
                std::move(expires_r.error()));
        }
        core::ActivePolicy ap;
        ap.policy_id_code = id_code_r.value();
        ap.expires_on     = expires_r.value();
        c.active_policies.push_back(std::move(ap));
    }

    return core::Result<core::CountryState>::success(std::move(c));
}

core::Result<core::FactionState> faction_from_json(const json& j,
                                                   std::size_t array_index,
                                                   std::string_view source) {
    const std::string ctx =
        std::string(source) + ": factions[" + std::to_string(array_index) + "]";

    if (!j.is_object()) {
        return core::Result<core::FactionState>::failure(
            ctx + ": expected JSON object");
    }

    core::FactionState f;

    auto id_value = require_u64(j, "id", ctx);
    if (!id_value) {
        return core::Result<core::FactionState>::failure(std::move(id_value.error()));
    }
    using fac_under = core::FactionId::underlying_type;
    constexpr auto kFacMaxId =
        static_cast<std::uint64_t>(std::numeric_limits<fac_under>::max());
    if (id_value.value() > kFacMaxId) {
        return core::Result<core::FactionState>::failure(
            ctx + ": 'id' is out of range for FactionId (max " +
            std::to_string(kFacMaxId) + ")");
    }
    f.id = core::FactionId{static_cast<fac_under>(id_value.value())};

    auto country_value = require_u64(j, "country", ctx);
    if (!country_value) {
        return core::Result<core::FactionState>::failure(std::move(country_value.error()));
    }
    // Use CountryId's own underlying type rather than borrowing
    // FactionId's. Both are int today, so this is semantics-only;
    // PR #13 review flagged this so the latent bug never bites if
    // the underlying types diverge later.
    using cty_under = core::CountryId::underlying_type;
    constexpr auto kCtyMaxId =
        static_cast<std::uint64_t>(std::numeric_limits<cty_under>::max());
    if (country_value.value() > kCtyMaxId) {
        return core::Result<core::FactionState>::failure(
            ctx + ": 'country' is out of range for CountryId (max " +
            std::to_string(kCtyMaxId) + ")");
    }
    f.country = core::CountryId{static_cast<cty_under>(country_value.value())};

    auto id_code = require_string(j, "id_code", ctx);
    if (!id_code) {
        return core::Result<core::FactionState>::failure(std::move(id_code.error()));
    }
    f.id_code = id_code.value();

    auto country_id_code = require_string(j, "country_id_code", ctx);
    if (!country_id_code) {
        return core::Result<core::FactionState>::failure(std::move(country_id_code.error()));
    }
    f.country_id_code = country_id_code.value();

    auto name = require_string(j, "name", ctx);
    if (!name) {
        return core::Result<core::FactionState>::failure(std::move(name.error()));
    }
    f.name = name.value();

    auto type = require_string(j, "type", ctx);
    if (!type) {
        return core::Result<core::FactionState>::failure(std::move(type.error()));
    }
    f.type = type.value();

    // Numeric scalars. Same load_num pattern as country_from_json
    // (see comment there about no [0,1] enforcement at load time).
    auto load_num = [&](const char* key, double& dst)
        -> core::Result<core::FactionState> {
        auto r = require_number(j, key, ctx);
        if (!r) {
            return core::Result<core::FactionState>::failure(std::move(r.error()));
        }
        dst = r.value();
        return core::Result<core::FactionState>::success(core::FactionState{});
    };
    if (auto r = load_num("support",    f.support);    !r) return r;
    if (auto r = load_num("influence",  f.influence);  !r) return r;
    if (auto r = load_num("radicalism", f.radicalism); !r) return r;
    if (auto r = load_num("loyalty",    f.loyalty);    !r) return r;
    if (auto r = load_num("resources",  f.resources);  !r) return r;

    // preferred_policies: required array of strings (may be empty).
    if (!j.contains("preferred_policies")) {
        return core::Result<core::FactionState>::failure(
            ctx + ": missing required field 'preferred_policies'");
    }
    const auto& pp = j.at("preferred_policies");
    if (!pp.is_array()) {
        return core::Result<core::FactionState>::failure(
            ctx + ": 'preferred_policies' has wrong type (expected array of strings)");
    }
    f.preferred_policies.reserve(pp.size());
    for (std::size_t i = 0; i < pp.size(); ++i) {
        if (!pp[i].is_string()) {
            return core::Result<core::FactionState>::failure(
                ctx + ": preferred_policies[" + std::to_string(i) +
                "] is not a string");
        }
        f.preferred_policies.push_back(pp[i].get<std::string>());
    }

    return core::Result<core::FactionState>::success(std::move(f));
}

core::Result<core::PolicyData> policy_from_json(const json& j,
                                                std::size_t array_index,
                                                std::string_view source) {
    const std::string ctx =
        std::string(source) + ": policies[" + std::to_string(array_index) + "]";

    if (!j.is_object()) {
        return core::Result<core::PolicyData>::failure(
            ctx + ": expected JSON object");
    }

    core::PolicyData p;

    auto id_value = require_u64(j, "id", ctx);
    if (!id_value) {
        return core::Result<core::PolicyData>::failure(std::move(id_value.error()));
    }
    using under = core::PolicyId::underlying_type;
    constexpr auto kMaxId =
        static_cast<std::uint64_t>(std::numeric_limits<under>::max());
    if (id_value.value() > kMaxId) {
        return core::Result<core::PolicyData>::failure(
            ctx + ": 'id' is out of range for PolicyId (max " +
            std::to_string(kMaxId) + ")");
    }
    p.id = core::PolicyId{static_cast<under>(id_value.value())};

    auto id_code = require_string(j, "id_code", ctx);
    if (!id_code) {
        return core::Result<core::PolicyData>::failure(std::move(id_code.error()));
    }
    p.id_code = id_code.value();

    auto name = require_string(j, "name", ctx);
    if (!name) {
        return core::Result<core::PolicyData>::failure(std::move(name.error()));
    }
    p.name = name.value();

    auto category = require_string(j, "category", ctx);
    if (!category) {
        return core::Result<core::PolicyData>::failure(std::move(category.error()));
    }
    p.category = category.value();

    auto duration = require_u64(j, "duration_days", ctx);
    if (!duration) {
        return core::Result<core::PolicyData>::failure(std::move(duration.error()));
    }
    constexpr std::uint64_t kIntMax =
        static_cast<std::uint64_t>(std::numeric_limits<int>::max());
    if (duration.value() > kIntMax) {
        return core::Result<core::PolicyData>::failure(
            ctx + ": 'duration_days' exceeds INT_MAX (got " +
            std::to_string(duration.value()) + ")");
    }
    p.duration_days = static_cast<int>(duration.value());

    auto admin_cost = require_number(j, "admin_cost", ctx);
    if (!admin_cost) {
        return core::Result<core::PolicyData>::failure(std::move(admin_cost.error()));
    }
    p.admin_cost = admin_cost.value();

    // effects array
    if (!j.contains("effects")) {
        return core::Result<core::PolicyData>::failure(
            ctx + ": missing required field 'effects'");
    }
    const auto& effs = j.at("effects");
    if (!effs.is_array()) {
        return core::Result<core::PolicyData>::failure(
            ctx + ": 'effects' has wrong type (expected array)");
    }
    p.effects.reserve(effs.size());
    for (std::size_t i = 0; i < effs.size(); ++i) {
        const std::string eff_ctx = ctx + ": effects[" + std::to_string(i) + "]";
        const auto& e = effs[i];
        if (!e.is_object()) {
            return core::Result<core::PolicyData>::failure(
                eff_ctx + ": expected JSON object");
        }
        auto target = require_string(e, "target", eff_ctx);
        if (!target) {
            return core::Result<core::PolicyData>::failure(std::move(target.error()));
        }
        auto op = require_string(e, "op", eff_ctx);
        if (!op) {
            return core::Result<core::PolicyData>::failure(std::move(op.error()));
        }
        auto value = require_number(e, "value", eff_ctx);
        if (!value) {
            return core::Result<core::PolicyData>::failure(std::move(value.error()));
        }
        core::PolicyEffect pe;
        pe.target = target.value();
        pe.op     = op.value();
        pe.value  = value.value();
        p.effects.push_back(std::move(pe));
    }

    return core::Result<core::PolicyData>::success(std::move(p));
}

core::Result<core::LogEntry> log_entry_from_json(const json& j,
                                                 std::size_t array_index,
                                                 std::string_view source) {
    const std::string ctx =
        std::string(source) + ": logs[" + std::to_string(array_index) + "]";

    if (!j.is_object()) {
        return core::Result<core::LogEntry>::failure(
            ctx + ": expected JSON object");
    }

    core::LogEntry e;

    auto date = require_date(j, "date", ctx);
    if (!date) {
        return core::Result<core::LogEntry>::failure(std::move(date.error()));
    }
    e.date = date.value();

    auto category = require_string(j, "category", ctx);
    if (!category) {
        return core::Result<core::LogEntry>::failure(std::move(category.error()));
    }
    e.category = category.value();

    auto sev_str = require_string(j, "severity", ctx);
    if (!sev_str) {
        return core::Result<core::LogEntry>::failure(std::move(sev_str.error()));
    }
    auto sev = severity_from_string(sev_str.value());
    if (!sev) {
        return core::Result<core::LogEntry>::failure(
            ctx + ": 'severity': " + sev.error());
    }
    e.severity = sev.value();

    auto source_field = require_string(j, "source", ctx);
    if (!source_field) {
        return core::Result<core::LogEntry>::failure(std::move(source_field.error()));
    }
    e.source = source_field.value();

    auto message = require_string(j, "message", ctx);
    if (!message) {
        return core::Result<core::LogEntry>::failure(std::move(message.error()));
    }
    e.message = message.value();

    // metadata is optional; if present, must be an object with string
    // values. Iteration order is JSON-defined (nlohmann::json preserves
    // insertion order from the parse stream).
    if (j.contains("metadata")) {
        if (!j.at("metadata").is_object()) {
            return core::Result<core::LogEntry>::failure(
                ctx + ": 'metadata': expected JSON object");
        }
        for (auto it = j.at("metadata").begin(); it != j.at("metadata").end(); ++it) {
            if (!it.value().is_string()) {
                return core::Result<core::LogEntry>::failure(
                    ctx + ": metadata['" + it.key() + "'] is not a string");
            }
            e.metadata.emplace_back(it.key(), it.value().get<std::string>());
        }
    }

    return core::Result<core::LogEntry>::success(std::move(e));
}

core::Result<std::string> read_whole_file(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        return core::Result<std::string>::failure(
            path.string() + ": cannot open file for reading");
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return core::Result<std::string>::success(ss.str());
}

}  // namespace

// ===========================================================================
// Public surface
// ===========================================================================

std::string serialize(const core::GameState& state) {
    json root = json::object();
    root["save_version"]          = kSaveFormatVersion;
    root["rng_algorithm_version"] = kRngAlgorithmVersion;
    root["current_date"]          = state.current_date.to_string();
    // M2.1: player_country is a signed int at the root level (-1 means
    // "no player / headless run"; any non-negative value indexes into
    // the countries array below).
    root["player_country"]        = state.player_country.value();

    json rng = json::object();
    rng["seed"]    = state.rng.seed;
    rng["counter"] = state.rng.counter;
    root["rng"]    = std::move(rng);

    json countries = json::array();
    for (const auto& c : state.countries) {
        countries.push_back(country_to_json(c));
    }
    root["countries"] = std::move(countries);

    // M4.1 provinces block (save v12). The M0 reserved-empty stub
    // is gone — each entry is now a typed ProvinceNode serialised
    // with id_code / name / owner (raw CountryId::value()) / x / y.
    json provinces = json::array();
    for (const auto& p : state.provinces) {
        json entry = json::object();
        entry["id_code"] = p.id_code;
        entry["name"]    = p.name;
        entry["owner"]   = p.owner.value();
        entry["x"]       = p.x;
        entry["y"]       = p.y;
        provinces.push_back(std::move(entry));
    }
    root["provinces"] = std::move(provinces);

    json factions = json::array();
    for (const auto& f : state.factions) {
        factions.push_back(faction_to_json(f));
    }
    root["factions"] = std::move(factions);

    json policies = json::array();
    for (const auto& p : state.policies) {
        policies.push_back(policy_to_json(p));
    }
    root["policies"] = std::move(policies);

    // M5.1 events block (save v13). The M0 reserved-empty stub is
    // gone — each entry is now a typed EventDefinition serialised
    // with id_code / name / description / triggers[] / effects[].
    // Triggers carry the M5.1 schema (target / op / value); effects
    // mirror PolicyEffect's serialised shape (same JSON keys), so
    // a future M5.x effect applicator can reuse policy effect
    // serialization helpers.
    json events_arr = json::array();
    for (const auto& ev : state.events) {
        json entry = json::object();
        entry["id_code"]        = ev.id_code;
        entry["name"]           = ev.name;
        entry["description"]    = ev.description;
        entry["visible_report"] = ev.visible_report;   // M6.2 (RFC-090 §6.2)
        entry["true_cause"]     = ev.true_cause;       // M6.1 (RFC-090 §6.1)
        entry["category"]       = ev.category;         // issue #112 — non-empty
        json triggers_arr = json::array();
        for (const auto& t : ev.triggers) {
            json tj = json::object();
            tj["target"] = t.target;
            tj["op"]     = t.op;
            tj["value"]  = t.value;
            triggers_arr.push_back(std::move(tj));
        }
        entry["triggers"] = std::move(triggers_arr);
        json effects_arr = json::array();
        for (const auto& f : ev.effects) {
            json fj = json::object();
            fj["target"] = f.target;
            fj["op"]     = f.op;
            fj["value"]  = f.value;
            effects_arr.push_back(std::move(fj));
        }
        entry["effects"] = std::move(effects_arr);

        // RCR-1: weight_modifiers (RFC-090 §5.3) — array of
        // {target, op, value, weight_delta}. May be empty.
        json wm_arr = json::array();
        for (const auto& wm : ev.weight_modifiers) {
            json wj = json::object();
            wj["target"]       = wm.target;
            wj["op"]           = wm.op;
            wj["value"]        = wm.value;
            wj["weight_delta"] = wm.weight_delta;
            wm_arr.push_back(std::move(wj));
        }
        entry["weight_modifiers"] = std::move(wm_arr);

        // RCR-1: options (RFC-090 §5.4 / §5.8) — array of
        // {id_code, label, effects[]}. May be empty.
        json opts_arr = json::array();
        for (const auto& opt : ev.options) {
            json oj = json::object();
            oj["id_code"] = opt.id_code;
            oj["label"]   = opt.label;
            json opt_effects = json::array();
            for (const auto& f : opt.effects) {
                json fj = json::object();
                fj["target"] = f.target;
                fj["op"]     = f.op;
                fj["value"]  = f.value;
                opt_effects.push_back(std::move(fj));
            }
            oj["effects"] = std::move(opt_effects);
            opts_arr.push_back(std::move(oj));
        }
        entry["options"] = std::move(opts_arr);

        // RCR-1: followup_event_ids (RFC-090 §5.12) — array of
        // non-empty event id_code strings. May be empty.
        json followup_arr = json::array();
        for (const auto& s : ev.followup_event_ids) {
            followup_arr.push_back(s);
        }
        entry["followup_event_ids"] = std::move(followup_arr);

        // Issue #112: option_effect_mode is SERIALISED ONLY when
        // `options` is non-empty. Pre-v18 saves did not have this
        // field; v18+ enforces "present iff options non-empty" at
        // both load and save sites.
        if (!ev.options.empty()) {
            const char* mode_str = "option_only";
            switch (ev.option_effect_mode) {
                case core::EventOptionEffectMode::OptionOnly:
                    mode_str = "option_only";     break;
                case core::EventOptionEffectMode::BaseThenOption:
                    mode_str = "base_then_option"; break;
                case core::EventOptionEffectMode::OptionThenBase:
                    mode_str = "option_then_base"; break;
            }
            entry["option_effect_mode"] = std::string(mode_str);
        }

        events_arr.push_back(std::move(entry));
    }
    root["events"] = std::move(events_arr);

    // RCR-1: relationships block (RFC-090 §3.6 / §3.7). Array of
    // {from, to, relationship, threat}. May be empty. Save format
    // v17 makes the block required at the save layer.
    json relations_arr = json::array();
    for (const auto& r : state.relationships) {
        json rj = json::object();
        rj["from"]         = r.from.value();
        rj["to"]           = r.to.value();
        rj["relationship"] = r.relationship;
        rj["threat"]       = r.threat;
        relations_arr.push_back(std::move(rj));
    }
    root["relationships"] = std::move(relations_arr);

    // Issue #112 (save v18): pending_player_events. Array of
    // {event_history_index, event_id_code, country_id_code}.
    // Required at the save layer; may be empty.
    json pending_arr = json::array();
    for (const auto& p : state.pending_player_events) {
        json pj = json::object();
        pj["event_history_index"] = p.event_history_index;
        pj["event_id_code"]       = p.event_id_code;
        pj["country_id_code"]     = p.country_id_code;
        pending_arr.push_back(std::move(pj));
    }
    root["pending_player_events"] = std::move(pending_arr);

    // M5.4: event_history array carries fired-event records
    // (EventInstance). M5.4 ships the data layer only — no system
    // creates these records yet (no auto-fire, no effects
    // application). Hand-built entries round-trip through this
    // path for future-firer forward-compat. Per-actor fields are
    // strings (kind / id_code / country_id_code) plus a transient
    // index hint; see entities.hpp for the EventInstance contract.
    json event_history_arr = json::array();
    for (const auto& inst : state.event_history) {
        json entry = json::object();
        entry["event_id_code"] = inst.event_id_code;
        entry["fired_on"]      = inst.fired_on.to_string();
        json actors_arr = json::array();
        for (const auto& a : inst.actors) {
            json aj = json::object();
            aj["kind"]            = a.kind;
            aj["id_code"]         = a.id_code;
            aj["country_id_code"] = a.country_id_code;
            aj["index"]           = a.index;
            actors_arr.push_back(std::move(aj));
        }
        entry["actors"] = std::move(actors_arr);
        event_history_arr.push_back(std::move(entry));
    }
    root["event_history"] = std::move(event_history_arr);

    json logs = json::array();
    for (const auto& e : state.logs) {
        logs.push_back(log_entry_to_json(e));
    }
    root["logs"] = std::move(logs);

    // M2.4 applied_commands block. Each entry: { applied_on,
    // command: { kind, <kind-specific payload> } }. Insertion order
    // matches commands::apply_pending append order. M2.5 added per-
    // kind payload shapes: EnactPolicy emits policy_id_code;
    // AdjustBudget emits budget_category + budget_delta.
    json applied = json::array();
    for (const auto& ac : state.applied_commands) {
        json entry = json::object();
        entry["applied_on"] = ac.applied_on.to_string();
        json cmd_obj = json::object();
        cmd_obj["kind"] = player_command_kind_to_string(ac.command.kind);
        switch (ac.command.kind) {
            case core::PlayerCommandKind::EnactPolicy:
                cmd_obj["policy_id_code"] = ac.command.policy_id_code;
                break;
            case core::PlayerCommandKind::AdjustBudget:
                cmd_obj["budget_category"] = ac.command.budget_category;
                cmd_obj["budget_delta"]    = ac.command.budget_delta;
                break;
            case core::PlayerCommandKind::ChooseEventOption:
                cmd_obj["event_history_index"] =
                    ac.command.event_history_index;
                cmd_obj["option_id_code"] = ac.command.option_id_code;
                break;
        }
        entry["command"] = std::move(cmd_obj);
        applied.push_back(std::move(entry));
    }
    root["applied_commands"] = std::move(applied);

    // M3.1 interest_groups block. Root-level array of POD entries;
    // each entry round-trips kind as its string spelling, the
    // owning country as its numeric handle (CountryId::value), and
    // the three behavioural ratios verbatim. Empty array is valid
    // and is always emitted so the strict v11 loader contract has
    // a stable target.
    json igs = json::array();
    for (const auto& g : state.interest_groups) {
        json entry = json::object();
        entry["id_code"]    = g.id_code;
        entry["name"]       = g.name;
        entry["kind"]       = interest_group_kind_to_string(g.kind);
        entry["country"]    = g.country.value();
        entry["influence"]  = g.influence;
        entry["loyalty"]    = g.loyalty;
        entry["radicalism"] = g.radicalism;
        igs.push_back(std::move(entry));
    }
    root["interest_groups"] = std::move(igs);

    // M7.1 faction_demands block (save v19). Always emitted as
    // an array; empty array is valid. Field order mirrors the
    // FactionDemand struct for read-stable round-trips.
    json fds = json::array();
    for (const auto& d : state.faction_demands) {
        json entry = json::object();
        entry["id_code"]         = d.id_code;
        entry["faction_id_code"] = d.faction_id_code;
        entry["country_id_code"] = d.country_id_code;
        entry["kind"]            =
            leviathan::systems::faction_demands::kind_to_string(d.kind);
        entry["created_on"]      = d.created_on.to_string();
        entry["expires_on"]      = d.expires_on.to_string();
        entry["status"]          =
            leviathan::systems::faction_demands::status_to_string(d.status);
        fds.push_back(std::move(entry));
    }
    root["faction_demands"] = std::move(fds);

    return root.dump(/*indent=*/2);
}

core::Result<bool> save(const core::GameState& state,
                        const std::filesystem::path& path) {
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            return core::Result<bool>::failure(
                path.string() + ": cannot create parent directory: " + ec.message());
        }
    }

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) {
        return core::Result<bool>::failure(
            path.string() + ": cannot open file for writing");
    }
    const std::string text = serialize(state);
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!f.good()) {
        return core::Result<bool>::failure(
            path.string() + ": write failed");
    }
    return core::Result<bool>::success(true);
}

core::Result<core::GameState> deserialize(std::string_view json_text,
                                          std::string_view source_label) {
    json root = json::parse(json_text, /*cb=*/nullptr,
                            /*allow_exceptions=*/false);
    if (root.is_discarded()) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label, "JSON parse error (malformed document)"));
    }
    if (!root.is_object()) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label, "top-level JSON value is not an object"));
    }

    // ---- version gates --------------------------------------------------
    auto save_v = require_u64(root, "save_version", source_label);
    if (!save_v) {
        return core::Result<core::GameState>::failure(std::move(save_v.error()));
    }
    if (save_v.value() != kSaveFormatVersion) {
        std::string msg = "unsupported save_version " +
                          std::to_string(save_v.value()) +
                          " (this binary supports " +
                          std::to_string(kSaveFormatVersion) + ")";
        if (save_v.value() == 17u) {
            msg += "; v17 -> v18 (issue #112) requires per-event "
                   "`category` (non-empty string), per-option-bearing-"
                   "event `option_effect_mode` (option_only / "
                   "base_then_option / option_then_base), and a "
                   "top-level `pending_player_events` array on "
                   "GameState";
        }
        return core::Result<core::GameState>::failure(fmt_err(source_label, msg));
    }

    auto algo_v = require_u64(root, "rng_algorithm_version", source_label);
    if (!algo_v) {
        return core::Result<core::GameState>::failure(std::move(algo_v.error()));
    }
    if (algo_v.value() != kRngAlgorithmVersion) {
        std::string msg = "unsupported rng_algorithm_version " +
                          std::to_string(algo_v.value()) +
                          " (this binary supports " +
                          std::to_string(kRngAlgorithmVersion) + ")";
        return core::Result<core::GameState>::failure(fmt_err(source_label, msg));
    }

    // ---- payload --------------------------------------------------------
    core::GameState state;

    auto date = require_date(root, "current_date", source_label);
    if (!date) {
        return core::Result<core::GameState>::failure(std::move(date.error()));
    }
    state.current_date = date.value();

    auto seed = require_u64(root, "rng.seed", source_label);
    if (!seed) {
        return core::Result<core::GameState>::failure(std::move(seed.error()));
    }
    state.rng.seed = seed.value();

    auto counter = require_u64(root, "rng.counter", source_label);
    if (!counter) {
        return core::Result<core::GameState>::failure(std::move(counter.error()));
    }
    state.rng.counter = counter.value();

    // countries
    if (root.contains("countries")) {
        const auto& arr = root.at("countries");
        if (!arr.is_array()) {
            return core::Result<core::GameState>::failure(
                fmt_err(source_label, "'countries' is not an array"));
        }
        state.countries.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
            auto c = country_from_json(arr[i], i, source_label);
            if (!c) {
                return core::Result<core::GameState>::failure(std::move(c.error()));
            }
            state.countries.push_back(std::move(c).value());
        }
    }

    // player_country (M2.1, required as of v8). Validation happens
    // AFTER countries are loaded so the index range check works
    // against the live country count.
    if (!root.contains("player_country")) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label, "missing required field 'player_country'"));
    }
    const auto& pc_node = root.at("player_country");
    if (!pc_node.is_number_integer()) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label,
                    "'player_country' is not an integer"));
    }
    const auto pc_raw = pc_node.get<std::int64_t>();
    if (pc_raw < -1) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label,
                    "'player_country' must be >= -1 (got " +
                    std::to_string(pc_raw) + ")"));
    }
    using under = core::CountryId::underlying_type;
    constexpr auto kMaxId =
        static_cast<std::int64_t>(std::numeric_limits<under>::max());
    if (pc_raw > kMaxId) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label,
                    "'player_country' " + std::to_string(pc_raw) +
                    " is out of range for CountryId (max " +
                    std::to_string(kMaxId) + ")"));
    }
    if (pc_raw != -1 &&
        static_cast<std::size_t>(pc_raw) >= state.countries.size()) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label,
                    "'player_country' index " + std::to_string(pc_raw) +
                    " is out of range for the " +
                    std::to_string(state.countries.size()) +
                    " loaded countries"));
    }
    state.player_country = core::CountryId{static_cast<under>(pc_raw)};

    // factions
    if (root.contains("factions")) {
        const auto& arr = root.at("factions");
        if (!arr.is_array()) {
            return core::Result<core::GameState>::failure(
                fmt_err(source_label, "'factions' is not an array"));
        }
        state.factions.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
            auto f = faction_from_json(arr[i], i, source_label);
            if (!f) {
                return core::Result<core::GameState>::failure(std::move(f.error()));
            }
            state.factions.push_back(std::move(f).value());
        }
    }

    // policies (M1.4)
    if (root.contains("policies")) {
        const auto& arr = root.at("policies");
        if (!arr.is_array()) {
            return core::Result<core::GameState>::failure(
                fmt_err(source_label, "'policies' is not an array"));
        }
        state.policies.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
            auto p = policy_from_json(arr[i], i, source_label);
            if (!p) {
                return core::Result<core::GameState>::failure(std::move(p.error()));
            }
            state.policies.push_back(std::move(p).value());
        }
    }

    // logs
    if (root.contains("logs")) {
        const auto& arr = root.at("logs");
        if (!arr.is_array()) {
            return core::Result<core::GameState>::failure(
                fmt_err(source_label, "'logs' is not an array"));
        }
        state.logs.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
            auto e = log_entry_from_json(arr[i], i, source_label);
            if (!e) {
                return core::Result<core::GameState>::failure(std::move(e.error()));
            }
            state.logs.push_back(std::move(e).value());
        }
    }

    // M2.4 applied_commands (required as of save format v9). Empty
    // array is fine; missing field is a hard failure (v8 saves
    // dropped here per the v9 history note in save_system.hpp).
    if (!root.contains("applied_commands")) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label,
                    "missing required field 'applied_commands'"));
    }
    const auto& ac_arr = root.at("applied_commands");
    if (!ac_arr.is_array()) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label,
                    "'applied_commands' has wrong type (expected"
                    " JSON array)"));
    }
    state.applied_commands.reserve(ac_arr.size());
    for (std::size_t i = 0; i < ac_arr.size(); ++i) {
        const std::string entry_ctx =
            std::string(source_label) + ": applied_commands[" +
            std::to_string(i) + "]";
        const auto& entry = ac_arr[i];
        if (!entry.is_object()) {
            return core::Result<core::GameState>::failure(
                entry_ctx + ": expected JSON object");
        }

        auto applied_on_r = require_date(entry, "applied_on", entry_ctx);
        if (!applied_on_r) {
            return core::Result<core::GameState>::failure(
                std::move(applied_on_r.error()));
        }

        if (!entry.contains("command")) {
            return core::Result<core::GameState>::failure(
                entry_ctx + ": missing required field 'command'");
        }
        const auto& cmd_obj = entry.at("command");
        if (!cmd_obj.is_object()) {
            return core::Result<core::GameState>::failure(
                entry_ctx + ": 'command' has wrong type (expected"
                " JSON object)");
        }
        auto kind_str = require_string(cmd_obj, "kind", entry_ctx);
        if (!kind_str) {
            return core::Result<core::GameState>::failure(
                std::move(kind_str.error()));
        }
        auto kind = player_command_kind_from_string(kind_str.value());
        if (!kind) {
            return core::Result<core::GameState>::failure(
                entry_ctx + ": 'command.kind': " + kind.error());
        }

        core::AppliedPlayerCommand ac;
        ac.applied_on   = applied_on_r.value();
        ac.command.kind = kind.value();

        // Per-kind payload (M2.5).
        switch (kind.value()) {
            case core::PlayerCommandKind::EnactPolicy: {
                auto policy_id_code =
                    require_string(cmd_obj, "policy_id_code", entry_ctx);
                if (!policy_id_code) {
                    return core::Result<core::GameState>::failure(
                        std::move(policy_id_code.error()));
                }
                ac.command.policy_id_code = policy_id_code.value();
                break;
            }
            case core::PlayerCommandKind::AdjustBudget: {
                auto budget_category =
                    require_string(cmd_obj, "budget_category", entry_ctx);
                if (!budget_category) {
                    return core::Result<core::GameState>::failure(
                        std::move(budget_category.error()));
                }
                auto budget_delta =
                    require_number(cmd_obj, "budget_delta", entry_ctx);
                if (!budget_delta) {
                    return core::Result<core::GameState>::failure(
                        std::move(budget_delta.error()));
                }
                ac.command.budget_category = budget_category.value();
                ac.command.budget_delta    = budget_delta.value();
                break;
            }
            case core::PlayerCommandKind::ChooseEventOption: {
                auto idx_r = require_u64(
                    cmd_obj, "event_history_index", entry_ctx);
                if (!idx_r) {
                    return core::Result<core::GameState>::failure(
                        std::move(idx_r.error()));
                }
                auto oid_r = require_string(
                    cmd_obj, "option_id_code", entry_ctx);
                if (!oid_r) {
                    return core::Result<core::GameState>::failure(
                        std::move(oid_r.error()));
                }
                ac.command.event_history_index =
                    static_cast<std::size_t>(idx_r.value());
                ac.command.option_id_code = oid_r.value();
                break;
            }
        }
        state.applied_commands.push_back(std::move(ac));
    }

    // M3.1 interest_groups block. Required as of save format v11.
    // Empty array is valid; missing key is rejected so a v10 save
    // that simply omits it (which would otherwise silently drop the
    // entire interest-group set) fails loudly. Each entry is fully
    // validated: id_code + name non-empty strings, kind a known
    // enum, country a non-negative index inside `state.countries`,
    // and the three behavioural ratios in [0, 1] via require_ratio.
    if (!root.contains("interest_groups")) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label,
                    "missing required field 'interest_groups'"));
    }
    const auto& ig_arr = root.at("interest_groups");
    if (!ig_arr.is_array()) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label,
                    "'interest_groups' has wrong type (expected"
                    " JSON array)"));
    }
    state.interest_groups.reserve(ig_arr.size());
    for (std::size_t i = 0; i < ig_arr.size(); ++i) {
        const std::string ig_ctx =
            std::string(source_label) + ": interest_groups[" +
            std::to_string(i) + "]";
        const auto& entry = ig_arr[i];
        if (!entry.is_object()) {
            return core::Result<core::GameState>::failure(
                ig_ctx + ": expected JSON object");
        }
        auto id_code_r = require_string(entry, "id_code", ig_ctx);
        if (!id_code_r) {
            return core::Result<core::GameState>::failure(
                std::move(id_code_r.error()));
        }
        if (id_code_r.value().empty()) {
            return core::Result<core::GameState>::failure(
                ig_ctx + ": 'id_code' must be non-empty");
        }
        auto name_r = require_string(entry, "name", ig_ctx);
        if (!name_r) {
            return core::Result<core::GameState>::failure(
                std::move(name_r.error()));
        }
        if (name_r.value().empty()) {
            return core::Result<core::GameState>::failure(
                ig_ctx + ": 'name' must be non-empty");
        }
        auto kind_r = require_string(entry, "kind", ig_ctx);
        if (!kind_r) {
            return core::Result<core::GameState>::failure(
                std::move(kind_r.error()));
        }
        auto kind = interest_group_kind_from_string(kind_r.value());
        if (!kind) {
            return core::Result<core::GameState>::failure(
                ig_ctx + ": 'kind': " + kind.error());
        }
        auto country_r = require_u64(entry, "country", ig_ctx);
        if (!country_r) {
            return core::Result<core::GameState>::failure(
                std::move(country_r.error()));
        }
        using under = core::CountryId::underlying_type;
        constexpr auto kIntMax =
            static_cast<std::uint64_t>(
                std::numeric_limits<under>::max());
        if (country_r.value() > kIntMax) {
            return core::Result<core::GameState>::failure(
                ig_ctx + ": 'country' is out of range for CountryId");
        }
        const auto country_int = static_cast<under>(country_r.value());
        if (country_int < 0 ||
            static_cast<std::size_t>(country_int) >=
                state.countries.size()) {
            return core::Result<core::GameState>::failure(
                ig_ctx + ": 'country' " + std::to_string(country_int) +
                " is not a valid index into state.countries");
        }
        auto influence_r  = require_ratio(entry, "influence",  ig_ctx);
        if (!influence_r) {
            return core::Result<core::GameState>::failure(
                std::move(influence_r.error()));
        }
        auto loyalty_r    = require_ratio(entry, "loyalty",    ig_ctx);
        if (!loyalty_r) {
            return core::Result<core::GameState>::failure(
                std::move(loyalty_r.error()));
        }
        auto radicalism_r = require_ratio(entry, "radicalism", ig_ctx);
        if (!radicalism_r) {
            return core::Result<core::GameState>::failure(
                std::move(radicalism_r.error()));
        }

        // Duplicate id_code is a hard failure — the save layer is the
        // strictest gate and a duplicate breaks identity-based lookups
        // any future system would need.
        for (const auto& existing : state.interest_groups) {
            if (existing.id_code == id_code_r.value()) {
                return core::Result<core::GameState>::failure(
                    ig_ctx + ": duplicate 'id_code' '" +
                    id_code_r.value() + "'");
            }
        }

        core::InterestGroupState g;
        g.id_code    = std::move(id_code_r).value();
        g.name       = std::move(name_r).value();
        g.kind       = kind.value();
        g.country    = core::CountryId{country_int};
        g.influence  = influence_r.value();
        g.loyalty    = loyalty_r.value();
        g.radicalism = radicalism_r.value();
        state.interest_groups.push_back(std::move(g));
    }

    // M4.1 provinces block. Required as of save format v12. Empty
    // array is valid; missing key is rejected so a v11 save's
    // reserved-empty `provinces` stub (or any old binary that
    // simply omits the field) fails loudly rather than silently
    // dropping any map nodes the user authored. Each entry is
    // fully validated: id_code + name non-empty strings, owner a
    // non-negative integer indexing into state.countries (no
    // unowned nodes in v12), and x / y finite in [0, 1].
    if (!root.contains("provinces")) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label,
                    "missing required field 'provinces'"));
    }
    const auto& pv_arr = root.at("provinces");
    if (!pv_arr.is_array()) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label,
                    "'provinces' has wrong type (expected JSON array)"));
    }
    state.provinces.reserve(pv_arr.size());
    for (std::size_t i = 0; i < pv_arr.size(); ++i) {
        const std::string pv_ctx =
            std::string(source_label) + ": provinces[" +
            std::to_string(i) + "]";
        const auto& entry = pv_arr[i];
        if (!entry.is_object()) {
            return core::Result<core::GameState>::failure(
                pv_ctx + ": expected JSON object");
        }
        auto id_code_r = require_string(entry, "id_code", pv_ctx);
        if (!id_code_r) {
            return core::Result<core::GameState>::failure(
                std::move(id_code_r.error()));
        }
        if (id_code_r.value().empty()) {
            return core::Result<core::GameState>::failure(
                pv_ctx + ": 'id_code' must be non-empty");
        }
        auto name_r = require_string(entry, "name", pv_ctx);
        if (!name_r) {
            return core::Result<core::GameState>::failure(
                std::move(name_r.error()));
        }
        if (name_r.value().empty()) {
            return core::Result<core::GameState>::failure(
                pv_ctx + ": 'name' must be non-empty");
        }
        auto owner_r = require_u64(entry, "owner", pv_ctx);
        if (!owner_r) {
            return core::Result<core::GameState>::failure(
                std::move(owner_r.error()));
        }
        using under = core::CountryId::underlying_type;
        constexpr auto kIntMax =
            static_cast<std::uint64_t>(
                std::numeric_limits<under>::max());
        if (owner_r.value() > kIntMax) {
            return core::Result<core::GameState>::failure(
                pv_ctx + ": 'owner' is out of range for CountryId");
        }
        const auto owner_int = static_cast<under>(owner_r.value());
        if (owner_int < 0 ||
            static_cast<std::size_t>(owner_int) >=
                state.countries.size()) {
            return core::Result<core::GameState>::failure(
                pv_ctx + ": 'owner' " + std::to_string(owner_int) +
                " is not a valid index into state.countries");
        }
        auto x_r = require_ratio(entry, "x", pv_ctx);
        if (!x_r) {
            return core::Result<core::GameState>::failure(
                std::move(x_r.error()));
        }
        auto y_r = require_ratio(entry, "y", pv_ctx);
        if (!y_r) {
            return core::Result<core::GameState>::failure(
                std::move(y_r.error()));
        }

        // Duplicate id_code rejected at the save layer — the
        // strictest gate, mirrors the M3.1 interest_groups rule.
        for (const auto& existing : state.provinces) {
            if (existing.id_code == id_code_r.value()) {
                return core::Result<core::GameState>::failure(
                    pv_ctx + ": duplicate 'id_code' '" +
                    id_code_r.value() + "'");
            }
        }

        core::ProvinceNode p;
        p.id_code = std::move(id_code_r).value();
        p.name    = std::move(name_r).value();
        p.owner   = core::CountryId{owner_int};
        p.x       = x_r.value();
        p.y       = y_r.value();
        state.provinces.push_back(std::move(p));
    }

    // M5.1 events block. Required as of save format v13. Empty
    // array is valid; missing key is rejected so a v12 save's
    // reserved-empty `events` stub fails loudly under the strict
    // version gate (which already rejects v12 saves above) AND
    // any future hand-crafted v13 save that simply omits the
    // field also fails loudly rather than silently dropping the
    // user-authored event definitions. Each entry is fully
    // validated: id_code + name non-empty strings, description a
    // string (may be empty), triggers a non-empty array (each
    // with allowlisted target / op + finite value), effects an
    // array (may be empty; each entry matches PolicyEffect
    // load-time rules — required target/op strings, finite
    // value, no target/op allowlist at load). Cross-save
    // duplicate id_code rejected.
    static const std::vector<std::string> kTriggerTargetsSave = {
        "country.stability",
        "country.legitimacy",
        "country.government_authority.bureaucratic_compliance",
        "interest_group.radicalism",
        "interest_group.loyalty",
    };
    static const std::vector<std::string> kTriggerOpsSave = {
        "lt", "lte", "gt", "gte",
    };
    auto is_allowed_save = [](const std::vector<std::string>& list,
                              const std::string& v) {
        for (const auto& s : list) {
            if (s == v) { return true; }
        }
        return false;
    };
    if (!root.contains("events")) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label,
                    "missing required field 'events'"));
    }
    const auto& ev_arr = root.at("events");
    if (!ev_arr.is_array()) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label,
                    "'events' has wrong type (expected JSON array)"));
    }
    state.events.reserve(ev_arr.size());
    for (std::size_t i = 0; i < ev_arr.size(); ++i) {
        const std::string ev_ctx =
            std::string(source_label) + ": events[" +
            std::to_string(i) + "]";
        const auto& entry = ev_arr[i];
        if (!entry.is_object()) {
            return core::Result<core::GameState>::failure(
                ev_ctx + ": expected JSON object");
        }
        auto id_code_r = require_string(entry, "id_code", ev_ctx);
        if (!id_code_r) {
            return core::Result<core::GameState>::failure(
                std::move(id_code_r.error()));
        }
        if (id_code_r.value().empty()) {
            return core::Result<core::GameState>::failure(
                ev_ctx + ": 'id_code' must be non-empty");
        }
        auto name_r = require_string(entry, "name", ev_ctx);
        if (!name_r) {
            return core::Result<core::GameState>::failure(
                std::move(name_r.error()));
        }
        if (name_r.value().empty()) {
            return core::Result<core::GameState>::failure(
                ev_ctx + ": 'name' must be non-empty");
        }
        // description: required string but may be empty.
        if (!entry.contains("description")
            || !entry.at("description").is_string()) {
            return core::Result<core::GameState>::failure(
                ev_ctx + ": 'description' missing or not a string");
        }
        const std::string desc =
            entry.at("description").get<std::string>();

        // M6.2 (RFC-090 §6.2): visible_report is required
        // non-empty. A v15 save's events[] entries lacked
        // visible_report; the M6.2 save bump (v15 -> v16) makes
        // this field required. Silently defaulting to an empty
        // string on reload would erase author intent (the
        // public-facing fired-report description). Same
        // rejection style as M6.1 true_cause.
        if (!entry.contains("visible_report")
            || !entry.at("visible_report").is_string()) {
            return core::Result<core::GameState>::failure(
                ev_ctx + ": 'visible_report' missing or not a string");
        }
        const std::string visible_report =
            entry.at("visible_report").get<std::string>();
        if (visible_report.empty()) {
            return core::Result<core::GameState>::failure(
                ev_ctx + ": 'visible_report' must be non-empty");
        }

        // M6.1 (RFC-090 §6.1): true_cause is required non-empty.
        // A v14 save's events[] entries lacked true_cause; the
        // M6.1 save bump (v14 -> v15) makes this field required.
        // Silently defaulting to an empty string on reload would
        // erase author intent (the author-written truth
        // narrative).
        if (!entry.contains("true_cause")
            || !entry.at("true_cause").is_string()) {
            return core::Result<core::GameState>::failure(
                ev_ctx + ": 'true_cause' missing or not a string");
        }
        const std::string true_cause =
            entry.at("true_cause").get<std::string>();
        if (true_cause.empty()) {
            return core::Result<core::GameState>::failure(
                ev_ctx + ": 'true_cause' must be non-empty");
        }

        // Issue #112 (save v18): category is required non-empty.
        if (!entry.contains("category")
            || !entry.at("category").is_string()) {
            return core::Result<core::GameState>::failure(
                ev_ctx + ": 'category' missing or not a string");
        }
        const std::string category =
            entry.at("category").get<std::string>();
        if (category.empty()) {
            return core::Result<core::GameState>::failure(
                ev_ctx + ": 'category' must be non-empty");
        }

        // triggers: required, non-empty array.
        if (!entry.contains("triggers")
            || !entry.at("triggers").is_array()) {
            return core::Result<core::GameState>::failure(
                ev_ctx + ": 'triggers' missing or not an array");
        }
        const auto& trig_arr = entry.at("triggers");
        if (trig_arr.empty()) {
            return core::Result<core::GameState>::failure(
                ev_ctx + ": 'triggers' must be non-empty");
        }
        std::vector<core::EventTrigger> triggers;
        triggers.reserve(trig_arr.size());
        for (std::size_t ti = 0; ti < trig_arr.size(); ++ti) {
            const std::string tctx =
                ev_ctx + ": triggers[" + std::to_string(ti) + "]";
            const auto& t = trig_arr[ti];
            if (!t.is_object()) {
                return core::Result<core::GameState>::failure(
                    tctx + ": expected JSON object");
            }
            auto target_r = require_string(t, "target", tctx);
            if (!target_r) {
                return core::Result<core::GameState>::failure(
                    std::move(target_r.error()));
            }
            if (!is_allowed_save(kTriggerTargetsSave, target_r.value())) {
                return core::Result<core::GameState>::failure(
                    tctx + ": 'target' '" + target_r.value() +
                    "' is not in the M5.1 allowlist");
            }
            auto op_r = require_string(t, "op", tctx);
            if (!op_r) {
                return core::Result<core::GameState>::failure(
                    std::move(op_r.error()));
            }
            if (!is_allowed_save(kTriggerOpsSave, op_r.value())) {
                return core::Result<core::GameState>::failure(
                    tctx + ": 'op' '" + op_r.value() +
                    "' is not in the M5.1 allowlist"
                    " (lt, lte, gt, gte)");
            }
            auto value_r = require_number(t, "value", tctx);
            if (!value_r) {
                return core::Result<core::GameState>::failure(
                    std::move(value_r.error()));
            }
            core::EventTrigger trig;
            trig.target = std::move(target_r).value();
            trig.op     = std::move(op_r).value();
            trig.value  = value_r.value();
            triggers.push_back(std::move(trig));
        }

        // effects: required array, may be empty.
        if (!entry.contains("effects")
            || !entry.at("effects").is_array()) {
            return core::Result<core::GameState>::failure(
                ev_ctx + ": 'effects' missing or not an array");
        }
        const auto& eff_arr = entry.at("effects");
        std::vector<core::PolicyEffect> effects;
        effects.reserve(eff_arr.size());
        for (std::size_t ei = 0; ei < eff_arr.size(); ++ei) {
            const std::string ectx =
                ev_ctx + ": effects[" + std::to_string(ei) + "]";
            const auto& f = eff_arr[ei];
            if (!f.is_object()) {
                return core::Result<core::GameState>::failure(
                    ectx + ": expected JSON object");
            }
            auto target_r = require_string(f, "target", ectx);
            if (!target_r) {
                return core::Result<core::GameState>::failure(
                    std::move(target_r.error()));
            }
            auto op_r = require_string(f, "op", ectx);
            if (!op_r) {
                return core::Result<core::GameState>::failure(
                    std::move(op_r.error()));
            }
            auto value_r = require_number(f, "value", ectx);
            if (!value_r) {
                return core::Result<core::GameState>::failure(
                    std::move(value_r.error()));
            }
            core::PolicyEffect eff;
            eff.target = std::move(target_r).value();
            eff.op     = std::move(op_r).value();
            eff.value  = value_r.value();
            effects.push_back(std::move(eff));
        }

        // RCR-1: weight_modifiers (RFC-090 §5.3). Required array
        // at the save layer; may be empty. Per entry: target / op
        // strings (free-form at this layer, mirrors triggers'
        // shape — full target/op allowlist applies at the
        // event_evaluator::rank_weighted_events consumer side),
        // value finite, weight_delta finite.
        if (!entry.contains("weight_modifiers")
            || !entry.at("weight_modifiers").is_array()) {
            return core::Result<core::GameState>::failure(
                ev_ctx + ": 'weight_modifiers' missing or not an array");
        }
        const auto& wm_arr = entry.at("weight_modifiers");
        std::vector<core::WeightModifier> weight_modifiers;
        weight_modifiers.reserve(wm_arr.size());
        for (std::size_t wi = 0; wi < wm_arr.size(); ++wi) {
            const std::string wctx =
                ev_ctx + ": weight_modifiers[" + std::to_string(wi) + "]";
            const auto& w = wm_arr[wi];
            if (!w.is_object()) {
                return core::Result<core::GameState>::failure(
                    wctx + ": expected JSON object");
            }
            auto target_r = require_string(w, "target", wctx);
            if (!target_r) {
                return core::Result<core::GameState>::failure(
                    std::move(target_r.error()));
            }
            auto op_r = require_string(w, "op", wctx);
            if (!op_r) {
                return core::Result<core::GameState>::failure(
                    std::move(op_r.error()));
            }
            auto value_r = require_number(w, "value", wctx);
            if (!value_r) {
                return core::Result<core::GameState>::failure(
                    std::move(value_r.error()));
            }
            auto delta_r = require_number(w, "weight_delta", wctx);
            if (!delta_r) {
                return core::Result<core::GameState>::failure(
                    std::move(delta_r.error()));
            }
            core::WeightModifier wm;
            wm.target       = std::move(target_r).value();
            wm.op           = std::move(op_r).value();
            wm.value        = value_r.value();
            wm.weight_delta = delta_r.value();
            weight_modifiers.push_back(std::move(wm));
        }

        // RCR-1: options (RFC-090 §5.4 / §5.8). Required array
        // at the save layer; may be empty. Per entry: id_code /
        // label strings (id_code non-empty), effects[] mirrors
        // EventDefinition.effects shape.
        if (!entry.contains("options")
            || !entry.at("options").is_array()) {
            return core::Result<core::GameState>::failure(
                ev_ctx + ": 'options' missing or not an array");
        }
        const auto& opts_arr = entry.at("options");
        std::vector<core::EventOption> options;
        options.reserve(opts_arr.size());
        for (std::size_t oi = 0; oi < opts_arr.size(); ++oi) {
            const std::string octx =
                ev_ctx + ": options[" + std::to_string(oi) + "]";
            const auto& o = opts_arr[oi];
            if (!o.is_object()) {
                return core::Result<core::GameState>::failure(
                    octx + ": expected JSON object");
            }
            auto oid_r = require_string(o, "id_code", octx);
            if (!oid_r) {
                return core::Result<core::GameState>::failure(
                    std::move(oid_r.error()));
            }
            if (oid_r.value().empty()) {
                return core::Result<core::GameState>::failure(
                    octx + ": 'id_code' must be non-empty");
            }
            auto label_r = require_string(o, "label", octx);
            if (!label_r) {
                return core::Result<core::GameState>::failure(
                    std::move(label_r.error()));
            }
            if (!o.contains("effects") || !o.at("effects").is_array()) {
                return core::Result<core::GameState>::failure(
                    octx + ": 'effects' missing or not an array");
            }
            const auto& opt_eff_arr = o.at("effects");
            std::vector<core::PolicyEffect> opt_effects;
            opt_effects.reserve(opt_eff_arr.size());
            for (std::size_t oei = 0; oei < opt_eff_arr.size(); ++oei) {
                const std::string oectx =
                    octx + ": effects[" + std::to_string(oei) + "]";
                const auto& f = opt_eff_arr[oei];
                if (!f.is_object()) {
                    return core::Result<core::GameState>::failure(
                        oectx + ": expected JSON object");
                }
                auto tr = require_string(f, "target", oectx);
                if (!tr) {
                    return core::Result<core::GameState>::failure(
                        std::move(tr.error()));
                }
                auto opr = require_string(f, "op", oectx);
                if (!opr) {
                    return core::Result<core::GameState>::failure(
                        std::move(opr.error()));
                }
                auto vr = require_number(f, "value", oectx);
                if (!vr) {
                    return core::Result<core::GameState>::failure(
                        std::move(vr.error()));
                }
                core::PolicyEffect eff;
                eff.target = std::move(tr).value();
                eff.op     = std::move(opr).value();
                eff.value  = vr.value();
                opt_effects.push_back(std::move(eff));
            }
            core::EventOption opt;
            opt.id_code = std::move(oid_r).value();
            opt.label   = std::move(label_r).value();
            opt.effects = std::move(opt_effects);
            options.push_back(std::move(opt));
        }

        // RCR-1: followup_event_ids (RFC-090 §5.12). Required
        // array at the save layer; may be empty. Each entry must
        // be a non-empty string. Cross-reference to state.events
        // entries is NOT enforced at the save layer (same
        // tolerance as event_history.event_id_code).
        if (!entry.contains("followup_event_ids")
            || !entry.at("followup_event_ids").is_array()) {
            return core::Result<core::GameState>::failure(
                ev_ctx + ": 'followup_event_ids' missing or not an array");
        }
        const auto& fu_arr = entry.at("followup_event_ids");
        std::vector<std::string> followup_event_ids;
        followup_event_ids.reserve(fu_arr.size());
        for (std::size_t fi = 0; fi < fu_arr.size(); ++fi) {
            const std::string fctx =
                ev_ctx + ": followup_event_ids[" + std::to_string(fi) + "]";
            const auto& v = fu_arr[fi];
            if (!v.is_string()) {
                return core::Result<core::GameState>::failure(
                    fctx + ": expected string");
            }
            std::string s = v.get<std::string>();
            if (s.empty()) {
                return core::Result<core::GameState>::failure(
                    fctx + ": must be non-empty");
            }
            followup_event_ids.push_back(std::move(s));
        }

        // Issue #112 (save v18): option_effect_mode is required
        // iff `options` is non-empty; rejected if present alongside
        // empty options.
        core::EventOptionEffectMode option_mode =
            core::EventOptionEffectMode::OptionOnly;
        const bool has_options = !options.empty();
        const bool has_mode_key = entry.contains("option_effect_mode");
        if (has_options && !has_mode_key) {
            return core::Result<core::GameState>::failure(
                ev_ctx + ": 'option_effect_mode' required when "
                "'options' is non-empty");
        }
        if (!has_options && has_mode_key) {
            return core::Result<core::GameState>::failure(
                ev_ctx + ": 'option_effect_mode' must be absent "
                "when 'options' is empty");
        }
        if (has_options) {
            if (!entry.at("option_effect_mode").is_string()) {
                return core::Result<core::GameState>::failure(
                    ev_ctx + ": 'option_effect_mode' must be a string");
            }
            const std::string mode_str =
                entry.at("option_effect_mode").get<std::string>();
            if (mode_str == "option_only") {
                option_mode = core::EventOptionEffectMode::OptionOnly;
            } else if (mode_str == "base_then_option") {
                option_mode = core::EventOptionEffectMode::BaseThenOption;
            } else if (mode_str == "option_then_base") {
                option_mode = core::EventOptionEffectMode::OptionThenBase;
            } else {
                return core::Result<core::GameState>::failure(
                    ev_ctx + ": 'option_effect_mode' must be one of "
                    "option_only / base_then_option / option_then_base");
            }
        }

        // Duplicate id_code rejected at the save layer — mirrors
        // the M3.1 interest_groups + M4.1 provinces rule.
        for (const auto& existing : state.events) {
            if (existing.id_code == id_code_r.value()) {
                return core::Result<core::GameState>::failure(
                    ev_ctx + ": duplicate 'id_code' '" +
                    id_code_r.value() + "'");
            }
        }

        core::EventDefinition ev;
        ev.id_code            = std::move(id_code_r).value();
        ev.name               = std::move(name_r).value();
        ev.description        = desc;
        ev.visible_report     = visible_report;   // M6.2
        ev.true_cause         = true_cause;       // M6.1
        ev.category           = category;         // issue #112
        ev.triggers           = std::move(triggers);
        ev.effects            = std::move(effects);
        ev.weight_modifiers   = std::move(weight_modifiers);   // RCR-1
        ev.options            = std::move(options);            // RCR-1
        ev.followup_event_ids = std::move(followup_event_ids); // RCR-1
        ev.option_effect_mode = option_mode;                   // issue #112
        state.events.push_back(std::move(ev));
    }

    // M5.4: event_history array. Required at the save layer (empty
    // array allowed) so a malformed v14 save (missing the key
    // outright) fails loudly rather than silently dropping any
    // hand-authored M5.4 history fixtures or future M5.x firer
    // output. Each entry: { event_id_code, fired_on, actors[] }.
    // Actor kind allowlist {"country", "interest_group"}; id_code
    // and country_id_code required non-empty; index required
    // non-negative integer. M5.4 does NOT cross-check that
    // event_id_code resolves to an entry in state.events — that
    // would prevent the legitimate "load this save into a
    // different scenario manifest" case.
    static const std::vector<std::string> kEventHistoryActorKindsSave = {
        "country", "interest_group",
    };
    auto is_actor_kind_allowed = [](const std::string& v) {
        for (const auto& s : kEventHistoryActorKindsSave) {
            if (s == v) { return true; }
        }
        return false;
    };
    if (!root.contains("event_history")) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label,
                    "missing required field 'event_history'"));
    }
    const auto& eh_arr = root.at("event_history");
    if (!eh_arr.is_array()) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label,
                    "'event_history' has wrong type (expected JSON array)"));
    }
    state.event_history.reserve(eh_arr.size());
    for (std::size_t i = 0; i < eh_arr.size(); ++i) {
        const std::string eh_ctx =
            std::string(source_label) + ": event_history[" +
            std::to_string(i) + "]";
        const auto& entry = eh_arr[i];
        if (!entry.is_object()) {
            return core::Result<core::GameState>::failure(
                eh_ctx + ": expected JSON object");
        }

        auto event_id_code_r = require_string(entry, "event_id_code", eh_ctx);
        if (!event_id_code_r) {
            return core::Result<core::GameState>::failure(
                std::move(event_id_code_r.error()));
        }
        if (event_id_code_r.value().empty()) {
            return core::Result<core::GameState>::failure(
                eh_ctx + ": 'event_id_code' must be non-empty");
        }

        auto fired_on_r = require_date(entry, "fired_on", eh_ctx);
        if (!fired_on_r) {
            return core::Result<core::GameState>::failure(
                std::move(fired_on_r.error()));
        }

        if (!entry.contains("actors") || !entry.at("actors").is_array()) {
            return core::Result<core::GameState>::failure(
                eh_ctx + ": 'actors' missing or not an array");
        }
        const auto& actors_arr = entry.at("actors");
        std::vector<core::EventInstanceActor> actors;
        actors.reserve(actors_arr.size());
        for (std::size_t ai = 0; ai < actors_arr.size(); ++ai) {
            const std::string actx =
                eh_ctx + ": actors[" + std::to_string(ai) + "]";
            const auto& a = actors_arr[ai];
            if (!a.is_object()) {
                return core::Result<core::GameState>::failure(
                    actx + ": expected JSON object");
            }
            auto kind_r = require_string(a, "kind", actx);
            if (!kind_r) {
                return core::Result<core::GameState>::failure(
                    std::move(kind_r.error()));
            }
            if (!is_actor_kind_allowed(kind_r.value())) {
                return core::Result<core::GameState>::failure(
                    actx + ": 'kind' '" + kind_r.value() +
                    "' is not in the M5.4 allowlist"
                    " (country, interest_group)");
            }
            auto id_code_r2 = require_string(a, "id_code", actx);
            if (!id_code_r2) {
                return core::Result<core::GameState>::failure(
                    std::move(id_code_r2.error()));
            }
            if (id_code_r2.value().empty()) {
                return core::Result<core::GameState>::failure(
                    actx + ": 'id_code' must be non-empty");
            }
            auto country_id_code_r =
                require_string(a, "country_id_code", actx);
            if (!country_id_code_r) {
                return core::Result<core::GameState>::failure(
                    std::move(country_id_code_r.error()));
            }
            if (country_id_code_r.value().empty()) {
                return core::Result<core::GameState>::failure(
                    actx + ": 'country_id_code' must be non-empty");
            }
            auto index_r = require_u64(a, "index", actx);
            if (!index_r) {
                return core::Result<core::GameState>::failure(
                    std::move(index_r.error()));
            }
            core::EventInstanceActor av;
            av.kind            = std::move(kind_r).value();
            av.id_code         = std::move(id_code_r2).value();
            av.country_id_code = std::move(country_id_code_r).value();
            av.index           = static_cast<std::size_t>(index_r.value());
            actors.push_back(std::move(av));
        }

        core::EventInstance inst;
        inst.event_id_code = std::move(event_id_code_r).value();
        inst.fired_on      = fired_on_r.value();
        inst.actors        = std::move(actors);
        state.event_history.push_back(std::move(inst));
    }

    // RCR-1: relationships block (RFC-090 §3.6 / §3.7). Required
    // at the save layer (v17); may be empty. Each entry: from /
    // to as integers indexing into state.countries (both validated
    // against state.countries.size() so a corrupted save fails
    // loudly), relationship as finite double in [-1, 1], threat
    // as finite double in [0, 1].
    if (!root.contains("relationships")) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label,
                    "missing required field 'relationships'"));
    }
    const auto& rel_arr = root.at("relationships");
    if (!rel_arr.is_array()) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label,
                    "'relationships' has wrong type (expected JSON array)"));
    }
    state.relationships.reserve(rel_arr.size());
    for (std::size_t i = 0; i < rel_arr.size(); ++i) {
        const std::string rctx =
            std::string(source_label) + ": relationships[" +
            std::to_string(i) + "]";
        const auto& entry = rel_arr[i];
        if (!entry.is_object()) {
            return core::Result<core::GameState>::failure(
                rctx + ": expected JSON object");
        }
        auto from_r = require_number(entry, "from", rctx);
        if (!from_r) {
            return core::Result<core::GameState>::failure(
                std::move(from_r.error()));
        }
        auto to_r = require_number(entry, "to", rctx);
        if (!to_r) {
            return core::Result<core::GameState>::failure(
                std::move(to_r.error()));
        }
        auto rel_r = require_number(entry, "relationship", rctx);
        if (!rel_r) {
            return core::Result<core::GameState>::failure(
                std::move(rel_r.error()));
        }
        auto threat_r = require_number(entry, "threat", rctx);
        if (!threat_r) {
            return core::Result<core::GameState>::failure(
                std::move(threat_r.error()));
        }
        const double from_d = from_r.value();
        const double to_d   = to_r.value();
        if (from_d < 0.0 || from_d != static_cast<int>(from_d)) {
            return core::Result<core::GameState>::failure(
                rctx + ": 'from' must be a non-negative integer");
        }
        if (to_d < 0.0 || to_d != static_cast<int>(to_d)) {
            return core::Result<core::GameState>::failure(
                rctx + ": 'to' must be a non-negative integer");
        }
        const std::size_t from_idx = static_cast<std::size_t>(from_d);
        const std::size_t to_idx   = static_cast<std::size_t>(to_d);
        if (from_idx >= state.countries.size()) {
            return core::Result<core::GameState>::failure(
                rctx + ": 'from' (" + std::to_string(from_idx) +
                ") out of range for countries.size() (" +
                std::to_string(state.countries.size()) + ")");
        }
        if (to_idx >= state.countries.size()) {
            return core::Result<core::GameState>::failure(
                rctx + ": 'to' (" + std::to_string(to_idx) +
                ") out of range for countries.size() (" +
                std::to_string(state.countries.size()) + ")");
        }
        const double rel_v    = rel_r.value();
        const double threat_v = threat_r.value();
        if (rel_v < -1.0 || rel_v > 1.0) {
            return core::Result<core::GameState>::failure(
                rctx + ": 'relationship' must be in [-1, 1]");
        }
        if (threat_v < 0.0 || threat_v > 1.0) {
            return core::Result<core::GameState>::failure(
                rctx + ": 'threat' must be in [0, 1]");
        }
        core::CountryRelation r;
        r.from         = core::CountryId{static_cast<core::CountryId::underlying_type>(from_idx)};
        r.to           = core::CountryId{static_cast<core::CountryId::underlying_type>(to_idx)};
        r.relationship = rel_v;
        r.threat       = threat_v;
        state.relationships.push_back(std::move(r));
    }

    // Issue #112 (save v18): pending_player_events. Required at the
    // save layer; may be empty. Each entry:
    //   { event_history_index, event_id_code, country_id_code }
    // event_history_index validated against state.event_history.size().
    if (!root.contains("pending_player_events")) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label,
                    "missing required field 'pending_player_events'"));
    }
    const auto& pending_arr = root.at("pending_player_events");
    if (!pending_arr.is_array()) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label,
                    "'pending_player_events' has wrong type (expected JSON array)"));
    }
    state.pending_player_events.reserve(pending_arr.size());
    for (std::size_t i = 0; i < pending_arr.size(); ++i) {
        const std::string pctx =
            std::string(source_label) + ": pending_player_events[" +
            std::to_string(i) + "]";
        const auto& entry = pending_arr[i];
        if (!entry.is_object()) {
            return core::Result<core::GameState>::failure(
                pctx + ": expected JSON object");
        }
        auto idx_r = require_u64(entry, "event_history_index", pctx);
        if (!idx_r) {
            return core::Result<core::GameState>::failure(
                std::move(idx_r.error()));
        }
        if (idx_r.value() >= state.event_history.size()) {
            return core::Result<core::GameState>::failure(
                pctx + ": 'event_history_index' (" +
                std::to_string(idx_r.value()) +
                ") out of range for event_history.size() (" +
                std::to_string(state.event_history.size()) + ")");
        }
        auto eid_r = require_string(entry, "event_id_code", pctx);
        if (!eid_r) {
            return core::Result<core::GameState>::failure(
                std::move(eid_r.error()));
        }
        if (eid_r.value().empty()) {
            return core::Result<core::GameState>::failure(
                pctx + ": 'event_id_code' must be non-empty");
        }
        auto cid_r = require_string(entry, "country_id_code", pctx);
        if (!cid_r) {
            return core::Result<core::GameState>::failure(
                std::move(cid_r.error()));
        }
        if (cid_r.value().empty()) {
            return core::Result<core::GameState>::failure(
                pctx + ": 'country_id_code' must be non-empty");
        }
        core::PendingPlayerEvent p;
        p.event_history_index = static_cast<std::size_t>(idx_r.value());
        p.event_id_code       = std::move(eid_r).value();
        p.country_id_code     = std::move(cid_r).value();
        state.pending_player_events.push_back(std::move(p));
    }

    // M7.1 (save v19): faction_demands. Required at the save layer;
    // may be empty. Each entry:
    //   { id_code, faction_id_code, country_id_code,
    //     kind, created_on, expires_on, status }
    // All fields strictly required; kind / status closed-allowlist;
    // dates parsed via GameDate::parse; faction_id_code and
    // country_id_code cross-checked against state.factions /
    // state.countries; the (faction, country) link is verified.
    if (!root.contains("faction_demands")) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label,
                    "missing required field 'faction_demands'"));
    }
    const auto& fd_arr = root.at("faction_demands");
    if (!fd_arr.is_array()) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label,
                    "'faction_demands' has wrong type (expected JSON array)"));
    }
    state.faction_demands.reserve(fd_arr.size());
    for (std::size_t i = 0; i < fd_arr.size(); ++i) {
        const std::string dctx =
            std::string(source_label) + ": faction_demands[" +
            std::to_string(i) + "]";
        const auto& entry = fd_arr[i];
        if (!entry.is_object()) {
            return core::Result<core::GameState>::failure(
                dctx + ": expected JSON object");
        }
        auto id_r = require_string(entry, "id_code", dctx);
        if (!id_r) {
            return core::Result<core::GameState>::failure(
                std::move(id_r.error()));
        }
        if (id_r.value().empty()) {
            return core::Result<core::GameState>::failure(
                dctx + ": 'id_code' must be non-empty");
        }
        auto fid_r = require_string(entry, "faction_id_code", dctx);
        if (!fid_r) {
            return core::Result<core::GameState>::failure(
                std::move(fid_r.error()));
        }
        if (fid_r.value().empty()) {
            return core::Result<core::GameState>::failure(
                dctx + ": 'faction_id_code' must be non-empty");
        }
        auto cid_r = require_string(entry, "country_id_code", dctx);
        if (!cid_r) {
            return core::Result<core::GameState>::failure(
                std::move(cid_r.error()));
        }
        if (cid_r.value().empty()) {
            return core::Result<core::GameState>::failure(
                dctx + ": 'country_id_code' must be non-empty");
        }
        auto kind_r = require_string(entry, "kind", dctx);
        if (!kind_r) {
            return core::Result<core::GameState>::failure(
                std::move(kind_r.error()));
        }
        auto kind_parsed = leviathan::systems::faction_demands::
            kind_from_string(kind_r.value());
        if (!kind_parsed) {
            return core::Result<core::GameState>::failure(
                dctx + ": " + kind_parsed.error());
        }
        auto created_r = require_string(entry, "created_on", dctx);
        if (!created_r) {
            return core::Result<core::GameState>::failure(
                std::move(created_r.error()));
        }
        auto created_parsed =
            core::GameDate::parse(created_r.value());
        if (!created_parsed) {
            return core::Result<core::GameState>::failure(
                dctx + ": 'created_on' invalid date '" +
                created_r.value() + "': " + created_parsed.error());
        }
        auto expires_r = require_string(entry, "expires_on", dctx);
        if (!expires_r) {
            return core::Result<core::GameState>::failure(
                std::move(expires_r.error()));
        }
        auto expires_parsed =
            core::GameDate::parse(expires_r.value());
        if (!expires_parsed) {
            return core::Result<core::GameState>::failure(
                dctx + ": 'expires_on' invalid date '" +
                expires_r.value() + "': " + expires_parsed.error());
        }
        if (expires_parsed.value() < created_parsed.value()) {
            return core::Result<core::GameState>::failure(
                dctx + ": 'expires_on' (" +
                expires_parsed.value().to_string() +
                ") is before 'created_on' (" +
                created_parsed.value().to_string() + ")");
        }
        auto status_r = require_string(entry, "status", dctx);
        if (!status_r) {
            return core::Result<core::GameState>::failure(
                std::move(status_r.error()));
        }
        auto status_parsed = leviathan::systems::faction_demands::
            status_from_string(status_r.value());
        if (!status_parsed) {
            return core::Result<core::GameState>::failure(
                dctx + ": " + status_parsed.error());
        }
        // Cross-check faction_id_code resolves to a faction in
        // state.factions AND that faction's country_id_code matches
        // the demand's country_id_code (rejects forged or stale
        // demand records).
        bool faction_found = false;
        for (const auto& f : state.factions) {
            if (f.id_code == fid_r.value()) {
                faction_found = true;
                if (f.country_id_code != cid_r.value()) {
                    return core::Result<core::GameState>::failure(
                        dctx + ": faction '" + fid_r.value() +
                        "' belongs to country '" + f.country_id_code +
                        "' but the demand records country '" +
                        cid_r.value() + "'");
                }
                break;
            }
        }
        if (!faction_found) {
            return core::Result<core::GameState>::failure(
                dctx + ": 'faction_id_code' '" + fid_r.value() +
                "' does not match any entry in state.factions");
        }
        bool country_found = false;
        for (const auto& c : state.countries) {
            if (c.id_code == cid_r.value()) {
                country_found = true;
                break;
            }
        }
        if (!country_found) {
            return core::Result<core::GameState>::failure(
                dctx + ": 'country_id_code' '" + cid_r.value() +
                "' does not match any entry in state.countries");
        }
        core::FactionDemand d;
        d.id_code         = std::move(id_r).value();
        d.faction_id_code = std::move(fid_r).value();
        d.country_id_code = std::move(cid_r).value();
        d.kind            = kind_parsed.value();
        d.created_on      = created_parsed.value();
        d.expires_on      = expires_parsed.value();
        d.status          = status_parsed.value();
        state.faction_demands.push_back(std::move(d));
    }

    return core::Result<core::GameState>::success(std::move(state));
}

core::Result<core::GameState> load(const std::filesystem::path& path) {
    auto text = read_whole_file(path);
    if (!text) {
        return core::Result<core::GameState>::failure(std::move(text.error()));
    }
    return deserialize(text.value(), path.string());
}

}  // namespace leviathan::systems::save_system
