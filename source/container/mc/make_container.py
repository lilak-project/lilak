from lilakcc import lilakcc

to_screen=True
to_file=False

################################################################################################### 
cc = lilakcc()
cc.add("""
/**
 * Virtual class for Tracklet container
 */
+class LKTracklet : public LKContainer 

+private    int fTrackID = -1;
+private    int fVertexID = -1; 
+private    int fPDG = -1;
+lname      pdg

+private    TVector3 fPositionHead = TVecotor3(-9.9,-9.9,-9.9);
+setter     void SetPositionHead(double x, double y, double z) { fPositionHead(x,y,z); }

+private    TVector3 fPositionTail = TVecotor3(-9.9,-9.9,-9.9);
+setter     void SetPositionTail(double x, double y, double z) { fPositionTail(x,y,z); }

+private    LKHitArray fHitArray = nullptr; ///<!
+clear      fHitArray.Clear(option);
+print      fHitArray.Print(option);
+getter     LKHitArray *GetHitArray() { return &fHitArray; }

+private    TGraphErrors *fTrajectoryOnPlane = nullptr; ///<! Graph object for drawing trajectory on 2D event display
+init       fTrajectoryOnPlane = new TGraphErrors();
+clear      fTrajectoryOnPlane.Set(0);

+public     virtual double Energy(int alpha=0) const = 0; ///< Kinetic energy of track at vertex.
+public     virtual TVector3 Momentum(int alpha=0) const = 0; ///< Momentum of track at vertex.

+public     virtual GetAlphaTail() const;       ///< Alpha at tail (reconstructed back end)
+public     virtual Double_t LengthToAlpha(double length) const = 0;        ///< Convert track-length (mm?) to alpha
+public     virtual Double_t AlphaToLength(double alpha) const = 0;             ///< Convert alpha to track-length (mm?)
+public     virtual Double_t TrackLength(double a1=0, double a2=1) const = 0;   ///< Length of track between two alphas (default: from vertex to head).

/**
 * Extrapolated position at given alpha.
 * Alpha (double) is scaled length variable along the track where
 * alpha=0 is position of vertex and
 * alpha=1 is position of head (reconstructed front end).
 */
+public virtual TVector3 ExtrapolateToAlpha(double alpha) const = 0;
+public virtual TVector3 ExtrapolateHead(Double_t dalpha) const = 0;        ///< Extrapolate head by dalpha and return position
+public virtual TVector3 ExtrapolateTail(Double_t dalpha) const = 0;        ///< Extrapolate tail by dalpha and return position
+public virtual TVector3 ExtrapolateToPosition(TVector3 point) const = 0;   ///< Extrapolate and return POCA from point
"""
)
