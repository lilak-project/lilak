#include "TFile.h"
#include "THttpServer.h"
#include "TKey.h"
#include "TCanvas.h"
#include "TError.h"
#include "TH1.h"

#include <vector>

TFile* gJSROOTFile = nullptr;
THttpServer* gJSROOTServer = nullptr;
std::vector<TObject*> gJSROOTObjects;

void close_jsroot()
{
    if (gJSROOTServer != nullptr) {
        delete gJSROOTServer;
        gJSROOTServer = nullptr;
    }

    for (auto object : gJSROOTObjects)
        delete object;
    gJSROOTObjects.clear();

    if (gJSROOTFile != nullptr) {
        gJSROOTFile->Close();
        delete gJSROOTFile;
        gJSROOTFile = nullptr;
    }
}

void open_jsroot(
        const char* filename = "canvas.root",
        int port = 9091)
{
    close_jsroot();

    gJSROOTFile = TFile::Open(filename);

    if (gJSROOTFile == nullptr || gJSROOTFile->IsZombie()) {
        Error("open_jsroot", "cannot open %s", filename);
        if (gJSROOTFile != nullptr) {
            delete gJSROOTFile;
            gJSROOTFile = nullptr;
        }
        return;
    }

    // THttpServer automatically exposes open files below the top-level
    // "Files" folder. Registering the TFile again as "Files" creates a
    // duplicate Objects/Files hierarchy and can confuse JSROOT item paths.
    gJSROOTServer = new THttpServer(
            Form("http:%d?loopback&top=LILAK", port));

    if (!gJSROOTServer->IsAnyEngine()) {
        Error("open_jsroot", "cannot start HTTP server on port %d", port);
        close_jsroot();
        return;
    }

    // Also read standard drawable keys and register the actual objects.
    // This bypasses TKey/file-range handling and provides a reliable path at
    // Objects/Canvases and Objects/Histograms in the default JSROOT browser.
    int numCanvases = 0;
    int numHistograms = 0;
    TIter nextKey(gJSROOTFile->GetListOfKeys());
    while (auto key = dynamic_cast<TKey*>(nextKey())) {
        auto object = key->ReadObj();
        if (object == nullptr)
            continue;

        if (object->InheritsFrom(TCanvas::Class())) {
            gJSROOTServer->Register("Canvases", object);
            gJSROOTObjects.push_back(object);
            ++numCanvases;
        }
        else if (object->InheritsFrom(TH1::Class())) {
            gJSROOTServer->Register("Histograms", object);
            gJSROOTObjects.push_back(object);
            ++numHistograms;
        }
        else {
            delete object;
        }
    }

    Info("open_jsroot", "registered %d canvas(es) and %d histogram(s)",
            numCanvases, numHistograms);
    Info("open_jsroot", "open http://localhost:%d", port);
}
