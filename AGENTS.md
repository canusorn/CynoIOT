# Repository Guidelines

## Project Structure & Module Organization
This repository is an Arduino library for CynoIoT.
- Core library code lives at the root: `cynoiot.h` and `cynoiot.cpp`.
- Arduino metadata files are `library.properties` and `keywords.txt`.
- Usage samples are under `examples/` (grouped by domain such as `sensor/`, `kit/`, `event/`, `debug/`).
- Additional technical notes live in `docs/` (for example `docs/AUTOMATION.md`).
- Local tooling includes `bin/arduino-cli.exe`.

## Build, Test, and Development Commands
Use Arduino CLI from the repo root.
- `./bin/arduino-cli.exe version` verifies the bundled CLI.
- `./bin/arduino-cli.exe lib list` confirms required libraries (notably `MQTT`) are installed.
- `./bin/arduino-cli.exe compile --fqbn esp32:esp32:esp32 examples/simple/simple.ino` compiles a baseline example for ESP32.
- `./bin/arduino-cli.exe compile --fqbn esp8266:esp8266:nodemcuv2 examples/simple/simple.ino` smoke-tests ESP8266 compatibility.
Run compile checks for any example you modify.

## Coding Style & Naming Conventions
- Language: Arduino C++.
- Follow existing style: 2-space indentation in library files, braces on same line for functions, and header include guards (`#ifndef cynoiot_h`).
- Keep public API naming consistent with existing methods (camelCase and current legacy names such as `setkeyname`).
- Use `UPPER_SNAKE_CASE` for macros/constants (for example `MAX_PUBLISH_TIME`).
- Prefer small, single-purpose helper methods over large monolithic blocks.

## Testing Guidelines
There is no dedicated unit test framework in this repository today.
- Treat example compilation as the primary regression test.
- Validate both target families when relevant: ESP32 and ESP8266.
- For automation behavior changes, include a minimal reproducible `.ino` under `examples/` or update an existing focused example.

## Commit & Pull Request Guidelines
Recent history favors short imperative commit subjects (for example `Update hydroponic.ino`, `Create pump_control.ino`).
- Keep subject lines concise and action-oriented: `Update <module>` / `Add <feature>` / `Fix <issue>`.
- One logical change per commit.
- PRs should include: purpose, impacted paths, hardware target(s) tested, and exact compile command(s) used.
- Link related issues and include serial logs/screenshots when behavior changes are user-visible.
