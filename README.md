# desktop-core

Server-side Python SDK for Dyte

## Usage

The API is fairly simple and exposes functions to join a room, listen for callbacks and consume/produce audio. Raw API usage can be found in the internal [transport](src/python/dyte_sdk/transport.py)

An end-to-end example can be found in the [examples](examples/chatbot) directory that makes use of the [pipecat](https://github.com/pipecat-ai/pipecat) framework to create a voice bot

Note that the SDK is currently in an alpha state - the API is not very Pythonic, and crashes might occasionally occur

## Directory Structure

```sh
.
├── CMakeLists.txt # For building the C++ Extension
├── pyproject.toml # Specification for building Python packages
├── README.md
├── src
│   ├── c++ # C++ Extension Source
│   └── python # Python code to expose the extension, also contains Pipecat transport
│       ├── dyte_sdk
│       └── examples
│           └── chatbot
└── third_party # Build dependencies
    ├── libcwebrtc
    ├── libmobilecore
    └── pybind11
```
