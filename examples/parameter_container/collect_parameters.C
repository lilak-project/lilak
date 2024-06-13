// InitAndCollectParameters method collect parameters during Init() methods of each task
void collect_parameters()
{
    auto run = new LKRun();
    run -> Add(new LKHTTrackingTask);
    run -> Add(new LKPulseShapeAnalysisTask);
    run -> Add(new LKEveTask);
    run -> InitAndCollectParameters("example.mac");
}
