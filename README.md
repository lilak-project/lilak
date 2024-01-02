# LILAK
*        *                      *               *            *
Low- and Intermediate-energy nucLear experiment Analysis toolKit

---

## How to build
Run `cofigure.py` and follow the instructions.
```zsh
./configure.py
```

---

## How to create project
Run `create_project.py` and follow the instructions.
```zsh
./create_project.py
```

---

## How to unlink lilak
Remove or comment out (#) Rint.Logon line as below in ~/.rootrc:
```
#Rint.Logon: /home/ejungwoo/lilak/macros/rootlogon.C
```
or run root with `-n` option when you run root to execute without logon and logoff macros.
```
root -n
```
