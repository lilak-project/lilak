void read_parameters()
{
    auto parc = new LKParameterContainer("example.mac");

    TString out = parc -> GetParString("LKRun/OutputPath");
    e_info << "LKRun/OutputPath = " << out << endl;

    int nhits = parc -> GetParInt("LKHTTrackingTask/num_hits_cut");
    e_info << "LKHTTrackingTask/num_hits_cut = " << nhits << endl;

    TVector3 center = parc -> GetParV3("LKHTTrackingTask/transform_ct");
    e_info << "LKHTTrackingTask/transform_ct =" << endl;
    center.Print();

    bool draw = parc -> GetParBool("LKEveTask/drawPlane");
    e_info << "LKEveTask/drawPlane = " << draw << endl; 

    bool exist = parc -> CheckPar("NON_EXISTING_PARAMETER");
    e_info << "check NON_EXISTING_PARAMETER: " << exist << endl;

    double update_x2 = -99;
    parc -> UpdatePar(update_x2,"LKHTTrackingTask/binning_x", 2);
    e_info << "UpdatePar update x2(-99) to " << update_x2 << endl;

    double update_x = -99;
    parc -> UpdatePar(update_x,"NON_EXISTING_PARAMETER");
    e_info << "UpdatePar do not update value and do not create error if parameter do not exist: update_x = " << update_x << endl;

    e_warning << "GetPar* create error and exit root if parameter do not exist!" << endl;
    parc -> GetParDouble("NON_EXISTING_PARAMETER");
}
