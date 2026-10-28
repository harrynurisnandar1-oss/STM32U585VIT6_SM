# ========================================================
# Stage 1: Build environment (compilation only)
# ========================================================
FROM alpine:latest AS builder

RUN apk update && \
    apk upgrade --no-cache && \
    apk add --no-cache \
        build-base \
        cmake \
        ninja

WORKDIR /workspace

COPY NonSecure /workspace/NonSecure
COPY tests /workspace/tests
COPY docker_test /workspace/docker_test

WORKDIR /workspace/docker_test
RUN cmake -S . -B build-docker -G Ninja -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build-docker --parallel

# ========================================================
# Stage 2: Minimal hardened runner (Zero CVEs)
# ========================================================
FROM alpine:latest AS runner

# Install only minimal runtime: python3 (for e2e client) and bash
RUN apk update && \
    apk upgrade --no-cache && \
    apk add --no-cache \
        python3 \
        bash

WORKDIR /workspace/docker_test

# Copy pre-compiled test binaries from builder
COPY --from=builder /workspace/docker_test/build-docker ./build-docker
COPY docker_test/run_tests.sh ./
COPY docker_test/test_dlms_e2e_client.py ./

RUN chmod +x run_tests.sh test_dlms_e2e_client.py

CMD ["./run_tests.sh"]

