#include "TCanvas.h"
#include "TClass.h"
#include "TDirectory.h"
#include "TError.h"
#include "TFile.h"
#include "TH1.h"
#include "THttpCallArg.h"
#include "THttpServer.h"
#include "TInterpreter.h"
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

#include <algorithm>
#include <deque>
#include <map>
#include <string>
#include <vector>

// ProcessLine() evaluates the loaded analysis macro in the global interpreter
// scope, so these two bridge values intentionally have external visibility.
Int_t gWebRunInputNumber = -1;
TString gWebRunTemporaryOutput;

namespace {
enum EJSRootBackend {
    kJSRootROOT = 0,
    kJSRootPython = 1,
    kJSRootBash = 2
};

THttpServer* gWebRunServer = nullptr;
TNamed* gWebRunStatus = nullptr;
TString gWebRunRootMacro;
TString gWebRunPythonMacro;
TString gWebRunBashScript;
TString gWebRunOutputDirectory;
Int_t gWebRunMaximumRegisteredResults = 10;
Bool_t gWebRunBusy = kFALSE;
std::map<std::string, std::vector<TObject*> > gWebRunObjects;
std::deque<std::string> gWebRunResultOrder;
}

namespace {
struct JSRootResultFile {
    std::string path;
    Int_t inputNumber;
    EJSRootBackend backend;
    Long_t modificationTime;
};

const char* jsroot_backend_key(EJSRootBackend backend)
{
    if (backend == kJSRootPython)
        return "python";
    if (backend == kJSRootBash)
        return "bash";
    return "root";
}

const char* jsroot_backend_title(EJSRootBackend backend)
{
    if (backend == kJSRootPython)
        return "Python";
    if (backend == kJSRootBash)
        return "Bash";
    return "ROOT";
}

TString jsroot_absolute_path(const char* path)
{
    TString result = path;
    gSystem->ExpandPathName(result);
    if (!gSystem->IsAbsoluteFileName(result))
        result = TString(gSystem->WorkingDirectory()) + "/" + result;
    return result;
}

std::string jsroot_shell_quote(const char* value)
{
    const std::string input = value == nullptr ? "" : value;
    std::string quoted = "'";
    for (char character : input) {
        if (character == '\'')
            quoted += "'\"'\"'";
        else
            quoted += character;
    }
    quoted += "'";
    return quoted;
}

void jsroot_close_result(const std::string& path)
{
    auto objects = gWebRunObjects.find(path);
    if (objects != gWebRunObjects.end()) {
        for (auto object : objects->second) {
            if (gWebRunServer != nullptr)
                gWebRunServer->Unregister(object);
            delete object;
        }
        gWebRunObjects.erase(objects);
    }

}

Int_t jsroot_register_result_objects(
        TFile* file,
        Int_t inputNumber,
        EJSRootBackend backend,
        const std::string& path)
{
    if (gWebRunServer == nullptr || file == nullptr)
        return 0;

    TDirectory::TContext directoryContext;
    const TString folder = Form("/Results/%s/Input_%06d",
            jsroot_backend_title(backend), inputNumber);
    std::vector<TObject*> objects;

    TIter nextKey(file->GetListOfKeys());
    while (auto key = dynamic_cast<TKey*>(nextKey())) {
        TClass* objectClass = TClass::GetClass(key->GetClassName());
        if (objectClass == nullptr)
            continue;
        if (!objectClass->InheritsFrom(TH1::Class())
                && !objectClass->InheritsFrom(TCanvas::Class()))
            continue;

        TObject* object = key->ReadObj();
        if (object == nullptr)
            continue;

        // Histograms read from a TFile otherwise remain owned by its directory.
        // Detach them because this macro manages their lifetime explicitly.
        if (auto histogram = dynamic_cast<TH1*>(object))
            histogram->SetDirectory(nullptr);

        if (!gWebRunServer->Register(folder, object)) {
            delete object;
            continue;
        }
        objects.push_back(object);
    }

    const Int_t count = static_cast<Int_t>(objects.size());
    gWebRunObjects[path] = objects;
    return count;
}

void jsroot_forget_result(const std::string& path)
{
    for (auto it = gWebRunResultOrder.begin();
            it != gWebRunResultOrder.end();) {
        if (*it == path)
            it = gWebRunResultOrder.erase(it);
        else
            ++it;
    }
}

void jsroot_set_status(const char* text)
{
    if (gWebRunStatus != nullptr)
        gWebRunStatus->SetTitle(text);
    Info("web_run_jsroot", "%s", text);
}

Bool_t jsroot_parse_six_digits(const TString& digits, Int_t& inputNumber)
{
    if (digits.Length() != 6)
        return kFALSE;

    inputNumber = 0;
    for (Int_t index = 0; index < digits.Length(); ++index) {
        const char character = digits[index];
        if (character < '0' || character > '9')
            return kFALSE;
        inputNumber = inputNumber * 10 + character - '0';
    }
    return kTRUE;
}

Bool_t jsroot_parse_result_name(
        const TString& fileName,
        Int_t& inputNumber,
        EJSRootBackend& backend)
{
    const TString prefix = "result_";
    const TString suffix = ".root";
    if (!fileName.BeginsWith(prefix) || !fileName.EndsWith(suffix))
        return kFALSE;

    TString body = fileName(
            prefix.Length(),
            fileName.Length() - prefix.Length() - suffix.Length());

    // Preserve files produced by the older one-backend example.
    backend = kJSRootROOT;
    if (body.BeginsWith("root_")) {
        body.Remove(0, 5);
    }
    else if (body.BeginsWith("python_")) {
        backend = kJSRootPython;
        body.Remove(0, 7);
    }
    else if (body.BeginsWith("bash_")) {
        backend = kJSRootBash;
        body.Remove(0, 5);
    }

    return jsroot_parse_six_digits(body, inputNumber);
}

Int_t jsroot_load_existing_results()
{
    TSystemDirectory directory(
            "jsroot_existing_results", gWebRunOutputDirectory);
    TList* entries = directory.GetListOfFiles();
    if (entries == nullptr)
        return 0;

    std::vector<JSRootResultFile> resultFiles;
    TIter next(entries);
    while (auto systemFile = dynamic_cast<TSystemFile*>(next())) {
        if (systemFile->IsDirectory())
            continue;

        const TString fileName = systemFile->GetName();
        Int_t inputNumber = -1;
        EJSRootBackend backend = kJSRootROOT;
        if (!jsroot_parse_result_name(fileName, inputNumber, backend))
            continue;

        const TString fullPath = gWebRunOutputDirectory + "/" + fileName;
        FileStat_t fileStatus;
        if (gSystem->GetPathInfo(fullPath, fileStatus) != 0)
            continue;

        JSRootResultFile result;
        result.path = fullPath.Data();
        result.inputNumber = inputNumber;
        result.backend = backend;
        result.modificationTime = fileStatus.fMtime;
        resultFiles.push_back(result);
    }
    delete entries;

    std::sort(resultFiles.begin(), resultFiles.end(),
            [](const JSRootResultFile& left,
                    const JSRootResultFile& right) {
                if (left.modificationTime != right.modificationTime)
                    return left.modificationTime < right.modificationTime;
                return left.path < right.path;
            });

    const std::size_t first = resultFiles.size()
            > static_cast<std::size_t>(gWebRunMaximumRegisteredResults)
            ? resultFiles.size() - gWebRunMaximumRegisteredResults : 0;

    Int_t loadedFiles = 0;
    for (std::size_t index = first; index < resultFiles.size(); ++index) {
        const JSRootResultFile& result = resultFiles[index];
        TFile* file = TFile::Open(result.path.c_str(), "READ");
        if (file == nullptr || file->IsZombie()) {
            Warning("jsroot_load_existing_results", "cannot open %s",
                    result.path.c_str());
            delete file;
            continue;
        }

        const Int_t registeredObjects = jsroot_register_result_objects(
                file, result.inputNumber, result.backend, result.path);
        file->Close();
        delete file;

        if (registeredObjects < 1) {
            jsroot_close_result(result.path);
            Warning("jsroot_load_existing_results",
                    "no drawable objects in %s", result.path.c_str());
            continue;
        }

        gWebRunResultOrder.push_back(result.path);
        ++loadedFiles;
        Info("jsroot_load_existing_results",
                "loaded %s input %d from %s (%d drawable objects)",
                jsroot_backend_title(result.backend), result.inputNumber,
                result.path.c_str(), registeredObjects);
    }
    return loadedFiles;
}

Bool_t jsroot_execute_external(
        const char* executable,
        const TString& script,
        const char* backendName)
{
    const std::string command = jsroot_shell_quote(executable)
            + " " + jsroot_shell_quote(script.Data())
            + " " + std::to_string(gWebRunInputNumber)
            + " " + jsroot_shell_quote(gWebRunTemporaryOutput.Data());

    const Int_t exitCode = gSystem->Exec(command.c_str());
    if (exitCode != 0) {
        Error("jsroot_execute_external", "%s exited with code %d",
                backendName, exitCode);
        return kFALSE;
    }
    return kTRUE;
}

Bool_t jsroot_execute_analysis(EJSRootBackend backend)
{
    if (backend == kJSRootROOT) {
        Int_t interpreterError = TInterpreter::kNoError;
        const Longptr_t analysisResult = gROOT->ProcessLine(
                "run_input_analysis(gWebRunInputNumber, "
                "gWebRunTemporaryOutput.Data())",
                &interpreterError);
        return interpreterError == TInterpreter::kNoError
            && analysisResult != 0;
    }

    if (backend == kJSRootPython) {
        const char* configured = gSystem->Getenv("LILAK_JSROOT_PYTHON");
        return jsroot_execute_external(
                configured == nullptr ? "python3" : configured,
                gWebRunPythonMacro, "Python");
    }

    const char* configured = gSystem->Getenv("LILAK_JSROOT_BASH");
    return jsroot_execute_external(
            configured == nullptr ? "/bin/bash" : configured,
            gWebRunBashScript, "Bash");
}
}

