#include "TestSupport.hpp"

#include <optiflow/numerics/ValueFunction.hpp>

int main() {
    return run_test([] {
        optiflow::ValueFunction values(3, 4);
        OPTIFLOW_REQUIRE(values.time_count() == 4);
        OPTIFLOW_REQUIRE(values.state_count() == 4);
        values.set(2, 3, 42.0);
        OPTIFLOW_REQUIRE_NEAR(values.at(2, 3), 42.0, 1.0e-12);
        require_throws([&] { static_cast<void>(values.at(4, 0)); });
        require_throws([] { optiflow::ValueFunction bad(1, 0); });
    });
}
