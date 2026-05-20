// preprocessing.hpp
// Minimal preprocessing for ApplyCalleTypeCpp

#ifndef PREPROCESSING_HPP
#define PREPROCESSING_HPP

#include <string>
#include <string_view>
#include <optional>

namespace preprocessing {

using Result = std::optional<std::string>;

Result preprocess_prototype(std::string_view input);

} // namespace preprocessing

#endif // PREPROCESSING_HPP