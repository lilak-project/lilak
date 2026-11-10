#ifndef LKCHANNEL_HH
#define LKCHANNEL_HH

#include "LKContainer.h"

#include "TObject.h"
#include "TObjArray.h"
#include "TVector3.h"

/**
 * General channel
 * Note that PadID is shared with DetID
 * Use PadID for TPC, and DetID for Other detectors
 */
class LKChannel : public LKContainer
{
    public:
        LKChannel();
        virtual ~LKChannel() {}

        virtual void Clear(Option_t *option = "");
        virtual void Copy(TObject &obj) const;
        virtual void Print(Option_t *option = "") const;

        virtual TObject *Clone(const char *newname="") const;

        void SetChannelID(int value) { fChannelID = value; }
        void SetPadID(int value) { fPadID = value; }
        void SetDetID(int value) { fPadID = value; }
        void SetTime(double value) { fTime = value; }
        void SetEnergy(double value) { fEnergy = value; }
        void SetPedestal(double value) { fPedestal = value; }
        /// Store the signed noise value; see fNoiseScale for the bipolar convention.
        void SetNoiseScale(double value) { fNoiseScale = value; }

        int GetChannelID() const { return fChannelID; }
        int GetDetID() const { return fPadID; }
        int GetPadID() const { return fPadID; }
        double GetTime() const { return fTime; }
        double GetEnergy() const { return fEnergy; }
        double GetPedestal() const { return fPedestal; }
        /// Return the stored signed value, including the bipolar marker.
        double GetNoiseScale() const { return fNoiseScale; }

    protected:
        int fChannelID = -1;
        int fPadID = -1;
        double fTime = -1;
        /// LKSetSiChannelTask uses SaturationEnergyFormula for saturated hits; default 10000 is a marker.
        /// This convention applies before energy calibration; old reco files may contain slope estimates.
        double fEnergy = -1;
        double fPedestal = -1;
        /**
         * Noise value persisted in the channel (also inherited by GETChannel and LKSiChannel).
         * LKSetSiChannelTask stores the population standard deviation of the configured
         * inclusive pedestal TB interval, in ADC counts. Normal channels store +stddev.
         * If LKChannelAnalyzer detects a bipolar waveform (an opposite-polarity lobe
         * near the signal with the configured percentage of its amplitude), the task
         * stores -max(stddev, 1.e-12) to mark it while retaining the noise magnitude.
         * Example: stddev=20 ADC is stored as +20 normally, or -20 for bipolar.
         * SKChannelFilterTask uses the sign for Filter/Bipolar and abs(value) for
         * noise-level and signal-to-noise cuts: SNR = Energy / abs(value).
         * The reset/default value -1 means unset; it is NOT itself proof of bipolar
         * detection in unprocessed or legacy data. Interpret the sign as a bipolar
         * marker only for channels processed with the above convention.
         */
        double fNoiseScale = -1;

    ClassDef(LKChannel, 2)
};

#endif
