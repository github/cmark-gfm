FROM debian:bookworm

RUN apt-get update && apt-get upgrade -y && apt-get install -y \
  autoconf \
  build-essential \
  clang \
  clang-format \
  gdb \
  git \
  libssl-dev \
  libtool \
  man \
  pkg-config \
  python3 \
  valgrind \
  wget \
  && apt-get clean

RUN wget http://lcamtuf.coredump.cx/afl/releases/afl-latest.tgz && \
	tar xf afl-latest.tgz && \
	cd afl-* && \
	make install && \
	cd .. && \
	rm -rf afl-*

RUN wget https://github.com/skvadrik/re2c/releases/download/0.15.3/re2c-0.15.3.tar.gz && \
	tar xf re2c-0.15.3.tar.gz && \
	cd re2c-* && \
	./configure && \
	make install && \
	cd .. && \
	rm -rf re2c-*

RUN wget https://cmake.org/files/v3.27/cmake-3.27.0.tar.gz && \
	tar xf cmake-3.27.0.tar.gz && \
	cd cmake-* && \
	./bootstrap && \
	make install && \
	cd .. && \
	rm -rf cmake-*

RUN apt-get update && apt-get install -y ninja-build
