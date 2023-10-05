# Use the latest Ubuntu image as the base
FROM ubuntu:latest

# Install wget, cmake, make, and git
RUN apt-get update && apt-get install -y \
    lsb-release \
    wget \
    software-properties-common \
    gnupg \
    cmake \
    make \
    git \
    && rm -rf /var/lib/apt/lists/*

# Download the LLVM installation script
RUN wget https://apt.llvm.org/llvm.sh

# Make the script executable
RUN chmod +x llvm.sh

# Run the script to install LLVM/Clang version 17.0.1
RUN ./llvm.sh 17
