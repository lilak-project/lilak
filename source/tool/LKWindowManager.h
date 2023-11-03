#ifndef LKWINDOWMANAGER_HH
#define LKWINDOWMANAGER_HH

#include "TObject.h"
#include "LKLogger.h"
#include "TCanvas.h"

#define lk_canvas(name)      LKWindowManager::GetWindowManager() -> Canvas(name,0)
#define lk_full_canvas(name) LKWindowManager::GetWindowManager() -> Canvas(name,1)

class LKWindowManager : public TObject
{
    public:
        LKWindowManager();
        virtual ~LKWindowManager() { ; }
        static LKWindowManager* GetWindowManager();

        bool Init();
        void Clear(Option_t *option="");
        void Print(Option_t *option="") const;

        Int_t  GetXCurrentDisplay() const  { return fXCurrentDisplay; }
        Int_t  GetYCurrentDisplay() const  { return fYCurrentDisplay; }
        UInt_t GetWCurrentDisplay() const  { return fWCurrentDisplay; }
        UInt_t GetHCurrentDisplay() const  { return fHCurrentDisplay; }

        void SetDeadFrameLeft  (UInt_t val);
        void SetDeadFrameRight (UInt_t val);
        void SetDeadFrameBottom(UInt_t val);
        void SetDeadFrameTop   (UInt_t val);
        void SetDeadFrame(UInt_t left, UInt_t right, UInt_t bottom, UInt_t top) {
            SetDeadFrameLeft(left);
            SetDeadFrameRight(right);
            SetDeadFrameBottom(bottom);
            SetDeadFrameTop(top);
        }
        void SetWCanvas (UInt_t wCanvas)  { fWDefaultOrigin = wCanvas; fWDefault = fWDefaultOrigin * fGeneralResizeFactor; }
        void SetHCanvas (UInt_t hCanvas)  { fHDefaultOrigin = hCanvas; fHDefault = fHDefaultOrigin * fGeneralResizeFactor; }
        void SetWSpacing(UInt_t dwCanvas) { fWSpacing = dwCanvas; }
        void SetHSpacing(UInt_t dhCanvas) { fHSpacing = dhCanvas; }

        /**
         * mode = 0 (kDefault)    : default canvas with fWDefault x fHDefault (600 x 450).
         * mode = 1 (kFull)       : full size canvas that fits in the current display.
         * mode = 2 (kFullRatio)  : full size canvas, size scaled by value1.
         * mode = 3 (kSquare)     : square canvas.
         * mode = 4 (kFullSquare) : full size square canvas.
         * mode = 5 (kResize)     : resize canvas from given w,h = (value1, value2) to show similar scale in the current display.
         */
        TCanvas *Canvas          (const char *name, Int_t mode=0, Double_t value1=-1, Double_t value2=-1);
        TCanvas *CanvasDefault   (const char* name, const char* title);
        TCanvas *CanvasFull      (const char* name, const char* title);
        TCanvas *CanvasFullRatio (const char* name, const char* title, Double_t ratio);
        TCanvas *CanvasSquare    (const char* name, const char* title);
        TCanvas *CanvasFullSquare(const char* name, const char* title, Double_t ratio);
        TCanvas *CanvasResize    (const char* name, const char* title, Int_t width0, Int_t height0);

        const Int_t kDefault    = 0;
        const Int_t kFull       = 1;
        const Int_t kFullRatio  = 2;
        const Int_t kSquare     = 3;
        const Int_t kFullSquare = 4;
        const Int_t kResize     = 5;

    private:
        void ConfigureDisplay();

    private:
        static LKWindowManager* fInstance;

        Int_t        fXCurrentDisplay = -1; /// relative width  position of current display from main display
        Int_t        fYCurrentDisplay = -1; /// relative height position of current display from main display

        UInt_t       fWCurrentDisplay = 0; /// width  of current display
        UInt_t       fHCurrentDisplay = 0; /// height of current display

        UInt_t       fDeadFrameSize[4] = {0,10,10,25}; /// height of the top bar

        Int_t        fXCurrentCanvas = 0;  /// width  position of next canvas
        Int_t        fYCurrentCanvas = 0;  /// height position of next canvas

        Double_t     fGeneralResizeFactor = 1;

        Int_t        fWDefaultOrigin = 700; /// default width  of canvas assuming that width of the display is 1250
        Int_t        fHDefaultOrigin = 500; /// default height of canvas assuming that width of the display is 1250

        Int_t        fWDefault = fWDefaultOrigin; /// default width  of canvas
        Int_t        fHDefault = fHDefaultOrigin; /// default height of canvas

        Int_t        fWSpacing = 25; /// default width  spacing of canvas
        Int_t        fHSpacing = 25; /// default height spacing of canvas

    ClassDef(LKWindowManager,1);
};

#endif
