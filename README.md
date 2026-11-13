# Rooftop Solar Shadow Analyzer

A C++17 and OpenGL-based 3D application for visualizing rooftop solar
potential through sun positioning, shadow mapping, panel-wise sunlight
exposure, and seasonal energy analysis.

## Overview

Rooftop Solar Shadow Analyzer creates a 3D building scene in which the
sun's position changes according to the selected date, time, and
geographic location. It visualizes shadows and estimates sunlight
exposure across individual rooftop solar panels.

The application combines real-time 3D graphics with solar-position
calculations to help explore how building geometry, shading, and
environmental conditions can affect solar-panel performance.

## Features

-   **3D building visualization** using OpenGL.
-   **Dynamic sun positioning** based on date, time, and location.
-   **Shadow mapping** to visualize building and surrounding-structure
    shadows.
-   **Rooftop solar-panel visualization** with individual panel
    transforms.
-   **Panel-wise sunlight exposure analysis** using sample points on
    each panel.
-   **Color-coded exposure heatmap** to compare panel-level sunlight
    availability.
-   **Sunlit and shaded panel statistics** and average exposure
    reporting.
-   **Solar power and energy estimates** based on the calculated
    exposure.
-   **Weather integration** for weather-aware analysis.
-   **Seasonal comparison export** to CSV for further analysis.

## Technology Stack

  Area                   Technology
  ---------------------- --------------
  Programming language   C++17
  Graphics               OpenGL, GLSL
  Window and input       GLFW
  Mathematics            GLM
  3D model loading       Assimp
  User interface         Dear ImGui
  Build system           CMake

## Project Structure

``` text
RooftopSolarShadowAnalyzer/
├── assets/
│   └── models/
├── shaders/
├── src/
├── CMakeLists.txt
├── solar_seasonal_comparison.csv
└── README.md
```

The repository may also contain dependency and build-support directories
that are not shown above.

## Prerequisites

-   macOS or another supported desktop platform
-   A C and C++ compiler with C++17 support
-   CMake 3.20 or newer
-   OpenGL-compatible graphics support
-   Project dependencies configured by the repository's CMake setup

## Build and Run

Clone the repository:

``` bash
git clone https://github.com/tanishqasharma29/Rooftop-Solar-Shadow-Analyzer.git
cd Rooftop-Solar-Shadow-Analyzer
```

Configure the project:

``` bash
cmake -S . -B build
```

Build:

``` bash
cmake --build build -j4
```

Run the application:

``` bash
./build/RooftopSolarShadowAnalyzer
```

If configuration or compilation fails, check that the required compiler,
CMake version, OpenGL support, and platform dependencies are installed.

## Seasonal Data Export

The application can export seasonal solar-analysis results to:

``` text
solar_seasonal_comparison.csv
```

The CSV can be opened in spreadsheet software or processed with Python
for further comparisons and visualizations. The exported values are
application estimates and should not be treated as measured production
data.

## Notes

-   Solar exposure and energy values are estimates produced by the
    application's model.
-   Results depend on the selected date, time, location, scene geometry,
    and available weather information.
-   The project is under development; features and implementation
    details may evolve.

## Author

**Tanishqa Sharma**

-   GitHub: [@tanishqasharma29](https://github.com/tanishqasharma29)

## License

No formal open-source license has been specified yet. Please contact the
author before reusing or redistributing this project.
