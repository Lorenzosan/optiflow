FROM debian:bookworm AS build

RUN apt-get update \
  && apt-get install -y --no-install-recommends build-essential cmake ninja-build \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DOPTIFLOW_BUILD_CLI=OFF \
  -DOPTIFLOW_BUILD_TESTS=OFF \
  -DOPTIFLOW_BUILD_SERVICES=ON
RUN cmake --build build --target optiflow_api_service

FROM debian:bookworm-slim
RUN apt-get update \
  && apt-get install -y --no-install-recommends wget \
  && rm -rf /var/lib/apt/lists/*
COPY --from=build /src/build/services/api_service/optiflow_api_service /usr/local/bin/optiflow_api_service
EXPOSE 8080
CMD ["optiflow_api_service"]
