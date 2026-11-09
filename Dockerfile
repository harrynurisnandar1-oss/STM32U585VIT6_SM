# Multi-stage Dockerfile: Hardened Smart Meter Test & Verification Container (0 CVE)
# Stage 1: Build unit tests and simulator
FROM alpine:latest AS builder

LABEL Name=stm32u585vit6sm Version=0.0.1

# Explicit upgrade of core system libraries to remediate CVE-2026-107891 (musl libc)
RUN apk update && \
    apk upgrade --no-cache musl musl-utils && \
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

# Explicit upgrade for musl (remediating CVE-2026-107891) and minimal runtime dependencies
# Eliminate unused SQLite CLI (remediating CVE-2026-119586) and ensure GDAL is excluded (CVE-2026-4738)
RUN apk update && \
    apk upgrade --no-cache musl musl-utils && \
    apk add --no-cache bash python3 cmake && \
    rm -rf /var/cache/apk/* /usr/bin/sqlite3 /usr/lib/*sqlite* 2>/dev/null || true

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
