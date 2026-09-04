#include "TCanvas.h"
#include "TClass.h"
#include "TDirectory.h"
#include "TEnv.h"
#include "TError.h"
#include "TFile.h"
#include "TFolder.h"
#include "TH1.h"
#include "THttpServer.h"
#include "TKey.h"
#include "TList.h"
#include "TNamed.h"
#include "TObject.h"
#include "TROOT.h"
#include "TRootSniffer.h"
#include "TString.h"
#include "TSystem.h"
#include "TSystemDirectory.h"
#include "TSystemFile.h"
#include "TTimer.h"

#include <algorithm>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <map>
#include <spawn.h>
#include <set>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern char** environ;

// Generic JSROOT server used by `lilak js`.
// Configured runners receive one decimal run-number argument. Their stdout and
// stderr are shown in Control/Log. The following optional line protocol drives
// the live monitor:
//
//   LILAK_JS_STATUS any status message
//   LILAK_JS_PROGRESS 42.5 any progress message

namespace {
struct LILAKJSFileStamp {
    Long_t inode;
    Long64_t size;
    Long_t modificationTime;
};

struct LILAKJSObjectToRegister {
    TString folder;
    TObject* object;
};

THttpServer* gLILAKJSServer = nullptr;
TTimer* gLILAKJSDirectoryTimer = nullptr;
TTimer* gLILAKJSProcessTimer = nullptr;
TNamed* gLILAKJSStatus = nullptr;
TString gLILAKJSAddress;
std::vector<TString> gLILAKJSShellScripts;
TString gLILAKJSPythonScript;
TString gLILAKJSRootMacro;
TString gLILAKJSRootFunction;
TString gLILAKJSDirectory;
Bool_t gLILAKJSBusy = kFALSE;
pid_t gLILAKJSProcessId = -1;
TString gLILAKJSProcessLogPath;
Long64_t gLILAKJSProcessLogOffset = 0;
std::string gLILAKJSPartialLogLine;
std::vector<std::string> gLILAKJSLogLines;
TString gLILAKJSProcessLabel;
Int_t gLILAKJSProcessRunNumber = -1;
Double_t gLILAKJSProgress = 0;
std::map<std::string, std::vector<TObject*> > gLILAKJSFileObjects;
std::map<std::string, LILAKJSFileStamp> gLILAKJSObservedFiles;
std::map<std::string, LILAKJSFileStamp> gLILAKJSAttemptedFiles;
std::map<std::string, LILAKJSFileStamp> gLILAKJSLoadedFiles;
}

void scan_lilak_js_directory(Bool_t force);

