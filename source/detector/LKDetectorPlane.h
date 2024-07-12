#ifndef LKDETECTORPLANE_HH
#define LKDETECTORPLANE_HH

#include "LKChannel.h"
#include "LKVector3.h"
#include "LKGear.h"
#include "LKRun.h"
#include "LKPad.h"
#include "LKHit.h"
#include "LKHitArray.h"
#include "LKChannelAnalyzer.h"

#include "TH2.h"
#include "TCanvas.h"
#include "TObject.h"
#include "TNamed.h"
#include "TObjArray.h"
#include "TClonesArray.h"

typedef LKVector3::Axis axis_t;

class LKRun;
class LKDetector;

class LKDetectorPlane : public TNamed, public LKGear
{
    protected:
        LKDetector *fDetector = nullptr;
        LKChannelAnalyzer* fChannelAnalyzer = nullptr;
        TString fDetName;

        int fPlaneID = -1; ///< Detector plane id
        axis_t fAxis1 = LKVector3::kNon; ///< Axis-1 lying in plane. Must be set by input parameter.
        axis_t fAxis2 = LKVector3::kNon; ///< Axis-2 lying in plane. Must be set by input parameter.
        axis_t fAxis3 = LKVector3::kNon; ///< Axis-3 = Axis-1 x Axis-2 perpendicular to plane
        axis_t fAxisDrift = LKVector3::kNon; ///< Axis perpendicular to plane, in electron drifting direction. Must be set by input parameter.
        double fTbToLength = 1; ///< time-bin (tb) to position conversion factor. Must be set by input parameter.
        double fPosition; ///< Global position of pad plane center.

        TObjArray *fChannelArray = nullptr;
        TCanvas *fCanvas = nullptr;
        TH2 *fH2Plane = nullptr;
        TCanvas *fCvsChannelBuffer = nullptr;
        TGraph *fGraphChannelBoundary = nullptr;
        TGraph *fGraphChannelBoundaryNb[20] = {0};
        int fFreePadIdx = 0;

        bool fPadDataIsSet = false;
        bool fHitDataIsSet = false;
        bool fActive = true;

    public:
        LKDetectorPlane();
        LKDetectorPlane(const char *name, const char *title);
        virtual ~LKDetectorPlane() {};

        void SetDetector(LKDetector *detector) { fDetector = detector; }
        void SetDetectorName(TString name) { fDetName = name; }
        void SetPlaneID(int id) { fPlaneID = id; }
        int GetPlaneID() const { return fPlaneID; }
        virtual LKChannelAnalyzer* GetChannelAnalyzer(int id=0);

        virtual axis_t GetAxis1(int iPad=0) const { return fAxis1; }
        virtual axis_t GetAxis2(int iPad=0) const { return fAxis2; }
        virtual axis_t GetAxis3() const { return fAxis3; }
        virtual axis_t GetAxisDrift() const { return fAxisDrift; }
        double GetTbToLength() const { return fTbToLength; }
        double GetPosition() const { return fPosition; }

        void SetActive(bool val) { fActive = val; }
        bool IsActive() const { return fActive; }

    public:
        virtual void Clear(Option_t *option = "");
        virtual void Print(Option_t *option = "") const;
        /// Implementation recommanded.
        /// Init() method should implement pad information (id, position, neighbor) and add channel using AddChannel() or AddPad() method.
        /// All parameters should be configured such as axis-1,2,3, fPosition, fTbToLength from parameter container.
        virtual bool Init() { return true; }

        virtual TCanvas *GetCanvas(Option_t *option = ""); ///< Implementation recommanded for event display.
        virtual void Draw(Option_t *option = ""); ///< Implementation recommanded for event display. Draw event display to the canvas.
        virtual TH2* GetHist(Option_t *option = "-1") { return (TH2*) nullptr; }
        virtual void DrawFrame(Option_t *option = "") {}
        virtual void DrawHist(Option_t *option = "");
        virtual int GetNumCPads() { return 1; } ///< Get number of inner pads of canvas
        virtual TPad *GetCPad(int iPad) { return (TPad*) GetCanvas(); } ///< For grabbing inner pads of canvas
        virtual TPad* Get3DEventPad() { return (TPad*) nullptr; } ///< If user wants to place 3D event display other than default canvas, this method should return corresponding TPad.

