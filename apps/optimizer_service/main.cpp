#include "optiflow/service/OptimizerServiceImpl.h"

#include <grpcpp/grpcpp.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

struct CliOptions {
    std::string address;
    bool help_requested;
};

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " [--address <host:port>]\n\n"
              << "Options:\n"
              << "  --address   Address for the local gRPC service. "
              << "Default: 127.0.0.1:50051.\n"
              << "  --help      Print this help message.\n";
}

CliOptions parse_args(int argc, char** argv) {
    CliOptions options{"127.0.0.1:50051", false};

    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "--address" && index + 1 < argc) {
            options.address = argv[++index];
        } else if (arg == "--help" || arg == "-h") {
            options.help_requested = true;
        } else {
            throw std::invalid_argument("unknown or incomplete argument: " + arg);
        }
    }

    if (options.address.empty()) {
        throw std::invalid_argument("--address must not be empty");
    }

    return options;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const CliOptions options = parse_args(argc, argv);
        if (options.help_requested) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }

        optiflow::service::OptimizerServiceImpl service;

        grpc::ServerBuilder builder;
        builder.AddListeningPort(options.address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);

        std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
        if (!server) {
            throw std::runtime_error("failed to start optimizer service on " + options.address);
        }

        std::cout << "OptiFlow optimizer service listening on " << options.address << '\n';
        server->Wait();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
}