namespace {
TString lilak_js_absolute_path(const char* path)
{
    TString result = path == nullptr ? "" : path;
    if (result.IsNull())
        return result;
    gSystem->ExpandPathName(result);
    if (!gSystem->IsAbsoluteFileName(result))
        result = TString(gSystem->WorkingDirectory()) + "/" + result;
    return result;
}

std::vector<TString> lilak_js_absolute_paths(const char* paths)
{
    std::vector<TString> results;
    if (paths == nullptr)
        return results;

    TString remaining = paths;
    while (!remaining.IsNull()) {
        const Ssiz_t separator = remaining.First('\n');
        TString path = separator == kNPOS
                ? remaining : remaining(0, separator);
        path = path.Strip(TString::kBoth);
        if (!path.IsNull())
            results.push_back(lilak_js_absolute_path(path));
        if (separator == kNPOS)
            break;
        remaining.Remove(0, separator + 1);
    }
    return results;
}

Bool_t lilak_js_same_stamp(
        const LILAKJSFileStamp& left,
        const LILAKJSFileStamp& right)
{
    return left.inode == right.inode
        && left.size == right.size
        && left.modificationTime == right.modificationTime;
}

void lilak_js_set_status(const char* status)
{
    if (gLILAKJSStatus != nullptr)
        gLILAKJSStatus->SetTitle(status);
    Info("lilak_js", "%s", status);
}

void lilak_js_update_monitor_items()
{
    if (gLILAKJSServer == nullptr)
        return;

    TString progress = Form("%.1f%%", gLILAKJSProgress);
    if (!gLILAKJSProcessLabel.IsNull() && gLILAKJSProcessRunNumber >= 0)
        progress += Form("  %s run %d",
                gLILAKJSProcessLabel.Data(), gLILAKJSProcessRunNumber);
    gLILAKJSServer->SetItemField(
            "/Control/Progress", "value", progress);

    TString logText;
    for (const auto& line : gLILAKJSLogLines) {
        if (!logText.IsNull())
            logText += "\n";
        logText += line;
    }
    if (logText.IsNull())
        logText = "No output yet";
    gLILAKJSServer->SetItemField("/Control/Log", "value", logText);
}

void lilak_js_append_log_line(const std::string& line)
{
    const std::string progressPrefix = "LILAK_JS_PROGRESS ";
    const std::string statusPrefix = "LILAK_JS_STATUS ";

    if (line.compare(0, progressPrefix.size(), progressPrefix) == 0) {
        const std::string payload = line.substr(progressPrefix.size());
        char* end = nullptr;
        const Double_t progress = std::strtod(payload.c_str(), &end);
        if (end != payload.c_str()) {
            gLILAKJSProgress = progress < 0 ? 0
                             : progress > 100 ? 100 : progress;
            while (*end == ' ' || *end == '\t')
                ++end;
            if (*end != '\0')
                lilak_js_set_status(end);
            return;
        }
    }
    else if (line.compare(0, statusPrefix.size(), statusPrefix) == 0) {
        lilak_js_set_status(line.substr(statusPrefix.size()).c_str());
        return;
    }

    gLILAKJSLogLines.push_back(line);
    const std::size_t maximumLines = 100;
    if (gLILAKJSLogLines.size() > maximumLines)
        gLILAKJSLogLines.erase(
                gLILAKJSLogLines.begin(),
                gLILAKJSLogLines.begin()
                    + (gLILAKJSLogLines.size() - maximumLines));
}

void lilak_js_read_process_log()
{
    if (gLILAKJSProcessLogPath.IsNull())
        return;

    std::ifstream input(gLILAKJSProcessLogPath.Data(), std::ios::binary);
    if (!input)
        return;
    input.seekg(0, std::ios::end);
    const std::streamoff end = input.tellg();
    if (end <= gLILAKJSProcessLogOffset)
        return;

    const std::streamsize count = end - gLILAKJSProcessLogOffset;
    std::string chunk(static_cast<std::size_t>(count), '\0');
    input.seekg(gLILAKJSProcessLogOffset, std::ios::beg);
    input.read(&chunk[0], count);
    chunk.resize(static_cast<std::size_t>(input.gcount()));
    gLILAKJSProcessLogOffset += chunk.size();
    gLILAKJSPartialLogLine += chunk;

    std::size_t newline = 0;
    while ((newline = gLILAKJSPartialLogLine.find('\n'))
            != std::string::npos) {
        std::string line = gLILAKJSPartialLogLine.substr(0, newline);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lilak_js_append_log_line(line);
        gLILAKJSPartialLogLine.erase(0, newline + 1);
    }
    lilak_js_update_monitor_items();
}

Bool_t lilak_js_start_process(
        const std::vector<std::string>& arguments,
        const char* label,
        Int_t runNumber)
{
    if (arguments.empty() || gLILAKJSBusy)
        return kFALSE;

    std::string logTemplate = TString::Format(
            "%s/lilak_js_XXXXXX", gSystem->TempDirectory()).Data();
    const Int_t logDescriptor = mkstemp(&logTemplate[0]);
    if (logDescriptor < 0) {
        Error("lilak_js_start_process", "cannot create temporary log");
        return kFALSE;
    }

    std::vector<char*> argv;
    for (const auto& argument : arguments)
        argv.push_back(const_cast<char*>(argument.c_str()));
    argv.push_back(nullptr);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(
            &actions, logDescriptor, STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(
            &actions, logDescriptor, STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, logDescriptor);

    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attributes, 0);

    pid_t processId = -1;
    const Int_t spawnError = posix_spawnp(
            &processId, argv[0], &actions, &attributes,
            argv.data(), environ);
    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&actions);
    close(logDescriptor);
    if (spawnError != 0) {
        gSystem->Unlink(logTemplate.c_str());
        Error("lilak_js_start_process", "cannot start %s (error %d)",
                arguments[0].c_str(), spawnError);
        return kFALSE;
    }

