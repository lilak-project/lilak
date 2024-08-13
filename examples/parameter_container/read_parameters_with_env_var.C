void read_parameters_with_env_var()
{
    cout << "==================================================" << endl;
    cout << "RUN with following commands to see the difference:" << endl;
    cout << endl;
    cout << "TYPE=A root read_parameters_with_env_var.C" << endl;
    cout << "TYPE=B root read_parameters_with_env_var.C" << endl;
    cout << "TYPE=C root read_parameters_with_env_var.C" << endl;
    cout << "TYPE=D root read_parameters_with_env_var.C" << endl;
    cout << endl;
    cout << "==================================================" << endl;
    auto run = new LKRun();
    run -> AddPar("environment_var.mac");
    run -> Init();
    auto parc = run->GetParameterContainer();

    cout << "type is " << parc -> GetParString("type") << endl;
    cout << "type id: " << parc -> GetParString("LKRun/RunIDRange") << endl;
}
