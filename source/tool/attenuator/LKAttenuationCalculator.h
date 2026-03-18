#ifndef LKATTENUATIONCALCULATOR_HH
#define LKATTENUATIONCALCULATOR_HH

#include "TNamed.h"
#include "LKCompiled.h"
#include "LKMisc.h"
#include "LKPainter.h"
#include "LKDrawing.h"
#include "LKDrawingGroup.h"

class LKAttenuationCalculator : public TNamed
{
    public:
        LKAttenuationCalculator() {}
        LKAttenuationCalculator(
                int factor, int exponent, double hole_diameter, double beam_radius, int beam_type,
                int grid_pattern, double attenuatorSize, double activeSize, double hole_x_offset=0., double hole_y_offset=0.,
                double calculationRange=0.5, double efficiencyRange1=0.7, double efficiencyRange2=1.3, double beamRadiusToSigma=0.3, int numEvaluationStepsInX=200, int numEvaluationStepsInY=200
                )
        {
            SetParameters(factor, exponent, hole_diameter, beam_radius, beam_type);
            SetGeometricalParameters(grid_pattern, attenuatorSize, activeSize, hole_x_offset, hole_y_offset);
            SetCalculationParameters(calculationRange, efficiencyRange1, efficiencyRange2, beamRadiusToSigma, numEvaluationStepsInX, numEvaluationStepsInY);
        }
        ~LKAttenuationCalculator() {}

        void SetNameTitleAuto();

        void SetParameters(int factor, int exponent, double hole_diameter, double beam_radius, int beam_type);
        void SetGeometricalParameters(int grid_pattern, double attenuatorSize, double activeSize, double hole_x_offset, double hole_y_offset);
        void SetCalculationParameters(double calculationRange, double efficiencyRange1, double efficiencyRange2, double beamRadiusToSigma, int numEvaluationStepsInX, int numEvaluationStepsInY);
        void SetHoleDiameterForCalculation(double hole_diameter) { fHoleDiameterCalc = hole_diameter; }
        LKDrawingGroup* Run(bool add_sketch=true, bool add_1d=true, bool add_2d=true, bool print_holes=false);

        double CircleIntersectionArea(double x1, double y1, double r1, double x2, double y2, double r2);
        const char* BeamTypeString(int beam_type);

    protected:
        TGraph* NewGraph(TString name="graph", int mst=20, double msz=0.6, int mcl=kBlack, int lst=-1, int lsz=-1, int lcl=-1);
        TGraphErrors* NewGraphErrors(TString name="graph", int mst=20, double msz=0.6, int mcl=kBlack, int lst=-1, int lsz=-1, int lcl=-1);
        double Gaussian2D(double x, double y, double xc, double yc, double sigmaX, double sigmaY);
        double IntegrateGaussian2D(double xc, double yc, double sigmaX, double sigmaY, double x0, double y0, double r0, int nPoints=100);
        double IntegrateGaussian2DFast(double xc, double yc, double sigmaX, double sigmaY, double x0, double y0, double r0, int nPoints=100);
        double IntegrateGaussian2D_SumGamma(double sigma, double cx, double r0);
        double IntegrateGaussian2DFaster(double xc, double yc, double sigmaX, double sigmaY, double x0, double y0, double r0);

    public:
        int    fBeamType;
        int    fFactor;
        int    fExponent;
        double fBeamRadius;
        double fHoleDiameter;
        double fHoleDiameterCalc;

        double  fAttenuation;

        int    fGridPattern = 0; // Pattern of the holes (0,1,2)
        double fAttenuatorSize = 40; //< size of attenuator (mm)
        double fActiveSize = 36; //< size of active attenuator area (mm)
        double fHoleXOffsetInR = 0.;
        double fHoleYOffsetInR = 0.;

        double fCalculationRange = 0.50;
        double fEfficiencyRange1 = 0.70; //< lower bound range of efficiency histogram.
        double fEfficiencyRange2 = 1.30; //< higher bound of efficiency histogram.
        int    fNumEvaluationStepsInX = 200; //< Number of attenuation evaluation points through x
        int    fNumEvaluationStepsInY = 200; //< Number of attenuation evaluation points through y
        double fBeamRadiusToSigma = 1./3; //< For guassian beam profile, sigma = fBeamRadiusToSigma * beam_radius;

    private:
        const int kUniformBeamProfile = 0;
        const int kGauss2DBeamProfile = 1;
};

#endif
