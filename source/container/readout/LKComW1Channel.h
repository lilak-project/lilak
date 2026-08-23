#ifndef LKCOMW1CHANNEL_H
#define LKCOMW1CHANNEL_H

#include "LKComChannel.h"

/**
 * Raw reconstructed CoMPASS W1 strip channel.
 *
 * This extends the generic CoMPASS channel with W1 side and local strip
 * information.  The side convention is false for junction/Y and true for
 * ohmic/X.
 */
class LKComW1Channel : public LKComChannel
{
    public:
        /// Constructor. Initializes all data members with invalid defaults.
        LKComW1Channel();
        virtual ~LKComW1Channel() {}

        /// Reset this channel to invalid/default values.
        virtual void Clear(Option_t *option="");

        /// Fill all channel fields at once.
        void Set(short detID, bool side, int strip, ULong64_t time, short energy, short energyShort, double ecal);

        bool fSide;  ///< false: junction/Y side, true: ohmic/X side.
        int fStrip;  ///< Local strip number, starting from 1.

    ClassDef(LKComW1Channel,1);
};

#endif
