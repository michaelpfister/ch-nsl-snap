#!/usr/bin/env bash
# Installs Ubuntu packages on a GitHub runner, for CI only. The runners
# fetch from Azure's Ubuntu mirror (mirror+file:/etc/apt/apt-mirrors.txt);
# when that mirror cannot be resolved, apt fails before anything is built
# (2026-11-11). So: retry each download, and from the second attempt on
# fetch from archive.ubuntu.com instead of the mirror list.
#
# Usage: scripts/ci-apt-install.sh package...
set -euo pipefail

if [ "$#" -eq 0 ]; then
    echo "usage: $0 package..." >&2
    exit 2
fi

attempts=3
apt_options=(-o Acquire::Retries=3 -o Acquire::http::Timeout=30)

for attempt in $(seq 1 "$attempts"); do
    if sudo apt-get "${apt_options[@]}" update \
        && sudo apt-get "${apt_options[@]}" install -y "$@"; then
        exit 0
    fi
    if [ "$attempt" -eq "$attempts" ]; then
        break
    fi
    echo "apt failed (attempt $attempt of $attempts); trying archive.ubuntu.com directly" >&2
    # The mirror list holds one mirror per line; leave only the main archive.
    if [ -f /etc/apt/apt-mirrors.txt ]; then
        echo "http://archive.ubuntu.com/ubuntu/" | sudo tee /etc/apt/apt-mirrors.txt >/dev/null
    fi
    sleep $((attempt * 15))
done

echo "apt failed $attempts times; giving up" >&2
exit 1
