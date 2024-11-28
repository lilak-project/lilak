#ifndef LKSIMPLEANALYSISMETHODS_HH
#define LKSIMPLEANALYSISMETHODS_HH

#include "TObject.h"
#include "TH1D.h"
#include "LKDrawing.h"

/// LILAK Simple Analysis Methods
class LKSAM : public TObject
{
    public:
        static LKSAM* GetSAM(); ///< Get LKSAM static pointer.

        LKSAM();
        virtual ~LKSAM() {};

        double FWHM(double *buffer, int length);
        double FWHM(double *buffer, int length, int    iPeak, double amplitude, double pedestal, double &xMin, double &xMax, double &half);
        double FWHM(TH1D* hist,                 double xPeak, double amplitude, double pedestal, double &iMin, double &iMax, double &half);

        /// See EvalPedestalSamplingMethod()
        double EvalPedestal(double *buffer, int length, int method=0);

        /**
         * @brief Calculates the pedestal value of a data buffer using a sampling method, with an option to subtract it.
         *
         * This method determines the pedestal level of a waveform or data buffer by dividing the buffer into segments,
         * calculating the mean and standard deviation for each segment, and selecting the most stable segments based on a
         * coefficient of variation (CV) threshold. The final pedestal value is an average of selected segments, representing
         * a stable baseline. An option is provided to subtract this pedestal value from the buffer.
         *
         * @param buffer Pointer to an array of doubles representing the data buffer.
         * @param length Integer length of the data buffer.
         * @param sampleLength Integer specifying the length of each segment to be analyzed for pedestal calculation.
         *        Default value is 50.
         * @param cvCut Double threshold for the coefficient of variation, used to filter segments with high variability.
         *        Default value is 0.2.
         * @param subtractPedestal Boolean flag; if true, subtracts the calculated pedestal value from each element in the buffer.
         *        Default value is false.
         *
         * @return The calculated pedestal value as a double.
         *
         * The function works as follows:
         * - Divides the buffer into `sampleLength` segments and calculates the mean and standard deviation for each segment.
         * - Flags segments with a coefficient of variation (CV) below `cvCut` for inclusion in the pedestal calculation.
         * - If two or more segments meet the CV threshold, selects the two most similar segments to estimate a stable pedestal.
         * - Calculates the final pedestal as an average over selected segments, using further filtering to maintain stability.
         * - Optionally subtracts the calculated pedestal value from each element in the original buffer.
         *
         * The pedestal is derived through several checks:
         * - Segments with zero values are flagged and skipped in the comparison.
         * - A reference pedestal mean is set based on the two closest matching segments, and an error threshold is defined.
         * - Segments within the error threshold are aggregated to form the final pedestal estimate.
         *
         * @note If the final pedestal calculation fails, a default value of 4096 is returned.
         */
        double EvalPedestalSamplingMethod(double *buffer, int length,  int sampleLength=50, double cvCut=0.2, bool subtractPedestal=false);

        /**
         * @brief Calculates the continuity index of a histogram based on bin content variations.
         *
         * This method evaluates the continuity index of a histogram by analyzing changes in
         * content values between consecutive bins. It identifies significant fluctuations and
         * zero-value regions in the histogram data to compute an index where a higher value
         * indicates a more continuous, stable histogram.
         *
         * @param hist Pointer to a TH1 histogram object.
         * @param thresholdRatio A double representing the ratio for the threshold value, used to
         *        determine significant fluctuations in bin content. The threshold is calculated as
         *        `threshold = max * thresholdRatio`, where `max` is the maximum bin content.
         * @return The continuity index as a double, ranging from 0 to 1, where 1 indicates a
         *         highly continuous histogram with minimal significant fluctuations.
         *
         * The method works as follows:
         * - It calculates a threshold based on the maximum bin content and the `thresholdRatio`.
         * - It iterates through the histogram bins, checking for regions with zero values or
         *   significant differences in bin-to-bin content.
         * - For bins with consecutive large changes in content, it increments a counter (`countV`)
         *   and for zero-value regions, it increments another counter (`countX`).
         * - The final continuity index is computed based on the ratio of bins without major
         *   fluctuations or zero regions to the total bins.
         *
         * @note For histograms with fewer than 5 bins, the method returns a default continuity index of 1.
         */
        double ContinuityIndex(TH1* hist, double thresholdRatio=0.05);

        /**
         * @brief Creates a polar histogram representation of a 1D histogram from 0 to 360 degrees.
         *
         * This function generates a polar histogram, displaying the contents of each bin as radial
         * bars, with optional grid lines and axis labeling. It is designed for histograms with bins
         * spanning a full circle (0 to 360 degrees) and allows various customization options for the
         * appearance of the polar plot.
         *
         * @param hist Pointer to a TH1D histogram with bins from 0 to 360 degrees.
         * @param rMin Radius of the inner axis representing the minimum value of the histogram.
         *        Default value is 5.
         * @param rMax Radius of the outer axis representing the maximum value of the histogram.
         *        Default value is 10.
         * @param numGrids Number of concentric grid lines between the inner and outer axes.
         *        Default value is 0.
         * @param numAxisDiv Number of divisions along the polar axis, creating an n-sided polygon.
         *        If `numAxisDiv` > 60, the axis is drawn as a circle. Default value is 12.
         * @param drawInfo Specifies which corner to display histogram statistics (e.g., entries, max).
         *        Valid values: 0-3 for different corners; -1 to skip drawing stats. Default value is 1.
         * @param usePolygonAxis If true, the polar axis is drawn as an n-sided polygon based on `numAxisDiv`.
         *        If false, the polar axis is drawn as a circle. Default value is true.
         * @param binContentOffRatio Specifies the offset ratio for displaying each bin’s content.
         *        Draws bin content between the inner and outer axis using this ratio; 0 to ignore.
         *        Default value is 0.
         * @param labelOffRatio Specifies the offset ratio for displaying angle labels, if they exist.
         *        Draws labels between the inner and outer axis using this ratio; 0 to ignore.
         *        Default value is 0.1.
         * @param binLabelOffRatio Specifies the offset for displaying angle labels at `numAxisDiv`
         *        points. Labels are offset by `dr * binLabelOffRatio`; 0 to ignore.
         *        Default value is 0.1.
         * @param graphAtt Pointer to a TGraph defining appearance attributes for the histogram bars.
         *        Use nullptr for default attributes (gray color). Default value is nullptr.
         *
         * @return Pointer to an LKDrawing object representing the configured polar histogram.
         *
         * The function includes:
         * - Drawing the frame and grid lines.
         * - Configuring a polygon or circular axis for the inner and outer radii.
         * - Labeling angle divisions based on `numAxisDiv`, with optional offsets for label and bin content.
         * - Drawing bin contents as radial bars according to the histogram values and specified offsets.
         * - Optionally displaying histogram information (e.g., entries, max) in a designated corner.
         */
        LKDrawing* MakeTH1Polar(
                TH1D* hist,
                double rMin=5,
                double rMax=10,
                int numGrids=0,
                int numAxisDiv=12,
                int drawInfo=1,
                bool usePolygonAxis=true,
                double binContentOffRatio=0,
                double labelOffRatio=0.1,
                double binLabelOffRatio=0.1,
                TGraph* graphAtt=nullptr
                );

    private:
        void InitBufferDouble(int length);

    private:
        double *fBufferDouble = nullptr;
        int fBufferDoubleSize = 0;      // Variable to keep track of the current buffer size


    private:
        static LKSAM *fInstance;

    ClassDef(LKSAM, 1)
};

#endif
