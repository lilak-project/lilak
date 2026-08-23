void run_example()
{
    int factor = 2;                // A0 = factor x 10^exponent
    int exponent = -3;             // A0 = 2x10^-3
    double holeDiameter = 0.2;      // mm
    double beamRadius = 3.0;        // mm
    int beamType = 0;

    int gridPattern = 0;            // hole grid pattern
    double attenuatorSize = 40.0;   // mm
    double activeSize = 36.0;       // mm
    double holeXOffset = 0.0;       // hole spacing fraction
    double holeYOffset = 0.0;       // hole spacing fraction

    double calculationRange = 0.5;  // hole spacing fraction
    double efficiencyRange1 = 0.2;  // A/A0 histogram min
    double efficiencyRange2 = 1.8;  // A/A0 histogram max
    double beamRadiusToSigma = 1.0 / 3.0; // gaussian sigma = value x beamRadius
    int numStepsX = 200;            // scan steps
    int numStepsY = 200;            // scan steps

    auto calculator = new LKAttenuationCalculator();

    auto top = new LKDrawingGroup();

    beamType = 0;
    calculator -> SetParameters(factor, exponent, holeDiameter, beamRadius, beamType);
    calculator -> SetGeometricalParameters(gridPattern, attenuatorSize, activeSize, holeXOffset, holeYOffset);
    calculator -> SetCalculationParameters(calculationRange, efficiencyRange1, efficiencyRange2, beamRadiusToSigma, numStepsX, numStepsY);
    auto drawings = calculator -> Run();
    top -> Add(drawings);
    //drawings -> Draw();

    beamType = 1;
    calculator -> SetParameters(factor, exponent, holeDiameter, beamRadius, beamType);
    calculator -> SetGeometricalParameters(gridPattern, attenuatorSize, activeSize, holeXOffset, holeYOffset);
    calculator -> SetCalculationParameters(calculationRange, efficiencyRange1, efficiencyRange2, beamRadiusToSigma, numStepsX, numStepsY);
    auto drawings2 = calculator -> Run();
    top -> Add(drawings2);
    //drawings2 -> Draw();

    top -> Draw();
}
