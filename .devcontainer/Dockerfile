FROM alpine:latest

RUN apk update && \
    apk add --no-cache g++ gdb cmake make git && \
    apk add --no-cache --virtual .build-deps wget ca-certificates && \
    wget -O /etc/apk/keys/sgerrand.rsa.pub https://alpine-pkgs.sgerrand.com/sgerrand.rsa.pub && \
    wget https://github.com/sgerrand/alpine-pkg-llvm/releases/download/17.0.1-r0/llvm17-17.0.1-r0.apk && \
    apk add --no-cache llvm17-17.0.1-r0.apk && \
    apk del .build-deps

RUN apk update \
     && apk add --no-cache g++ gdb clang clang-extra-tools cmake make git
