# LILAK
Low- and Intermediate-energy nucLear experiment Analysis toolKit

## Required packages
 - ROOT
 - Git
 - Geant4 (optional)
 - Doxygen (optional)

## How to build
```zsh
./configure.py
echo export LILAK_PATH=\"/home/ejungwoo/lilak\" >> ~/.zshrc # or to ~/.bashrc
echo Rint.Logon: /home/ejungwoo/lilak/macros/rootlogon.C >> ~/.rootrc
mkdir build
cd build
cmake ..
make
```
You should check if you are not using multiple `rootlogon.C` files in `.rootrc`.
If you are, you have to merge `rootlogon.C` files into one.

## How to unlink LILAK
Remove or below line from ~/.rootrc:
```
Rint.Logon: /home/ejungwoo/lilak/macros/rootlogon.C
```
or run with `-n` option when you run root which do not execute logon and logoff macros as specified in .rootrc:
```
root -n
```
