// Checks LKGETFrameFileIndex (the frame table shared with the Scope viewer) on frames built here.
// Run: root -l -b -q macros/test_get_frame_file_index.C
#include "get_convter/LKGETFrameFileIndex.h"

namespace {
// CoBo full-readout frame: 256-byte blocks, big endian, n 4-byte items, event id at bytes 22..25.
std::vector<uint8_t> CoboFrame(uint32_t eventId, uint32_t n)
{
    std::vector<uint8_t> b(256, 0);
    b[0] = 0x08;
    const uint32_t blocks = (256 + 4 * n) / 256;
    b[1] = blocks >> 16; b[2] = blocks >> 8; b[3] = blocks;
    b[6] = 1;            // frame type 1
    b[9] = 1;            // header: 1 block
    b[11] = 4;           // item size
    b[12] = n >> 24; b[13] = n >> 16; b[14] = n >> 8; b[15] = n;
    b[22] = eventId >> 24; b[23] = eventId >> 16; b[24] = eventId >> 8; b[25] = eventId;
    b.resize(256 + 4 * n, 0);
    return b;
}
// Merged (layered 0xFF01) frame, little endian, 1-byte blocks, 24-byte header.
std::vector<uint8_t> MergedFrame(uint32_t eventId, const std::vector<std::vector<uint8_t>>& children)
{
    std::vector<uint8_t> b(24, 0);
    for (const auto& c : children) b.insert(b.end(), c.begin(), c.end());
    const uint32_t size = b.size();
    b[1] = size; b[2] = size >> 8; b[3] = size >> 16;
    b[5] = 0x01; b[6] = 0xFF;
    b[8] = 24;
    b[12] = children.size();
    b[16] = eventId; b[17] = eventId >> 8; b[18] = eventId >> 16; b[19] = eventId >> 24;
    return b;
}
}

void test_get_frame_file_index()
{
    const std::string source = std::string(gSystem->TempDirectory()) + Form("/lilak_test_frames_%d.graw", gSystem->GetPid());
    auto cobo = CoboFrame(7, 64);
    auto merged = MergedFrame(9, {CoboFrame(9, 64), CoboFrame(9, 64)});
    {
        std::ofstream out(source, std::ios::binary);
        out.write((const char*) cobo.data(), cobo.size());
        out.write((const char*) merged.data(), merged.size());
        out.write((const char*) cobo.data(), 100); // last frame cut off
    }

    bool ok = true;
    auto check = [&](bool condition, const char* what) { if (!condition) { cout << "FAIL " << what << endl; ok = false; } };

    std::vector<LKGETFrameFileIndex::Record> records;
    uint64_t cutAt = 0;
    std::string error;
    check(LKGETFrameFileIndex::Scan(source, records, cutAt, error), "scan");
    check(records.size() == 2, "two whole frames");
    if (records.size() == 2) {
        check(records[0].offset == 0 && records[0].size == 512 && records[0].frameType == 1 && records[0].eventId == 7, "CoBo frame record");
        check(records[1].offset == 512 && records[1].size == merged.size() && records[1].frameType == 0xFF01 && records[1].eventId == 9, "merged frame record");
    }
    check(cutAt == 512 + merged.size(), "cut-off frame found");

    uint64_t size = 0, mtimeNs = 0;
    check(LKGETFrameFileIndex::Fingerprint(source, size, mtimeNs), "fingerprint");
    check(LKGETFrameFileIndex::Save(source, size, mtimeNs, records), "save");
    std::vector<LKGETFrameFileIndex::Record> loaded;
    check(LKGETFrameFileIndex::Load(source, size, mtimeNs, loaded) && loaded.size() == records.size()
            && loaded[1].offset == records[1].offset && loaded[1].eventId == 9, "load what was saved");
    check(!LKGETFrameFileIndex::Load(source, size + 1, mtimeNs, loaded), "a changed source is not loaded");

    gSystem->Unlink(source.c_str());
    gSystem->Unlink(LKGETFrameFileIndex::PathFor(source).c_str());
    cout << (ok ? "test_get_frame_file_index: OK" : "test_get_frame_file_index: FAILED") << endl;
    if (!ok) gSystem->Exit(1);
}
