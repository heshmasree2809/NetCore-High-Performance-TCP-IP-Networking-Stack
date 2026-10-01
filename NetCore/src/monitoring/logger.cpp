#include "monitoring/logger.hpp"

namespace netcore {
// Logger singleton and methods are defined in header for performance and inlining,
// but logger.cpp ensures proper translation unit linkage for any future static symbols.
} // namespace netcore
