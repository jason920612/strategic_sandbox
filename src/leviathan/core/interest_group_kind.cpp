#include "leviathan/core/interest_group_kind.hpp"

#include <string>
#include <string_view>
#include <utility>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::core {

std::string interest_group_kind_to_string(InterestGroupKind kind) {
    switch (kind) {
        case InterestGroupKind::Bureaucracy:  return "Bureaucracy";
        case InterestGroupKind::Military:     return "Military";
        case InterestGroupKind::Workers:      return "Workers";
        case InterestGroupKind::Farmers:      return "Farmers";
        case InterestGroupKind::Religious:    return "Religious";
        case InterestGroupKind::Media:        return "Media";
        case InterestGroupKind::Students:     return "Students";
        case InterestGroupKind::LocalElites:  return "LocalElites";
        case InterestGroupKind::Business:     return "Business";
        case InterestGroupKind::Technocrats:  return "Technocrats";
    }
    return "UnknownInterestGroupKind";
}

Result<InterestGroupKind> interest_group_kind_from_string(std::string_view s) {
    if (s == "Bureaucracy")  return Result<InterestGroupKind>::success(InterestGroupKind::Bureaucracy);
    if (s == "Military")     return Result<InterestGroupKind>::success(InterestGroupKind::Military);
    if (s == "Workers")      return Result<InterestGroupKind>::success(InterestGroupKind::Workers);
    if (s == "Farmers")      return Result<InterestGroupKind>::success(InterestGroupKind::Farmers);
    if (s == "Religious")    return Result<InterestGroupKind>::success(InterestGroupKind::Religious);
    if (s == "Media")        return Result<InterestGroupKind>::success(InterestGroupKind::Media);
    if (s == "Students")     return Result<InterestGroupKind>::success(InterestGroupKind::Students);
    if (s == "LocalElites")  return Result<InterestGroupKind>::success(InterestGroupKind::LocalElites);
    if (s == "Business")     return Result<InterestGroupKind>::success(InterestGroupKind::Business);
    if (s == "Technocrats")  return Result<InterestGroupKind>::success(InterestGroupKind::Technocrats);
    std::string msg = "unknown interest_group kind '";
    msg.append(s.data(), s.size());
    msg += "' (expected Bureaucracy|Military|Workers|Farmers|"
           "Religious|Media|Students|LocalElites|Business|Technocrats)";
    return Result<InterestGroupKind>::failure(std::move(msg));
}

}  // namespace leviathan::core
