FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    clang-format-18 \
    clang-tidy-18 \
    clang-18 \
    cmake \
    ninja-build \
    git \
    && rm -rf /var/lib/apt/lists/*
