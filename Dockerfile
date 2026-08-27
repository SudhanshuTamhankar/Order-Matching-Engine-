# Multi-Stage / High-Performance Build for C++ Matching Engine & FastAPI Gateway
FROM python:3.11-slim-bookworm

# Set environment variables
ENV PYTHONUNBUFFERED=1 \
    DEBIAN_FRONTEND=noninteractive \
    PORT=10000

# Install build dependencies: GCC/G++, CMake, OpenMP, Make
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    libomp-dev \
    git \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy all project source code
COPY . /app

# Build the C++ Matching Engine in Release mode with OpenMP & -O2
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j$(nproc)

# Pre-generate initial benchmark logs and snapshots
RUN mkdir -p results && ./build/matching_engine

# Install Python requirements
RUN pip install --no-cache-dir -r requirements.txt

# Expose Render default port
EXPOSE 10000

# Start FastAPI ASGI server with WebSocket support
CMD ["sh", "-c", "uvicorn dashboard.fastapi_server:app --host 0.0.0.0 --port ${PORT:-10000}"]
