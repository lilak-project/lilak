#ifndef LKLISTFILEPARSER_HH
#define LKLISTFILEPARSER_HH

#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <ctime>

#include "TVector3.h"
#include "TString.h"
#include "TObject.h"
#include "TClonesArray.h"
#include "TObject.h"

using namespace std;

/**
 * @brief Parse data file to list of given data container class in a form of TClonesArray.
 *
 * @detail 
 * Corresponding container class should have SetValue(TString keyword, TString value) method defined.
 * Each column of data file may be linked to each member of container class through "format".
 * "format" is a list of keywords joined with colon(:) with an order of columns in data file.
 * One should refere to the container class for the keywords.
 *
 * - Example:
@code{.cpp}
{
    LKListFileParser parser;
    parser.SetClass("CAACMapData");
    bool readFile = parser.ReadFile("file.txt","id:cobo:asad:aget:chan:x0:z0:x1:z1:x2:z2:x3:z3");
    if (readFile==false) {
        e_error << "Something wrong!" << endl;
        return;
    }
    auto dataArray = parser.GetDataArray();
    auto n = dataArray -> GetEntries();
    for (auto i=0; i<n; ++i)
    {
        auto data = (CAACMapData*) dataArray -> At(i);
        cout << data -> cobo << endl;
        // ...
    }
}
@endcode
 */
class LKListFileParser : public TObject
{
    private:
        TString fFileName;
        TString fFormat;
        TString fClassName;
        TClonesArray *fDataArray = nullptr;

    public:
        LKListFileParser() {}

        void SetClass(TString className, int size=1000);

        /// Format should be a line of keywords joined by space or colon(:).
        /// The keywords are defined inside the given class.
        /// Refere to the SetValue(TString, TString) method of corresponding container class.
        /// Example:
        ///   id:cobo:asad:aget:chan:x0:z0:x1:z1:x2:z2:x3:z3
        /// Return false, if class find any keywords inside the format that do not match to one of member data 
        bool ReadFile(TString fileName, TString format, bool skipFirstLine);

        void Print(Option_t *option="") const;

        TClonesArray *GetDataArray() { return fDataArray; }

    ClassDef(LKListFileParser,1)
};

#endif