// This is the only analysis entry point exposed by the web commands.
Bool_t run_jsroot_input(Int_t inputNumber, Int_t backendValue)
{
    if (backendValue < kJSRootROOT || backendValue > kJSRootBash)
        return kFALSE;
    const EJSRootBackend backend =
            static_cast<EJSRootBackend>(backendValue);

    if (gWebRunServer == nullptr || gWebRunStatus == nullptr)
        return kFALSE;
    if (gWebRunBusy) {
        jsroot_set_status("another analysis is already running");
        return kFALSE;
    }
    if (inputNumber < 0 || inputNumber > 999999) {
        jsroot_set_status("input number must be between 0 and 999999");
        return kFALSE;
    }

    gWebRunBusy = kTRUE;
    gWebRunInputNumber = inputNumber;

    const TString finalOutput = Form(
            "%s/result_%s_%06d.root",
            gWebRunOutputDirectory.Data(),
            jsroot_backend_key(backend), inputNumber);
    gWebRunTemporaryOutput = finalOutput + ".part";
    const std::string finalPath = finalOutput.Data();

    gSystem->Unlink(gWebRunTemporaryOutput);

    jsroot_set_status(Form("running %s input %d",
            jsroot_backend_title(backend), inputNumber));

    Bool_t succeeded = jsroot_execute_analysis(backend)
                    && !gSystem->AccessPathName(gWebRunTemporaryOutput);

    // Readers never see a half-written ROOT file: publish only after the
    // analysis macro has closed its temporary output successfully.
    if (succeeded) {
        // Keep an older result registered while the new analysis is running,
        // then close it immediately before atomically replacing the file.
        jsroot_close_result(finalPath);
        jsroot_forget_result(finalPath);
        if (gSystem->Rename(gWebRunTemporaryOutput, finalOutput) != 0) {
            Error("run_jsroot_input", "cannot rename %s to %s",
                    gWebRunTemporaryOutput.Data(), finalOutput.Data());
            succeeded = kFALSE;
        }
    }

    if (!succeeded) {
        gSystem->Unlink(gWebRunTemporaryOutput);
        jsroot_set_status(Form("%s input %d failed",
                jsroot_backend_title(backend), inputNumber));
        gWebRunBusy = kFALSE;
        return kFALSE;
    }

    TFile* resultFile = TFile::Open(finalOutput, "READ");
    if (resultFile == nullptr || resultFile->IsZombie()) {
        delete resultFile;
        jsroot_set_status(Form(
                "input %d finished, but the result cannot be opened",
                inputNumber));
        gWebRunBusy = kFALSE;
        return kFALSE;
    }

    const Int_t registeredObjects = jsroot_register_result_objects(
            resultFile, inputNumber, backend, finalPath);
    resultFile->Close();
    delete resultFile;
    gWebRunResultOrder.push_back(finalPath);

    while (static_cast<Int_t>(gWebRunResultOrder.size())
            > gWebRunMaximumRegisteredResults) {
        const std::string oldest = gWebRunResultOrder.front();
        gWebRunResultOrder.pop_front();
        jsroot_close_result(oldest);
    }

    jsroot_set_status(Form(
            "%s input %d finished: %s (%d drawable objects)",
            jsroot_backend_title(backend), inputNumber,
            finalOutput.Data(), registeredObjects));
    gWebRunBusy = kFALSE;
    return kTRUE;
}

