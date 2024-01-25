#ifndef LKELECTRONICSTASK_HH
#define LKELECTRONICSTASK_HH

#include "TClonesArray.h"
#include "LKLogger.h"
#include "LKParameterContainer.h"
#include "LKRun.h"
#include "LKTask.h"

/*
 * Remove this comment block after reading it through
 * or use print_example_comments=False option to omit printing
 *
 * # Example LILAK task class
 *
 * - Write Init() method.
 * - Write Exec() or/and EndOfRun() method.
 */

class LKElectronicsTask : public LKTask
{
    public:
        LKElectronicsTask();
        virtual ~LKElectronicsTask() { ; }

        bool Init();
        void Exec(Option_t *option="");
        bool EndOfRun();

    ClassDef(LKElectronicsTask,1);
};

#endif
