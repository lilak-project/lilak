// cloned parameter container after run initialization will not copy temporary parameter
void clone_parameters()
{
    auto run = new LKRun();
    run -> AddPar("example.mac");
    run -> Add(new LKHTTrackingTask);
    run -> Add(new LKPulseShapeAnalysisTask);
    run -> Add(new LKEveTask);
    run -> Init();
    auto parc = run -> GetParameterContainer();
    auto parc_new = parc -> CloneParameterContainer("example_clone");
    parc_new -> Print("example_clone.mac");
}