// RegisterCommand normally substitutes %arg1% directly into a C++ expression.
// Instead, this command reads the untouched HTTP query and accepts digits only
// before calling run_jsroot_input(). This prevents the input box from becoming
// a general gROOT->ProcessLine() interface.
Bool_t jsroot_get_request_input(Int_t& inputNumber)
{
    if (gWebRunServer == nullptr)
        return kFALSE;

    TRootSniffer* sniffer = gWebRunServer->GetSniffer();
    THttpCallArg* call = sniffer->SetCurrentCallArg(nullptr);
    const std::string query = call == nullptr ? "" : call->GetQuery();
    sniffer->SetCurrentCallArg(call);

    const std::string prefix = "arg1=";
    if (query.compare(0, prefix.size(), prefix) != 0) {
        jsroot_set_status("missing input number");
        return kFALSE;
    }

    const std::string value = query.substr(prefix.size());
    if (value.empty() || value.size() > 6) {
        jsroot_set_status("input number must contain 1 to 6 digits");
        return kFALSE;
    }

    inputNumber = 0;
    for (char character : value) {
        if (character < '0' || character > '9') {
            jsroot_set_status("input number must contain digits only");
            return kFALSE;
        }
        inputNumber = inputNumber * 10 + character - '0';
    }

    return kTRUE;
}