        virtual double PadDisplacement() const { return 5; } ///< Return average pad displacement to each other
        virtual bool IsInBoundary(double i, double j) { return true; } ///< Implementation recommanded. Return if position (i,j) is inside the effective detector plane boundary
        virtual TVector3 GlobalToLocalAxis(TVector3 posGlobal) { return TVector3(); } ///< Implementation recommanded. Convert global position to local detector plane position 
        virtual TVector3 LocalToGlobalAxis(TVector3 posLocal) { return TVector3(); } ///< Implementation recommanded. Convert local position to global detector plane position 
        /// Implementation recommanded.
        /// Return position (x,y,z).
        /// x,y is pad position in pad plane.
        /// z is pad plane position of electron-drift-axis (for simulation)
        /// @param posGlobal[in] Given global position of the electron.
        /// @param posFinal[in] Final position of electron after full drift (at the pad plane).
        /// @param driftLength[out] Final drift length of the electron
        virtual void DriftElectron(TVector3 posGlobal, TVector3 &posFinal, double &driftLength);
        /// Implementation recommanded.
        /// Set poseReco to global position using pad position, and time-bin tb.
        /// Pad position must be set from Init().
        /// Conversion from time-bin (tb) to position in time-axis (post) is calculated by : post = fTbToLength * tb + fPosition
        /// [fTbToLength] is tb to position conversion factor with direction and [fPosition] is detector plane position in time-axis.
        /// @param pad[in] channel (pad)
        /// @param tb[in] Time-bin
        /// @param posReco[out] Reconstructed position of electron(?) before drift.
        /// @param driftLength[out] Drift length of the electron from the reconstructed position.
        virtual void DriftElectronBack(LKPad* pad, double tb, TVector3 &posReco, double &driftLength);
        virtual void DriftElectronBack(int padID, double tb, TVector3 &posReco, double &driftLength);
        /// Implementation recommanded.
        /// return drift length
        virtual double DriftElectronBack(double tb);
        virtual TVector3 GetPositionError(int padID) { return TVector3(1,1,1); }


    public:
        void AddChannel(LKChannel *channel) { fChannelArray -> Add(channel); } ///< Add channel to channel array in detector plane. For interanl use.
        void AddPad(LKPad *pad) { fChannelArray -> Add(pad); } ///< Add pad to pad array in detector plane. For interanl use.

        int GetNChannels() { return fChannelArray -> GetEntriesFast(); }
        int GetNumPads() { return GetNChannels(); }
        TObjArray *GetChannelArray() { return fChannelArray; }
        TObjArray *GetPadArray();

        virtual int FindChannelID(double i, double j) { return -1; } ///< Implementation recommanded. Find channel using position.
        virtual int FindChannelID(int section, int layer, int row) { return -1; } ///< Implementation recommanded. Find channel using section, layer, row info.
        virtual int FindPadID(double i, double j) { return FindChannelID(i,j); }
        virtual int FindPadID(int section, int layer, int row) { return FindChannelID(section,layer,row); }
        virtual int FindPadID(int cobo, int asad, int aget, int chan) { return -1; }
        LKChannel *GetChannelFast(int idx) { return (LKChannel *) fChannelArray -> At(idx); }
        LKPad *GetPadFast(int idx) { return (LKPad *) fChannelArray -> At(idx); }
        LKChannel *GetChannel(int idx);
        LKPad *GetPad(int padID);
        LKPad *GetPad(double i, double j);
        LKPad *GetPad(int section, int layer, int row);
        LKPad *GetPad(int cobo, int asad, int aget, int chan);

    public:
        virtual bool SetDataFromBranch(); ///< Implementation recommanded. Set waveform and hit data from input tree branches to pads
        virtual void FillDataToHist(); ///< Implementation recommanded. Fill data to histograms.

        void SetPadArray(TClonesArray *padArray);
        void SetHitArray(TClonesArray *hitArray);

        /// Find pad corresponding to the position (i,j) and fill time-bin by amount of charge.
        void FillPlane(double i, double j, double tb, double charge, int trackID = -1);
        void AddHit(LKHit *hit);

        virtual void ResetHitMap(); ///< For tracking use
        virtual void ResetEvent(); ///< For tracking use
        virtual LKHit *PullOutNextFreeHit(); ///< For tracking use
        void PullOutNeighborHits(vector<LKHit*> *hits, vector<LKHit*> *neighborHits); ///< For tracking use
        void PullOutNeighborHits(TVector2 p, int range, vector<LKHit*> *neighborHits); ///< For tracking use
        void PullOutNeighborHits(double x, double y, int range, vector<LKHit*> *neighborHits); ///< For tracking use
        void PullOutNeighborHits(double x, double y, int range, LKHitArray *neighborHits); ///< For tracking use
        void PullOutNeighborHits(LKHitArray *hits, LKHitArray *neighborHits); ///< For tracking use
        void GrabNeighborPads(vector<LKPad*> *pads, vector<LKPad*> *neighborPads); ///< For tracking use

    public:
        /// Self debugging tool. Result will be printed out.
        /// For each pad, check if pad found from FindPadID() using pad position of itself matches.
        bool PadPositionChecker(bool checkCorners = true);
        /// Self debugging tool. Result will be printed out.
        /// For each pad, check number of registered neighbor pads.
        bool PadNeighborChecker();
        /// Self debugging tool. Result will be printed out.
        /// For each pad, check
        /// 1) section, layer, row map
        /// 2) cobo, aget, asad, chan map
        bool PadMapChecker();

    ClassDef(LKDetectorPlane, 2)
};

#endif
