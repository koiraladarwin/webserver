# HTTP Server With No Dependency

A **high-performance, memory-efficient HTTP server** written in C.  
Designed for **speed**, **low memory usage**, and compatibility with **older hardware**.  

This server is primarily targeted at **GNU/Linux systems**. Support for Windows or macOS is **not guaranteed**.


## Features

--request handling using `epoll` and minimal threading
- Memory-efficient design suitable for old or low-RAM hardware
- Lightweight, no external dependencies
- Simple, clean C codebase for learning, modification, and customization
- Handles high concurrency and pipelined requests efficiently


## Folder Structure

```
build/      # Compiled binaries and object files
includes/   # Header files
src/        # Source files
Makefile    # Build script
```


## Build & Run

### Build the server
```bash
make        # Compile the server
```

### Run the server
```bash
sudo ./build/server
```

You can also clean and rebuild in one step:
```bash
make run
```


