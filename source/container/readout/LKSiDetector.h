#ifndef LKSiDetector_HH
#define LKSiDetector_HH

#include "LKContainer.h"
#include "LKSiChannel.h"

class LKSiDetector : public LKContainer
{
    public:
        LKSiDetector();
        virtual ~LKSiDetector() { ; }

        virtual void Clear(Option_t *option="");
        virtual void Copy(TObject &object) const;
        virtual void Print(Option_t *option="") const;
        virtual void Draw(Option_t *option="");

        virtual TObject *Clone(const char *newname="") const;

        virtual const char* GetName() const;
        virtual const char* GetTitle() const;

        TString GetNameType() const;
        TString GetTitleType() const;

        TString GetDetTypeName() const  { return fDetTypeName; }
        int GetDetType() const  { return fDetType; }
        int GetDetIndex() const  { return fDetIndex; }
        int GetDetID() const  { return fDetID; }
        int GetLayer() const  { return fLayer; }
        int GetRow() const  { return fRow; }
        TVector3 GetPosition() const { return fPosition; }
        double GetPhi1() const { return fPhi1; }
        double GetPhi2() const { return fPhi2; }
        double GetTheta1() const { return fTheta1; }
        double GetTheta2() const { return fTheta2; }
        int GetNumSides() const { return fNumSides; }
        int GetNumJunctionStrips() const { return fNumJunctionStrips; }
        int GetNumOhmicStrips() const { return fNumOhmicStrips; }
        int GetNumJunctionDirection() const { return fNumJunctionUD; }
        int GetNumOhmicDirection() const { return fNumOhmicLR; }
        bool GetUseJunctionUD() const { return (fNumJunctionUD==2); }
        bool GetUseOhmicLR() const { return (fNumOhmicLR==2); }
        int GetNumJunctionChannels() const { return GetNumJunctionStrips() * GetNumJunctionDirection(); }
        int GetNumOhmicChannels() const { return GetNumOhmicStrips() * GetNumOhmicDirection(); }
        int GetNumChannels() const { return GetNumJunctionChannels() * GetNumOhmicChannels(); }

        void SetDetTypeName(TString name) { fDetTypeName = name; }
        void SetDetType(int type) { fDetType = type; }
        void SetDetIndex(int detIndex) { fDetIndex = detIndex; }
        void SetDetID(int detID) { fDetID = detID; }
        void SetPosition(TVector3 pos) { fPosition = pos; }
        void SetLayer(int layer) { fLayer = layer; }
        void SetRow(int row) { fRow = row; }
        void SetPhi1(double phi) { fPhi1 = phi; }
        void SetPhi2(double phi) { fPhi2 = phi; }
        void SetTheta1(double theta) { fTheta1 = theta; }
        void SetTheta2(double theta) { fTheta2 = theta; }

        virtual void ClearData();
        /**
         * Initialize Si detector setting
         * @param detTypeName detector type name (s1, s3, x6, etc.)
         * @param detType detector type index
         * @param detIndex detector index numbered from 0
         * @param detID detector id numbered for users
         * @param numSides 1 for single side, 2 for both side (junction, ohmic)
         * @param numJunctionStrips number of strips in junction side
         * @param numOhmicStrips number of strips in ohmic side
         * @param useJunctionLR true if up and down channels are used for junction side
         * @param useOhmicLR true if left and right channels are used for ohmic side
         */
        void SetSiType(TString detTypeName, int detType, int detIndex, int detID, int numSides, int numJunctionStrips, int numOhmicStrips, bool useJunctionLR, bool useOhmicLR);
        void SetSiPosition(TVector3 position, int layer, int row, double phi1, double phi2, double theta1, double theta2);
        void SetChannel(LKSiChannel* channel);
        void AddChannel(LKSiChannel* channel);
        void SetChannel(GETChannel* channel, int side, int strip, int lr);
        void AddChannel(GETChannel* channel, int side, int strip, int lr);
        int GetNumActiveChannels() const { return fChannelArray.size(); }
        LKChannel* GetActiveChannel(int i) { return fChannelArray.at(i); }

        TH2* CreateHistJunction(TString name="", TString title="", double x1=-1, double x2=-1, double y1=-1, double y2=-1, TString option="");
        TH2* CreateHistOhmic   (TString name="", TString title="", double x1=-1, double x2=-1, double y1=-1, double y2=-1, TString option="");
        TH2* GetHistJunction() { return fHistJunction; }
        TH2* GetHistOhmic() { return fHistOhmic; }
        void FillHistEnergy();
        void FillHistEnergySum();
        void FillHistCount();

    protected:
        TString fDetTypeName;
        int fDetType = -1; ///< Si detector type (user defined index corresponding to s1, s3, x6...)
        int fDetIndex = -1; ///< Si detector index starting from 0
        int fDetID = -1; ///< Si detector id (user defined index to physical si detector)
        int fLayer = -1;
        int fRow = -1;
        TVector3 fPosition;
        double fPhi1 = 0;
        double fPhi2 = 0;
        double fTheta1 = 0;
        double fTheta2 = 0;

        int fNumSides = 0;
        int fNumJunctionStrips = 0;
        int fNumOhmicStrips = 0;
        int fNumJunctionUD = 0;
        int fNumOhmicLR = 0;

        double ***fEnergyArray; //!< (side, strip, left/right)
        double ***fEnergySumArray; //!< (side, strip, left/right)
        int ***fCountArray; //!< (side, strip, left/right)
        int ***fIdxArray; //!< idx of fChannelArray (side, strip, left/right)
        vector<GETChannel*> fChannelArray;

        TH2* fHistJunction = nullptr;
        TH2* fHistOhmic = nullptr;

    ClassDef(LKSiDetector,1);
};

#endif
