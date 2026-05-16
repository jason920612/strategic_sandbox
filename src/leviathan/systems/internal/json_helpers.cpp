#include "internal/json_helpers.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace leviathan::systems::detail {

std::string fmt_err(std::string_view source, std::string_view msg) {
    std::string out;
    out.reserve(source.size() + msg.size() + 2);
    out.append(source.data(), source.size());
    out.append(": ");
    out.append(msg.data(), msg.size());
    return out;
}

const json* navigate(const json& root, std::string_view dotted_path) {
    const json* cur = &root;
    std::size_t pos = 0;
    while (pos <= dotted_path.size()) {
        const std::size_t dot = dotted_path.find('.', pos);
        const std::size_t end = (dot == std::string_view::npos)
                                ? dotted_path.size()
                                : dot;
        const std::string segment(dotted_path.substr(pos, end - pos));
        if (!cur->is_object()) return nullptr;
        if (!cur->contains(segment)) return nullptr;
        cur = &cur->at(segment);
        if (dot == std::string_view::npos) break;
        pos = dot + 1;
    }
    return cur;
}

core::Result<std::string> require_string(const json& root,
                                         std::string_view path,
                                         std::string_view source) {
    const json* v = navigate(root, path);
    if (v == nullptr) {
        std::string msg = "missing required field '";
        msg.append(path.data(), path.size());
        msg += "'";
        return core::Result<std::string>::failure(fmt_err(source, msg));
    }
    if (!v->is_string()) {
        std::string msg = "'";
        msg.append(path.data(), path.size());
        msg += "' has wrong type (expected string)";
        return core::Result<std::string>::failure(fmt_err(source, msg));
    }
    return core::Result<std::string>::success(v->get<std::string>());
}

core::Result<double> require_number(const json& root,
                                    std::string_view path,
                                    std::string_view source) {
    const json* v = navigate(root, path);
    if (v == nullptr) {
        std::string msg = "missing required field '";
        msg.append(path.data(), path.size());
        msg += "'";
        return core::Result<double>::failure(fmt_err(source, msg));
    }
    if (!v->is_number()) {
        std::string msg = "'";
        msg.append(path.data(), path.size());
        msg += "' has wrong type (expected number)";
        return core::Result<double>::failure(fmt_err(source, msg));
    }
    const double d = v->get<double>();
    if (!std::isfinite(d)) {
        std::string msg = "'";
        msg.append(path.data(), path.size());
        msg += "' is not finite";
        return core::Result<double>::failure(fmt_err(source, msg));
    }
    return core::Result<double>::success(d);
}

core::Result<double> require_nonneg_number(const json& root,
                                           std::string_view path,
                                           std::string_view source) {
    auto n = require_number(root, path, source);
    if (!n) return core::Result<double>::failure(std::move(n.error()));
    if (n.value() < 0.0) {
        std::string msg = "'";
        msg.append(path.data(), path.size());
        msg += "' must be >= 0 (got ";
        msg += std::to_string(n.value());
        msg += ")";
        return core::Result<double>::failure(fmt_err(source, msg));
    }
    return n;
}

core::Result<double> require_ratio(const json& root,
                                   std::string_view path,
                                   std::string_view source) {
    auto n = require_number(root, path, source);
    if (!n) return core::Result<double>::failure(std::move(n.error()));
    if (n.value() < 0.0 || n.value() > 1.0) {
        std::string msg = "'";
        msg.append(path.data(), path.size());
        msg += "' must be in [0, 1] (got ";
        msg += std::to_string(n.value());
        msg += ")";
        return core::Result<double>::failure(fmt_err(source, msg));
    }
    return n;
}

core::Result<std::uint64_t> require_u64(const json& root,
                                        std::string_view path,
                                        std::string_view source) {
    const json* v = navigate(root, path);
    if (v == nullptr) {
        std::string msg = "missing required field '";
        msg.append(path.data(), path.size());
        msg += "'";
        return core::Result<std::uint64_t>::failure(fmt_err(source, msg));
    }
    if (v->is_number_unsigned()) {
        return core::Result<std::uint64_t>::success(v->get<std::uint64_t>());
    }
    if (v->is_number_integer()) {
        const auto s = v->get<std::int64_t>();
        if (s < 0) {
            std::string msg = "'";
            msg.append(path.data(), path.size());
            msg += "' is negative";
            return core::Result<std::uint64_t>::failure(fmt_err(source, msg));
        }
        return core::Result<std::uint64_t>::success(static_cast<std::uint64_t>(s));
    }
    std::string msg = "'";
    msg.append(path.data(), path.size());
    msg += "' has wrong type (expected unsigned integer)";
    return core::Result<std::uint64_t>::failure(fmt_err(source, msg));
}

}  // namespace leviathan::systems::detail
