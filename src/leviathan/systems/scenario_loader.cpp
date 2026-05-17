#include "leviathan/systems/scenario_loader.hpp"

#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>

#include "internal/json_helpers.hpp"
#include "leviathan/core/entities.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/interest_group_kind.hpp"
#include "leviathan/systems/data_loader.hpp"
#include "leviathan/systems/policy_system.hpp"

namespace leviathan::systems::scenario_loader {

namespace {

using leviathan::systems::detail::fmt_err;
using leviathan::systems::detail::json;

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

core::Result<std::vector<std::filesystem::path>>
require_path_array(const json& root, std::string_view path,
                   std::string_view source) {
    if (!root.contains(path) || !root.at(path).is_array()) {
        std::string msg = "'scenario.";
        msg.append(path.data(), path.size());
        msg += "' is missing or not an array";
        return core::Result<std::vector<std::filesystem::path>>::failure(
            fmt_err(source, msg));
    }
    std::vector<std::filesystem::path> out;
    const auto& arr = root.at(path);
    out.reserve(arr.size());
    for (std::size_t i = 0; i < arr.size(); ++i) {
        if (!arr[i].is_string()) {
            std::string msg = "'scenario.";
            msg.append(path.data(), path.size());
            msg += "[" + std::to_string(i) + "]' is not a string";
            return core::Result<std::vector<std::filesystem::path>>::failure(
                fmt_err(source, msg));
        }
        out.emplace_back(arr[i].get<std::string>());
    }
    return core::Result<std::vector<std::filesystem::path>>::success(std::move(out));
}

}  // namespace

core::Result<ScenarioManifest> parse_manifest(std::string_view json_text,
                                              std::string_view source_label) {
    json root = json::parse(json_text, /*cb=*/nullptr,
                            /*allow_exceptions=*/false,
                            /*ignore_comments=*/false);
    if (root.is_discarded()) {
        return core::Result<ScenarioManifest>::failure(
            fmt_err(source_label, "JSON parse error (malformed document)"));
    }
    if (!root.is_object()) {
        return core::Result<ScenarioManifest>::failure(
            fmt_err(source_label, "top-level JSON value is not an object"));
    }
    if (!root.contains("scenario") || !root.at("scenario").is_object()) {
        return core::Result<ScenarioManifest>::failure(
            fmt_err(source_label, "'scenario' missing or not an object"));
    }

    const json& s = root.at("scenario");

    ScenarioManifest m;

    auto countries_r = require_path_array(s, "countries", source_label);
    if (!countries_r) {
        return core::Result<ScenarioManifest>::failure(std::move(countries_r.error()));
    }
    m.countries = std::move(countries_r).value();

    auto factions_r = require_path_array(s, "factions", source_label);
    if (!factions_r) {
        return core::Result<ScenarioManifest>::failure(std::move(factions_r.error()));
    }
    m.factions = std::move(factions_r).value();

    auto policies_r = require_path_array(s, "policies", source_label);
    if (!policies_r) {
        return core::Result<ScenarioManifest>::failure(std::move(policies_r.error()));
    }
    m.policies = std::move(policies_r).value();

    // ---- M1.13: optional starting_policies array ------------------
    // Missing key is allowed (M1.11 manifests stay valid). Present-
    // but-wrong-type is rejected. Each entry must be an object with
    // string `policy` and `actor` fields.
    if (s.contains("starting_policies")) {
        const json& arr = s.at("starting_policies");
        if (!arr.is_array()) {
            return core::Result<ScenarioManifest>::failure(
                fmt_err(source_label,
                        "'scenario.starting_policies' is not an array"));
        }
        m.starting_policies.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (!arr[i].is_object()) {
                return core::Result<ScenarioManifest>::failure(
                    fmt_err(source_label,
                            "'scenario.starting_policies[" +
                            std::to_string(i) + "]' is not an object"));
            }
            const json& e = arr[i];
            if (!e.contains("policy") || !e.at("policy").is_string()) {
                return core::Result<ScenarioManifest>::failure(
                    fmt_err(source_label,
                            "'scenario.starting_policies[" +
                            std::to_string(i) +
                            "].policy' missing or not a string"));
            }
            if (!e.contains("actor") || !e.at("actor").is_string()) {
                return core::Result<ScenarioManifest>::failure(
                    fmt_err(source_label,
                            "'scenario.starting_policies[" +
                            std::to_string(i) +
                            "].actor' missing or not a string"));
            }
            StartingPolicy sp;
            sp.policy_id_code = e.at("policy").get<std::string>();
            sp.actor_id_code  = e.at("actor").get<std::string>();
            m.starting_policies.push_back(std::move(sp));
        }
    }

