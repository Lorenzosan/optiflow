#include "TestSupport.hpp"

#include <optiflow/numerics/Interpolator.hpp>

int main() {
    return run_test([] {
        const optiflow::StateGrid grid(0.0, 100.0, 3);
        optiflow::ValueFunction values(1, grid.size());
        values.set(0, 0, 0.0);
        values.set(0, 1, 50.0);
        values.set(0, 2, 100.0);
        OPTIFLOW_REQUIRE_NEAR(optiflow::Interpolator::interpolate(grid, values, 0, 25.0), 25.0, 1.0e-12);
        OPTIFLOW_REQUIRE_NEAR(optiflow::Interpolator::interpolate(grid, values, 0, 75.0), 75.0, 1.0e-12);
        OPTIFLOW_REQUIRE_NEAR(optiflow::Interpolator::interpolate(grid, values, 0, -1.0), 0.0, 1.0e-12);
        OPTIFLOW_REQUIRE_NEAR(optiflow::Interpolator::interpolate(grid, values, 0, 101.0), 100.0, 1.0e-12);
    });
}
