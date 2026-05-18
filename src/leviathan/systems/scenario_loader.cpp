#include "leviathan/systems/scenario_loader.hpp"

#include <cmath>
#include <cstddef>
#include <fstream>
#include <optional>
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

// M4.1: parse one province file. Each file is a JSON object with
// a top-level `provinces` array of `{id, name, owner, x, y}`
// records. Cross-references to loaded countries (owner_id_code →
// CountryId) happen in `load_into_state` once all countries are
// known, so this helper only validates JSON shape + per-field
// range. The caller passes the file's `source_label` so error
// messages name the offending province file rather than the
// manifest.
core::Result<std::vector<ManifestProvince>>
parse_province_file(std::string_view json_text,
                    std::string_view source_label) {
    json root = json::parse(json_text, /*cb=*/nullptr,
                            /*allow_exceptions=*/false,
                            /*ignore_comments=*/false);
    if (root.is_discarded()) {
        return core::Result<std::vector<ManifestProvince>>::failure(
            fmt_err(source_label,
                    "JSON parse error (malformed document)"));
    }
    if (!root.is_object()) {
        return core::Result<std::vector<ManifestProvince>>::failure(
            fmt_err(source_label,
                    "top-level JSON value is not an object"));
    }
    if (!root.contains("provinces") || !root.at("provinces").is_array()) {
        return core::Result<std::vector<ManifestProvince>>::failure(
            fmt_err(source_label,
                    "'provinces' missing or not an array"));
    }
    const json& arr = root.at("provinces");
    std::vector<ManifestProvince> out;
    out.reserve(arr.size());
    for (std::size_t i = 0; i < arr.size(); ++i) {
        const std::string ctx =
            "'provinces[" + std::to_string(i) + "]'";
        if (!arr[i].is_object()) {
            return core::Result<std::vector<ManifestProvince>>::failure(
                fmt_err(source_label, ctx + " is not an object"));
        }
        const json& e = arr[i];

        auto need_string = [&](const char* key,
                               std::string& dst) -> core::Result<bool> {
            if (!e.contains(key) || !e.at(key).is_string()) {
                return core::Result<bool>::failure(
                    fmt_err(source_label,
                            ctx + "." + key +
                            " missing or not a string"));
            }
            dst = e.at(key).get<std::string>();
            if (dst.empty()) {
                return core::Result<bool>::failure(
                    fmt_err(source_label,
                            ctx + "." + key + " must be non-empty"));
            }
            return core::Result<bool>::success(true);
        };
        auto need_ratio = [&](const char* key,
                              double& dst) -> core::Result<bool> {
            if (!e.contains(key) || !e.at(key).is_number()) {
                return core::Result<bool>::failure(
                    fmt_err(source_label,
                            ctx + "." + key +
                            " missing or not a number"));
            }
            const double v = e.at(key).get<double>();
            if (!(v >= 0.0 && v <= 1.0)) {
                return core::Result<bool>::failure(
                    fmt_err(source_label,
                            ctx + "." + key +
                            " out of range (expected [0, 1])"));
            }
            dst = v;
            return core::Result<bool>::success(true);
        };

        ManifestProvince p;
        if (auto r = need_string("id",    p.id_code);       !r) {
            return core::Result<std::vector<ManifestProvince>>::failure(
                std::move(r.error()));
        }
        if (auto r = need_string("name",  p.name);          !r) {
            return core::Result<std::vector<ManifestProvince>>::failure(
                std::move(r.error()));
        }
        if (auto r = need_string("owner", p.owner_id_code); !r) {
            return core::Result<std::vector<ManifestProvince>>::failure(
                std::move(r.error()));
        }
        if (auto r = need_ratio("x", p.x); !r) {
            return core::Result<std::vector<ManifestProvince>>::failure(
                std::move(r.error()));
        }
        if (auto r = need_ratio("y", p.y); !r) {
            return core::Result<std::vector<ManifestProvince>>::failure(
                std::move(r.error()));
        }
        out.push_back(std::move(p));
    }
    return core::Result<std::vector<ManifestProvince>>::success(std::move(out));
}

