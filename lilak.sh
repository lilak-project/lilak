#!/bin/bash
set +x

# Get the directory of the currently executing script (whether sourced or executed)
LILAK_PATH2=$(realpath "$(dirname "${BASH_SOURCE[0]}")")

# Check if the script is being sourced or executed
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    # Script is executed
    if [[ $# -eq 1 ]]; then
        "${LILAK_PATH2}/macros/lilak_configuration.py" "$1"
    else
        "${LILAK_PATH2}/macros/lilak_configuration.py"
    fi
    source "${LILAK_PATH2}/macros/command_lilak.sh"
else
    # Script is sourced
    source "${LILAK_PATH2}/macros/command_lilak.sh"
fi
