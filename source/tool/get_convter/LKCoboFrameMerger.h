#ifndef LKCOBOFRAMEMERGER_HH
#define LKCOBOFRAMEMERGER_HH

#include "TObject.h"
#include "TString.h"

#include "LKGETRawFrame.h"

#include <cstdint>
#include <map>
#include <vector>

/**
 * In-memory equivalent of GET's cobo-frame-merge tool.
 *
 * cobo-frame-merge reads basic CoBo frames (frame type 0x1/0x2) from several
 * input files, groups them by event identifier, and writes one layered frame
 * (frame type 0xFF01) per event gathering the frames that share the same id.
 * LKCoboFrameMergerTask then needs the very same layered frames to feed
 * LKGETRawConverter, so that online merging removes the need for a pre-merged
 * file on disk.
 *
 * This class performs the grouping in a streaming fashion. Each input is a
 * "source" (one CoBo file or one online buffer); within a single source the
 * frames arrive in non-decreasing event-id order. The merger therefore keeps a
 * per-source high-water-mark (the largest event id seen so far) and a low-water
 * mark = min(high-water) over all still-open sources. An event id E is final,
 * i.e. no source can deliver another frame for it, once E < lowWaterMark, and
 * its frames can be flushed as a layered frame. When every source is closed the
 * remaining events are flushed in ascending id order.
 */
class LKCoboFrameMerger : public TObject
{
  public:
    LKCoboFrameMerger();
    virtual ~LKCoboFrameMerger() {}

    /// Forget all sources and pending frames.
    void Clear();

    /// Declare how many input sources will feed the merger.
    void SetNumSources(Int_t numSources);
    Int_t GetNumSources() const { return fNumSources; }

    /// Append one parsed basic CoBo frame (type 0x1/0x2) coming from sourceId.
    void AddFrame(Int_t sourceId, const LKGETRawFrame& frame);

    /// Mark a source as finished (end-of-file or disconnected); its frames no
    /// longer constrain the low-water-mark.
    void CloseSource(Int_t sourceId);

    /// Pop the smallest completed event into merged as a layered frame.
    /// Returns false when no event is ready yet (caller should read more data).
    bool PopReadyEvent(LKGETRawFrame& merged);

    Bool_t HasPending() const { return !fPending.empty(); }
    size_t NumPending() const { return fPending.size(); }

  private:
    void BuildLayeredFrame(uint32_t eventId, std::vector<LKGETRawFrame>& children, LKGETRawFrame& merged) const; //!

    TString fName = "LKCoboFrameMerger";                          //!
    Int_t fNumSources = 0;                                        //!
    std::vector<Bool_t> fSourceOpen;                              //!
    std::vector<Bool_t> fSourceSeen;                              //!
    std::vector<uint32_t> fSourceHighWater;                       //!
    std::map<uint32_t, std::vector<LKGETRawFrame>> fPending;      //!

    ClassDef(LKCoboFrameMerger, 1);
};

#endif