// M5.1: parse one event file. Each file is a JSON object with
// a top-level `events` array of event-definition records:
//   {
//     "id":             "<id_code>",   // required non-empty string
//     "name":           "<title>",      // required non-empty string
//     "description":    "...",          // required string (may be empty)
//     "visible_report": "...",          // required non-empty string (M6.2)
//     "true_cause":     "...",          // required non-empty string (M6.1)
//     "triggers":       [ {target, op, value}, ... ],  // required non-empty
//     "effects":        [ {target, op, value}, ... ]   // required, may be empty
//   }
// M6.1 added `true_cause` (RFC-090 §6.1) as a required non-empty
// string — author-written truth narrative. M6.2 adds
// `visible_report` (RFC-090 §6.2) as a required non-empty string
// — author-written public-facing fired-report description.
// Both are stored and validated but neither is consumed by any
// system yet. Later M6 sub-milestones (6.3 information_accuracy,
// 6.4 reported value, 6.5 bias/noise, etc.) will read them.
// Trigger ops are allowlisted at load time {lt, lte, gt, gte};
// trigger targets are allowlisted against a small set tied to
// existing M1–M3 state. Effects validation mirrors the
// `data_loader::parse_policy` pattern — required target/op
// strings + finite value, no target/op allowlist at load (the
// existing PolicySystem applies that gate at apply time; M5
// will follow the same pattern in a future sub-milestone).
//
// Cross-file uniqueness of event id_code is enforced inside
// `load_into_state`. M5.1 only loads + validates + stores; no
// firing, no evaluator, no effects application yet.
core::Result<std::vector<ManifestEvent>>
parse_event_file(std::string_view json_text,
                 std::string_view source_label) {
    json root = json::parse(json_text, /*cb=*/nullptr,
                            /*allow_exceptions=*/false,
                            /*ignore_comments=*/false);
    if (root.is_discarded()) {
        return core::Result<std::vector<ManifestEvent>>::failure(
            fmt_err(source_label,
                    "JSON parse error (malformed document)"));
    }
    if (!root.is_object()) {
        return core::Result<std::vector<ManifestEvent>>::failure(
            fmt_err(source_label,
                    "top-level JSON value is not an object"));
    }
    if (!root.contains("events") || !root.at("events").is_array()) {
        return core::Result<std::vector<ManifestEvent>>::failure(
            fmt_err(source_label,
                    "'events' missing or not an array"));
    }

    // M5.1 allowlists.
    static const std::vector<std::string> kTriggerTargets = {
        "country.stability",
        "country.legitimacy",
        "country.government_authority.bureaucratic_compliance",
        "interest_group.radicalism",
        "interest_group.loyalty",
    };
    static const std::vector<std::string> kTriggerOps = {
        "lt", "lte", "gt", "gte",
    };
    auto is_allowed = [](const std::vector<std::string>& list,
                         const std::string& v) {
        for (const auto& s : list) {
            if (s == v) { return true; }
        }
        return false;
    };

    const json& arr = root.at("events");
    std::vector<ManifestEvent> out;
    out.reserve(arr.size());
    for (std::size_t i = 0; i < arr.size(); ++i) {
        const std::string ctx =
            "'events[" + std::to_string(i) + "]'";
        if (!arr[i].is_object()) {
            return core::Result<std::vector<ManifestEvent>>::failure(
                fmt_err(source_label, ctx + " is not an object"));
        }
        const json& e = arr[i];

        auto need_string_nonempty = [&](const char* key,
                                        std::string& dst)
                -> core::Result<bool> {
            if (!e.contains(key) || !e.at(key).is_string()) {
                return core::Result<bool>::failure(
                    fmt_err(source_label,
                            ctx + "." + key +
                            " missing or not a string"));
            }
            dst = e.at(key).get<std::string>();
            if (dst.empty()) {
                return core::Result<bool>::failure(
                    fmt_err(source_label,
                            ctx + "." + key + " must be non-empty"));
            }
            return core::Result<bool>::success(true);
        };
        // description: required string but may be empty.
        auto need_string_maybe_empty = [&](const char* key,
                                           std::string& dst)
                -> core::Result<bool> {
            if (!e.contains(key) || !e.at(key).is_string()) {
                return core::Result<bool>::failure(
                    fmt_err(source_label,
                            ctx + "." + key +
                            " missing or not a string"));
            }
            dst = e.at(key).get<std::string>();
            return core::Result<bool>::success(true);
        };

        ManifestEvent ev;
        if (auto r = need_string_nonempty("id", ev.id_code); !r) {
            return core::Result<std::vector<ManifestEvent>>::failure(
                std::move(r.error()));
        }
        if (auto r = need_string_nonempty("name", ev.name); !r) {
            return core::Result<std::vector<ManifestEvent>>::failure(
                std::move(r.error()));
        }
        if (auto r = need_string_maybe_empty("description",
                                             ev.description); !r) {
            return core::Result<std::vector<ManifestEvent>>::failure(
                std::move(r.error()));
        }
        // M6.2 (RFC-090 §6.2): visible_report is required non-empty.
        // It is the author-written public-facing fired-report
        // description; M6.2 stores and round-trips it but no
        // system consumes it yet.
        if (auto r = need_string_nonempty("visible_report",
                                          ev.visible_report); !r) {
            return core::Result<std::vector<ManifestEvent>>::failure(
                std::move(r.error()));
        }
        // M6.1 (RFC-090 §6.1): true_cause is required non-empty.
        // It is the author-written truth narrative; M6.1 stores
        // and round-trips it but no system consumes it yet.
        if (auto r = need_string_nonempty("true_cause",
                                          ev.true_cause); !r) {
            return core::Result<std::vector<ManifestEvent>>::failure(
                std::move(r.error()));
        }

        // triggers: required, non-empty array.
        if (!e.contains("triggers") || !e.at("triggers").is_array()) {
            return core::Result<std::vector<ManifestEvent>>::failure(
                fmt_err(source_label,
                        ctx + ".triggers missing or not an array"));
        }
        const json& trig_arr = e.at("triggers");
        if (trig_arr.empty()) {
            return core::Result<std::vector<ManifestEvent>>::failure(
                fmt_err(source_label,
                        ctx + ".triggers must be non-empty"));
        }
        ev.triggers.reserve(trig_arr.size());
        for (std::size_t ti = 0; ti < trig_arr.size(); ++ti) {
            const std::string tctx =
                ctx + ".triggers[" + std::to_string(ti) + "]";
            if (!trig_arr[ti].is_object()) {
                return core::Result<std::vector<ManifestEvent>>::failure(
                    fmt_err(source_label, tctx + " is not an object"));
            }
            const json& t = trig_arr[ti];
            core::EventTrigger trig;
            // target: required string, allowlisted.
            if (!t.contains("target") || !t.at("target").is_string()) {
                return core::Result<std::vector<ManifestEvent>>::failure(
                    fmt_err(source_label,
                            tctx + ".target missing or not a string"));
            }
            trig.target = t.at("target").get<std::string>();
            if (!is_allowed(kTriggerTargets, trig.target)) {
                return core::Result<std::vector<ManifestEvent>>::failure(
                    fmt_err(source_label,
                            tctx + ".target '" + trig.target +
                            "' is not in the M5.1 allowlist"
                            " (country.stability, country.legitimacy,"
                            " country.government_authority"
                            ".bureaucratic_compliance,"
                            " interest_group.radicalism,"
                            " interest_group.loyalty)"));
            }
            // op: required string, allowlisted.
            if (!t.contains("op") || !t.at("op").is_string()) {
                return core::Result<std::vector<ManifestEvent>>::failure(
                    fmt_err(source_label,
                            tctx + ".op missing or not a string"));
            }
            trig.op = t.at("op").get<std::string>();
            if (!is_allowed(kTriggerOps, trig.op)) {
                return core::Result<std::vector<ManifestEvent>>::failure(
                    fmt_err(source_label,
                            tctx + ".op '" + trig.op +
                            "' is not in the M5.1 allowlist"
                            " (lt, lte, gt, gte)"));
            }
            // value: required, finite number.
            if (!t.contains("value") || !t.at("value").is_number()) {
                return core::Result<std::vector<ManifestEvent>>::failure(
                    fmt_err(source_label,
                            tctx + ".value missing or not a number"));
            }
            trig.value = t.at("value").get<double>();
            if (!std::isfinite(trig.value)) {
                return core::Result<std::vector<ManifestEvent>>::failure(
                    fmt_err(source_label,
                            tctx + ".value must be finite"));
            }
            ev.triggers.push_back(std::move(trig));
        }

        // effects: required array, MAY be empty (warning-only events).
        if (!e.contains("effects") || !e.at("effects").is_array()) {
            return core::Result<std::vector<ManifestEvent>>::failure(
                fmt_err(source_label,
                        ctx + ".effects missing or not an array"));
        }
        const json& eff_arr = e.at("effects");
        ev.effects.reserve(eff_arr.size());
        for (std::size_t ei = 0; ei < eff_arr.size(); ++ei) {
            const std::string ectx =
                ctx + ".effects[" + std::to_string(ei) + "]";
            if (!eff_arr[ei].is_object()) {
                return core::Result<std::vector<ManifestEvent>>::failure(
                    fmt_err(source_label, ectx + " is not an object"));
            }
            const json& f = eff_arr[ei];
            // Mirror data_loader::parse_policy effect validation:
            // require target/op strings + finite value; no
            // allowlist at load (PolicySystem applies that at
            // apply time; future M5.x effect applicator will too).
            core::PolicyEffect eff;
            if (!f.contains("target") || !f.at("target").is_string()) {
                return core::Result<std::vector<ManifestEvent>>::failure(
                    fmt_err(source_label,
                            ectx + ".target missing or not a string"));
            }
            eff.target = f.at("target").get<std::string>();
            if (!f.contains("op") || !f.at("op").is_string()) {
                return core::Result<std::vector<ManifestEvent>>::failure(
                    fmt_err(source_label,
                            ectx + ".op missing or not a string"));
            }
            eff.op = f.at("op").get<std::string>();
            if (!f.contains("value") || !f.at("value").is_number()) {
                return core::Result<std::vector<ManifestEvent>>::failure(
                    fmt_err(source_label,
                            ectx + ".value missing or not a number"));
            }
            eff.value = f.at("value").get<double>();
            if (!std::isfinite(eff.value)) {
                return core::Result<std::vector<ManifestEvent>>::failure(
                    fmt_err(source_label,
                            ectx + ".value must be finite"));
            }
            ev.effects.push_back(std::move(eff));
        }

        // RCR-1 (RFC-090 §5.3): optional weight_modifiers[].
        if (e.contains("weight_modifiers")) {
            if (!e.at("weight_modifiers").is_array()) {
                return core::Result<std::vector<ManifestEvent>>::failure(
                    fmt_err(source_label,
                            ctx + ".weight_modifiers is not an array"));
            }
            const json& wm_arr = e.at("weight_modifiers");
            ev.weight_modifiers.reserve(wm_arr.size());
            for (std::size_t wi = 0; wi < wm_arr.size(); ++wi) {
                const std::string wctx =
                    ctx + ".weight_modifiers[" + std::to_string(wi) + "]";
                const json& w = wm_arr[wi];
                if (!w.is_object()) {
                    return core::Result<std::vector<ManifestEvent>>::failure(
                        fmt_err(source_label, wctx + " is not an object"));
                }
                core::WeightModifier wm;
                if (!w.contains("target") || !w.at("target").is_string()) {
                    return core::Result<std::vector<ManifestEvent>>::failure(
                        fmt_err(source_label,
                                wctx + ".target missing or not a string"));
                }
                wm.target = w.at("target").get<std::string>();
                if (!w.contains("op") || !w.at("op").is_string()) {
                    return core::Result<std::vector<ManifestEvent>>::failure(
                        fmt_err(source_label,
                                wctx + ".op missing or not a string"));
                }
                wm.op = w.at("op").get<std::string>();
                if (!w.contains("value") || !w.at("value").is_number()) {
                    return core::Result<std::vector<ManifestEvent>>::failure(
                        fmt_err(source_label,
                                wctx + ".value missing or not a number"));
                }
                wm.value = w.at("value").get<double>();
                if (!std::isfinite(wm.value)) {
                    return core::Result<std::vector<ManifestEvent>>::failure(
                        fmt_err(source_label, wctx + ".value must be finite"));
                }
                if (!w.contains("weight_delta")
                    || !w.at("weight_delta").is_number()) {
                    return core::Result<std::vector<ManifestEvent>>::failure(
                        fmt_err(source_label,
                                wctx + ".weight_delta missing or not a number"));
                }
                wm.weight_delta = w.at("weight_delta").get<double>();
                if (!std::isfinite(wm.weight_delta)) {
                    return core::Result<std::vector<ManifestEvent>>::failure(
                        fmt_err(source_label,
                                wctx + ".weight_delta must be finite"));
                }
                ev.weight_modifiers.push_back(std::move(wm));
            }
        }

        // RCR-1 (RFC-090 §5.4 / §5.8): optional options[].
        if (e.contains("options")) {
            if (!e.at("options").is_array()) {
                return core::Result<std::vector<ManifestEvent>>::failure(
                    fmt_err(source_label, ctx + ".options is not an array"));
            }
            const json& opts_arr = e.at("options");
            ev.options.reserve(opts_arr.size());
            for (std::size_t oi = 0; oi < opts_arr.size(); ++oi) {
                const std::string octx =
                    ctx + ".options[" + std::to_string(oi) + "]";
                const json& o = opts_arr[oi];
                if (!o.is_object()) {
                    return core::Result<std::vector<ManifestEvent>>::failure(
                        fmt_err(source_label, octx + " is not an object"));
                }
                core::EventOption opt;
                if (!o.contains("id_code") || !o.at("id_code").is_string()) {
                    return core::Result<std::vector<ManifestEvent>>::failure(
                        fmt_err(source_label,
                                octx + ".id_code missing or not a string"));
                }
                opt.id_code = o.at("id_code").get<std::string>();
                if (opt.id_code.empty()) {
                    return core::Result<std::vector<ManifestEvent>>::failure(
                        fmt_err(source_label, octx + ".id_code must be non-empty"));
                }
                if (!o.contains("label") || !o.at("label").is_string()) {
                    return core::Result<std::vector<ManifestEvent>>::failure(
                        fmt_err(source_label,
                                octx + ".label missing or not a string"));
                }
                opt.label = o.at("label").get<std::string>();
                if (!o.contains("effects") || !o.at("effects").is_array()) {
                    return core::Result<std::vector<ManifestEvent>>::failure(
                        fmt_err(source_label,
                                octx + ".effects missing or not an array"));
                }
                const json& oe_arr = o.at("effects");
                opt.effects.reserve(oe_arr.size());
                for (std::size_t oei = 0; oei < oe_arr.size(); ++oei) {
                    const std::string oectx =
                        octx + ".effects[" + std::to_string(oei) + "]";
                    const json& f = oe_arr[oei];
                    if (!f.is_object()) {
                        return core::Result<std::vector<ManifestEvent>>::failure(
                            fmt_err(source_label, oectx + " is not an object"));
                    }
                    core::PolicyEffect eff;
                    if (!f.contains("target") || !f.at("target").is_string()) {
                        return core::Result<std::vector<ManifestEvent>>::failure(
                            fmt_err(source_label,
                                    oectx + ".target missing or not a string"));
                    }
                    eff.target = f.at("target").get<std::string>();
                    if (!f.contains("op") || !f.at("op").is_string()) {
                        return core::Result<std::vector<ManifestEvent>>::failure(
                            fmt_err(source_label,
                                    oectx + ".op missing or not a string"));
                    }
                    eff.op = f.at("op").get<std::string>();
                    if (!f.contains("value") || !f.at("value").is_number()) {
                        return core::Result<std::vector<ManifestEvent>>::failure(
                            fmt_err(source_label,
                                    oectx + ".value missing or not a number"));
                    }
                    eff.value = f.at("value").get<double>();
                    if (!std::isfinite(eff.value)) {
                        return core::Result<std::vector<ManifestEvent>>::failure(
                            fmt_err(source_label,
                                    oectx + ".value must be finite"));
                    }
                    opt.effects.push_back(std::move(eff));
                }
                ev.options.push_back(std::move(opt));
            }
        }

        // RCR-1 (RFC-090 §5.12): optional followup_event_ids[].
        if (e.contains("followup_event_ids")) {
            if (!e.at("followup_event_ids").is_array()) {
                return core::Result<std::vector<ManifestEvent>>::failure(
                    fmt_err(source_label,
                            ctx + ".followup_event_ids is not an array"));
            }
            const json& fu_arr = e.at("followup_event_ids");
            ev.followup_event_ids.reserve(fu_arr.size());
            for (std::size_t fi = 0; fi < fu_arr.size(); ++fi) {
                const std::string fctx = ctx + ".followup_event_ids[" +
                                         std::to_string(fi) + "]";
                const json& v = fu_arr[fi];
                if (!v.is_string()) {
                    return core::Result<std::vector<ManifestEvent>>::failure(
                        fmt_err(source_label, fctx + " is not a string"));
                }
                std::string s = v.get<std::string>();
                if (s.empty()) {
                    return core::Result<std::vector<ManifestEvent>>::failure(
                        fmt_err(source_label, fctx + " must be non-empty"));
                }
                ev.followup_event_ids.push_back(std::move(s));
            }
        }

        out.push_back(std::move(ev));
    }
    return core::Result<std::vector<ManifestEvent>>::success(std::move(out));
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

    // ---- M4.1: optional provinces array (of file paths) -----------
    // Missing key allowed (M1.11 / M2 / pre-M4 manifests stay
    // valid). Present-but-wrong-type rejected. Each entry must be a
    // string path; the actual province file is loaded later by
    // `load_into_state`.
    if (s.contains("provinces")) {
        const json& arr = s.at("provinces");
        if (!arr.is_array()) {
            return core::Result<ScenarioManifest>::failure(
                fmt_err(source_label,
                        "'scenario.provinces' is not an array"));
        }
        m.provinces.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (!arr[i].is_string()) {
                return core::Result<ScenarioManifest>::failure(
                    fmt_err(source_label,
                            "'scenario.provinces[" + std::to_string(i) +
                            "]' is not a string"));
            }
            m.provinces.emplace_back(arr[i].get<std::string>());
        }
    }

    // ---- M5.1: optional events array (of file paths) --------------
    // Missing key allowed (every pre-M5 manifest stays valid).
    // Present-but-wrong-type rejected. Each entry must be a string
    // path; the actual event file is loaded later by
    // `load_into_state`. Mirror M4.1 provinces parsing shape.
    if (s.contains("events")) {
        const json& arr = s.at("events");
        if (!arr.is_array()) {
            return core::Result<ScenarioManifest>::failure(
                fmt_err(source_label,
                        "'scenario.events' is not an array"));
        }
        m.events.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (!arr[i].is_string()) {
                return core::Result<ScenarioManifest>::failure(
                    fmt_err(source_label,
                            "'scenario.events[" + std::to_string(i) +
                            "]' is not a string"));
            }
            m.events.emplace_back(arr[i].get<std::string>());
        }
    }

    // ---- Issue #108 fix: optional relationships block ----------------
    // RFC-090 §3.6 / §3.7: pairwise inter-country relationship +
    // threat record. Authored on the manifest as
    //   "relationships": [
    //     { "from": "GER", "to": "FRA", "relationship": -0.4, "threat": 0.6 },
    //     ...
    //   ]
    // Each entry's `from` / `to` id_code is resolved against
    // `state.countries` inside `load_into_state`. Range validation
    // (relationship in [-1, 1]; threat in [0, 1]) happens here at
    // parse time so a malformed manifest fails before any state
    // mutation.
    if (s.contains("relationships")) {
        const json& arr = s.at("relationships");
        if (!arr.is_array()) {
            return core::Result<ScenarioManifest>::failure(
                fmt_err(source_label,
                        "'scenario.relationships' is not an array"));
        }
        m.relationships.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
            const std::string ctx =
                "'scenario.relationships[" + std::to_string(i) + "]'";
            if (!arr[i].is_object()) {
                return core::Result<ScenarioManifest>::failure(
                    fmt_err(source_label, ctx + " is not an object"));
            }
            const json& e = arr[i];
            ManifestRelation rel;
            if (!e.contains("from") || !e.at("from").is_string()) {
                return core::Result<ScenarioManifest>::failure(
                    fmt_err(source_label,
                            ctx + ".from missing or not a string"));
            }
            rel.from_id_code = e.at("from").get<std::string>();
            if (rel.from_id_code.empty()) {
                return core::Result<ScenarioManifest>::failure(
                    fmt_err(source_label, ctx + ".from must be non-empty"));
            }
            if (!e.contains("to") || !e.at("to").is_string()) {
                return core::Result<ScenarioManifest>::failure(
                    fmt_err(source_label,
                            ctx + ".to missing or not a string"));
            }
            rel.to_id_code = e.at("to").get<std::string>();
            if (rel.to_id_code.empty()) {
                return core::Result<ScenarioManifest>::failure(
                    fmt_err(source_label, ctx + ".to must be non-empty"));
            }
            if (!e.contains("relationship")
                || !e.at("relationship").is_number()) {
                return core::Result<ScenarioManifest>::failure(
                    fmt_err(source_label,
                            ctx + ".relationship missing or not a number"));
            }
            rel.relationship = e.at("relationship").get<double>();
            if (!std::isfinite(rel.relationship)
                || rel.relationship < -1.0 || rel.relationship > 1.0) {
                return core::Result<ScenarioManifest>::failure(
                    fmt_err(source_label,
                            ctx + ".relationship out of range"
                            " (expected [-1, 1])"));
            }
            if (!e.contains("threat") || !e.at("threat").is_number()) {
                return core::Result<ScenarioManifest>::failure(
                    fmt_err(source_label,
                            ctx + ".threat missing or not a number"));
            }
            rel.threat = e.at("threat").get<double>();
            if (!std::isfinite(rel.threat)
                || rel.threat < 0.0 || rel.threat > 1.0) {
                return core::Result<ScenarioManifest>::failure(
                    fmt_err(source_label,
                            ctx + ".threat out of range (expected [0, 1])"));
            }
            m.relationships.push_back(std::move(rel));
        }
    }

    return core::Result<ScenarioManifest>::success(std::move(m));
}