    gLILAKJSBusy = kTRUE;
    gLILAKJSProcessId = processId;
    gLILAKJSProcessLogPath = logTemplate;
    gLILAKJSProcessLogOffset = 0;
    gLILAKJSPartialLogLine.clear();
    gLILAKJSLogLines.clear();
    gLILAKJSProcessLabel = label;
    gLILAKJSProcessRunNumber = runNumber;
    gLILAKJSProgress = 0;
    lilak_js_set_status(Form("%s run %d started",
            label, runNumber));
    lilak_js_update_monitor_items();

    if (gLILAKJSProcessTimer == nullptr)
        gLILAKJSProcessTimer = new TTimer(
                "poll_lilak_js_process()", 200, kFALSE);
    gLILAKJSProcessTimer->TurnOn();
    return kTRUE;
}

TString lilak_js_sanitize_folder_name(const TString& name)
{
    TString sanitized = name;
    for (Int_t index = 0; index < sanitized.Length(); ++index) {
        const unsigned char character = sanitized[index];
        if (!std::isalnum(character) && character != '_' && character != '-')
            sanitized[index] = '_';
    }
    if (sanitized.IsNull())
        sanitized = "directory";
    return sanitized;
}

TString lilak_js_file_folder(const TString& fileName)
{
    TString stem = fileName;
    if (stem.EndsWith(".root"))
        stem.Remove(stem.Length() - 5);
    return "/Directory/" + lilak_js_sanitize_folder_name(stem);
}

void lilak_js_collect_directory_objects(
        TDirectory* directory,
        const TString& webFolder,
        std::vector<LILAKJSObjectToRegister>& objects)
{
    if (directory == nullptr)
        return;

    // Select the newest cycle for every object in this ROOT directory.
    std::map<std::string, TKey*> newestKeys;
    TIter nextKey(directory->GetListOfKeys());
    while (auto key = dynamic_cast<TKey*>(nextKey())) {
        const std::string name = key->GetName();
        const auto found = newestKeys.find(name);
        if (found == newestKeys.end()
                || key->GetCycle() > found->second->GetCycle())
            newestKeys[name] = key;
    }

    for (const auto& entry : newestKeys) {
        TKey* key = entry.second;
        TClass* objectClass = TClass::GetClass(key->GetClassName());
        if (objectClass == nullptr)
            continue;

        if (objectClass->InheritsFrom(TDirectory::Class())) {
            TDirectory* subdirectory = directory->GetDirectory(key->GetName());
            if (subdirectory == nullptr)
                continue;
            const TString subfolder = webFolder + "/"
                    + lilak_js_sanitize_folder_name(key->GetName());
            lilak_js_collect_directory_objects(
                    subdirectory, subfolder, objects);
            continue;
        }

        if (!objectClass->InheritsFrom(TH1::Class())
                && !objectClass->InheritsFrom(TCanvas::Class()))
            continue;

        TObject* object = key->ReadObj();
        if (object == nullptr)
            continue;
        if (auto histogram = dynamic_cast<TH1*>(object))
            histogram->SetDirectory(nullptr);
        objects.push_back({webFolder, object});
    }
}

void lilak_js_remove_file_objects(const std::string& path)
{
    auto found = gLILAKJSFileObjects.find(path);
    if (found == gLILAKJSFileObjects.end())
        return;

    for (auto object : found->second) {
        if (gLILAKJSServer != nullptr)
            gLILAKJSServer->Unregister(object);
        delete object;
    }
    gLILAKJSFileObjects.erase(found);
}

