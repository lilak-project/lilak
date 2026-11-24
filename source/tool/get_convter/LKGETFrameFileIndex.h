#ifndef LKGETFRAMEFILEINDEX_HH
#define LKGETFRAMEFILEINDEX_HH

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

/**
 * Frame table of one raw GET file (.graw, .dat, .mfm), shared with the Scope viewer
 * (~/web_service/scope, src/raw.rs) so that whichever reads a file first saves the work.
 *
 * Stored beside the source as the hidden file ".<file name>.scope_index":
 *   "GETIDX03" | source size u64 | source modification time in ns u64 |
 *   one 22-byte record per frame: offset u64, size u64, event id u32, frame type u16
 * All numbers little endian. Every frame of the file is listed, in file order; a reader skips
 * the frame types it does not use. The event id is set for CoBo (1, 2) and merged (0xFF01,
 * 0xFF02) frames and 0 otherwise. The table is valid while the source keeps its size and
 * modification time; a file cut off in the middle of a frame is not indexed.
 */
class LKGETFrameFileIndex
{
  public:
    struct Record
    {
        uint64_t offset = 0;
        uint64_t size = 0;
        uint32_t eventId = 0;
        uint16_t frameType = 0;
    };

    static std::string PathFor(const std::string& source)
    {
        const auto slash = source.find_last_of('/');
        if (slash == std::string::npos)
            return "." + source + ".scope_index";
        return source.substr(0, slash + 1) + "." + source.substr(slash + 1) + ".scope_index";
    }

    /// Size and modification time (ns) of a file; false if it cannot be read.
    static bool Fingerprint(const std::string& path, uint64_t& size, uint64_t& mtimeNs)
    {
        struct stat status = {};
        if (::stat(path.c_str(), &status) != 0)
            return false;
        size = static_cast<uint64_t>(status.st_size);
#if defined(__APPLE__)
        mtimeNs = uint64_t(status.st_mtimespec.tv_sec) * 1000000000ULL + status.st_mtimespec.tv_nsec;
#else
        mtimeNs = uint64_t(status.st_mtim.tv_sec) * 1000000000ULL + status.st_mtim.tv_nsec;
#endif
        return true;
    }

    /// Reads the table saved for a source of this size and time; false if there is none or it is stale.
    static bool Load(const std::string& source, uint64_t size, uint64_t mtimeNs, std::vector<Record>& records)
    {
        records.clear();
        std::ifstream input(PathFor(source), std::ios::binary);
        char head[24];
        if (!input.read(head, sizeof(head)) || std::memcmp(head, "GETIDX03", 8) != 0
                || Get(head + 8, 8) != size || Get(head + 16, 8) != mtimeNs)
            return false;
        char b[22];
        while (input.read(b, sizeof(b))) {
            Record r;
            r.offset = Get(b, 8);
            r.size = Get(b + 8, 8);
            r.eventId = static_cast<uint32_t>(Get(b + 16, 4));
            r.frameType = static_cast<uint16_t>(Get(b + 20, 2));
            if (r.size < 8 || r.offset + r.size < r.offset || r.offset + r.size > size) {
                records.clear();
                return false;
            }
            records.push_back(r);
        }
        if (input.gcount() != 0) { // a record cut short
            records.clear();
            return false;
        }
        return true;
    }

