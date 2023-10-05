# Use the official Ubuntu image as the base
FROM ubuntu:latest

# Install required packages
RUN apt-get update && \
    apt-get install -y wget cmake make git && \
    rm -rf /var/lib/apt/lists/*

# Download and install Clang 17 using the installation script
RUN wget -O - https://apt.llvm.org/llvm.sh | bash -s -- 17

# Verify the installation
RUN clang --version
