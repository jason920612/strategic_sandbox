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
        case core::PlayerCommandKind::EnactPolicy:  return "EnactPolicy";
        case core::PlayerCommandKind::AdjustBudget: return "AdjustBudget";
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
    std::string msg = "unknown player command kind '";
    msg.append(s.data(), s.size());
    msg += "' (expected EnactPolicy|AdjustBudget)";
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

    // factions are populated as of M1.2; provinces / policies / events
    // remain reserved-empty until their shapes are pinned in later
    // M1 sub-milestones.
    root["provinces"] = json::array();

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

    root["events"]    = json::array();

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

    // provinces, policies, events: keys reserved, contents not yet
    // schema-pinned. Tolerate present-but-empty arrays.
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
