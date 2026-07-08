#include "TestSupport.hpp"

#include <optiflow/numerics/StateGrid.hpp>

#include <vector>

int main() {
    return run_test([] {
        const optiflow::StateGrid grid(0.0, 100.0, 6);
        OPTIFLOW_REQUIRE(grid.size() == 6);
        OPTIFLOW_REQUIRE_NEAR(grid.at(0), 0.0, 1.0e-12);
        OPTIFLOW_REQUIRE_NEAR(grid.at(5), 100.0, 1.0e-12);
        OPTIFLOW_REQUIRE(grid.nearest_index(41.0) == 2);
        const optiflow::StateGrid anchored_grid(0.0, 100.0, 6, std::vector<double>{41.0, 60.0});
        OPTIFLOW_REQUIRE(anchored_grid.size() == 7);
        OPTIFLOW_REQUIRE_NEAR(anchored_grid.at(3), 41.0, 1.0e-12);
        OPTIFLOW_REQUIRE(anchored_grid.nearest_index(41.0) == 3);

        require_throws([] { optiflow::StateGrid bad(0.0, 1.0, 1); });
        require_throws([] { optiflow::StateGrid bad(1.0, 0.0, 2); });
        require_throws([] { optiflow::StateGrid bad(0.0, 1.0, 2, std::vector<double>{2.0}); });
    });
}