    /// Writes the table beside the source (through a temporary file); false if the folder is not writable.
    static bool Save(const std::string& source, uint64_t size, uint64_t mtimeNs, const std::vector<Record>& records)
    {
        const auto path = PathFor(source);
        const auto temporary = path + ".tmp." + std::to_string(::getpid());
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output)
                return false;
            output.write("GETIDX03", 8);
            Put(output, size, 8);
            Put(output, mtimeNs, 8);
            for (const auto& r : records) {
                Put(output, r.offset, 8);
                Put(output, r.size, 8);
                Put(output, r.eventId, 4);
                Put(output, r.frameType, 2);
            }
            if (!output.good()) {
                ::unlink(temporary.c_str());
                return false;
            }
        }
        if (std::rename(temporary.c_str(), path.c_str()) != 0) {
            ::unlink(temporary.c_str());
            return false;
        }
        ::chmod(path.c_str(), 0664); // data folders are shared by the group
        return true;
    }

    /**
     * Lists the frames of a source from their headers only, the way Scope's raw.rs (header,
     * read_record, index) does, so both write the same table. A frame cut off by the end of the
     * file ends the list and cutAt is set to its offset (0 for a whole file); such a list is not
     * to be saved. False with error set on a header that cannot be read.
     */
    static bool Scan(const std::string& source, std::vector<Record>& records, uint64_t& cutAt, std::string& error)
    {
        records.clear();
        cutAt = 0;
        uint64_t length = 0, mtimeNs = 0;
        std::ifstream input(source, std::ios::binary);
        if (!input || !Fingerprint(source, length, mtimeNs)) {
            error = "cannot open " + source;
            return false;
        }
        uint8_t b[28];
        for (uint64_t pos = 0; pos < length;) {
            const auto n = static_cast<size_t>(std::min<uint64_t>(28, length - pos));
            input.seekg(static_cast<std::streamoff>(pos));
            if (!input.read(reinterpret_cast<char*>(b), n)) {
                error = "cannot read " + source;
                return false;
            }
            Header h;
            if (!ReadHeader(b, n, h, error)) {
                error += " at byte " + std::to_string(pos) + " of " + source;
                return false;
            }
            if (h.size > length - pos) {
                if (pos == 0) {
                    error = "truncated frame at byte 0 of " + source;
                    return false;
                }
                cutAt = pos;
                return true;
            }
            Record r;
            r.offset = pos;
            r.size = h.size;
            r.frameType = h.kind;
            if (h.kind == 1 || h.kind == 2)
                r.eventId = static_cast<uint32_t>(Num(b + 22, 4, false));
            else if (h.kind == 0xFF01 || h.kind == 0xFF02)
                r.eventId = static_cast<uint32_t>(Num(b + 16, 4, h.little));
            records.push_back(r);
            pos += h.size;
        }
        return true;
    }

  private:
    struct Header
    {
        uint16_t kind = 0;
        uint64_t size = 0;
        uint64_t header = 8;
        uint64_t count = 0;
        bool little = false;
    };

    static uint64_t Num(const uint8_t* b, int n, bool little)
    {
        uint64_t value = 0;
        for (int i = 0; i < n; ++i)
            value = (value << 8) | b[little ? n - 1 - i : i];
        return value;
    }

    // MFM primary header: byte 0 low nibble = log2 block size, 1..3 frame size in blocks,
    // 5..6 frame type, 8..9 header size in blocks, 12..15 item count. CoBo frames are big
    // endian; merged (layered) frames may be either, decided as in Scope.
    static bool ReadHeader(const uint8_t* b, size_t n, Header& h, std::string& error)
    {
        if (n < 8) {
            error = "short MFM header";
            return false;
        }
        const auto be = static_cast<uint16_t>(Num(b + 5, 2, false));
        const auto le = static_cast<uint16_t>(Num(b + 5, 2, true));
        auto known = [](uint16_t k, bool little) {
            return k == 1 || k == 2 || k == 7 || k == 8 || k == 0xFF01 || k == 0xFF02 || (little && k == 0xFF11);
        };
        const uint16_t kind = known(be, false) ? be : (known(le, true) ? le : be);
        const uint64_t block = 1ULL << (b[0] & 15);
        const bool layered = (kind == 0xFF01 || kind == 0xFF02);
        if ((layered && n < 20) || ((kind == 1 || kind == 2) && n < 28)) {
            error = "short event header";
            return false;
        }
        auto layout = [&](bool little) {
            Header l;
            l.kind = kind;
            l.little = little;
            l.size = Num(b + 1, 3, little) * block;
            if (n >= 16) {
                l.header = Num(b + 8, 2, little) * block;
                l.count = Num(b + 12, 4, little);
            }
            return l;
        };
        auto valid = [](const Header& l) {
            return l.header >= 20 && l.size >= l.header && l.count <= (l.size - l.header) / 8;
        };
        const Header a = layout(false);
        const Header z = layout(true);
        h = (kind == 0xFF11 || (layered && valid(z) && (!valid(a) || z.header < a.header))) ? z : a;
        if (h.size < 8 || (layered && !valid(h))) {
            error = "invalid MFM frame size";
            return false;
        }
        return true;
    }

    static uint64_t Get(const char* bytes, int n)
    {
        uint64_t value = 0;
        for (int i = n - 1; i >= 0; --i)
            value = (value << 8) | static_cast<uint8_t>(bytes[i]);
        return value;
    }

    static void Put(std::ofstream& output, uint64_t value, int n)
    {
        for (int i = 0; i < n; ++i)
            output.put(static_cast<char>((value >> (8 * i)) & 0xFF));
    }
};

#endif
