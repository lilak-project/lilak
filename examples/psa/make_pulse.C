double pulse_shape(double *x, double *p)
{ double u = x[0] - p[2];
    if (u <= 0)
        return p[0];

    double alpha = p[3];
    double tau = p[4];
    double peakU = alpha * tau;
    return p[0] + p[1] * pow(u / peakU, alpha) * exp(alpha - u / tau);
}

void make_pulse()
{
    TString fileName = TString(gSystem -> Getenv("LILAK_PATH")) + "/common/pulseReference.root";
    if (gSystem -> Getenv("LILAK_PATH") == nullptr)
        fileName = "common/pulseReference.root";

    auto file = new TFile(fileName);
    auto pulse = (TGraphErrors*) file -> Get("pulse");

    auto fit = new TF1("fit_pulse_shape", pulse_shape, -20, 60, 5);
    fit -> SetParNames("baseline", "peak", "t0", "alpha", "tau");
    fit -> SetParameters(0.006, 1.0, -2.0, 3.0, 4.5);
    fit -> SetParLimits(0, -0.1, 0.1);
    fit -> SetParLimits(1, 0.1, 2.0);
    fit -> SetParLimits(2, -20.0, 10.0);
    fit -> SetParLimits(3, 0.2, 20.0);
    fit -> SetParLimits(4, 0.2, 50.0);

    pulse -> Fit(fit, "R");
    pulse -> SetTitle(";time bucket from hit;normalized amplitude");
    pulse -> Draw("ap");
    fit -> Draw("same");

    cout << endl;
    cout << "baseline = " << fit -> GetParameter(0) << endl;
    cout << "peak     = " << fit -> GetParameter(1) << endl;
    cout << "t0       = " << fit -> GetParameter(2) << endl;
    cout << "alpha    = " << fit -> GetParameter(3) << endl;
    cout << "tau      = " << fit -> GetParameter(4) << endl;
    cout << "tPeak    = " << fit -> GetParameter(2) + fit -> GetParameter(3) * fit -> GetParameter(4) << endl;
}
