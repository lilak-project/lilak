from lilakcc import lilakcc

###################################################################################################
lilakcc("""
+class LKWindowManager

+private Int_t fXDisplay = -1;
+private Int_t fYDisplay = -1;
+private UInt_t fXFull = -1;
+private UInt_t fYFull = -1;

+public void ConfigureDisplay()
+source 
Drawable_t id = gClient->GetRoot()->GetId();
gVirtualX -> GetWindowSize(id, fXDisplay, fYDisplay, fXFull, fYFull);
"""
).print_tool()
