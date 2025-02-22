#!/bin/sh

echo "\n\033[34mInitializing west...\033[0m"

if [ ! -d ".venv" ]; then
  python3 -m venv .venv
  # Manually:
  # source .venv/bin/activate
fi

if [ -z "$VIRTUAL_ENV" ]; then
  echo "\033[33mManually activate the python env 'source .venv/bin/activate'\033[0m"
  exit 1
fi

if ! command -v west >/dev/null 2>&1; then
  pip3 install --no-cache-dir west
fi

if [ ! -d "../.west" ] || [ ! -f "../.west/config" ]; then
    west init --local
    west update
    west zephyr-export
    pip install -r ../zephyr/scripts/requirements.txt
else
    echo "\033[33mZephyr already initialized.\033[0m"
fi

echo "\n\033[32mSuccess!\033[0m\n"
