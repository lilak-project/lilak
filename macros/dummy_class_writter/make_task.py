from lilakcc import lilakcc

###################################################################################################
lilakcc(
"""
+class TTHitFindingTask

+idata  fChannelArray
+bclass MMChannel 200
+bname  RawData

+odata  fHitArray
+bclass LKHit 200
+bname  Hit

+kdata  fEventHeader
+bclass TTEventHeader
+bname  EventHeader
"""
).print_task()

###################################################################################################
lilakcc(
"""
+class TTRootConversionTask
/// Simple conversion from pre-converted root file

+enum eType
    kNon
    kLeftStrip   // 0
    kRightStrip  // 1
    kLeftChain   // 2
    kRightChain  // 3
    kLowCenter   // 4
    kHighCenter  // 5
    kForwardSi   // 6
    kForwardCsI  // 7
    kMMJr        // 8
    kCENSX6      // 10
    kCENSCsI     // 11
    kExternal    // 100
+enum eDetLoc
    kNon
    kLeft          // 0
    kRight         // 1
    kCenterFront   // 2
    kBottomLeftX6  // 10
    kBottomRightX6 // 11
    kCsI           // -1

+odata  fChannelArray
+bclass MMChannel 200
+bname  RawData

-private TFile* fInputFile;
-private TTree* fInputTree;
-private Int_t   mmMult;
-private Int_t   mmHit;
-private Int_t   mmEventIdx;
-private Int_t   mmFrameNo[1030];   //[mmMult]
-private Int_t   mmDecayNo[1030];   //[mmMult]
-private Int_t   mmCobo[1030];   //[mmMult]
-private Int_t   mmAsad[1030];   //[mmMult]
-private Int_t   mmAget[1030];   //[mmMult]
-private Int_t   mmChan[1030];   //[mmMult]
-private Float_t mmTime[1030];   //[mmMult]
-private Float_t mmEnergy[1030];   //[mmMult]
-private Int_t   mmWaveformX[1030][512];   //[mmMult][time]
-private Int_t   mmWaveformY[1030][512];   //[mmMult][time]
-private eType fType[3][4][4][68];
-private eDetLoc fDetLoc[3][4][4][68];
-private const Int_t mmnum = 1024; // # of all channels
-private const Int_t sinum = 45; // quadrant*9
-private const Int_t X6num = 600; // 20chan*30det
-private const Int_t CsInum = 64; // 1chan*64det
@private TString fInputFileName = /home/ejungwoo/data/texat/run_0824.dat.19-03-23_23h42m36s.38.root 
@private TString fmapmmFileName = /home/ejungwoo/data/txtfiles/mapchantomm.txt
@private TString fmapsiFileName = /home/ejungwoo/data/txtfiles/mapchantosi_CRIB.txt
@private TString fmapX6FileName = /home/ejungwoo/data/txtfiles/mapchantoX6_CRIB.txt
@private TString fmapCsIFileName = /home/ejungwoo/data/txtfiles/mapchantoCsI_CRIB.txt 
"""
).print_task()
