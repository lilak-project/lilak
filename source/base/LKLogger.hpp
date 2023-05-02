#ifndef LKLOGGER_HH
#define LKLOGGER_HH

#include <iostream>
#include <fstream>
#include "TString.h"
#include "LKCompiled.h"
#include "LKLogger.hpp"

/// lilak logger shortcut macros
#define lk_logger(logFileName) LKLogManager::RunLogger(logFileName, forceNewLogger)
#define lk_logger_name() LKLogManager::GetLogFileName()

/// lilak logger manager
class LKLogManager
{
    private:
        static LKLogManager *fLogManager;
        static TString fLogFileName;
        static std::ofstream fLogFile;
        static bool fLogToConsol;
        static bool fLogToFile;

    public:
        LKLogManager(TString fileName);
        static LKLogManager* RunLogger(TString fileName="", bool forceNewLogger=false);
        static TString GetLogFileName();
        static std::ofstream& GetLogFile();
        static bool LogToConsol();
        static bool LogToFile();
};

/// lilak debug logger
#define lk_debug   LKLogger(__FILE__,__LINE__)
#define lx_debug   LKLogger(__FILE__,__LINE__)

/// lilak logger
#define lk_cout    LKLogger(fName,__FUNCTION__,fRank,1)
#define lk_info    LKLogger(fName,__FUNCTION__,fRank,2)
#define lk_warning LKLogger(fName,__FUNCTION__,fRank,3)
#define lk_error   LKLogger(fName,__FUNCTION__,fRank,4)

/// lilak logger for non-LKGear classes and macros. These will not create line-header
#define lx_cout     LKLogger("","",0,1)
#define lx_info     LKLogger("","",0,2)
#define lx_warning  LKLogger("","",0,3)
#define lx_error    LKLogger("","",0,4)

/// lilak logger
class LKLogger
{
    public:
        LKLogger(TString name, const std::string &title ,int rank, int option);
        LKLogger(const std::string &title ,int line);

        template <class T> LKLogger &operator<<(const T &v)
        {
            if (LKLogManager::LogToConsol()) std::cout << v;
            if (LKLogManager::LogToFile()) LKLogManager::GetLogFile() << v;
            return *this;
        }

        LKLogger &operator<<(std::ostream&(*f)(std::ostream&))
        {
            if (LKLogManager::LogToConsol()) std::cout << *f;
            if (LKLogManager::LogToFile()) LKLogManager::GetLogFile() << *f;
            return *this;
        }
};

#endif
