# esp32-win98-server

An ESP-IDF web server that leverages Nix for a reproducible development environment, styled with a Windows 98 aesthetic.

## Features

-   **Sysinfo:** Sysinfo page for a quick look at the ESP hardware's statistics.
-   **Guestbook:** Stores guestbook entries on [littlefs](https://github.com/joltwallet/esp_littlefs).
-   **Reproducible Environment:** Powered by Nix Flakes, ensuring everyone on the team has the exact same development setup.
-   **ESP-IDF Integration:** Uses the `esp-idf` framework for ESP32 development.
-   **Web Server:** Includes a basic web server to serve static files.
-   **Custom Build Script:** A simple `espbuild` command to prepare assets and build the project.
-   **Code Formatting:** Integrated with `treefmt-nix` for consistent code style.

## Prerequisites

-   [Nix](https://nixos.org/download.html) with Flakes support enabled.
-   For non-Nix users, follow the [official ESP-IDF Get Started page](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html#introduction) and check the `flake.nix` to see how the `espbuild` script works.

## Getting Started

1.  **Enter the Development Shell:**

    ```bash
    nix develop
    ```

    This command will download all the necessary dependencies and drop you into a shell with the development environment ready.

2.  **Build the Project:**

    Once inside the shell, before running the custom build command:
    run:
    ```bash
    idf.py menuconfig
    ```
    and set `Wifi SSID` and `Wifi Password` from the `Web server config` menu.
    then run:

    ```bash
    espbuild
    ```

    This will:
    -   Copy and optimize web assets from `main/pages` and `main/assets` into `main/dist`.
    -   Run `idf.py build` to compile the project.

## Usage

After building the project, you can flash it to your ESP32 board.

1.  **Flash the Firmware:**

    Connect your ESP32 board and run:

    ```bash
    idf.py -p /dev/ttyUSB0 flash
    ```

    (Replace `/dev/ttyUSB0` with your board's serial port).

2.  **Monitor the Output:**

    To see the logs from the device:

    ```bash
    idf.py monitor
    ```

## Code Formatting

This project uses `treefmt-nix` to format the codebase. To format all files, run:

```bash
nix fmt
```
