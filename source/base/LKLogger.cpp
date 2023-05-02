#include "LKLogger.hpp"

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

LKLogger::LKLogger(TString name, const std::string &title ,int rank, int option)
{
    if (option == 0)
        return;

    TString header;
    if (!name.IsNull()) {
        for (auto i=0; i<rank; ++i)
            header = header + "  ";
        header = header + Form("[%s::%s] ", name.Data(), title.c_str());
    }

    if (LKLogManager::LogToConsol())
    {
        switch (option)
        {
            case 1:  std::cout << header; break;
            case 2:  std::cout << header << "\033[0;32m" << "info> "     << "\033[0m"; break;
            case 3:  std::cout << header << "\033[0;33m" << "warning> " << "\033[0m"; break;
            case 4:  std::cout << header << "\033[0;31m" << "error> "    << "\033[0m"; break;
            default: ;
        }

        if (LKLogManager::LogToFile())
        {
            switch (option)
            {
                case 1:  LKLogManager::GetLogFile() << header; break;
                case 2:  LKLogManager::GetLogFile() << header << "info> "; break;
                case 3:  LKLogManager::GetLogFile() << header << "warning> "; break;
                case 4:  LKLogManager::GetLogFile() << header << "error> "; break;
                default: ;
            }
        }
    }
}

LKLogger::LKLogger(const std::string &title ,int line)
{
    if (LKLogManager::LogToConsol())
        std::cout<<"+\033[0;36m"<<Form("%d ",line)<<"\033[0m "<<Form("%s \033[0;36m#\033[0m ", title.c_str());
    if (LKLogManager::LogToFile())
        LKLogManager::GetLogFile()<<"+"<<Form("%d ",line)<<" "<<Form("%s # ", title.c_str());
}
