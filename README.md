# High-Performance Order Matching Engine

This project is a high-performance financial order matching engine built in C++. It is capable of processing over **500,000 orders per second** and features a live-replay dashboard to visualize the simulation.

The engine is built from scratch and demonstrates several advanced software engineering concepts, including multithreading, concurrent data structures, and decoupled (producer-consumer) system design.

---

## 🚀 Features

### C++ Backend
* **High Throughput:** Processes 1,000,000 orders in ~2 seconds, achieving a throughput of **~500,000 orders/sec**.
* **Multithreaded:** Uses OpenMP to generate a high-volume load and `std::thread` for a dedicated, asynchronous logging thread.
* **Thread-Safe:** Uses `std::mutex` and `std::lock_guard` to protect the order books, allowing for safe concurrent `add_order` calls.
* **Decoupled Logging:** Implements a producer-consumer pattern. The "hot" matching threads produce `Trade` objects and push them onto a `ThreadSafeQueue`. A "cold" logger thread consumes from this queue and handles the slow disk I/O, ensuring the matching logic is never blocked.
* **Order Types:** Supports `LIMIT`, `MARKET`, and `IOC` (Immediate-Or-Cancel) orders.
* **Modern C++:** Written in C++17, using `<mutex>`, `<thread>`, `<condition_variable>`, and `<filesystem>`.
* **CMake Build:** Includes a `CMakeLists.txt` for easy, cross-platform compilation.

### Python & Web Dashboard
* **Live Simulation Replay:** The dashboard fetches all 1,000,000 trades at once and then "replays" the ~2-second simulation over ~30 seconds for a realistic, engaging visualization.
* **Real-Time Vibe:** Features a "Last Price" ticker that flashes green/red, a scrolling "Live Trade Feed," and a line chart that draws itself in real-time.
* **Client-Server Architecture:** A Python Flask server acts as a simple API backend to serve the trade data.
* **Modern Frontend:** A clean, responsive dashboard built with HTML5, CSS Grid/Flexbox, and vanilla JavaScript.
* **Data Visualization:** Uses **Chart.js** to render the live price chart.

---

## 🛠️ How to Build and Run (WSL / Linux)

These instructions assume you are in a WSL (Ubuntu) or Linux environment.

### 1. Install Dependencies

First, you need to install the C++ build tools (`g++`, `cmake`, `make`) and the Python environment.

```bash
# Update package list
sudo apt update

# Install C++ build tools
sudo apt install build-essential cmake

# Install Python and the 'venv' module
sudo apt install python3 python3-pip python3.12-venv