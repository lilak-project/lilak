// Checks ?! (missing-or-empty) conditions next to the existing ? and @- forms.
// Run: root -l -b -q macros/test_parameter_condition.C
void test_parameter_condition()
{
    LKParameterContainer par;
    par.AddLine("a      1");
    par.AddLine("empty");

    par.AddLine("?!missing/p1   v");  // missing   -> added
    par.AddLine("?!a/p2         v");  // set       -> skipped
    par.AddLine("?!empty/p3     v");  // empty     -> added
    par.AddLine("?!{a}/p4       v");  // set       -> skipped
    par.AddLine("?!{missing}/p5 v");  // missing   -> added
    par.AddLine("?a==1/p6       v");  // existing ? still works
    par.AddLine("@-missing/p7   v");  // @- still works, same meaning as ?!
    par.AddLine("?!/p8          v");  // no name   -> skipped
    par.AddLine("g/?!missing/p9 v");  // ?! inside a path -> stored as g/p9
    par.AddLine("dup 1");
    par.AddLine("dup 2");             // overwrites; FindPar must skip the legacy "dup 1"

    bool ok = true;
    auto expect = [&](const char *name, bool exists) {
        bool found = par.CheckPar(name);
        if (found != exists) {
            cout << "FAIL " << name << ": expected " << (exists ? "present" : "absent") << endl;
            ok = false;
        }
    };
    expect("p1", true);
    expect("p2", false);
    expect("p3", true);
    expect("p4", false);
    expect("p5", true);
    expect("p6", true);
    expect("p7", true);
    expect("p8", false);
    expect("g/p9", true);
    expect("missing/p1", false); // ?! does not keep the condition in the name
    if (par.GetParInt("dup") != 2) { cout << "FAIL dup: expected 2" << endl; ok = false; }

    cout << (ok ? "test_parameter_condition: OK" : "test_parameter_condition: FAILED") << endl;
    if (!ok) gSystem -> Exit(1);
}
