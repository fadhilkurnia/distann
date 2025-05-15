# syntax=docker/dockerfile-upstream:master-labs
FROM ubuntu:22.04

# install build tools, CMake, python3, pip3, and curl
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      build-essential \
      cmake \
      python3-pip \
      curl \
      libcurl4-gnutls-dev \
    && rm -rf /var/lib/apt/lists/*

# install dependency for embedding server: flask, pytorch, pillow
RUN pip3 install flask transformers torch pillow

# install dependency: drogon
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      git gcc g++ \
      libjsoncpp-dev \
      uuid-dev \
      zlib1g-dev \
      libcurl4-gnutls-dev
WORKDIR /tmp
RUN git clone https://github.com/drogonframework/drogon && \
      cd drogon && \
      git submodule update --init && \
      mkdir build && \
      cd build && \
      cmake -DCMAKE_BUILD_TYPE=Release .. && \
      make && make install

WORKDIR /src

# copy the entire source tree
COPY --exclude=build . .

# out-of-source build
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20 && \
    make -j$(nproc)

WORKDIR /src/build
EXPOSE 1001
CMD [ "./backend", "1001", "backend", "random_one", "100"]