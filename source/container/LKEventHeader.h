#ifndef LKEVENTHEADER_HH
#define LKEVENTHEADER_HH

#include "LKContainer.h"

class LKEventHeader : public LKContainer
{
    public:
        LKEventHeader();
        virtual ~LKEventHeader() { ; }

        virtual void Clear(Option_t *option="");
        virtual void Print(Option_t *option="") const;
        virtual void Copy(TObject &object) const;

        void SetEventNumber(int eventNumber) { fEventNumber = eventNumber; }
        void SetIsGoodEvent(bool isGoodEvent) { fIsGoodEvent = isGoodEvent; }
        void SetBufferStart(size_t buffer) { fBufferStart = buffer; }
        void SetBufferSize(int size) { fBufferSize = size; }
        void SetEventTime(UInt_t time) { fEventTime = time; }

        bool IsGoodEvent() const { return fIsGoodEvent; }
        int GetEventNumber() const { return fEventNumber; }
        size_t GetBufferStart() const { return fBufferStart; }
        int GetBufferSize() const { return fBufferSize; }
        UInt_t GetEventTime() const { return fEventTime; }

        bool fIsGoodEvent = false;
        int fEventNumber = -1;
        size_t fBufferStart = -1;
        int fBufferSize = -1;
        UInt_t fEventTime = 0;
        double fMTETimeStamp = 0;

    ClassDef(LKEventHeader,2);
};

#endif
