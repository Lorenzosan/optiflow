# Protobuf service boundary

`optimizer.proto` is the intended production service contract between the API service and the optimizer service.

The current local demo services use a tiny dependency-free HTTP transport so that the repository builds without installing gRPC. The proto file is kept as the contract to migrate the lightweight `/v1/optimize` endpoint to a real gRPC service once `grpc++` and `protobuf` are added to the toolchain.
