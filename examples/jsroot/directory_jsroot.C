#include "TDirectory.h"
#include "TError.h"
#include "TFile.h"
#include "THttpServer.h"
#include "TList.h"
#include "TROOT.h"
#include "TString.h"
#include "TSystem.h"
#include "TSystemDirectory.h"
#include "TSystemFile.h"
#include "TTimer.h"

#include <map>
#include <set>
#include <string>

namespace {
THttpServer* gDirectoryServer = nullptr;
TTimer* gDirectoryTimer = nullptr;
TString gWatchedDirectory;
std::map<std::string, TFile*> gDirectoryFiles;
}

void scan_jsroot_directory()
{
    if (gWatchedDirectory.IsNull())
        return;

    TDirectory::TContext directoryContext;
    TSystemDirectory directory("jsroot_watch", gWatchedDirectory);
    TList* entries = directory.GetListOfFiles();
    if (entries == nullptr)
        return;

    std::set<std::string> foundPaths;
    TIter next(entries);
    while (auto systemFile = dynamic_cast<TSystemFile*>(next())) {
        if (systemFile->IsDirectory())
            continue;

        TString fileName = systemFile->GetName();
        if (!fileName.EndsWith(".root"))
            continue;

        TString fullPath = gWatchedDirectory + "/" + fileName;
        const std::string path = fullPath.Data();
        foundPaths.insert(path);

        if (gDirectoryFiles.find(path) != gDirectoryFiles.end())
            continue;

        TFile* file = TFile::Open(fullPath, "READ");
        if (file == nullptr || file->IsZombie()) {
            delete file;
            continue;
        }

        gDirectoryFiles[path] = file;
        Info("scan_jsroot_directory", "opened %s", fullPath.Data());
    }
    delete entries;

    for (auto it = gDirectoryFiles.begin(); it != gDirectoryFiles.end();) {
        if (foundPaths.find(it->first) != foundPaths.end()) {
            ++it;
            continue;
        }

        Info("scan_jsroot_directory", "removed %s", it->first.c_str());
        it->second->Close();
        delete it->second;
        it = gDirectoryFiles.erase(it);
    }
}

void close_directory_jsroot()
{
    if (gDirectoryTimer != nullptr) {
        gDirectoryTimer->Stop();
        delete gDirectoryTimer;
        gDirectoryTimer = nullptr;
    }

    if (gDirectoryServer != nullptr) {
        delete gDirectoryServer;
        gDirectoryServer = nullptr;
    }

    for (auto& item : gDirectoryFiles) {
        item.second->Close();
        delete item.second;
    }
    gDirectoryFiles.clear();
    gWatchedDirectory.Clear();
}

void directory_jsroot(
        const char* directoryName = ".",
        int port = 9091,
        int scanIntervalMs = 1000)
{
    close_directory_jsroot();

    if (scanIntervalMs < 100)
        scanIntervalMs = 100;

    gWatchedDirectory = directoryName;
    gSystem->ExpandPathName(gWatchedDirectory);
    if (!gSystem->IsAbsoluteFileName(gWatchedDirectory))
        gWatchedDirectory = TString(gSystem->WorkingDirectory())
                          + "/" + gWatchedDirectory;

    TSystemDirectory directory("jsroot_watch_check", gWatchedDirectory);
    TList* entries = directory.GetListOfFiles();
    if (entries == nullptr) {
        Error("directory_jsroot", "cannot open directory %s",
                gWatchedDirectory.Data());
        gWatchedDirectory.Clear();
        return;
    }
    delete entries;

    gROOT->SetBatch(kTRUE);
    scan_jsroot_directory();

    gDirectoryServer = new THttpServer(
            Form("http:%d?loopback&top=LILAK", port));
    if (!gDirectoryServer->IsAnyEngine()) {
        Error("directory_jsroot", "cannot start HTTP server on port %d", port);
        close_directory_jsroot();
        return;
    }

    // Scan independently of the browser. Reload then requests the current
    // global Files hierarchy from THttpServer.
    gDirectoryTimer = new TTimer(
            "scan_jsroot_directory()", scanIntervalMs, kFALSE);
    gDirectoryTimer->TurnOn();

    Info("directory_jsroot", "watching %s", gWatchedDirectory.Data());
    Info("directory_jsroot", "opened %zu ROOT file(s)",
            gDirectoryFiles.size());
    Info("directory_jsroot", "open http://localhost:%d", port);
    Info("directory_jsroot", "press JSROOT Reload to refresh Files");
}