core::Result<ScenarioLoadOutcome> load_into_state(
        core::GameState& state,
        const std::filesystem::path& manifest_path) {
    namespace dl = leviathan::systems::data_loader;

    // ---- 0. Reject pre-populated state -----------------------------
    // Issue #110 §4: every loader-populated root container on
    // GameState must be empty on entry, not just the first three.
    // The 7 loader-populated containers are: countries, provinces,
    // factions, policies, events, interest_groups, relationships.
    // The remaining root containers (logs, applied_commands,
    // event_history) are runtime accumulations and NOT in scope.
    {
        std::string non_empty;
        auto note = [&](const char* name, bool flag) {
            if (flag) {
                if (!non_empty.empty()) { non_empty += ", "; }
                non_empty += name;
            }
        };
        note("countries",       !state.countries.empty());
        note("provinces",       !state.provinces.empty());
        note("factions",        !state.factions.empty());
        note("policies",        !state.policies.empty());
        note("events",          !state.events.empty());
        note("interest_groups", !state.interest_groups.empty());
        note("relationships",   !state.relationships.empty());
        if (!non_empty.empty()) {
            return core::Result<ScenarioLoadOutcome>::failure(
                manifest_path.string() +
                ": load_into_state requires an empty GameState"
                " (non-empty containers: " + non_empty + ")");
        }
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

    // ---- 7. M4.1 provinces ----------------------------------------
    // Each path in `manifest.provinces` points at a province file
    // resolved against `base_dir`. The file is parsed via
    // `parse_province_file`, then each entry's `owner_id_code` is
    // resolved against `country_by_id_code` and pushed to
    // `state.provinces`. Cross-file uniqueness of `id_code` is
    // enforced as we go.
    {
        std::unordered_map<std::string, std::size_t> province_index;
        for (std::size_t fi = 0; fi < manifest.provinces.size(); ++fi) {
            const auto province_path = base_dir / manifest.provinces[fi];
            auto text_r = read_whole_file(province_path);
            if (!text_r) {
                return core::Result<ScenarioLoadOutcome>::failure(
                    std::move(text_r.error()));
            }
            auto entries_r =
                parse_province_file(text_r.value(),
                                    province_path.string());
            if (!entries_r) {
                return core::Result<ScenarioLoadOutcome>::failure(
                    std::move(entries_r.error()));
            }
            const auto entries = std::move(entries_r).value();
            for (std::size_t i = 0; i < entries.size(); ++i) {
                const auto& entry = entries[i];

                auto owner_it = country_by_id_code.find(entry.owner_id_code);
                if (owner_it == country_by_id_code.end()) {
                    return core::Result<ScenarioLoadOutcome>::failure(
                        province_path.string() +
                        ": provinces[" + std::to_string(i) +
                        "] references unknown country id_code '" +
                        entry.owner_id_code + "'");
                }
                if (province_index.count(entry.id_code) != 0) {
                    return core::Result<ScenarioLoadOutcome>::failure(
                        province_path.string() +
                        ": provinces[" + std::to_string(i) +
                        "].id '" + entry.id_code +
                        "' is a duplicate across the scenario's"
                        " province files");
                }

                core::ProvinceNode p;
                p.id_code = entry.id_code;
                p.name    = entry.name;
                p.owner   = owner_it->second;
                p.x       = entry.x;
                p.y       = entry.y;
                province_index.emplace(p.id_code, state.provinces.size());
                state.provinces.push_back(std::move(p));
            }
        }
        outcome.provinces_loaded = static_cast<int>(state.provinces.size());
    }

    // ---- 8. M5.1 events -------------------------------------------
    // Each path in `manifest.events` points at an event file
    // resolved against `base_dir`. The file is parsed via
    // `parse_event_file`, which enforces the M5.1 trigger
    // target/op allowlists + value finiteness + non-empty
    // triggers + effects validation that mirrors
    // `data_loader::parse_policy`. Cross-file uniqueness of
    // `id_code` is enforced as we go.
    //
    // M5.1 loads + validates + stores ONLY — no event firing,
    // no trigger evaluator, no effects application, no history,
    // no monthly integration. The runner output artefacts are
    // byte-identical with M4 except for the save schema (v12 →
    // v13) and the canonical save's `events` block now carries
    // the two canonical-scenario event definitions instead of
    // an empty array.
    {
        std::unordered_map<std::string, std::size_t> event_index;
        for (std::size_t fi = 0; fi < manifest.events.size(); ++fi) {
            const auto event_path = base_dir / manifest.events[fi];
            auto text_r = read_whole_file(event_path);
            if (!text_r) {
                return core::Result<ScenarioLoadOutcome>::failure(
                    std::move(text_r.error()));
            }
            auto entries_r =
                parse_event_file(text_r.value(),
                                 event_path.string());
            if (!entries_r) {
                return core::Result<ScenarioLoadOutcome>::failure(
                    std::move(entries_r.error()));
            }
            const auto entries = std::move(entries_r).value();
            for (std::size_t i = 0; i < entries.size(); ++i) {
                const auto& entry = entries[i];
                if (event_index.count(entry.id_code) != 0) {
                    return core::Result<ScenarioLoadOutcome>::failure(
                        event_path.string() +
                        ": events[" + std::to_string(i) +
                        "].id '" + entry.id_code +
                        "' is a duplicate across the scenario's"
                        " event files");
                }
                core::EventDefinition ev;
                ev.id_code            = entry.id_code;
                ev.name               = entry.name;
                ev.description        = entry.description;
                ev.visible_report     = entry.visible_report;   // M6.2
                ev.true_cause         = entry.true_cause;
                ev.triggers           = entry.triggers;
                ev.effects            = entry.effects;
                // RCR-1 (RFC-090 §5.3 / §5.4 / §5.12):
                ev.weight_modifiers   = entry.weight_modifiers;
                ev.options            = entry.options;
                ev.followup_event_ids = entry.followup_event_ids;
                event_index.emplace(ev.id_code, state.events.size());
                state.events.push_back(std::move(ev));
            }
        }
        outcome.events_loaded = static_cast<int>(state.events.size());
    }

    // ---- Issue #108 fix: resolve manifest relationships --------------
    // After state.countries is fully populated, walk
    // manifest.relationships and resolve each from/to id_code to a
    // CountryId by linear scan. Unresolvable id_codes are a hard
    // failure (mirrors M1.13 starting_policies' "unknown actor"
    // rejection). The resolved CountryRelation entries are
    // appended to state.relationships in manifest order.
    if (!manifest.relationships.empty()) {
        auto resolve_country = [&](const std::string& id_code)
            -> std::optional<core::CountryId> {
            for (std::size_t k = 0; k < state.countries.size(); ++k) {
                if (state.countries[k].id_code == id_code) {
                    return core::CountryId{
                        static_cast<core::CountryId::underlying_type>(k)};
                }
            }
            return std::nullopt;
        };
        state.relationships.reserve(manifest.relationships.size());
        for (std::size_t i = 0; i < manifest.relationships.size(); ++i) {
            const auto& mr = manifest.relationships[i];
            const std::string ctx =
                "relationships[" + std::to_string(i) + "]";
            auto from = resolve_country(mr.from_id_code);
            if (!from.has_value()) {
                return core::Result<ScenarioLoadOutcome>::failure(
                    manifest_path.string() + ": " + ctx +
                    ": unknown 'from' country id_code '" +
                    mr.from_id_code + "'");
            }
            auto to = resolve_country(mr.to_id_code);
            if (!to.has_value()) {
                return core::Result<ScenarioLoadOutcome>::failure(
                    manifest_path.string() + ": " + ctx +
                    ": unknown 'to' country id_code '" +
                    mr.to_id_code + "'");
            }
            core::CountryRelation r;
            r.from         = from.value();
            r.to           = to.value();
            r.relationship = mr.relationship;
            r.threat       = mr.threat;
            state.relationships.push_back(std::move(r));
        }
    }

    return core::Result<ScenarioLoadOutcome>::success(outcome);
}

}  // namespace leviathan::systems::scenario_loader