    // ---- M3.1: optional interest_groups array ---------------------
    // Missing key allowed (M1.11 / M2 manifests stay valid). Present-
    // but-wrong-type rejected. Each entry validated for required
    // string fields + numeric ratios; cross-reference to a loaded
    // country happens later in `load_into_state` once countries are
    // resolved.
    if (s.contains("interest_groups")) {
        const json& arr = s.at("interest_groups");
        if (!arr.is_array()) {
            return core::Result<ScenarioManifest>::failure(
                fmt_err(source_label,
                        "'scenario.interest_groups' is not an array"));
        }
        m.interest_groups.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
            const std::string ctx =
                "'scenario.interest_groups[" + std::to_string(i) + "]'";
            if (!arr[i].is_object()) {
                return core::Result<ScenarioManifest>::failure(
                    fmt_err(source_label, ctx + " is not an object"));
            }
            const json& e = arr[i];
            auto need_string = [&](const char* key,
                                   std::string& out) -> core::Result<bool> {
                if (!e.contains(key) || !e.at(key).is_string()) {
                    return core::Result<bool>::failure(
                        fmt_err(source_label,
                                ctx + "." + key + " missing or not a string"));
                }
                out = e.at(key).get<std::string>();
                if (out.empty()) {
                    return core::Result<bool>::failure(
                        fmt_err(source_label,
                                ctx + "." + key + " must be non-empty"));
                }
                return core::Result<bool>::success(true);
            };
            auto need_ratio = [&](const char* key,
                                  double& out) -> core::Result<bool> {
                if (!e.contains(key) || !e.at(key).is_number()) {
                    return core::Result<bool>::failure(
                        fmt_err(source_label,
                                ctx + "." + key + " missing or not a number"));
                }
                const double v = e.at(key).get<double>();
                if (!(v >= 0.0 && v <= 1.0)) {
                    return core::Result<bool>::failure(
                        fmt_err(source_label,
                                ctx + "." + key +
                                " out of range (expected [0, 1])"));
                }
                out = v;
                return core::Result<bool>::success(true);
            };

            ManifestInterestGroup g;
            if (auto r = need_string("id_code",  g.id_code);          !r) {
                return core::Result<ScenarioManifest>::failure(std::move(r.error()));
            }
            if (auto r = need_string("name",     g.name);             !r) {
                return core::Result<ScenarioManifest>::failure(std::move(r.error()));
            }
            if (auto r = need_string("kind",     g.kind);             !r) {
                return core::Result<ScenarioManifest>::failure(std::move(r.error()));
            }
            if (auto r = need_string("country",  g.country_id_code);  !r) {
                return core::Result<ScenarioManifest>::failure(std::move(r.error()));
            }
            if (auto r = need_ratio("influence",  g.influence);       !r) {
                return core::Result<ScenarioManifest>::failure(std::move(r.error()));
            }
            if (auto r = need_ratio("loyalty",    g.loyalty);         !r) {
                return core::Result<ScenarioManifest>::failure(std::move(r.error()));
            }
            if (auto r = need_ratio("radicalism", g.radicalism);      !r) {
                return core::Result<ScenarioManifest>::failure(std::move(r.error()));
            }
            // Duplicate id_code inside the manifest is a hard
            // failure here — a future system that looks up by
            // id_code would otherwise silently pick one or the
            // other.
            for (const auto& prev : m.interest_groups) {
                if (prev.id_code == g.id_code) {
                    return core::Result<ScenarioManifest>::failure(
                        fmt_err(source_label,
                                ctx + ".id_code '" + g.id_code +
                                "' is a duplicate within the manifest"));
                }
            }
            m.interest_groups.push_back(std::move(g));
        }
    }

    return core::Result<ScenarioManifest>::success(std::move(m));
}

