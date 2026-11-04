# Multi-stage Dockerfile: Single Unified Smart Meter Test & Verification Container
# Stage 1: Build unit tests and simulator
FROM alpine:latest AS builder

LABEL Name=stm32u585vit6sm Version=0.0.1

RUN apk update && apk upgrade --no-cache && \
    apk add --no-cache build-base cmake ninja

WORKDIR /workspace

# Copy source directories required for building tests and simulator
COPY NonSecure /workspace/NonSecure
COPY tests /workspace/tests
COPY docker_test /workspace/docker_test

# Configure and compile all 14 test suites and gurux_simulator using Ninja
RUN cmake -S tests -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --parallel

# Stage 2: Minimal hardened runtime container
FROM alpine:latest AS runner

LABEL Name=stm32u585vit6sm Version=0.0.1

RUN apk update && apk upgrade --no-cache && \
    apk add --no-cache bash python3 cmake

WORKDIR /workspace

# Copy compiled binaries and test metadata
COPY --from=builder /workspace/build ./build
COPY --from=builder /workspace/tests ./tests

# Copy test orchestration scripts
COPY docker_test/run_all_docker_tests.sh ./run_all_docker_tests.sh
COPY docker_test/test_dlms_e2e_client.py ./test_dlms_e2e_client.py

RUN chmod +x ./run_all_docker_tests.sh ./test_dlms_e2e_client.py

# Expose DLMS simulator port (HDLC-over-TCP / Wrapper)
EXPOSE 4059

CMD ["./run_all_docker_tests.sh"]
