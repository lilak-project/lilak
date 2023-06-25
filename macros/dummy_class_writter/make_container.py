from lilakcc import lilakcc

###################################################################################################
lilakcc(
"""
/// Raw event data from GET
+class MMChannel : public LKChannel
+public     Int_t   fFrameNo = -1;
+public     Int_t   fDecayNo = -1;
+public     Int_t   fCobo = -1;
+public     Int_t   fAsad = -1;
+public     Int_t   fAget = -1;
+public     Int_t   fChan = -1;
+public     Int_t   fDChan = -1;
+public     Float_t fTime = -1;
+public     Float_t fEnergy = -1;

+public     Int_t   fWaveformX[512];
+clear      for (auto i=0; i<512; ++i) fWaveformX[i] = -1;
+getter     Int_t *GetWaveformX() { return fWaveformX; }
+setter     void SetWaveformX(const Int_t *waveform) { memcpy(fWaveformX, waveform, sizeof(Int_t)*512); }
+print      for (auto i=0; i<512; ++i) lx_cout << fWaveformX[i] << " "; lx_cout << std::endl;

+public     Int_t   fWaveformY[512];
+clear      for (auto i=0; i<512; ++i) fWaveformY[i] = -1;
+getter     Int_t *GetWaveformY() { return fWaveformY; }
+setter     void SetWaveformY(const Int_t *waveform) { memcpy(fWaveformY, waveform, sizeof(Int_t)*512); }
+print      for (auto i=0; i<512; ++i) lx_cout << fWaveformY[i] << " "; lx_cout << std::endl;
"""
).print_container(inheritance="LKChannel")

###################################################################################################
lilakcc(
"""
/// TexAT event header
+class TTEventHeader
+public     bool fIsGoodEvent = true;
+public     bool fIsMMEvent = true;
+public     Int_t SiBLR = -1;
+public     Int_t siLhit = -1;
+public     Int_t siRhit = -1;
+public     Int_t siChit = -1;
+public     Int_t X6Lhit = -1;
+public     Int_t X6Rhit = -1;
"""
).print_container()