void lilak_js_sort_folder(TFolder* folder, Bool_t descending = kFALSE)
{
    if (folder == nullptr)
        return;

    TList* contents = dynamic_cast<TList*>(folder->GetListOfFolders());
    if (contents == nullptr)
        return;

    std::vector<TObject*> objects;
    TIter next(contents);
    while (auto object = next()) {
        objects.push_back(object);
        if (auto subfolder = dynamic_cast<TFolder*>(object))
            lilak_js_sort_folder(subfolder);
    }

    std::stable_sort(
            objects.begin(), objects.end(),
            [descending](const TObject* left, const TObject* right) {
                TString leftName = left == nullptr ? "" : left->GetName();
                TString rightName = right == nullptr ? "" : right->GetName();
                const Int_t insensitive = leftName.CompareTo(
                        rightName, TString::kIgnoreCase);
                const Bool_t less = insensitive == 0
                        ? leftName.CompareTo(rightName) < 0
                        : insensitive < 0;
                if (!descending)
                    return less;

                const Bool_t equal = insensitive == 0
                        && leftName.CompareTo(rightName) == 0;
                return !equal && !less;
            });

    for (auto object : objects)
        contents->Remove(object);
    for (auto object : objects)
        contents->Add(object);
}

void lilak_js_sort_directory_hierarchy()
{
    if (gLILAKJSServer == nullptr)
        return;

    TFolder* top = gLILAKJSServer->GetSniffer()->GetTopFolder();
    TFolder* directory = top == nullptr
            ? nullptr : dynamic_cast<TFolder*>(top->FindObject("Directory"));
    lilak_js_sort_folder(directory, kTRUE);
}

Bool_t lilak_js_load_file(
        const TString& fullPath,
        const TString& fileName,
        const LILAKJSFileStamp& stamp)
{
    TDirectory::TContext directoryContext;
    TFile* file = TFile::Open(fullPath, "READ");
    if (file == nullptr || file->IsZombie()) {
        Warning("lilak_js_load_file", "cannot open %s", fullPath.Data());
        delete file;
        return kFALSE;
    }

    const TString folder = lilak_js_file_folder(fileName);
    std::vector<LILAKJSObjectToRegister> objects;
    lilak_js_collect_directory_objects(file, folder, objects);
    file->Close();
    delete file;

    const std::string path = fullPath.Data();
    lilak_js_remove_file_objects(path);

    std::vector<TObject*> registered;
    for (const auto& item : objects) {
        if (gLILAKJSServer->Register(item.folder, item.object))
            registered.push_back(item.object);
        else
            delete item.object;
    }

    gLILAKJSFileObjects[path] = registered;
    gLILAKJSLoadedFiles[path] = stamp;
    Info("lilak_js_load_file", "loaded %s (%zu drawable object(s))",
            fullPath.Data(), registered.size());
    return kTRUE;
}

Bool_t lilak_js_parse_address(
        const TString& address,
        TString& host,
        Int_t& port)
{
    const Ssiz_t separator = address.Last(':');
    if (separator == kNPOS) {
        host = "127.0.0.1";
        port = address.Atoi();
    }
    else {
        host = address(0, separator);
        port = TString(address(separator + 1, address.Length())).Atoi();
    }

    if (host.IsNull())
        host = "127.0.0.1";
    if (port < 1 || port > 65535)
        return kFALSE;

    for (Int_t index = 0; index < host.Length(); ++index) {
        const unsigned char character = host[index];
        if (!std::isalnum(character)
                && character != '.' && character != '-')
            return kFALSE;
    }
    return kTRUE;
}

}