core::Result<ScenarioLoadOutcome> load_into_state(
        core::GameState& state,
        const std::filesystem::path& manifest_path) {
    namespace dl = leviathan::systems::data_loader;

    // ---- 0. Reject pre-populated state -----------------------------
    if (!state.countries.empty() || !state.factions.empty()
        || !state.policies.empty()) {
        return core::Result<ScenarioLoadOutcome>::failure(
            manifest_path.string() +
            ": load_into_state requires an empty GameState"
            " (state.countries / state.factions / state.policies"
            " must all be empty)");
    }

    // ---- 1. Read + parse manifest ----------------------------------
    auto text_r = read_whole_file(manifest_path);
    if (!text_r) {
        return core::Result<ScenarioLoadOutcome>::failure(
            std::move(text_r.error()));
    }
    auto manifest_r = parse_manifest(text_r.value(), manifest_path.string());
    if (!manifest_r) {
        return core::Result<ScenarioLoadOutcome>::failure(
            std::move(manifest_r.error()));
    }
    const ScenarioManifest manifest = std::move(manifest_r).value();

    // Path-resolution base: manifest's grandparent directory. For a
    // manifest at data/scenarios/1930_minimal.json this yields data/.
    const auto base_dir = manifest_path.parent_path().parent_path();

    ScenarioLoadOutcome outcome;

    // ---- 2. Countries ----------------------------------------------
    std::unordered_map<std::string, core::CountryId> country_by_id_code;
    state.countries.reserve(manifest.countries.size());
    for (std::size_t i = 0; i < manifest.countries.size(); ++i) {
        const auto path = base_dir / manifest.countries[i];
        auto c_r = dl::load_country(path);
        if (!c_r) {
            return core::Result<ScenarioLoadOutcome>::failure(
                std::move(c_r.error()));
        }
        auto c = std::move(c_r).value();
        if (country_by_id_code.count(c.id_code) != 0) {
            return core::Result<ScenarioLoadOutcome>::failure(
                manifest_path.string() +
                ": duplicate country id_code '" + c.id_code +
                "' (already loaded from a prior entry)");
        }
        c.id = core::CountryId{static_cast<int>(i)};
        country_by_id_code.emplace(c.id_code, c.id);
        state.countries.push_back(std::move(c));
    }
    outcome.countries_loaded = static_cast<int>(state.countries.size());

    // ---- 3. Factions -----------------------------------------------
    std::unordered_map<std::string, core::FactionId> faction_by_id_code;
    state.factions.reserve(manifest.factions.size());
    for (std::size_t i = 0; i < manifest.factions.size(); ++i) {
        const auto path = base_dir / manifest.factions[i];
        auto f_r = dl::load_faction(path);
        if (!f_r) {
            return core::Result<ScenarioLoadOutcome>::failure(
                std::move(f_r.error()));
        }
        auto f = std::move(f_r).value();
        if (faction_by_id_code.count(f.id_code) != 0) {
            return core::Result<ScenarioLoadOutcome>::failure(
                manifest_path.string() +
                ": duplicate faction id_code '" + f.id_code + "'");
        }
        auto it = country_by_id_code.find(f.country_id_code);
        if (it == country_by_id_code.end()) {
            return core::Result<ScenarioLoadOutcome>::failure(
                manifest_path.string() +
                ": faction '" + f.id_code +
                "' references missing country id_code '" +
                f.country_id_code + "'");
        }
        f.id      = core::FactionId{static_cast<int>(i)};
        f.country = it->second;
        faction_by_id_code.emplace(f.id_code, f.id);
        state.factions.push_back(std::move(f));
    }
    outcome.factions_loaded = static_cast<int>(state.factions.size());

    // ---- 4. Policies -----------------------------------------------
    std::unordered_map<std::string, core::PolicyId> policy_by_id_code;
    state.policies.reserve(manifest.policies.size());
    for (std::size_t i = 0; i < manifest.policies.size(); ++i) {
        const auto path = base_dir / manifest.policies[i];
        auto p_r = dl::load_policy(path);
        if (!p_r) {
            return core::Result<ScenarioLoadOutcome>::failure(
                std::move(p_r.error()));
        }
        auto p = std::move(p_r).value();
        if (policy_by_id_code.count(p.id_code) != 0) {
            return core::Result<ScenarioLoadOutcome>::failure(
                manifest_path.string() +
                ": duplicate policy id_code '" + p.id_code + "'");
        }
        p.id = core::PolicyId{static_cast<int>(i)};
        policy_by_id_code.emplace(p.id_code, p.id);
        state.policies.push_back(std::move(p));
    }
    outcome.policies_loaded = static_cast<int>(state.policies.size());

    // ---- 5. Day-0 starting policies (M1.13) ------------------------
    // For each entry resolve `policy` → loaded PolicyData and `actor`
    // → loaded CountryId, then invoke policy::apply_policy_effects
    // exactly once. M1.5 guarantees each call is atomic per-policy
    // (pre-flight rejection on bad target). Day-0 enactments earlier
    // in the list that have already succeeded stay applied if a
    // later one fails — documented non-atomic across the list.
    namespace pol = leviathan::systems::policy;
    for (std::size_t i = 0; i < manifest.starting_policies.size(); ++i) {
        const auto& entry = manifest.starting_policies[i];

        auto policy_it = policy_by_id_code.find(entry.policy_id_code);
        if (policy_it == policy_by_id_code.end()) {
            return core::Result<ScenarioLoadOutcome>::failure(
                manifest_path.string() +
                ": starting_policies[" + std::to_string(i) +
                "] references unknown policy id_code '" +
                entry.policy_id_code + "'");
        }
        auto actor_it = country_by_id_code.find(entry.actor_id_code);
        if (actor_it == country_by_id_code.end()) {
            return core::Result<ScenarioLoadOutcome>::failure(
                manifest_path.string() +
                ": starting_policies[" + std::to_string(i) +
                "] references unknown actor country id_code '" +
                entry.actor_id_code + "'");
        }

        const auto& policy_data =
            state.policies[static_cast<std::size_t>(policy_it->second.value())];
        auto apply_r = pol::apply_policy_effects(state, actor_it->second,
                                                 policy_data);
        if (!apply_r) {
            return core::Result<ScenarioLoadOutcome>::failure(
                manifest_path.string() +
                ": starting_policies[" + std::to_string(i) +
                "] apply_policy_effects(" + entry.policy_id_code +
                " by " + entry.actor_id_code + ") failed: " +
                std::move(apply_r.error()));
        }
        ++outcome.starting_policies_applied;
    }

    // ---- 6. M3.1 interest_groups -----------------------------------
    // Resolve each manifest entry's `country` id_code against the
    // loaded countries map, parse the `kind` string into the enum,
    // and append to state.interest_groups. The save layer enforces
    // its own (stricter) validation; this is the scenario-JSON
    // entry point, which fails loudly on the same shape issues so
    // a typo in the manifest doesn't silently produce a half-
    // populated political map.
    {
        // Kind-string mapping is the shared
        // `core::interest_group_kind_from_string` (M3.5). Adding a
        // new `InterestGroupKind` variant only edits one file.

        state.interest_groups.reserve(manifest.interest_groups.size());
        for (std::size_t i = 0; i < manifest.interest_groups.size(); ++i) {
            const auto& entry = manifest.interest_groups[i];

            auto country_it = country_by_id_code.find(entry.country_id_code);
            if (country_it == country_by_id_code.end()) {
                return core::Result<ScenarioLoadOutcome>::failure(
                    manifest_path.string() +
                    ": interest_groups[" + std::to_string(i) +
                    "] references unknown country id_code '" +
                    entry.country_id_code + "'");
            }

            auto kind_r = core::interest_group_kind_from_string(entry.kind);
            if (!kind_r) {
                return core::Result<ScenarioLoadOutcome>::failure(
                    manifest_path.string() +
                    ": interest_groups[" + std::to_string(i) +
                    "]: " + std::move(kind_r.error()));
            }

            core::InterestGroupState g;
            g.id_code    = entry.id_code;
            g.name       = entry.name;
            g.kind       = kind_r.value();
            g.country    = country_it->second;
            g.influence  = entry.influence;
            g.loyalty    = entry.loyalty;
            g.radicalism = entry.radicalism;
            state.interest_groups.push_back(std::move(g));
        }
    }

    return core::Result<ScenarioLoadOutcome>::success(outcome);
}

}  // namespace leviathan::systems::scenario_loader
