/**
 * @brief MC Event Generator
 * @author JungWoo Lee (Korea Univ.)
 *
 * -# EventGen file must have following format.
 * -# 1) Second line should be "p" or "e": "p" for momentum (Mev/c), "e" for energy (MeV).
 * -# 1) Second line is [nEvents].
 * -# 2) Repeat 3) to 5) [nEvents] times.
 * -# 3) First line of event is 3 components of vertex position in next event.
 * -# 4) Next line is [nTracks].
 * -# 5) Next [nTracks] lines follows [pdg], [px], [py] and [pz].
 *
 * [p/e]
 * [nEvents]
 * [eventID] [nTracks] [vx] [vy] [vz]
 * [pdg] [px] [py] [pz]
 * [pdg] [px] [py] [pz]
 * ...
 * 
 * [eventID] [nTracks] [vx] [vy] [vz]
 * [pdg] [px] [py] [pz]
 * [pdg] [px] [py] [pz]
 * ...
 * ...
 *
 * example)
 * 10
 * 0 181 0 0 0
 * 1000020040 0.592732 0.259583 3.45222
 * 1000020040 -0.506001 0.292528 2.11579
 * ...
 * 1 58 0 0 100
 * 1000020040 -0.192129 0.0461225 1.57347
 * 2212 -0.0527492 -0.214962 0.696101
 * ...
 * ...
 *
 * @param [nEvents] total number of events
 * @param [eventID] event ID
 * @param [nTracks] total number of tracks
 * @param [vx] x-component of vertex [mm]
 * @param [vy] y-component of vertex [mm]
 * @param [vz] z-component of vertex [mm]
 * @param [pdg] particle PDG code
 * @param [px] x-component of momentum [MeV/c] (option "p") or energy [MeV] (option "e")
 * @param [py] y-component of momentum [MeV/c] (option "p") or energy [MeV] (option "e")
 * @param [pz] z-component of momentum [MeV/c] (option "p") or energy [MeV] (option "e")
 */

#ifndef LKMCEVENTGENGENERATOR_HH
#define LKMCEVENTGENGENERATOR_HH

#include <fstream>
#include "TVector3.h"
#include "TClonesArray.h"

#define g4gen_info LKLogger("LKMCEventGenerator",__FUNCTION__,0,2)

class LKMCEventGenerator
{
    public:
        LKMCEventGenerator();
        LKMCEventGenerator(TString fileName);
        virtual ~LKMCEventGenerator();

        bool ReadNextEvent(Double_t &vx, Double_t &vy, Double_t &vz);
        bool ReadNextTrack(Int_t &pdg, Double_t &px, Double_t &py, Double_t &pz);

        Int_t GetNumEvents() { return fNumEvents; };
        bool ReadMomentumOrEnergy() { return fReadMomentumOrEnergy; }

    private:
        std::ifstream fInputFile;
        bool fReadMomentumOrEnergy;
        Int_t fNumEvents;
        Int_t fNumTracks;
        Int_t fCurrentTrackID;
};

#endif
