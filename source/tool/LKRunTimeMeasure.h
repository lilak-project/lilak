#ifndef LK_RUN_TIME_MEASURE_H
#define LK_RUN_TIME_MEASURE_H

#include <iostream>
#include <chrono>
#include <unordered_map>
#include <iomanip>
#include <thread>
#include "TObject.h"

using namespace std;

class LKRunTimeMeasure : public TObject
{
    private:
        unordered_map<int, chrono::high_resolution_clock::time_point> startTimes;
        unordered_map<int, double> elapsedTimes;

    public:
        chrono::high_resolution_clock::time_point Start(int i=0);
        chrono::high_resolution_clock::time_point Stop(int i=0);
        chrono::high_resolution_clock::time_point Pause(int i) { return Stop(i); }
        chrono::high_resolution_clock::time_point Resume(int i) { return Start(i); }

        void StopAll();
        void Print() const;

        chrono::high_resolution_clock::time_point Sleep(int t);

    ClassDef(LKRunTimeMeasure, 1)
};

#endif
