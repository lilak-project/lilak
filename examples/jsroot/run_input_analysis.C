#include "TCanvas.h"
#include "TError.h"
#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TRandom3.h"
#include "TString.h"
#include "TSystem.h"

#include <cstdio>

// Example analysis macro used by `lilak js -R run_input_analysis.C`.
// Replace the body of this function with the real analysis while keeping the
// two arguments and the Bool_t return value.
Bool_t run_input_analysis(
        Int_t inputNumber,
        const char* outputFileName = "")
{
    if (inputNumber < 0) {
        Error("run_input_analysis", "input number must be non-negative");
        return kFALSE;
    }

    const char* configuredDirectory = gSystem->Getenv("LILAK_JS_DIRECTORY");
    const TString defaultOutput = configuredDirectory == nullptr
            ? Form("result_root_%06d.root", inputNumber)
            : Form("%s/result_root_%06d.root",
                    configuredDirectory, inputNumber);
    const char* actualOutput = outputFileName == nullptr
                            || outputFileName[0] == '\0'
                            ? defaultOutput.Data() : outputFileName;

    Printf("LILAK_JS_STATUS ROOT analysis is opening input %d", inputNumber);
    TFile outputFile(actualOutput, "RECREATE");
    if (outputFile.IsZombie()) {
        Error("run_input_analysis", "cannot create %s", actualOutput);
        return kFALSE;
    }

    TH1D energy(
            "energy",
            Form("Input %d energy;Energy;Counts", inputNumber),
            200, 0, 4000);
    TH2D energyVsChannel(
            "energy_vs_channel",
            Form("Input %d energy vs. channel;Channel;Energy", inputNumber),
            32, 0, 32,
            200, 0, 4000);
    energy.SetDirectory(nullptr);
    energyVsChannel.SetDirectory(nullptr);

    // Make each input number produce a reproducible, visibly different result.
    TRandom3 random(static_cast<UInt_t>(inputNumber + 1));
    const Double_t peak = 900.0 + 40.0 * (inputNumber % 40);
    for (Int_t event = 0; event < 50000; ++event) {
        const Int_t channel = random.Integer(32);
        const Double_t value = random.Gaus(peak + 8.0 * channel, 120.0);
        energy.Fill(value);
        energyVsChannel.Fill(channel, value);
        if ((event + 1) % 5000 == 0) {
            Printf("LILAK_JS_PROGRESS %.1f processed %d / 50000 events",
                    100.0 * (event + 1) / 50000.0, event + 1);
            std::fflush(stdout);
        }
    }

    TCanvas canvas(
            "summary",
            Form("Analysis summary for input %d", inputNumber),
            1200, 600);
    canvas.Divide(2, 1);
    canvas.cd(1);
    energy.Draw();
    canvas.cd(2);
    energyVsChannel.Draw("colz");

    outputFile.cd();
    energy.Write();
    energyVsChannel.Write();
    canvas.Write();
    outputFile.Write();
    outputFile.Close();

    Info("run_input_analysis", "created %s", actualOutput);
    return kTRUE;
}
