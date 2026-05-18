// M6.4: ReportedValue implementation.
//
// See include/leviathan/systems/reported_value.hpp for the
// public contract (placeholder formula, range, validation,
// composition with M6.3, deliberate non-goals).
//
// The body is intentionally tiny: validate both inputs and
// return `true_value * accuracy`. M6.5 will wrap this in a
// bias / noise path; M6.6 / M6.7 will reduce the upstream
// M6.3 accuracy below 1.0; M6.8 / M6.9 will branch on debug
// vs non-debug display. None of those change THIS function's
// body — they compose around it.

#include "leviathan/systems/reported_value.hpp"

#include <cmath>
#include <string>

namespace leviathan::systems::reported_value {

namespace {

// Format a double for error messages with enough precision to
// uniquely identify NaN / Inf / out-of-range values. Mirrors
// the M1.5 / M5.6 effect-validation message format.
std::string format_double_for_error(double v) {
    if (std::isnan(v)) {
        return "nan";
    }
    if (std::isinf(v)) {
        return v < 0 ? "-inf" : "+inf";
    }
    // M6.4 doesn't need byte-stable formatting (the messages
    // are runtime-only, not save-serialised). std::to_string
    // is good enough.
    return std::to_string(v);
}

}  // namespace

core::Result<double> from_true_value(double true_value,
                                     double accuracy) {
    if (!std::isfinite(true_value)) {
        return core::Result<double>::failure(
            "reported_value::from_true_value: true_value "
            + format_double_for_error(true_value) +
            " is not finite");
    }
    if (!std::isfinite(accuracy)) {
        return core::Result<double>::failure(
            "reported_value::from_true_value: accuracy "
            + format_double_for_error(accuracy) +
            " is not finite");
    }
    if (accuracy < 0.0 || accuracy > 1.0) {
        return core::Result<double>::failure(
            "reported_value::from_true_value: accuracy "
            + format_double_for_error(accuracy) +
            " is outside the [0, 1] range");
    }

    // M6.4 skeleton formula: linear interpolation toward 0
    // by the accuracy weight. At accuracy=1.0 the player
    // sees true_value verbatim; at accuracy=0.0 the player
    // sees 0; intermediate values blend linearly.
    return core::Result<double>::success(true_value * accuracy);
}

}  // namespace leviathan::systems::reported_value
