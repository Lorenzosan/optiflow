#include <optiflow/core/StorageTypes.hpp>

namespace optiflow {

std::size_t DeterministicSeries::size() const noexcept { return points.size(); }

bool DeterministicSeries::empty() const noexcept { return points.empty(); }

} // namespace optiflow