void scan_lilak_js_directory(Bool_t force = kFALSE)
{
    if (gLILAKJSServer == nullptr || gLILAKJSDirectory.IsNull())
        return;

    TSystemDirectory directory("lilak_js_watch", gLILAKJSDirectory);
    TList* entries = directory.GetListOfFiles();
    if (entries == nullptr)
        return;

    std::set<std::string> foundPaths;
    TIter next(entries);
    while (auto systemFile = dynamic_cast<TSystemFile*>(next())) {
        if (systemFile->IsDirectory())
            continue;

        const TString fileName = systemFile->GetName();
        if (!fileName.EndsWith(".root"))
            continue;

        const TString fullPath = gLILAKJSDirectory + "/" + fileName;
        FileStat_t status;
        if (gSystem->GetPathInfo(fullPath, status) != 0)
            continue;

        const std::string path = fullPath.Data();
        foundPaths.insert(path);
        const LILAKJSFileStamp stamp = {
            status.fIno, status.fSize, status.fMtime
        };

        const auto observed = gLILAKJSObservedFiles.find(path);
        const Bool_t stable = observed != gLILAKJSObservedFiles.end()
                           && lilak_js_same_stamp(observed->second, stamp);
        gLILAKJSObservedFiles[path] = stamp;

        const auto attempted = gLILAKJSAttemptedFiles.find(path);
        const Bool_t changed = attempted == gLILAKJSAttemptedFiles.end()
                            || !lilak_js_same_stamp(attempted->second, stamp);
        if (changed && (force || stable)) {
            gLILAKJSAttemptedFiles[path] = stamp;
            lilak_js_load_file(fullPath, fileName, stamp);
        }
    }
    delete entries;

    for (auto it = gLILAKJSLoadedFiles.begin();
            it != gLILAKJSLoadedFiles.end();) {
        if (foundPaths.find(it->first) != foundPaths.end()) {
            ++it;
            continue;
        }
        lilak_js_remove_file_objects(it->first);
        it = gLILAKJSLoadedFiles.erase(it);
    }
    for (auto it = gLILAKJSObservedFiles.begin();
            it != gLILAKJSObservedFiles.end();) {
        if (foundPaths.find(it->first) != foundPaths.end()) {
            ++it;
            continue;
        }
        gLILAKJSAttemptedFiles.erase(it->first);
        it = gLILAKJSObservedFiles.erase(it);
    }

    lilak_js_sort_directory_hierarchy();
}

void poll_lilak_js_process()
{
    if (gLILAKJSProcessId <= 0)
        return;

    lilak_js_read_process_log();

    Int_t processStatus = 0;
    const pid_t result = waitpid(
            gLILAKJSProcessId, &processStatus, WNOHANG);
    if (result == 0)
        return;
    if (result < 0) {
        lilak_js_set_status("cannot query child process status");
        gLILAKJSBusy = kFALSE;
        gLILAKJSProcessId = -1;
        if (gLILAKJSProcessTimer != nullptr)
            gLILAKJSProcessTimer->Stop();
        gSystem->Unlink(gLILAKJSProcessLogPath);
        gLILAKJSProcessLogPath.Clear();
        return;
    }

    // The child has closed the log descriptor, so one final read is complete.
    lilak_js_read_process_log();
    if (!gLILAKJSPartialLogLine.empty()) {
        lilak_js_append_log_line(gLILAKJSPartialLogLine);
        gLILAKJSPartialLogLine.clear();
    }

    const Bool_t succeeded = WIFEXITED(processStatus)
                          && WEXITSTATUS(processStatus) == 0;
    if (succeeded) {
        gLILAKJSProgress = 100;
        scan_lilak_js_directory(kTRUE);
        lilak_js_set_status(Form("%s run %d finished",
                gLILAKJSProcessLabel.Data(),
                gLILAKJSProcessRunNumber));
    }
    else if (WIFEXITED(processStatus)) {
        lilak_js_set_status(Form("%s run %d failed with exit code %d",
                gLILAKJSProcessLabel.Data(),
                gLILAKJSProcessRunNumber,
                WEXITSTATUS(processStatus)));
    }
    else {
        lilak_js_set_status(Form("%s run %d was terminated",
                gLILAKJSProcessLabel.Data(),
                gLILAKJSProcessRunNumber));
    }

    gLILAKJSBusy = kFALSE;
    gLILAKJSProcessId = -1;
    if (gLILAKJSProcessTimer != nullptr)
        gLILAKJSProcessTimer->Stop();
    gSystem->Unlink(gLILAKJSProcessLogPath);
    gLILAKJSProcessLogPath.Clear();
    lilak_js_update_monitor_items();
}

