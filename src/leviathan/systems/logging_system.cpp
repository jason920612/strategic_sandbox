#include "leviathan/systems/logging_system.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace leviathan::systems::logging {

namespace {

// Minimal JSON string escape covering the cases that actually appear
// in log fields: quote, backslash, control characters. Non-ASCII
// bytes are passed through verbatim, which is correct for valid UTF-8
// input (the simulation's strings are all UTF-8).
void append_json_string(std::string& out, std::string_view text) {
    out += '"';
    for (char raw : text) {
        const unsigned char c = static_cast<unsigned char>(raw);
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    // Other ASCII control characters -> \u00XX
                    std::array<char, 8> buf{};
                    const int n = std::snprintf(buf.data(), buf.size(),
                                                "\\u%04x", c);
                    if (n > 0) out.append(buf.data(), static_cast<std::size_t>(n));
                } else {
                    out += static_cast<char>(c);
                }
                break;
        }
    }
    out += '"';
}

void append_metadata(std::string& out, const core::LogMetadata& metadata) {
    out += '{';
    bool first = true;
    for (const auto& [key, value] : metadata) {
        if (!first) out += ',';
        first = false;
        append_json_string(out, key);
        out += ':';
        append_json_string(out, value);
    }
    out += '}';
}

const std::string* metadata_value(const core::LogMetadata& metadata,
                                  std::string_view key) {
    for (const auto& [k, v] : metadata) {
        if (k == key) {
            return &v;
        }
    }
    return nullptr;
}

core::Result<double> parse_required_number(const core::LogEntry& entry,
                                           std::string_view key) {
    const std::string* text = metadata_value(entry.metadata, key);
    if (text == nullptr) {
        return core::Result<double>::failure(
            "event_reports.jsonl: event_fired '" + entry.message +
            "' missing required metadata '" + std::string(key) + "'");
    }
    char* end = nullptr;
    const double value = std::strtod(text->c_str(), &end);
    if (end == text->c_str() || end == nullptr || *end != '\0' ||
        !std::isfinite(value)) {
        return core::Result<double>::failure(
            "event_reports.jsonl: event_fired '" + entry.message +
            "' metadata '" + std::string(key) +
            "' is not a finite number");
    }
    return core::Result<double>::success(value);
}

core::Result<std::string> parse_required_string(const core::LogEntry& entry,
                                                std::string_view key) {
    const std::string* text = metadata_value(entry.metadata, key);
    if (text == nullptr || text->empty()) {
        return core::Result<std::string>::failure(
            "event_reports.jsonl: event_fired '" + entry.message +
            "' missing required non-empty metadata '" +
            std::string(key) + "'");
    }
    return core::Result<std::string>::success(*text);
}

void append_json_number(std::string& out, double value) {
    char buf[32];
    const int n = std::snprintf(buf, sizeof(buf), "%.17g", value);
    if (n > 0) {
        out.append(buf, static_cast<std::size_t>(n));
    }
}

}  // namespace

std::string severity_to_string(core::LogSeverity severity) {
    switch (severity) {
        case core::LogSeverity::Debug: return "debug";
        case core::LogSeverity::Info:  return "info";
        case core::LogSeverity::Warn:  return "warn";
        case core::LogSeverity::Error: return "error";
    }
    return "info";  // unreachable under a sane LogSeverity value
}

void log(core::GameState& state,
         std::string category,
         core::LogSeverity severity,
         std::string source,
         std::string message,
         core::LogMetadata metadata) {
    core::LogEntry entry;
    entry.date     = state.current_date;
    entry.category = std::move(category);
    entry.severity = severity;
    entry.source   = std::move(source);
    entry.message  = std::move(message);
    entry.metadata = std::move(metadata);
    state.logs.push_back(std::move(entry));
}

void log_debug(core::GameState& state, std::string category,
               std::string source, std::string message,
               core::LogMetadata metadata) {
    log(state, std::move(category), core::LogSeverity::Debug,
        std::move(source), std::move(message), std::move(metadata));
}

void log_info(core::GameState& state, std::string category,
              std::string source, std::string message,
              core::LogMetadata metadata) {
    log(state, std::move(category), core::LogSeverity::Info,
        std::move(source), std::move(message), std::move(metadata));
}

void log_warn(core::GameState& state, std::string category,
              std::string source, std::string message,
              core::LogMetadata metadata) {
    log(state, std::move(category), core::LogSeverity::Warn,
        std::move(source), std::move(message), std::move(metadata));
}

void log_error(core::GameState& state, std::string category,
               std::string source, std::string message,
               core::LogMetadata metadata) {
    log(state, std::move(category), core::LogSeverity::Error,
        std::move(source), std::move(message), std::move(metadata));
}

