#ifndef LKCOMW1HIT_H
#define LKCOMW1HIT_H

#include "LKComHit.h"

/**
 * Reconstructed CoMPASS W1 detector hit.
 *
 * W1 channels from the same detector within one event window are combined into
 * one hit.  X or Y is left as -1 when the corresponding detector side is
 * missing in the event.
 */
class LKComW1Hit : public LKComHit
{
    public:
        /// Constructor. Initializes all data members with invalid defaults.
        LKComW1Hit();
        virtual ~LKComW1Hit() {}

        /// Reset this hit to invalid/default values.
        virtual void Clear(Option_t *option="");

        short fX;  ///< Ohmic/X strip number, or -1 if absent.
        short fY;  ///< Junction/Y strip number, or -1 if absent.

    ClassDef(LKComW1Hit,1);
};

#endif
