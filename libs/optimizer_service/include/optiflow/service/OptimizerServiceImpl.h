#pragma once

#include "optiflow/runner/OptimizationRunner.h"

#include "optiflow/optimizer/v1/optimizer.grpc.pb.h"

#include <grpcpp/grpcpp.h>

namespace optiflow::service {

/**
 * @brief gRPC implementation of the optimizer service.
 *
 * The service is intentionally thin. It converts protobuf requests to the core
 * scenario model, calls OptimizationRunner, and converts the result back to the
 * protobuf response type.
 */
class OptimizerServiceImpl final : public optimizer::v1::OptimizerService::Service {
public:
    /**
     * @brief Construct a service with a default optimization runner.
     */
    OptimizerServiceImpl();

    /**
     * @brief Construct a service with an explicit optimization runner.
     *
     * @param runner Optimization runner used to execute requests.
     */
    explicit OptimizerServiceImpl(runner::OptimizationRunner runner);

    /**
     * @brief Run one optimization request.
     *
     * @param context gRPC server context.
     * @param request Optimization request.
     * @param response Optimization response to fill on success.
     * @return gRPC status describing success or failure.
     */
    grpc::Status Optimize(grpc::ServerContext* context,
                          const optimizer::v1::OptimizeRequest* request,
                          optimizer::v1::OptimizeResponse* response) override;

private:
    runner::OptimizationRunner runner_;
};

}  // namespace optiflow::service
