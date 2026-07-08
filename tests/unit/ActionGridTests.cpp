#include "TestSupport.hpp"

#include <optiflow/numerics/ActionGrid.hpp>

int main() {
    return run_test([] {
        const optiflow::ActionGrid grid(optiflow::ActionGridConfig{80.0, 60.0, 4, 3});
        OPTIFLOW_REQUIRE(grid.size() == 8);
        OPTIFLOW_REQUIRE(grid.at(0).mode == optiflow::HydroMode::Idle);
        for (const auto& action : grid.actions()) {
            OPTIFLOW_REQUIRE(!(action.turbine_flow_m3_s > 0.0 && action.pump_flow_m3_s > 0.0));
        }
        require_throws([] { optiflow::ActionGrid bad(optiflow::ActionGridConfig{1.0, 0.0, 0, 0}); });
        require_throws([] { optiflow::ActionGrid bad(optiflow::ActionGridConfig{0.0, 1.0, 0, 0}); });
    });
}
