# LILAK
Low- and Intermediate-energy nucLear experiment Analysis toolKit

---

### Download
Clone LILAK git repository from [LILAK Github](https://github.com/lilak-project/lilak)
```sh
git clone git@github.com:lilak-project/lilak.git
```

Move into the LILAK main directory and xlone project repositories from [LILAK Projects](https://github.com/lilak-project)
```sh
cd lilak/
git clone git@github.com:lilak-project/atomx.git
```

---

### Build
Run `lilak.sh` and follow the instructions.
```sh
./lilak.sh
```

### Deactivate
Remove or comment out (#) Rint.Logon line as below in ~/.rootrc:
```
#Rint.Logon: /path/to/lilak/macros/rootlogon.C
```

---

### Links

* [LILAK main repository](https://github.com/lilak-project/lilak)
* [LILAK repositories](https://github.com/orgs/lilak-project/repositories)
* [LILAK doxygen](https://lilak-project.github.io/lilak_doxygen/)
