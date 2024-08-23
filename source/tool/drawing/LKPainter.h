#ifndef LKWINDOWMANAGER_HH
#define LKWINDOWMANAGER_HH

#include "TObject.h"
#include "LKLogger.h"
#include "TCanvas.h"

#define e_painter()                             LKPainter::GetPainter()
#define e_cvs(name)                             LKPainter::GetPainter() -> CanvasDefault(name);
#define e_cvs_full(name,ratio)                  LKPainter::GetPainter() -> CanvasFull(name,ratio);
#define e_cvs_square(name,ratio)                LKPainter::GetPainter() -> CanvasSquare(name,ratio);
#define e_cvs_default(name,ratio)               LKPainter::GetPainter() -> CanvasDefault(name,ratio);
#define e_cvs_resize(name,width0,height0,ratio) LKPainter::GetPainter() -> CanvasResize(name, width0, height0,ratio);

class LKPainter : public TObject
{
    public:
        LKPainter(bool useConfiguration=true);
        virtual ~LKPainter() { ; }
        static LKPainter* GetPainter(bool useConfiguration=true);

        bool Init(bool useConfiguration=true);
        void Clear(Option_t *option="");
        void Print(Option_t *option="") const;

        int  GetXCurrentDisplay() const  { return fXCurrentDisplay; }
        int  GetYCurrentDisplay() const  { return fYCurrentDisplay; }
        UInt_t GetWCurrentDisplay() const  { return fWCurrentDisplay; }
        UInt_t GetHCurrentDisplay() const  { return fHCurrentDisplay; }

        double GetResizeFactor() const { return fResizeFactor; }

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
        void SetWCanvas (UInt_t wCanvas)  { fWDefaultOrigin = wCanvas; fWDefault = fWDefaultOrigin * fResizeFactor; }
        void SetHCanvas (UInt_t hCanvas)  { fHDefaultOrigin = hCanvas; fHDefault = fHDefaultOrigin * fResizeFactor; }
        void SetWSpacing(UInt_t dwCanvas) { fWSpacing = dwCanvas; }
        void SetHSpacing(UInt_t dhCanvas) { fHSpacing = dhCanvas; }

        double SetRatio(double ratio, double defaultValue=1);
        void UpdateNextCanvasPosition();
        void FixCanvasPosition() { fFixCanvasPosition = true; }
        TString ConfigureName(TString name);
        TCanvas *NewCanvas(TString name, TString title, int x, int y, int width, int height);

        /**
         * mode = 0 (kDefault)    : default canvas with fWDefault x fHDefault (600 x 450).
         * mode = 1 (kFull)       : full size canvas that fits in the current display.
         * mode = 2 (kSquare)     : square canvas.
         * mode = 3 (kResize)     : resize canvas from given w,h = (value1, value2) to show similar scale in the current display.
         */
        TCanvas *Canvas          (TString name="cvs_lilak", int mode=0, double value1=1, double value2=-1, double value3=-1);
        TCanvas *CanvasDefault   (TString name="cvs_lilak", double ratio=1);
        TCanvas *CanvasFull      (TString name="cvs_lilak", double ratio=1, double ratio2=-1);
        TCanvas *CanvasSquare    (TString name="cvs_lilak", double ratio=1);
        TCanvas *CanvasResize    (TString name, int width0, int height0, double ratio=-1);

        void GetSizeDefault  (int &width, int &height, double ratio=1);
        void GetSizeFull     (int &width, int &height, double ratio=1, double ratio2=-1);
        void GetSizeSquare   (int &width, int &height, double ratio=1);
        void GetSizeResize   (int &width, int &height, int width0, int height0, double ratio=-1);

        const int kDefault    = 0;
        const int kFull       = 1;
        const int kSquare     = 2;
        const int kResize     = 3;

        bool fSkipConfiguration = false;

    private:
        static LKPainter* fInstance;

        Bool_t fFixCanvasPosition = true;
        UInt_t fDeadFrameSize[4]  = {25,25,80,50};
        UInt_t fWCurrentDisplay   = 0; /// width  of current display
        UInt_t fHCurrentDisplay   = 0; /// height of current display
        int    fXCurrentDisplay   = -1; /// relative width  position of current display from main display
        int    fYCurrentDisplay   = -1; /// relative height position of current display from main display
        int    fXCurrentCanvas    = 0;  /// width  position of next canvas
        int    fYCurrentCanvas    = 0;  /// height position of next canvas
        int    fWDefaultOrigin    = 700; /// default width  of canvas assuming that width of the display is 1250
        int    fHDefaultOrigin    = 500; /// default height of canvas assuming that width of the display is 1250
        int    fCountCanvases     = 0;
        int    fWDefault          = fWDefaultOrigin; /// default width  of canvas
        int    fHDefault          = fHDefaultOrigin; /// default height of canvas
        int    fWSpacing          = 25; /// default width  spacing of canvas
        int    fHSpacing          = 25; /// default height spacing of canvas
        double fResizeFactor      = 1;

    ClassDef(LKPainter,1);
};

#endif
