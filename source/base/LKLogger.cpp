#include <iomanip>
#include "LKLogger.h"

LKLogManager* LKLogManager::fLogManager = nullptr;
TString LKLogManager::fLogFileName = "";
std::ofstream LKLogManager::fLogFile;
bool LKLogManager::fLogToConsol = true;
bool LKLogManager::fLogToFile = false;

LKLogManager::LKLogManager(TString fileName)
{
    fLogManager = this;
    if (fileName.IsNull())
        fileName = TString(LILAK_PATH)+"/data/auto_lilak_logger.log";

    fLogToFile = true;
    LKLogManager::fLogFileName = fileName;
    fLogFile.open(fileName);
    std::cout << "Log file is set: " << fileName << std::endl;
}

LKLogManager* LKLogManager::RunLogger(TString fileName, bool forceNewLogger)
{
    if (fLogManager != nullptr) {
        std::cout << "Logger is already running :" << LKLogManager::fLogFileName << std::endl;
        if (forceNewLogger) {
            std::cout << "forceNewLogger flag is set. Creating new logger " << fileName << std::endl;
            return new LKLogManager(fileName);
        }
        return fLogManager;
    }
    return new LKLogManager(fileName);
}

TString LKLogManager::GetLogFileName() { return fLogFileName; }
std::ofstream& LKLogManager::GetLogFile() { return fLogFile; }
bool LKLogManager::LogToConsol() { return fLogToConsol; }
bool LKLogManager::LogToFile() { return fLogToFile; }
void LKLogManager::SetLogToConsol (bool val) { fLogToConsol = val; }
void LKLogManager::SetLogToFile   (bool val) { fLogToFile = val; }
bool LKLogManager::fPrintPlane    = false;
bool LKLogManager::fPrintDebug    = true;
bool LKLogManager::fPrintCout     = true;
bool LKLogManager::fPrintInfo     = true;
bool LKLogManager::fPrintWarn     = true;
bool LKLogManager::fPrintNote     = true;
bool LKLogManager::fPrintError    = true;
bool LKLogManager::fPrintList     = true;
bool LKLogManager::fPrintTest     = true;
bool LKLogManager::fPrintCurrent  = true;
bool LKLogManager::fPrintMessage  = true;
void LKLogManager::SetPrintCout   (bool val) { fPrintCout = val; }
void LKLogManager::SetPrintInfo   (bool val) { fPrintInfo = val; }
void LKLogManager::SetPrintWarn   (bool val) { fPrintWarn = val; }
void LKLogManager::SetPrintNote   (bool val) { fPrintNote = val; }
void LKLogManager::SetPrintList   (bool val) { fPrintList = val; }
void LKLogManager::SetPrintPlane  (bool val) { fPrintPlane = val; }
void LKLogManager::SetPrintError  (bool val) { fPrintError = val; }
void LKLogManager::SetPrintDebug  (bool val) { fPrintDebug = val; }
void LKLogManager::SetPrintTest   (bool val) { fPrintTest = val; }
void LKLogManager::SetPrintCurrent(bool val) { fPrintCurrent = val; } 
void LKLogManager::SetPrintMessage(bool val) { fPrintMessage = val; } 
bool LKLogManager::PrintPlane()   { return fPrintPlane; }
bool LKLogManager::PrintDebug()   { return fPrintDebug; }
bool LKLogManager::PrintCout()    { return fPrintCout; }
bool LKLogManager::PrintInfo()    { return fPrintInfo; }
bool LKLogManager::PrintWarn()    { return fPrintWarn; }
bool LKLogManager::PrintNote()    { return fPrintNote; }
bool LKLogManager::PrintError()   { return fPrintError; }
bool LKLogManager::PrintList()    { return fPrintList; }
bool LKLogManager::PrintTest()    { return fPrintTest; }
bool LKLogManager::PrintCurrent() { return fPrintCurrent; }
bool LKLogManager::PrintMessage() { return fPrintMessage; }


