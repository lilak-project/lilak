#include "LKLogger.h"

void run_logger()
{
    //lk_set_message(false);
    //lk_set_plane(true);
    //lk_set_cout(false);
    //lk_set_info(false);
    //lk_set_warning(false);
    //lk_set_strong(false);
    //lk_set_error(false);
    //lk_set_test(false);
    //lk_set_list(false);
    //lk_set_debug(false);

    TString logFileName = "example_log.txt";
    lk_logger(logFileName);

    e_cout << endl;
    e_info << "lk_logger(file_name) start logger with \"file_name\"" << endl;
    e_info << "lk_set_message(false) turn off all LILAK logger messages" << endl;
    e_info << "lk_set_plane(true) change all messages into plane style messages except for debug logger" << endl;

    e_cout << endl;
    e_info << "lk_set_cout(false)    turn off all e_cout    and lk_cout    messages" << endl;
    e_info << "lk_set_info(false)    turn off all e_info    and lk_info    messages" << endl;
    e_info << "lk_set_warning(false) turn off all e_warning and lk_warning messages" << endl;
    e_info << "lk_set_strong(false)  turn off all e_strong  and lk_strong  messages" << endl;
    e_info << "lk_set_error(false)   turn off all e_error   and lk_error   messages" << endl;
    e_info << "lk_set_test(false)    turn off all e_test    and lk_test    messages" << endl;
    e_info << "lk_set_list(false)    turn off all e_list    and lk_list    messages" << endl;
    e_info << "lk_set_debug(false)   turn off all e_debug   and lk_debug   messages" << endl;

    e_cout     << endl;
    e_cout     << "e_cout prints with no format" << endl;
    e_info     << "e_info : informative messgae" << endl;
    e_warning  << "e_warning : warning messgae (program do not have to stop)" << endl;
    e_error    << "e_error : error message (program is likely to stop)" << endl;

    e_cout     << endl;
    e_list(0)  << "e_list(0) : message for list" << endl;
    e_list(1)  << "e_list(1) : message for list" << endl;
    e_list(2)  << "e_list(2) : message for list" << endl;
    e_list(3)  << "e_list(3) : message for list" << endl;
    e_cout     << "..." << endl;

    e_cout     << endl;
    e_test     << "e_test : test message" << endl;
    e_strong   << "e_strong : strong message" << endl;
    e_debug    << "e_debug and lk_debug print file path and line number" << endl;

    TString fName = "name";
    e_cout     << endl;
    e_cout     << "lk_* loggers print out fName and function name" << endl;
    lk_cout    << "lk_cout" << endl;
    lk_info    << "lk_info" << endl;
    lk_warning << "lk_warning" << endl;
    lk_error   << "lk_error" << endl;
    lk_test    << "lk_test" << endl;
    lk_strong  << "lk_strong" << endl;
}
