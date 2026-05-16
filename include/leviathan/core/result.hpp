// Minimal Result<T, E> for fallible operations in the simulation core.
//
// Pre-C++23 we cannot use std::expected. We deliberately do not throw
// exceptions across the simulation boundary, so every operation that can
// fail returns a Result with either a value or an error.
//
// E defaults to std::string because the most common case is "data loader
// rejected this file, here is the message". Specialise E with a richer
// error type if a caller needs structured error categories later.

#ifndef LEVIATHAN_CORE_RESULT_HPP
#define LEVIATHAN_CORE_RESULT_HPP

#include <cassert>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace leviathan::core {

template <typename T, typename E = std::string>
class Result {
    static_assert(!std::is_same_v<T, E>,
                  "Result<T, E> requires T and E to be distinct types");

public:
    static Result success(T value) {
        return Result(std::in_place_index<0>, std::move(value));
    }

    static Result failure(E error) {
        return Result(std::in_place_index<1>, std::move(error));
    }

    bool ok() const noexcept { return data_.index() == 0; }
    bool failed() const noexcept { return data_.index() == 1; }
    explicit operator bool() const noexcept { return ok(); }

    const T& value() const& {
        assert(ok() && "Result::value() called on a failed Result");
        return std::get<0>(data_);
    }
    T& value() & {
        assert(ok() && "Result::value() called on a failed Result");
        return std::get<0>(data_);
    }
    T&& value() && {
        assert(ok() && "Result::value() called on a failed Result");
        return std::move(std::get<0>(data_));
    }

    const E& error() const& {
        assert(failed() && "Result::error() called on a successful Result");
        return std::get<1>(data_);
    }
    E& error() & {
        assert(failed() && "Result::error() called on a successful Result");
        return std::get<1>(data_);
    }

    T value_or(T fallback) const& {
        return ok() ? std::get<0>(data_) : std::move(fallback);
    }

private:
    template <std::size_t I, typename Arg>
    Result(std::in_place_index_t<I>, Arg&& arg)
        : data_(std::in_place_index<I>, std::forward<Arg>(arg)) {}

    std::variant<T, E> data_;
};

}  // namespace leviathan::core

#endif  // LEVIATHAN_CORE_RESULT_HPP
