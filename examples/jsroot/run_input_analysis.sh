#!/bin/bash

# Bash backend example used by `lilak js -S run_input_analysis.sh`.
# Bash orchestrates the job; PyROOT performs the ROOT-file serialization.

set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "usage: $0 INPUT_NUMBER [OUTPUT_FILE]" >&2
    exit 2
fi

input_number=$1

if [[ ! $input_number =~ ^[0-9]{1,6}$ ]]; then
    echo "input number must contain 1 to 6 digits" >&2
    exit 2
fi
output_directory=${LILAK_JS_DIRECTORY:-.}
output_file=${2:-"$output_directory/result_bash_$(printf '%06d' "$input_number").root"}

script_directory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
python_executable=${LILAK_JSROOT_PYTHON:-python3}

echo "LILAK_JS_STATUS Bash analysis is starting input $input_number"
LILAK_ANALYSIS_LABEL=Bash \
    "$python_executable" \
    "$script_directory/run_input_analysis.py" \
    "$input_number" \
    "$output_file"
echo "LILAK_JS_STATUS Bash analysis finished input $input_number"
