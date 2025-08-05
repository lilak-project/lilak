#ifndef MISCELANEOUS_HH
#define MISCELANEOUS_HH

#include "TObject.h"
#include "LKLogger.h"
#include <iostream>
using namespace std;

class LKMisc : public TObject
{
    public:
        static double FWHM(double *buffer, int length);
        static double FWHM(double *buffer, int length, int xPeak, double amplitude, double pedestal, double &xMin, double &xMax);

        static double EvalPedestal(double *buffer, int length, int method=0);
        /// cvCut: stdDev/mean cut for collecting samples used for calculating pedestal.
        static double EvalPedestalSamplingMethod(double *buffer, int length,  int sampleLength=50, double cvCut=0.2, bool subtractPedestal=false);
    
        static void DrawColors();
        static void DrawMarkers();
        static void DrawColors(vector<int> colors);
        static void DrawColors(vector<TString> colors);
        static void DrawFonts();

        static int     FindOptionInt   (TString option, TString name, int emptyValue);
        static double  FindOptionDouble(TString option, TString name, double emptyValue);
        static TString FindOptionString(TString option, TString name, TString emptyValue);
        static TString FindOption      (TString &option, TString name, bool removeAfter=false, int addValue=0);
        static bool RemoveOption(TString &option, TString name);
        static bool CheckOption(TString &option, TString name, bool removeAfter=false, int addValue=0);

        static void AddOption(TString &original, TString adding);
        static void AddOption(TString &original, TString adding, TString value);
        static void AddOption(TString &original, TString adding, double value);
        static void AddOption(TString &original, TString adding, int value);

        static int Index1(TString option, TString pat);

        static bool ValueIsInArray(TString value, vector<TString> array);
        static bool ValueIsInArray(double value, vector<double> array);
        static bool ValueIsInArray(int value, vector<int> array);

        static TString RemoveTrailing0(TString value, bool removeLastDot=false);
        static TString RemoveTrailing0(double value, bool removeLastDot=false);

    public:
        LKMisc();
        virtual ~LKMisc() {}
        static LKMisc* GetMisc();

    private:
        static LKMisc* fInstance;

    ClassDef(LKMisc,1);
};

#endif
