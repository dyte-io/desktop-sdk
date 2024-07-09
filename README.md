# desktop-core

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