Bool_t run_jsroot_root_request()
{
    Int_t inputNumber = -1;
    return jsroot_get_request_input(inputNumber)
        && run_jsroot_input(inputNumber, kJSRootROOT);
}

Bool_t run_jsroot_python_request()
{
    Int_t inputNumber = -1;
    return jsroot_get_request_input(inputNumber)
        && run_jsroot_input(inputNumber, kJSRootPython);
}

Bool_t run_jsroot_bash_request()
{
    Int_t inputNumber = -1;
    return jsroot_get_request_input(inputNumber)
        && run_jsroot_input(inputNumber, kJSRootBash);
}

void close_web_run_jsroot()
{
    // Unregister drawable objects before destroying the server hierarchy.
    while (!gWebRunObjects.empty())
        jsroot_close_result(gWebRunObjects.begin()->first);

    if (gWebRunServer != nullptr) {
        delete gWebRunServer;
        gWebRunServer = nullptr;
    }

    gWebRunObjects.clear();
    gWebRunResultOrder.clear();

    delete gWebRunStatus;
    gWebRunStatus = nullptr;
    gWebRunRootMacro.Clear();
    gWebRunPythonMacro.Clear();
    gWebRunBashScript.Clear();
    gWebRunOutputDirectory.Clear();
    gWebRunTemporaryOutput.Clear();
    gWebRunInputNumber = -1;
    gWebRunBusy = kFALSE;
}