Bool_t run_lilak_js_root(Int_t runNumber)
{
    if (runNumber < 0 || gLILAKJSRootMacro.IsNull())
        return kFALSE;
    TString escapedMacro = gLILAKJSRootMacro;
    escapedMacro.ReplaceAll("\\", "\\\\");
    escapedMacro.ReplaceAll("\"", "\\\"");
    const TString expression = Form(
            "Int_t e=0; gROOT->Macro(\"%s(%d)\",&e); "
            "gSystem->Exit(e==0 ? 0 : 1);",
            escapedMacro.Data(), runNumber);
    const std::vector<std::string> arguments = {
        "root", "-l", "-b", "-q", "-e", expression.Data()
    };
    return lilak_js_start_process(arguments, "ROOT", runNumber);
}

Bool_t run_lilak_js_python(Int_t runNumber)
{
    if (runNumber < 0 || gLILAKJSPythonScript.IsNull())
        return kFALSE;
    const char* configured = gSystem->Getenv("LILAK_JSROOT_PYTHON");
    const std::vector<std::string> arguments = {
        configured == nullptr ? "python3" : configured,
        "-u", gLILAKJSPythonScript.Data(), std::to_string(runNumber)
    };
    return lilak_js_start_process(arguments, "Python", runNumber);
}

Bool_t run_lilak_js_shell(Int_t scriptIndex, Int_t runNumber)
{
    if (runNumber < 0
            || scriptIndex < 0
            || scriptIndex >= static_cast<Int_t>(gLILAKJSShellScripts.size()))
        return kFALSE;
    const TString& script = gLILAKJSShellScripts[scriptIndex];
    const char* configured = gSystem->Getenv("LILAK_JSROOT_BASH");
    const std::vector<std::string> arguments = {
        configured == nullptr ? "/bin/bash" : configured,
        script.Data(), std::to_string(runNumber)
    };
    const TString label = Form("Shell %s", gSystem->BaseName(script));
    return lilak_js_start_process(arguments, label, runNumber);
}

void close_lilak_js()
{
    if (gLILAKJSDirectoryTimer != nullptr) {
        gLILAKJSDirectoryTimer->Stop();
        delete gLILAKJSDirectoryTimer;
        gLILAKJSDirectoryTimer = nullptr;
    }
    if (gLILAKJSProcessTimer != nullptr) {
        gLILAKJSProcessTimer->Stop();
        delete gLILAKJSProcessTimer;
        gLILAKJSProcessTimer = nullptr;
    }
    if (gLILAKJSProcessId > 0) {
        Warning("close_lilak_js", "terminating active analysis process %d",
                static_cast<Int_t>(gLILAKJSProcessId));
        kill(-gLILAKJSProcessId, SIGTERM);
        waitpid(gLILAKJSProcessId, nullptr, 0);
    }
    if (!gLILAKJSProcessLogPath.IsNull())
        gSystem->Unlink(gLILAKJSProcessLogPath);

    while (!gLILAKJSFileObjects.empty())
        lilak_js_remove_file_objects(gLILAKJSFileObjects.begin()->first);

    if (gLILAKJSServer != nullptr) {
        delete gLILAKJSServer;
        gLILAKJSServer = nullptr;
    }

    delete gLILAKJSStatus;
    gLILAKJSStatus = nullptr;
    gLILAKJSFileObjects.clear();
    gLILAKJSObservedFiles.clear();
    gLILAKJSAttemptedFiles.clear();
    gLILAKJSLoadedFiles.clear();
    gLILAKJSAddress.Clear();
    gLILAKJSShellScripts.clear();
    gLILAKJSPythonScript.Clear();
    gLILAKJSRootMacro.Clear();
    gLILAKJSRootFunction.Clear();
    gLILAKJSDirectory.Clear();
    gLILAKJSBusy = kFALSE;
    gLILAKJSProcessId = -1;
    gLILAKJSProcessLogPath.Clear();
    gLILAKJSProcessLogOffset = 0;
    gLILAKJSPartialLogLine.clear();
    gLILAKJSLogLines.clear();
    gLILAKJSProcessLabel.Clear();
    gLILAKJSProcessRunNumber = -1;
    gLILAKJSProgress = 0;
}

