#ifndef LKCOMCHANNEL_H
#define LKCOMCHANNEL_H

#include "TObject.h"

/**
 * Raw reconstructed CoMPASS single-channel readout.
 *
 * One object corresponds to one CoMPASS raw hit mapped to one detector channel.
 * Single-channel detectors can use this class directly.  Strip detectors can
 * inherit from it and add geometry-specific fields.
 */
class LKComChannel : public TObject
{
    public:
        /// Constructor. Initializes all data members with invalid defaults.
        LKComChannel();
        virtual ~LKComChannel() {}

        /// Reset this channel to invalid/default values.
        virtual void Clear(Option_t *option="");

        /// Fill all channel fields at once.
        void Set(short detID, ULong64_t time, short energy, short energyShort, double ecal);

        short fDetectorID;   ///< Detector identifier.
        ULong64_t fTime;     ///< Raw timestamp.
        short fEnergy;       ///< Raw long-gate energy.
        short fEnergyShort;  ///< Raw short-gate energy.
        double fECal;        ///< Calibrated energy.

    ClassDef(LKComChannel,1);
};

#endif
