#include "optiflow/service/OptimizerServiceImpl.h"

#include "optiflow/service/ProtoConversion.h"

#include <exception>
#include <stdexcept>
#include <utility>

namespace optiflow::service {

OptimizerServiceImpl::OptimizerServiceImpl() = default;

OptimizerServiceImpl::OptimizerServiceImpl(runner::OptimizationRunner runner)
    : runner_(std::move(runner)) {}

grpc::Status OptimizerServiceImpl::Optimize(grpc::ServerContext* context,
                                            const optimizer::v1::OptimizeRequest* request,
                                            optimizer::v1::OptimizeResponse* response) {
    (void)context;

    try {
        const core::ScenarioBundle bundle = toScenarioBundle(*request);
        const runner::OptimizationResult result = runner_.run(bundle);
        fillOptimizeResponse(result, *response);
        return grpc::Status::OK;
    } catch (const std::invalid_argument& error) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, error.what());
    } catch (const std::out_of_range& error) {
        return grpc::Status(grpc::StatusCode::OUT_OF_RANGE, error.what());
    } catch (const std::exception& error) {
        return grpc::Status(grpc::StatusCode::INTERNAL, error.what());
    }
}

}  // namespace optiflow::service
