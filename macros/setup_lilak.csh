# Determine LILAK_PATH based on how the script is run
if ($?tcsh) then
    set script_path = `dirname $0`
    set LILAK_PATH = `realpath $script_path`
else
    set script_path = `dirname $0`
    set LILAK_PATH = `realpath $script_path`
endif

# Check if script is sourced or executed
if ($0 == "$shell") then
    # Script is executed
    if ($#argv == 1) then
        ${LILAK_PATH}/macros/lilak_configuration.py $1
    else
        ${LILAK_PATH}/macros/lilak_configuration.py
    endif
    source ${LILAK_PATH}/macros/command_lilak.csh
else
    # Script is sourced
    source ${LILAK_PATH}/macros/command_lilak.csh
endif
