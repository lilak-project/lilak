#ifndef LKSILICONMAPPING_HH
#define LKSILICONMAPPING_HH

#include "TNamed.h"
#include "TString.h"

#include <unordered_map>
#include <vector>

class LKSiliconMapping : public TNamed
{
    public:
        struct DetectorInfo
        {
            TString detType = "";
            int detIndex = -1;
            int detNumber = -1;
            double detThickness = 0;
            double detWidth = 0;
            double detHeight = 0;
            int ringNumber = -1;
            TString ringType = "";
            double ringRadius = 0;
            double ringZ = 0;
            TString dEE = "";
            int phiNumber = -1;
            double phi = 0;
            double phi1 = 0;
            double phi2 = 0;

            int cobo = -1;
            int asad = -1;
            TString zapJNo = "";
            TString zapONo = "";
            TString markNo = "";
            int markID = -1;
            int fb = -1;
            int polarID = -1;
            bool isLegacy = false;

            /// one-line dump of every detector field
            TString Summary() const;
            /// option (case insensitive):
            ///   ""/"all"  : full block, all detector fields + the channel list (cobo/asad/aget/chan/side/strip)
            ///   "line"    : the one-line Summary()
            ///   "nochannel" : full block without the channel list
            void Print(Option_t *option="") const;
            const LKSiliconMapping *GetMapping() const { return fMapping; }

            private:
                const LKSiliconMapping *fMapping = nullptr; //! set by LKSiliconMapping when the mapping is loaded
                friend class LKSiliconMapping;
        };

        struct ChannelInfo
        {
            int channelIndex = -1;
            int cobo = -1;
            int asad = -1;
            int aget = -1;
            int chan = -1;
            int detIndex = -1;
            int detNumber = -1;
            int phiNumber = -1;
            int side = -1;
            int strip = -1;

            int chan2 = -1;
            int lr = -1;
            TString detType = "";
            double detRadius = 0;
            double detDistance = 0;
            bool isLegacy = false;

            /// one-line dump of every channel field
            TString Summary() const;
            /// option (case insensitive): ""/"line" one line, "det"/"all" also print the detector it belongs to
            void Print(Option_t *option="") const;
            const LKSiliconMapping *GetMapping() const { return fMapping; }
            const DetectorInfo *GetDetector() const;

            private:
                const LKSiliconMapping *fMapping = nullptr; //! set by LKSiliconMapping when the mapping is loaded
                friend class LKSiliconMapping;
        };

    public:
        LKSiliconMapping();
        LKSiliconMapping(const LKSiliconMapping &other);
        LKSiliconMapping &operator=(const LKSiliconMapping &other);
        virtual ~LKSiliconMapping() {}

        bool Load(TString mappingPath);
        bool Load(TString detectorFileName, TString channelFileName);
        bool LoadDetectorMapping(TString fileName);
        bool LoadChannelMapping(TString fileName);
        void Clear(Option_t *option="");
        /// Print everything that is known about the loaded mapping.
        /// option (case insensitive, may contain several keywords):
        ///   "summary"  : file names, styles and counters only
        ///   "detector" : detector table (all detector fields)
        ///   "channel"  : channel table (all channel fields, incl. cobo/asad/aget/chan)
        ///   "group"    : channels listed under the detector they belong to
        ///   "all"      : summary + detector + channel  (default)
        void Print(Option_t *option="") const;
        void PrintSummary() const;
        void PrintDetectors(int numPrint = -1) const;
        /// full block of one detector (same as FindDetectorByIndex(detIndex)->Print())
        void PrintDetector(int detIndex) const;
        void PrintChannels(int numPrint = -1) const;
        /// Channels grouped by detector. detIndex<0 prints all detectors.
        void PrintDetectorChannels(int detIndex = -1) const;
        void PrintChannelLookup(int cobo, int asad, int aget, int chan) const;
        void PrintTest(int queryCobo = 0, int queryAsad = 0, int queryAget = 3, int queryChan = 0, int numPrintDetectors = 20, int numPrintChannels = 20) const;

        int GetNumDetectors() const { return fDetectors.size(); }
        int GetNumChannels() const { return fChannels.size(); }
        int GetMaxDetectorNumber() const { return fMaxDetectorNumber; }
        bool IsNewDetectorStyle() const { return fDetectorStyleIsNew; }
        bool IsNewChannelStyle() const { return fChannelStyleIsNew; }

        const DetectorInfo *GetDetectorByVectorIndex(int index) const;
        const DetectorInfo *FindDetectorByIndex(int detIndex) const;
        const DetectorInfo *FindDetectorByNumber(int detNumber) const;
        const ChannelInfo *GetChannelByVectorIndex(int index) const;
        const ChannelInfo *FindChannel(int cobo, int asad, int aget, int chan) const;

        const std::vector<int> &GetChannelVectorIndicesOfDetector(int detIndex) const;
        int GetNumChannelsOfDetector(int detIndex) const;
        /// cobo/asad the detector is read out by, taken from its channels (-1 if unknown/mixed is resolved to the first one found)
        bool GetDetectorElectronics(int detIndex, int &cobo, int &asad) const;

        int FindChannelIndex(int cobo, int asad, int aget, int chan) const;
        int FindDetectorIndex(int cobo, int asad, int aget, int chan) const;
        int FindDetectorNumber(int cobo, int asad, int aget, int chan) const;

    private:
        TString FindFirstMappingFile(TString mappingPath, TString suffix) const;
        bool ParseNewDetectorRow(const std::vector<TString> &columns, DetectorInfo &info) const;
        bool ParseLegacyDetectorRow(const std::vector<TString> &columns, DetectorInfo &info) const;
        bool ParseNewChannelRow(const std::vector<TString> &columns, ChannelInfo &info) const;
        bool ParseLegacyChannelRow(const std::vector<TString> &columns, ChannelInfo &info, int rowIndex) const;
        void CopyFrom(const LKSiliconMapping &other);
        void RebuildLookupTables();
        void RebuildDetectorChannelTable();
        void RebuildOwnerPointers();
        Long64_t MakeChannelKey(int cobo, int asad, int aget, int chan) const;
        bool HasLegacyDetector() const;
        bool HasLegacyChannel() const;

    private:
        TString fDetectorMappingFileName = "";
        TString fChannelMappingFileName = "";
        bool fDetectorStyleIsNew = false;
        bool fChannelStyleIsNew = false;
        int fMaxDetectorNumber = -1;

        std::vector<DetectorInfo> fDetectors;
        std::vector<ChannelInfo> fChannels;
        std::vector<int> fDetectorVectorIndexByDetIndex;
        std::vector<int> fDetectorVectorIndexByDetNumber;
        std::unordered_map<Long64_t, int> fChannelVectorIndexByCAAC;
        std::vector<std::vector<int>> fChannelVectorIndicesByDetIndex;
        std::vector<int> fEmptyChannelIndices;

    ClassDef(LKSiliconMapping, 1);
};

#endif
