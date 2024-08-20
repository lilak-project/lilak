#ifndef LKLOGGER_HH
#define LKLOGGER_HH

#include <iostream>
#include <fstream>
#include "TString.h"
#include "LKLogger.h"
#include "LKCompiled.h"

/// lilak logger shortcut macros
#define lk_logger(logFileName) LKLogManager::RunLogger(logFileName, false)
#define lk_logger_new(logFileName,forceNewLogger) LKLogManager::RunLogger(logFileName, true)
#define lk_logger_name() LKLogManager::GetLogFileName()

/// set
#define lk_set_message(val) LKLogManager::SetPrintMessage(val)
#define lk_set_plane(val)   LKLogManager::SetPrintPlane(val)
#define lk_set_cout(val)    LKLogManager::SetPrintCout(val)
#define lk_set_info(val)    LKLogManager::SetPrintInfo(val)
#define lk_set_warning(val) LKLogManager::SetPrintWarn(val)
#define lk_set_note(val)    LKLogManager::SetPrintNote(val)
#define lk_set_error(val)   LKLogManager::SetPrintError(val)
#define lk_set_test(val)    LKLogManager::SetPrintTest(val)
#define lk_set_list(val)    LKLogManager::SetPrintList(val)
#define lk_set_debug(val)   LKLogManager::SetPrintDebug(val)

/// lilak debug logger
#define lk_debug    LKLogger(__FILE__,__LINE__)
#define lk_cout     LKLogger(fName,__FUNCTION__,0,1)
#define lk_info     LKLogger(fName,__FUNCTION__,0,2)
#define lk_warning  LKLogger(fName,__FUNCTION__,0,3)
#define lk_note     LKLogger(fName,__FUNCTION__,0,7)
#define lk_error    LKLogger(fName,__FUNCTION__,0,4)
#define lk_test     LKLogger(fName,__FUNCTION__,0,5)
#define lk_list(i)  LKLogger(fName,__FUNCTION__,i,6)

/// lilak logger for non-LKGear classes and macros. These will not create line-header
#define e_debug     LKLogger(__FILE__,__LINE__)
#define e_cout      LKLogger("","",0,1)
#define e_info      LKLogger("","",0,2)
#define e_warning   LKLogger("","",0,3)
#define e_note      LKLogger("","",0,7)
#define e_error     LKLogger("","",0,4)
#define e_test      LKLogger("","",0,5)
#define e_list(i)   LKLogger("","",i,6)

/// lilak logger manager
class LKLogManager
{
    private:
        static LKLogManager *fLogManager;
        static TString fLogFileName;
        static std::ofstream fLogFile;
        static bool fLogToConsol;
        static bool fLogToFile;

        static bool fPrintPlane;
        static bool fPrintDebug;
        static bool fPrintCout;
        static bool fPrintInfo;
        static bool fPrintWarn;
        static bool fPrintNote;
        static bool fPrintError;
        static bool fPrintList;
        static bool fPrintTest;
        static bool fPrintCurrent;
        static bool fPrintMessage;

    public:
        LKLogManager(TString fileName);
        static LKLogManager* RunLogger(TString fileName="", bool forceNewLogger=false);
        static TString GetLogFileName();
        static std::ofstream& GetLogFile();
        static bool LogToConsol();
        static bool LogToFile();
        static void SetLogToConsol(bool val);
        static void SetLogToFile(bool val);
        static void SetPrintPlane(bool val);

        static void SetPrintDebug(bool val);
        static void SetPrintCout(bool val);
        static void SetPrintInfo(bool val);
        static void SetPrintWarn(bool val);
        static void SetPrintNote(bool val);
        static void SetPrintError(bool val);
        static void SetPrintList(bool val);
        static void SetPrintTest(bool val);
        static void SetPrintCurrent(bool val); 
        static void SetPrintMessage(bool val); 

        static bool PrintPlane();
        static bool PrintDebug();
        static bool PrintCout();
        static bool PrintInfo();
        static bool PrintWarn();
        static bool PrintNote();
        static bool PrintError();
        static bool PrintList();
        static bool PrintTest();
        static bool PrintCurrent();
        static bool PrintMessage();
};

/// lilak logger
class LKLogger
{
    public:
        LKLogger(TString name, const std::string &title ,int rank, int option);
        LKLogger(const std::string &title ,int line);

        template <class T> LKLogger &operator<<(const T &v)
        {
            if (LKLogManager::PrintMessage()==false||LKLogManager::PrintCurrent()==false)
                return *this;

            if (LKLogManager::LogToConsol()) std::cout << v;
            if (LKLogManager::LogToFile()) LKLogManager::GetLogFile() << v;
            return *this;
        }

        LKLogger &operator<<(std::ostream&(*f)(std::ostream&))
        {
            if (LKLogManager::PrintMessage()==false||LKLogManager::PrintCurrent()==false)
                return *this;

            if (LKLogManager::LogToConsol()) std::cout << *f;
            if (LKLogManager::LogToFile()) LKLogManager::GetLogFile() << *f;
            return *this;
        }
};

#endif