void web_run_jsroot(
        int port = 9091,
        const char* rootMacro = "",
        const char* outputDirectory = "",
        int maximumRegisteredResults = 10,
        const char* pythonMacro = "",
        const char* bashScript = "")
{
    close_web_run_jsroot();

    TString exampleDirectory = gSystem->DirName(__FILE__);
    if (exampleDirectory.IsNull())
        exampleDirectory = ".";

    gWebRunRootMacro = rootMacro;
    if (gWebRunRootMacro.IsNull())
        gWebRunRootMacro = exampleDirectory + "/run_input_analysis.C";
    gWebRunRootMacro = jsroot_absolute_path(gWebRunRootMacro);

    gWebRunPythonMacro = pythonMacro;
    if (gWebRunPythonMacro.IsNull())
        gWebRunPythonMacro = exampleDirectory + "/run_input_analysis.py";
    gWebRunPythonMacro = jsroot_absolute_path(gWebRunPythonMacro);

    gWebRunBashScript = bashScript;
    if (gWebRunBashScript.IsNull())
        gWebRunBashScript = exampleDirectory + "/run_input_analysis.sh";
    gWebRunBashScript = jsroot_absolute_path(gWebRunBashScript);

    gWebRunOutputDirectory = outputDirectory;
    if (gWebRunOutputDirectory.IsNull())
        gWebRunOutputDirectory = exampleDirectory + "/results";
    gWebRunOutputDirectory = jsroot_absolute_path(gWebRunOutputDirectory);

    const TString scripts[] = {
        gWebRunRootMacro, gWebRunPythonMacro, gWebRunBashScript
    };
    for (const auto& script : scripts) {
        if (gSystem->AccessPathName(script)) {
            Error("web_run_jsroot", "analysis script not found: %s",
                    script.Data());
            close_web_run_jsroot();
            return;
        }
    }
    if (gSystem->mkdir(gWebRunOutputDirectory, kTRUE) != 0
            && gSystem->AccessPathName(gWebRunOutputDirectory)) {
        Error("web_run_jsroot", "cannot create output directory: %s",
                gWebRunOutputDirectory.Data());
        close_web_run_jsroot();
        return;
    }

    gWebRunMaximumRegisteredResults = maximumRegisteredResults < 1
                                    ? 1 : maximumRegisteredResults;
    gROOT->SetBatch(kTRUE);

    if (gROOT->LoadMacro(gWebRunRootMacro) < 0) {
        Error("web_run_jsroot", "cannot load analysis macro: %s",
                gWebRunRootMacro.Data());
        close_web_run_jsroot();
        return;
    }

    gWebRunStatus = new TNamed("Status", "ready");
    gWebRunServer = new THttpServer(
            Form("http:%d?loopback&top=LILAK;noglobal", port));
    if (!gWebRunServer->IsAnyEngine()) {
        Error("web_run_jsroot", "cannot start HTTP server on port %d", port);
        close_web_run_jsroot();
        return;
    }

    gWebRunServer->Register("/Control", gWebRunStatus);
    gWebRunServer->RegisterCommand(
            "/Control/RunROOT",
            "run_jsroot_root_request()",
            "button;rootsys/icons/ed_execute.png");
    gWebRunServer->RegisterCommand(
            "/Control/RunPython",
            "run_jsroot_python_request()",
            "button;rootsys/icons/ed_execute.png");
    gWebRunServer->RegisterCommand(
            "/Control/RunBash",
            "run_jsroot_bash_request()",
            "button;rootsys/icons/ed_execute.png");
    // Ask JSROOT for one argument without substituting it into C++ code.
    const char* commands[] = {
        "/Control/RunROOT",
        "/Control/RunPython",
        "/Control/RunBash"
    };
    for (const char* command : commands) {
        gWebRunServer->SetItemField(command, "_numargs", "1");
        gWebRunServer->SetItemField(command, "_hreload", "true");
    }

    const Int_t loadedResults = jsroot_load_existing_results();
    jsroot_set_status(Form(
            "ready: loaded %d existing result file(s)", loadedResults));

    Info("web_run_jsroot", "open http://localhost:%d", port);
    Info("web_run_jsroot", "choose Control/RunROOT, RunPython, or RunBash");
    Info("web_run_jsroot", "results are written to %s",
            gWebRunOutputDirectory.Data());
    Info("web_run_jsroot", "ROOT macro: %s", gWebRunRootMacro.Data());
    Info("web_run_jsroot", "Python macro: %s", gWebRunPythonMacro.Data());
    Info("web_run_jsroot", "Bash script: %s", gWebRunBashScript.Data());
    Info("web_run_jsroot", "at most %d result set(s) stay registered",
            gWebRunMaximumRegisteredResults);
}
