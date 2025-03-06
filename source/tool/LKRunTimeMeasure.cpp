#include "LKRunTimeMeasure.h"

ClassImp(LKRunTimeMeasure);

chrono::high_resolution_clock::time_point LKRunTimeMeasure::Start(int i) {
    auto now = chrono::high_resolution_clock::now();
    startTimes[i] = now;
    return now;
}

chrono::high_resolution_clock::time_point LKRunTimeMeasure::Stop(int i) {
    auto now = chrono::high_resolution_clock::now();
    auto it = startTimes.find(i);
    if (it != startTimes.end()) {
        double elapsed = chrono::duration<double>(now - it->second).count();
        elapsedTimes[i] += elapsed;
        startTimes.erase(it);
        //cout << "Timer " << i << " stopped. Elapsed time: " << fixed << setprecision(6) << elapsed << " seconds.\n";
    } else {
        cerr << "Timer " << i << " was not started or already stopped.\n";
    }
    return now;
}

void LKRunTimeMeasure::StopAll() {
    for (auto it = startTimes.begin(); it != startTimes.end(); ++it) {
        auto now = chrono::high_resolution_clock::now();
        double elapsed = chrono::duration<double>(now - it->second).count();
        elapsedTimes[it->first] += elapsed;
        //cout << "Timer " << it->first << " stopped. Elapsed time: " << fixed << setprecision(6) << elapsed << " seconds.\n";
    }
    startTimes.clear();
}

void LKRunTimeMeasure::Print() const {
    cout << "--- Timer Report ---\n";
    for (const auto& pair : elapsedTimes) {
        cout << "Timer " << pair.first << ": " << fixed << setprecision(6) << pair.second << " seconds.\n";
    }
    for (const auto& pair : startTimes) {
        cout << "Timer " << pair.first << " is still running.\n";
    }
}

chrono::high_resolution_clock::time_point LKRunTimeMeasure::Sleep(int t) {
    this_thread::sleep_for(chrono::seconds(t));
    return chrono::high_resolution_clock::now();
}
