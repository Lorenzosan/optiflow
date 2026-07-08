#include "TestSupport.hpp"

#include <optiflow/numerics/StateGrid.hpp>

int main() {
    return run_test([] {
        const optiflow::StateGrid grid(0.0, 100.0, 6);
        OPTIFLOW_REQUIRE(grid.size() == 6);
        OPTIFLOW_REQUIRE_NEAR(grid.at(0), 0.0, 1.0e-12);
        OPTIFLOW_REQUIRE_NEAR(grid.at(5), 100.0, 1.0e-12);
        OPTIFLOW_REQUIRE(grid.nearest_index(41.0) == 2);
        require_throws([] { optiflow::StateGrid bad(0.0, 1.0, 1); });
        require_throws([] { optiflow::StateGrid bad(1.0, 0.0, 2); });
    });
}