LKLogger::LKLogger(TString name, const std::string &title ,int rank, int option)
{
    if (LKLogManager::PrintPlane())
        option = 1;

    if (option == 0)
        return;

    if (LKLogManager::PrintMessage()==false)
        return;

    TString header;
    if (!name.IsNull()) {
        for (auto i=0; i<rank; ++i)
            header = header + "  ";
        header = header + Form("[%s::%s] ", name.Data(), title.c_str());
    }

    switch (option)
    {
        case 1:  (LKLogManager::PrintCout())   ? LKLogManager::SetPrintCurrent(true) : LKLogManager::SetPrintCurrent(false); break;
        case 2:  (LKLogManager::PrintInfo())   ? LKLogManager::SetPrintCurrent(true) : LKLogManager::SetPrintCurrent(false); break;
        case 3:  (LKLogManager::PrintWarn())   ? LKLogManager::SetPrintCurrent(true) : LKLogManager::SetPrintCurrent(false); break;
        case 7:  (LKLogManager::PrintNote())   ? LKLogManager::SetPrintCurrent(true) : LKLogManager::SetPrintCurrent(false); break;
        case 5:  (LKLogManager::PrintTest())   ? LKLogManager::SetPrintCurrent(true) : LKLogManager::SetPrintCurrent(false); break;
        case 6:  (LKLogManager::PrintList())   ? LKLogManager::SetPrintCurrent(true) : LKLogManager::SetPrintCurrent(false); break;
        case 4:  (LKLogManager::PrintError())  ? LKLogManager::SetPrintCurrent(true) : LKLogManager::SetPrintCurrent(false); break;
        default: ;
    }

    if (LKLogManager::PrintCurrent())
    {
        if (LKLogManager::LogToConsol())
        {
            switch (option)
            {
                case 1:  std::cout << header; break;
                case 2:  std::cout << header << "\033[0;32m" << "info> "  << "\033[0m"; break;
                case 3:  std::cout << header << "\033[0;33m" << "warn> "  << "\033[0m"; break;
                case 5:  std::cout << header << "\033[0;36m" << "test> "  << "\033[0m"; break;
                case 4:  std::cout << header << "\033[0;31m" << "error> " << "\033[0m"; break;
                case 7:  std::cout << header << "\033[0;31m" << "LOOK> "  << "\033[0m"; break;
                case 6:
                         if (rank<0)
                             std::cout << header << "\033[0;34m" << std::right << std::setw(4) << "*" << "  " << "\033[0m";
                         else
                             std::cout << header << "\033[0;34m" << std::right << std::setw(4) << rank << ". " << "\033[0m";
                         break;
                default: ;
            }

            if (LKLogManager::LogToFile())
            {
                switch (option)
                {
                    case 1:  LKLogManager::GetLogFile() << header; break;
                    case 2:  LKLogManager::GetLogFile() << header << "info> "; break;
                    case 3:  LKLogManager::GetLogFile() << header << "warn> "; break;
                    case 4:  LKLogManager::GetLogFile() << header << "error> "; break;
                    case 5:  LKLogManager::GetLogFile() << header << "test> "; break;
                    case 7:  LKLogManager::GetLogFile() << header << "LOOK> "; break;
                    case 6:  LKLogManager::GetLogFile() << header << std::right << std::setw(4) << rank << " "; break;
                    default: ;
                }
            }
        }
    }
}

LKLogger::LKLogger(const std::string &title ,int line)
{
    if (LKLogManager::PrintMessage()==false)
        return;

    (LKLogManager::PrintDebug()) ? LKLogManager::SetPrintCurrent(true) : LKLogManager::SetPrintCurrent(false);

    if (LKLogManager::PrintCurrent())
    {
        if (LKLogManager::LogToConsol())
            std::cout<<"+\033[0;36m"<<Form("%d ",line)<<"\033[0m "<<Form("%s \033[0;36m#\033[0m ", title.c_str());
        if (LKLogManager::LogToFile())
            LKLogManager::GetLogFile()<<"+"<<Form("%d ",line)<<" "<<Form("%s # ", title.c_str());
    }
}
