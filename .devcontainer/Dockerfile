# Old tools
#RUN apk update \
#     && apk add --no-cache g++ gdb clang clang-extra-tools cmake make git

# Alpine is a Linux distribution built around musl libc and BusyBox.
# The image is 5 MB in size and has access to a package repository 
# that is much more complete than other BusyBox based images. 
#
# https://alpinelinux.org/
#
FROM alpine:latest

LABEL Description = "A Clang 14 Build Service on Alpine Linux Latest."

ENV PROJECT_DIR /project

WORKDIR ${PROJECT_DIR}

# Install dev tools to build the C++ project.
#
RUN apk update \
  && apk upgrade \
  && apk add --no-cache \
  bash \
    alpine-sdk \
    ccache \
    clang \
    clang-dev \
    cmake \
    dpkg
    # nano
  
# Finalize Setup
#
RUN ln -sf /usr/bin/clang /usr/bin/cc \
  && ln -sf /usr/bin/clang++ /usr/bin/c++ \
  && update-alternatives --install /usr/bin/cc cc /usr/bin/clang 14\
  && update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++ 14\
  && update-alternatives --auto cc \
  && update-alternatives --auto c++ \
  && update-alternatives --display cc \
  && update-alternatives --display c++ \
  && ls -l /usr/bin/cc /usr/bin/c++ \
  && cc --version \
  && c++ --version \
  && clang --version 
