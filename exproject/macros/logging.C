#include "LKLogger.hpp"

void logging() {
    lk_logger("test.log");
    lx_info << "info" << endl;
    lk_debug << "db" << endl;
}
