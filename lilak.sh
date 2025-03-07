#!/bin/bash
set +x

if [[ "$0" == "bash" || "$0" == "zsh" ]]; then
    # Script is sourced, get the directory from the source command
    LILAK_PATH2=$(realpath "$(dirname "${BASH_ARGV[0]}")")
else
    # Script is executed, use $0 to get the script directory
    LILAK_PATH2=$(realpath "$(dirname "$0")")
fi

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    # Script is executed
    if [[ $# -eq 1 ]]; then
        ${LILAK_PATH2}/macros/lilak_configuration.py "$1"
    else
        ${LILAK_PATH2}/macros/lilak_configuration.py
    fi
    source ${LILAK_PATH2}/macros/command_lilak.sh
else
    # Script is sourced
    source ${LILAK_PATH2}/macros/command_lilak.sh
fi
