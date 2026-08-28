#!/usr/bin/env bash

set -eo pipefail

if [[ $# -ne 1 || ! $1 =~ ^[0-9]+$ ]]; then
    echo "Usage: $0 RUN_NUMBER" >&2
    exit 2
fi

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
lilak_root="$(cd -- "$script_dir/../.." && pwd)"
cd "$script_dir"

source "$lilak_root/lilak.sh"
export RUN="$1"
echo "LILAK_JS_STATUS reconstructing run $RUN"
lilak run run_reco.mac
