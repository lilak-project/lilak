#include "LKListFileParser.h"
#include "LKParameter.h"
#include "LKContainer.h"
#include "LKLogger.h"

ClassImp(LKListFileParser)

void LKListFileParser::SetClass(TString className, int size)
{
    if (fDataArray==nullptr) {
        fClassName = className;
        fDataArray = new TClonesArray(fClassName, size);
    }
}

bool LKListFileParser::ReadFile(TString fileName, TString format, bool skipFirstLine)
{
    fFileName = fileName;
    fFormat = format;

    format.ReplaceAll(":"," ");
    LKParameter parKeywords(format);
    int nEl = parKeywords.GetN();
    auto exampleData = (LKContainer*) fDataArray -> ConstructedAt(0);
    for (auto iEl=0; iEl<nEl; ++iEl) {
        bool setValue = exampleData -> SetValue(parKeywords.GetString(iEl), "0");
        if (setValue==false) {
            e_error << "Cannot find " << parKeywords.GetString(iEl) << " in class " << fClassName << endl;
            e_error << "File:   " << fileName << endl;
            e_error << "Format: " << format << endl;
            return false;
        }
    }

    ifstream mapFile(fileName);
    if (!mapFile.is_open())
        e_error << "Failed to open file " << fileName << endl;
    string line;
    if (skipFirstLine)
        getline(mapFile, line);
    int count = 0;
    while (getline(mapFile, line))
    {
        istringstream ss(line);
        TString sline = line;
        LKParameter parValues(sline);
        parKeywords.GetString(0);
        parValues.GetString(0);
        auto lineData = (LKContainer*) fDataArray -> ConstructedAt(count);
        for (auto iEl=0; iEl<nEl; ++iEl) {
            lineData -> SetValue(parKeywords.GetString(iEl), parValues.GetString(iEl));
        }
        count++;
    }

    return true;
}

void LKListFileParser::Print(Option_t *option) const
{
    if (fFileName.IsNull())
        e_info << "Waiting for ReadFile()" << endl;
    else {
        e_info << "Input file: " << fFileName << endl;
        e_info << "Map format: " << fFormat << endl;
        e_info << "Data class: " << fClassName << endl;
        e_info << "# of data: " << fDataArray -> GetEntries() << endl;
    }
}
