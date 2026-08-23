void run_reco(TString inputFileName="data/dummry_run0025.root")
{
    auto reco = new LKCompassReco();
    reco -> SetInputFile(inputFileName);
    //reco -> SetOutputFile("data/DataR_run0025_dummy.reco.root");
    reco -> SetTimeWindow(4000000);

    reco -> SetW1MainJunction(); // ystrips
    //reco -> SetW1MainOhmic(); // xstrips

    reco -> AddW1(1,0,0,16);
    reco -> AddW1(2,1,32,48);
    reco -> AddCom(3,2,0);
    reco -> AddCom(4,2,1);
    reco -> AddCom(5,2,2);
    reco -> AddCom(6,2,3);

    //reco -> SetECalParametersW1(1,0,1);
    //reco -> SetECalParametersW1(2,0,1);
    //reco -> SetECalParametersCom(3,0,1);
    //reco -> SetECalParametersCom(4,0,1);
    //reco -> SetECalParametersCom(5,0,1);
    //reco -> SetECalParametersCom(6,0,1);
    //reco -> SetECalParametersW1(1,0,1,0.0001);
    //reco -> SetECalParametersCom(3,0,1,0.0001);

    //reco -> SetHistogramBinningW1(1,"mult",LKBinning(32,0,32));
    //reco -> SetHistogramBinningW1(1,"hit_energy",LKBinning(500,0,15000));
    //reco -> SetHistogramBinningW1(1,"hit_ecal",LKBinning(500,0,15000));
    //reco -> SetHistogramBinningW1(1,"time_diff",LKBinning(1000,0,500));
    //reco -> SetHistogramBinningCom(3,"hit_energy",LKBinning(500,0,12000));
    //reco -> SetHistogramBinningCom(3,"time_diff",LKBinning(1000,0,500));

    reco -> SetChannelBranches();
    reco -> SetHitBranches();

    reco -> Run();
    reco -> Draw("viewer");
}
