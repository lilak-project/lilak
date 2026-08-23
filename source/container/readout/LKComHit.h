#ifndef LKCOMHIT_H
#define LKCOMHIT_H

#include "TObject.h"

/**
 * Reconstructed CoMPASS detector hit.
 *
 * Channels from one detector in one event window are combined into one hit.
 * Single-channel detectors usually have multiplicity one.  More complex
 * detectors can inherit from this class and add position fields.
 */
class LKComHit : public TObject
{
    public:
        /// Constructor. Initializes all data members with invalid defaults.
        LKComHit();
        virtual ~LKComHit() {}

        /// Reset this hit to invalid/default values.
        virtual void Clear(Option_t *option="");

        short fDetectorID;   ///< Detector identifier.
        short fChannelMult;  ///< Number of channels used to build this hit.
        ULong64_t fTime;     ///< Earliest channel timestamp in the hit.
        short fEnergy;       ///< Summed raw energy.
        double fECal;        ///< Summed calibrated energy.

    ClassDef(LKComHit,1);
};

#endif
