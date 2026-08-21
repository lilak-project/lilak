import os
from pathlib import Path
import ROOT

repo_path = Path(__file__).resolve().parents[2]
lilak_path = os.environ.get("LILAK_PATH", str(repo_path))
ROOT.gSystem.Load(f"{lilak_path}/build/libLILAK")
print(f"To keep ROOT canvases open, run: python3 -i {Path(__file__).as_posix()}")

factor = 2                 # A0 = factor x 10^exponent
exponent = -3              # A0 = 2x10^-3
hole_diameter = 0.2        # mm
beam_radius = 3.0          # mm
beam_type = 0

grid_pattern = 0           # hole grid pattern
attenuator_size = 40.0     # mm
active_size = 36.0         # mm
hole_x_offset = 0.0        # hole spacing fraction
hole_y_offset = 0.0        # hole spacing fraction

calculation_range = 0.5    # hole spacing fraction
efficiency_range1 = 0.2    # A/A0 histogram min
efficiency_range2 = 1.8    # A/A0 histogram max
beam_radius_to_sigma = 1.0 / 3.0 # gaussian sigma = value x beam_radius
num_steps_x = 200          # scan steps
num_steps_y = 200          # scan steps

calculators = []
drawings = []

calculator = ROOT.LKAttenuationCalculator()

beam_type = 0
calculator.SetParameters(factor, exponent, hole_diameter, beam_radius, beam_type)
calculator.SetGeometricalParameters(grid_pattern, attenuator_size, active_size, hole_x_offset, hole_y_offset)
calculator.SetCalculationParameters(calculation_range, efficiency_range1, efficiency_range2, beam_radius_to_sigma, num_steps_x, num_steps_y)
drawing = calculator.Run()
drawing.Draw()
calculators.append(calculator)
drawings.append(drawing)

beam_type = 1
calculator.SetParameters(factor, exponent, hole_diameter, beam_radius, beam_type)
calculator.SetGeometricalParameters(grid_pattern, attenuator_size, active_size, hole_x_offset, hole_y_offset)
calculator.SetCalculationParameters(calculation_range, efficiency_range1, efficiency_range2, beam_radius_to_sigma, num_steps_x, num_steps_y)
drawing = calculator.Run()
drawing.Draw()
calculators.append(calculator)
drawings.append(drawing)
