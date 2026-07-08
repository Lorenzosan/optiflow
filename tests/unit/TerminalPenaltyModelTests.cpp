#include "TestSupport.hpp"

#include <optiflow/model/TerminalPenaltyModel.hpp>

int main() {
    return run_test([] {
        const optiflow::TerminalPenaltyModel model(1000.0, 0.25);

        OPTIFLOW_REQUIRE_NEAR(model.deviation_m3(1000.0), 0.0, 1.0e-12);
        OPTIFLOW_REQUIRE_NEAR(model.penalty_cost_eur(1000.0), 0.0, 1.0e-12);
        OPTIFLOW_REQUIRE_NEAR(model.value(1000.0), 0.0, 1.0e-12);

        OPTIFLOW_REQUIRE_NEAR(model.deviation_m3(800.0), 200.0, 1.0e-12);
        OPTIFLOW_REQUIRE_NEAR(model.penalty_cost_eur(800.0), 50.0, 1.0e-12);
        OPTIFLOW_REQUIRE_NEAR(model.value(800.0), -50.0, 1.0e-12);

        OPTIFLOW_REQUIRE_NEAR(model.deviation_m3(1200.0), 200.0, 1.0e-12);
        OPTIFLOW_REQUIRE_NEAR(model.penalty_cost_eur(1200.0), 50.0, 1.0e-12);
        OPTIFLOW_REQUIRE_NEAR(model.value(1200.0), -50.0, 1.0e-12);
    });
}
