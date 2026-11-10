#include <iostream>
#include <memory>
#include <string>

#include "TClonesArray.h"
#include "TFile.h"
#include "TTree.h"
#include "GETChannel.h"
#include "LKEventHeader.h"
#include "LKSiChannel.h"

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    std::unique_ptr<TFile> file(TFile::Open(argv[1], "READ"));
    if (!file || file->IsZombie()) return 3;
    auto *tree = dynamic_cast<TTree *>(file->Get("event"));
    if (!tree) return 4;
    std::string branchName = argv[2];
    if (branchName == "-") {
        std::cout << "BRANCHES " << tree->GetEntries();
        for (const char *candidate : {"RawData", "SiChannel"}) {
            if (tree->GetBranch(candidate)) std::cout << ' ' << candidate;
        }
        std::cout << std::endl;
        return 0;
    }
    if (!tree->GetBranch(branchName.c_str())) return 5;
    TClonesArray *channels = nullptr;
    TClonesArray *headers = nullptr;
    tree->SetBranchAddress(branchName.c_str(), &channels);
    if (tree->GetBranch("FrameHeader")) tree->SetBranchAddress("FrameHeader", &headers);
    std::cout << "READY " << tree->GetEntries() << std::endl;
    long long entry = -1;
    while (std::cin >> entry) {
        if (entry < 0 || entry >= tree->GetEntries() || tree->GetEntry(entry) <= 0) {
            std::cout << "ERROR entry" << std::endl;
            continue;
        }
        int frame = static_cast<int>(entry);
        unsigned long long eventTime = 0;
        if (headers && headers->GetEntriesFast() > 0) {
            auto *header = dynamic_cast<LKEventHeader *>(headers->At(0));
            if (header) {
                frame = header->GetEventNumber();
                eventTime = header->GetEventTime();
            }
        }
        std::cout << "EVENT " << frame << ' ' << eventTime << ' '
                  << (channels ? channels->GetEntriesFast() : 0) << '\n';
        if (channels) {
            for (int index = 0; index < channels->GetEntriesFast(); ++index) {
                auto *channel = dynamic_cast<GETChannel *>(channels->At(index));
                if (!channel) continue;
                std::cout << channel->GetCobo() << ' ' << channel->GetAsad() << ' '
                          << channel->GetAget() << ' ' << channel->GetChan() << ' '
                          << channel->GetPedestal() << ' ' << channel->GetTime() << ' '
                          << channel->GetEnergy() << ' ' << channel->GetNoiseScale();
                auto *siChannel = dynamic_cast<LKSiChannel *>(channel);
                std::cout << ' ' << (siChannel ? siChannel->GetStrip() : -1)
                          << ' ' << (siChannel ? siChannel->GetDetNum() : -1)
                          << ' ' << (siChannel && siChannel->GetInverted() ? 1 : 0);
                int *waveform = channel->GetWaveformY();
                for (int tb = 0; tb < 512; ++tb) std::cout << ' ' << waveform[tb];
                std::cout << '\n';
            }
        }
        std::cout << "END" << std::endl;
    }
    return 0;
}
