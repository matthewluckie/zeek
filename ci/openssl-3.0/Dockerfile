FROM ubuntu:20.04

ENV DEBIAN_FRONTEND="noninteractive" TZ="America/Los_Angeles"

RUN apt-get update && apt-get -y install \
    git \
    cmake \
    make \
    gcc \
    g++ \
    flex \
    bison \
    libpcap-dev \
    libssl-dev \
    python3 \
    python3-dev \
    python3-pip\
    swig \
    zlib1g-dev \
    libmaxminddb-dev \
    libkrb5-dev \
    bsdmainutils \
    sqlite3 \
    curl \
    wget \
    unzip \
    ruby \
    bc \
    lcov \
  && rm -rf /var/lib/apt/lists/*

# Note - the symlink is important, otherwise cmake uses the wrong .so files.
RUN wget https://www.openssl.org/source/openssl-3.0.0.tar.gz && tar xvf ./openssl-3.0.0.tar.gz && cd ./openssl-3.0.0 && ./Configure --prefix=/opt/openssl && make install && cd .. && rm -rf openssl-3.0.0 && ln -sf /opt/openssl/lib64 /opt/openssl/lib

RUN pip3 install junit2html
RUN gem install coveralls-lcov
