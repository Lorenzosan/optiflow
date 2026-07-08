#include "TestSupport.hpp"

#include <optiflow/io/CsvReader.hpp>

int main() {
    return run_test([] {
        const auto prices = write_temp_file("optiflow_prices_valid.csv",
                                            "time_index,price_eur_per_mwh\n0,10.5\n1,-2.0\n");
        const auto inflows = write_temp_file("optiflow_inflows_valid.csv",
                                             "time_index,natural_inflow_m3_s\n0,1.0\n1,2.0\n");

        const auto series = optiflow::read_deterministic_series(prices, inflows);
        OPTIFLOW_REQUIRE(series.size() == 2);
        OPTIFLOW_REQUIRE_NEAR(series.points.at(0).price_eur_per_mwh, 10.5, 1.0e-12);
        OPTIFLOW_REQUIRE_NEAR(series.points.at(1).price_eur_per_mwh, -2.0, 1.0e-12);
        OPTIFLOW_REQUIRE_NEAR(series.points.at(1).natural_inflow_m3_s, 2.0, 1.0e-12);

        const auto bad_header = write_temp_file("optiflow_prices_bad_header.csv",
                                                "time,price_eur_per_mwh\n0,10.0\n");
        require_throws([&] { optiflow::read_deterministic_series(bad_header, inflows); });

        const auto duplicate = write_temp_file("optiflow_prices_duplicate.csv",
                                               "time_index,price_eur_per_mwh\n0,10.0\n0,11.0\n");
        require_throws([&] { optiflow::read_deterministic_series(duplicate, inflows); });

        const auto negative_inflow = write_temp_file("optiflow_inflows_negative.csv",
                                                     "time_index,natural_inflow_m3_s\n0,-1.0\n1,2.0\n");
        require_throws([&] { optiflow::read_deterministic_series(prices, negative_inflow); });

        const auto mismatched = write_temp_file("optiflow_inflows_mismatched.csv",
                                                "time_index,natural_inflow_m3_s\n0,1.0\n2,2.0\n");
        require_throws([&] { optiflow::read_deterministic_series(prices, mismatched); });
    });
}