void lilak_js()
{
    close_lilak_js();
    gROOT->SetBatch(kTRUE);

    // A watched directory is written to by a running analysis job, so a scan
    // regularly opens a file the writer has not closed yet.  TFile::Recover()
    // then rebuilds the keys of a half-written file, reads a garbage key
    // length and smashes the heap -- an abort that killed this server three
    // times in one session.  Without recovery such a file is only flagged a
    // zombie, which lilak_js_load_file already skips, and the next scan
    // retries it once its stamp settles.
    gEnv->SetValue("TFile.Recover", 0);

    const char* addressEnvironment = gSystem->Getenv("LILAK_JS_ADDRESS");
    const char* shellEnvironment = gSystem->Getenv("LILAK_JS_SHELL");
    const char* pythonEnvironment = gSystem->Getenv("LILAK_JS_PYTHON");
    const char* rootEnvironment = gSystem->Getenv("LILAK_JS_ROOT");
    const char* directoryEnvironment = gSystem->Getenv("LILAK_JS_DIRECTORY");

    gLILAKJSAddress = addressEnvironment == nullptr
                    ? "127.0.0.1:9091" : addressEnvironment;
    gLILAKJSShellScripts = lilak_js_absolute_paths(shellEnvironment);
    gLILAKJSPythonScript = lilak_js_absolute_path(pythonEnvironment);
    gLILAKJSRootMacro = lilak_js_absolute_path(rootEnvironment);
    gLILAKJSDirectory = lilak_js_absolute_path(directoryEnvironment);

    if (gLILAKJSShellScripts.empty()
            && gLILAKJSPythonScript.IsNull()
            && gLILAKJSRootMacro.IsNull()
            && gLILAKJSDirectory.IsNull())
        gLILAKJSDirectory = gSystem->WorkingDirectory();

    TString host;
    Int_t port = 0;
    if (!lilak_js_parse_address(gLILAKJSAddress, host, port)) {
        Error("lilak_js", "invalid address: %s (use HOST:PORT)",
                gLILAKJSAddress.Data());
        close_lilak_js();
        return;
    }

    std::vector<TString> configuredPaths = gLILAKJSShellScripts;
    configuredPaths.push_back(gLILAKJSPythonScript);
    configuredPaths.push_back(gLILAKJSRootMacro);
    for (const auto& path : configuredPaths) {
        if (!path.IsNull() && gSystem->AccessPathName(path)) {
            Error("lilak_js", "script not found: %s", path.Data());
            close_lilak_js();
            return;
        }
    }
    if (!gLILAKJSDirectory.IsNull()) {
        TSystemDirectory directory("lilak_js_check", gLILAKJSDirectory);
        TList* entries = directory.GetListOfFiles();
        if (entries == nullptr) {
            Error("lilak_js", "directory not found: %s",
                    gLILAKJSDirectory.Data());
            close_lilak_js();
            return;
        }
        delete entries;
    }

    if (!gLILAKJSRootMacro.IsNull()) {
        gLILAKJSRootFunction = gSystem->BaseName(gLILAKJSRootMacro);
        const Ssiz_t extension = gLILAKJSRootFunction.Last('.');
        if (extension != kNPOS)
            gLILAKJSRootFunction.Remove(extension);
        for (Int_t index = 0; index < gLILAKJSRootFunction.Length(); ++index) {
            const unsigned char character = gLILAKJSRootFunction[index];
            if ((!std::isalnum(character) && character != '_')
                    || (index == 0 && std::isdigit(character))) {
                Error("lilak_js",
                        "ROOT macro file name must be a C++ identifier: %s",
                        gLILAKJSRootMacro.Data());
                close_lilak_js();
                return;
            }
        }
    }

    TString engine;
    if (host == "127.0.0.1" || host == "localhost")
        engine = Form("http:%d?loopback&top=LILAK;noglobal", port);
    else
        engine = Form("http:%s:%d?top=LILAK;noglobal", host.Data(), port);

    gLILAKJSStatus = new TNamed("Status", "ready");
    gLILAKJSServer = new THttpServer(engine);
    if (!gLILAKJSServer->IsAnyEngine()) {
        Error("lilak_js", "cannot start HTTP server at %s",
                gLILAKJSAddress.Data());
        close_lilak_js();
        return;
    }
    gLILAKJSServer->Register("/Control", gLILAKJSStatus);
    gLILAKJSServer->CreateItem("/Control/Progress", "Analysis progress");
    gLILAKJSServer->SetItemField("/Control/Progress", "_kind", "Text");
    gLILAKJSServer->SetItemField("/Control/Progress", "value", "0.0%");
    gLILAKJSServer->CreateItem("/Control/Log", "Analysis output");
    gLILAKJSServer->SetItemField("/Control/Log", "_kind", "Text");
    gLILAKJSServer->SetItemField("/Control/Log", "value", "No output yet");
    gLILAKJSServer->SetItemField(
            "/Control/Status", "_monitoring", "500");
    gLILAKJSServer->SetItemField(
            "/Control/Progress", "_monitoring", "500");
    gLILAKJSServer->SetItemField(
            "/Control/Log", "_monitoring", "500");

    std::vector<TString> commands;
    if (!gLILAKJSRootMacro.IsNull()) {
        gLILAKJSServer->RegisterCommand(
                "/Control/RunROOT", "run_lilak_js_root(%arg1%)",
                "button;rootsys/icons/ed_execute.png");
        commands.push_back("/Control/RunROOT");
    }
    if (!gLILAKJSPythonScript.IsNull()) {
        gLILAKJSServer->RegisterCommand(
                "/Control/RunPython", "run_lilak_js_python(%arg1%)",
                "button;rootsys/icons/ed_execute.png");
        commands.push_back("/Control/RunPython");
    }
    std::set<std::string> shellCommandNames;
    for (size_t index = 0; index < gLILAKJSShellScripts.size(); ++index) {
        TString scriptName = gSystem->BaseName(gLILAKJSShellScripts[index]);
        const Ssiz_t extension = scriptName.Last('.');
        if (extension != kNPOS)
            scriptName.Remove(extension);
        for (Int_t characterIndex = 0;
                characterIndex < scriptName.Length(); ++characterIndex) {
            const unsigned char character = scriptName[characterIndex];
            if (!std::isalnum(character) && character != '_')
                scriptName[characterIndex] = '_';
        }
        TString command = "/Control/RunShell_" + scriptName;
        if (!shellCommandNames.insert(command.Data()).second)
            command += Form("_%zu", index + 1);
        const TString expression = Form(
                "run_lilak_js_shell(%zu,%%arg1%%)", index);
        gLILAKJSServer->RegisterCommand(
                command, expression,
                "button;rootsys/icons/ed_execute.png");
        commands.push_back(command);
    }
    for (const auto& command : commands) {
        gLILAKJSServer->SetItemField(command, "_numargs", "1");
        gLILAKJSServer->SetItemField(command, "_hreload", "true");
    }

    if (!gLILAKJSDirectory.IsNull()) {
        scan_lilak_js_directory(kTRUE);
        gLILAKJSDirectoryTimer = new TTimer(
                "scan_lilak_js_directory()", 1000, kFALSE);
        gLILAKJSDirectoryTimer->TurnOn();
        Info("lilak_js", "watching %s", gLILAKJSDirectory.Data());
    }

    lilak_js_set_status("ready");
    Info("lilak_js", "open http://%s", gLILAKJSAddress.Data());
    Info("lilak_js", "press JSROOT Reload to refresh changed directory items");
    if (TString(gSystem->Getenv("LILAK_JS_DAEMON")) == "1") {
        while (true) {
            gSystem->ProcessEvents();
            gSystem->Sleep(10);
        }
    }
}
