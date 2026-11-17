# Multi-stage Dockerfile: Ultra-lightweight Smart Meter DLMS Simulator (< 10 MB)
# Stage 1: Build & strip simulator binary
FROM alpine:latest AS builder

RUN apk update && \
    apk upgrade --no-cache musl musl-utils && \
    apk add --no-cache build-base cmake ninja

WORKDIR /workspace
COPY Secure /workspace/Secure
COPY NonSecure /workspace/NonSecure
COPY tests /workspace/tests

# Compile tests, verify suite passes, and strip target gurux_simulator
RUN cmake -S tests -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build && \
    ctest --test-dir build --output-on-failure && \
    (strip --strip-all build/host-tools/gurux_simulator 2>/dev/null || strip --strip-all build/gurux_simulator 2>/dev/null || true)

# Stage 2: Minimal hardened micro-container (Only Alpine + simulator binary)
FROM alpine:latest AS runner

LABEL Name=stm32u585vit6sm Version=0.0.1 Description="Ultra-light DLMS Simulator for GXDLMSDirector"

# Enforce musl upgrade for zero CVE, no extra packages (no python, no sqlite, no compiler)
RUN apk update && \
    apk upgrade --no-cache musl musl-utils && \
    rm -rf /var/cache/apk/* /lib/apk/db/* /etc/apk/world

WORKDIR /app

# Copy only the lightweight stripped simulator binary
COPY --from=builder /workspace/build/host-tools/gurux_simulator /app/gurux_simulator

EXPOSE 4059

ENTRYPOINT ["/app/gurux_simulator", "4059"]
