void read()
{
    auto run = new LKRun();
    run -> AddInputFile("/home/ejungwoo/lilak/data/point_creator_0001.master.6.4c94cc5.root");
    run -> SetTag("read");
    run -> Init();
    run -> Print();
    TClonesArray *fPointArray = run -> GetBranchA("PointArray");
    TClonesArray *fGausArray = run -> GetBranchA("GausArray");
    cout << fPointArray -> GetEntries() << endl;
    cout << fGausArray -> GetEntries() << endl;
}