std::vector<core::LogEntry> recent(const core::GameState& state,
                                   std::size_t n) {
    const std::size_t total = state.logs.size();
    const std::size_t take  = (n < total) ? n : total;
    const std::size_t start = total - take;
    return std::vector<core::LogEntry>(state.logs.begin() + static_cast<std::ptrdiff_t>(start),
                                       state.logs.end());
}

void write_jsonl_line(std::ostream& out, const core::LogEntry& entry,
                      bool debug_mode) {
    // Build the line in a buffer first so we can write it as a single
    // ostream operation - reduces interleaving risk if the same stream
    // is shared.
    std::string line;
    line.reserve(128 + entry.message.size());

    line += "{\"date\":";
    append_json_string(line, entry.date.to_string());

    line += ",\"category\":";
    append_json_string(line, entry.category);

    line += ",\"severity\":";
    append_json_string(line, severity_to_string(entry.severity));

    line += ",\"source\":";
    append_json_string(line, entry.source);

    line += ",\"message\":";
    append_json_string(line, entry.message);

    line += ",\"metadata\":";
    // M6.8 (RFC-090 §6.8 "debug 模式顯示真相"): when debug_mode
    // is false, strip the `true_cause` metadata key that
    // event_firer attaches unconditionally to every event_fired
    // entry. The filtered list is reconstructed as a temporary so
    // the original `entry.metadata` (and therefore the source
    // state.logs entry, and therefore the save.json `logs`
    // serialisation) is unaffected — the truth stays available
    // for inspection, replay, and debug tooling; only the
    // events.jsonl artefact hides it.
    if (debug_mode) {
        append_metadata(line, entry.metadata);
    } else {
        core::LogMetadata filtered;
        filtered.reserve(entry.metadata.size());
        for (const auto& kv : entry.metadata) {
            if (kv.first != "true_cause") {
                filtered.push_back(kv);
            }
        }
        append_metadata(line, filtered);
    }

    line += "}\n";

    out.write(line.data(), static_cast<std::streamsize>(line.size()));
}

void export_jsonl(std::ostream& out, const core::GameState& state,
                  bool debug_mode) {
    for (const auto& entry : state.logs) {
        write_jsonl_line(out, entry, debug_mode);
    }
}

core::Result<bool> export_event_reports_jsonl(
        std::ostream& out,
        const core::GameState& state) {
    for (const auto& entry : state.logs) {
        if (entry.category != "event_fired") {
            continue;
        }

        auto event_id = parse_required_string(entry, "event_id_code");
        if (!event_id) {
            return core::Result<bool>::failure(std::move(event_id.error()));
        }
        auto country_id = parse_required_string(entry, "country_id_code");
        if (!country_id) {
            return core::Result<bool>::failure(std::move(country_id.error()));
        }
        auto public_text = parse_required_string(entry, "publicText");
        if (!public_text) {
            return core::Result<bool>::failure(std::move(public_text.error()));
        }
        auto true_intensity = parse_required_number(entry, "true_intensity");
        if (!true_intensity) {
            return core::Result<bool>::failure(std::move(true_intensity.error()));
        }
        auto reported_intensity =
            parse_required_number(entry, "reported_intensity");
        if (!reported_intensity) {
            return core::Result<bool>::failure(
                std::move(reported_intensity.error()));
        }
        auto bias_total = parse_required_number(entry, "bias_total");
        if (!bias_total) {
            return core::Result<bool>::failure(std::move(bias_total.error()));
        }
        auto noise_sample = parse_required_number(entry, "noise_sample");
        if (!noise_sample) {
            return core::Result<bool>::failure(std::move(noise_sample.error()));
        }
        auto perceived_intensity =
            parse_required_number(entry, "perceived_intensity");
        if (!perceived_intensity) {
            return core::Result<bool>::failure(
                std::move(perceived_intensity.error()));
        }

        std::string line;
        line += "{\"date\":";
        append_json_string(line, entry.date.to_string());
        line += ",\"event_id_code\":";
        append_json_string(line, event_id.value());
        line += ",\"country_id_code\":";
        append_json_string(line, country_id.value());
        line += ",\"publicText\":";
        append_json_string(line, public_text.value());
        line += ",\"true_intensity\":";
        append_json_number(line, true_intensity.value());
        line += ",\"reported_intensity\":";
        append_json_number(line, reported_intensity.value());
        line += ",\"bias_total\":";
        append_json_number(line, bias_total.value());
        line += ",\"noise_sample\":";
        append_json_number(line, noise_sample.value());
        line += ",\"perceived_intensity\":";
        append_json_number(line, perceived_intensity.value());
        line += "}\n";
        out.write(line.data(), static_cast<std::streamsize>(line.size()));
    }
    return core::Result<bool>::success(true);
}

}  // namespace leviathan::systems::logging
