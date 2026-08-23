# Building from source

GOL requires a C++20 compiler, CMake, and Git. Dependencies are downloaded
automatically during configuration.

## Linux

Install the build tools and OpenSSL development package. For example, on
Ubuntu/Debian:

    sudo apt install build-essential cmake git libssl-dev

Then build:

    git clone https://github.com/clarisma/geodesk-gol.git
    cd geodesk-gol
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel

The executable is written to the build directory.

## Windows

Install Visual Studio 2022 with the "Desktop development with C++" workload,
as well as Git and CMake.

Then, from a Developer Command Prompt:

    git clone https://github.com/clarisma/geodesk-gol.git
    cd geodesk-gol
    cmake -S . -B build
    cmake --build build --config Release --parallel

The executable is written to the Release directory below the build directory.

### macOS

Install Xcode command-line tools, CMake, Git, and OpenSSL.

For example, using Homebrew:

    xcode-select --install
    brew install cmake openssl@3

Then:

    git clone https://github.com/clarisma/geodesk-gol.git
    cd geodesk-gol
    cmake -S . -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
    cmake --build build --parallel