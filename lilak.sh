#!/bin/bash
set +x

# Find lilak path
if [ -n "$ZSH_VERSION" ]; then
   LILAK_PATH="$( cd "$( dirname "${(%):-%x}" )" && pwd )"
elif [ -n "$tcsh" ]; then
   LILAK_PATH="$( cd "$( dirname "$0" )" && pwd )"
elif [ -n "$BASH_VERSION" ]; then
   LILAK_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
else
   echo "neither bash or zsh is used, abort"
   exit 1
fi


# Check if the script is being sourced or executed
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    # Script is executed
    if [[ $# -eq 1 ]]; then
        "${LILAK_PATH}/macros/lilak_configuration.py" "$1"
    else
        "${LILAK_PATH}/macros/lilak_configuration.py"
    fi
    source "${LILAK_PATH}/macros/command_lilak.sh"
else
    # Script is sourced
    source "${LILAK_PATH}/macros/command_lilak.sh"
fi
