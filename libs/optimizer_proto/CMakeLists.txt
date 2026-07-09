find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

set(OPTIFLOW_PROTO_ROOT
    "${CMAKE_SOURCE_DIR}/proto"
)

set(OPTIFLOW_PROTO_FILE
    "${OPTIFLOW_PROTO_ROOT}/optiflow/optimizer/v1/optimizer.proto"
)

set(OPTIFLOW_PROTO_GEN_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

set(OPTIFLOW_PROTO_GENERATED_SOURCES
    "${OPTIFLOW_PROTO_GEN_DIR}/optiflow/optimizer/v1/optimizer.pb.cc"
    "${OPTIFLOW_PROTO_GEN_DIR}/optiflow/optimizer/v1/optimizer.grpc.pb.cc"
)

set(OPTIFLOW_PROTO_GENERATED_HEADERS
    "${OPTIFLOW_PROTO_GEN_DIR}/optiflow/optimizer/v1/optimizer.pb.h"
    "${OPTIFLOW_PROTO_GEN_DIR}/optiflow/optimizer/v1/optimizer.grpc.pb.h"
)

file(MAKE_DIRECTORY "${OPTIFLOW_PROTO_GEN_DIR}")

add_custom_command(
    OUTPUT
        ${OPTIFLOW_PROTO_GENERATED_SOURCES}
        ${OPTIFLOW_PROTO_GENERATED_HEADERS}
    COMMAND protobuf::protoc
    ARGS
        --proto_path=${OPTIFLOW_PROTO_ROOT}
        --cpp_out=${OPTIFLOW_PROTO_GEN_DIR}
        --grpc_out=${OPTIFLOW_PROTO_GEN_DIR}
        --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
        ${OPTIFLOW_PROTO_FILE}
    DEPENDS
        ${OPTIFLOW_PROTO_FILE}
    COMMENT
        "Generating optimizer protobuf and gRPC sources"
    VERBATIM
)

set_source_files_properties(
    ${OPTIFLOW_PROTO_GENERATED_SOURCES}
    ${OPTIFLOW_PROTO_GENERATED_HEADERS}
    PROPERTIES GENERATED TRUE
)

add_library(optiflow_optimizer_proto STATIC
    ${OPTIFLOW_PROTO_GENERATED_SOURCES}
    ${OPTIFLOW_PROTO_GENERATED_HEADERS}
)

target_include_directories(optiflow_optimizer_proto
    PUBLIC
        "${OPTIFLOW_PROTO_GEN_DIR}"
)

target_link_libraries(optiflow_optimizer_proto
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
)
