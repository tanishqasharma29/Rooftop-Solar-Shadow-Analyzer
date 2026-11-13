
#include <glad/gl.h>
#include "HeatmapBaker.hpp"
#include "SolarPanel.hpp"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "WeatherClient.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Window.hpp"
#include "Camera.hpp"
#include "Shader.hpp"
#include "Mesh.hpp"
#include "Model.hpp"
#include "SunCalculator.hpp"
#include "ShadowMap.hpp"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <iostream>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <stdexcept>
#include <vector>

// Forward declaration: used by BakeHeatmap before its definition.
std::string FormatTime(double decimalHours);

// Diagnostic helper: reports every pending OpenGL error with its render stage.
void CheckOpenGLError(const char* location)
{
    GLenum error = GL_NO_ERROR;

    while ((error = glGetError()) != GL_NO_ERROR)
    {
        std::cerr
            << "[OpenGL ERROR] "
            << location
            << " | Error code: "
            << error
            << std::endl;
    }
}

// Clears stale errors so the next stage can be tested independently.
void ClearOpenGLErrors()
{
    while (glGetError() != GL_NO_ERROR)
    {
        // Drain errors from earlier stages.
    }
}


// --------------------------------------------------
// GLOBAL OBJECTS
// --------------------------------------------------

Camera camera(
    glm::vec3(4.0f, 3.0f, 7.0f),
    glm::vec3(0.0f, 1.0f, 0.0f),
    -120.0f,
    -15.0f
);

SunCalculator sunCalculator;
// --------------------------------------------------
// DATE AND TIME
// --------------------------------------------------

int selectedYear = 2026;
int selectedMonth = 11;
int selectedDay = 11;

float localTime = 12.0f;

bool animationPlaying = false;
float animationSpeed = 30.0f;

// --------------------------------------------------
// SHADOW SETTINGS
// --------------------------------------------------

bool shadowsEnabled = true;
bool pcfEnabled = true;
// --------------------------------------------------
// M5 HEAT-MAP SETTINGS
// --------------------------------------------------

bool heatmapEnabled = true;

bool heatmapBaked = false;
bool showSolarPanels = true;
int heatmapMode = 0;

// 0 = Selected date
// 1 = Summer solstice
// 2 = Equinox
// 3 = Winter solstice

constexpr float HEATMAP_INTERVAL = 15.0f;


// M6.5: Solar energy model assumptions.
float panelEfficiency = 0.20f;       // 20% module efficiency
float peakSunHours = 5.0f;           // Assumed equivalent full-sun hours/day
constexpr float SYSTEM_PERFORMANCE_RATIO = 0.80f;
constexpr float STANDARD_IRRADIANCE = 1000.0f; // W/m^2
// --------------------------------------------------
// CAMERA SETTINGS
// --------------------------------------------------

bool cameraMode = false;
bool tabWasPressed = false;

// --------------------------------------------------
// KEYBOARD INPUT
// --------------------------------------------------

void processInput(
    GLFWwindow* window,
    float deltaTime
)
{
    bool tabPressed =
        glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;

    if (tabPressed && !tabWasPressed)
    {
        cameraMode = !cameraMode;

        glfwSetInputMode(
            window,
            GLFW_CURSOR,
            cameraMode
                ? GLFW_CURSOR_DISABLED
                : GLFW_CURSOR_NORMAL
        );
    }

    tabWasPressed = tabPressed;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }

    if (!cameraMode)
        return;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(
            CameraMovement::FORWARD, deltaTime
        );

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(
            CameraMovement::BACKWARD, deltaTime
        );

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(
            CameraMovement::LEFT, deltaTime
        );

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(
            CameraMovement::RIGHT, deltaTime
        );

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.ProcessKeyboard(
            CameraMovement::UP, deltaTime
        );

    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        camera.ProcessKeyboard(
            CameraMovement::DOWN, deltaTime
        );
}

// --------------------------------------------------
// GROUND MESH
// --------------------------------------------------

Mesh CreateGround()
{
    std::vector<Vertex> vertices =
    {
        {
            glm::vec3(-15.0f, 0.0f, -15.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        },
        {
            glm::vec3(15.0f, 0.0f, -15.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        },
        {
            glm::vec3(15.0f, 0.0f, 15.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        },
        {
            glm::vec3(-15.0f, 0.0f, 15.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        }
    };

    std::vector<unsigned int> indices =
    {
        0, 1, 2,
        0, 2, 3
    };

    return Mesh(vertices, indices);
}

// --------------------------------------------------
// DRAW ONE BUILDING
// --------------------------------------------------

void DrawBuilding(
    const Model& building,
    const Shader& shader,
    const glm::vec3& position,
    const glm::vec3& scale,
    const glm::vec3& color
)
{
    glm::mat4 model(1.0f);

    model = glm::translate(model, position);
    model = glm::scale(model, scale);

    shader.SetMat4("model", model);

    // This uniform exists only in the lighting shader.
    // It is harmless to omit it from the depth pass.
    GLint colorLocation = glGetUniformLocation(
        shader.ID,
        "objectColor"
    );

    if (colorLocation >= 0)
    {
        shader.SetVec3("objectColor", color);
    }

    building.Draw();
}

// --------------------------------------------------
// DRAW ALL SCENE OBJECTS
// --------------------------------------------------

void RenderScene(
    const Model& building,
    const Mesh& ground,
    const Shader& shader,
    bool depthPass
)
{
    // Ground transformation.
    glm::mat4 groundModel(1.0f);

    shader.SetMat4(
        "model",
        groundModel
    );

    if (!depthPass)
    {
        shader.SetVec3(
            "objectColor",
            glm::vec3(0.25f, 0.48f, 0.25f)
        );
    }

    ground.Draw();

    // Main building.
    DrawBuilding(
        building,
        shader,
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f),
        glm::vec3(0.83f, 0.82f, 0.76f)
    );

    // Neighbouring building 1.
    DrawBuilding(
        building,
        shader,
        glm::vec3(3.5f, 0.0f, 0.0f),
        glm::vec3(0.65f, 0.75f, 0.65f),
        glm::vec3(0.65f, 0.70f, 0.76f)
    );

    // TEMPORARY SHADOW TEST:
    // Taller and closer neighbour to check whether the shadow map
    // produces spatial variation on the main rooftop.
    DrawBuilding(
        building,
        shader,
        glm::vec3(-1.8f, 0.0f, -1.0f),
        glm::vec3(0.75f, 2.5f, 0.75f),
        glm::vec3(0.76f, 0.65f, 0.56f)
    );
}

// --------------------------------------------------
// LIGHT SPACE MATRIX
// --------------------------------------------------

glm::mat4 CalculateLightSpaceMatrix(
    const glm::vec3& sunDirection
)
{
    glm::vec3 sceneCenter(0.0f, 0.0f, 0.0f);

    glm::vec3 lightPosition =
        sceneCenter + sunDirection * 35.0f;

    glm::vec3 upVector(0.0f, 1.0f, 0.0f);

    if (std::abs(glm::dot(
        glm::normalize(sunDirection),
        upVector
    )) > 0.95f)
    {
        upVector = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    glm::mat4 lightView = glm::lookAt(
        lightPosition,
        sceneCenter,
        upVector
    );

    glm::mat4 lightProjection =
        glm::ortho(
            -16.0f, 16.0f,
            -16.0f, 16.0f,
            1.0f, 70.0f
        );

    return lightProjection * lightView;
}

bool BakeHeatmap(
    HeatmapBaker& heatmapBaker,
    ShadowMap& shadowMap,
    Shader& shadowShader,
    Shader& heatmapBakeShader,
    const Model& building,
    const Mesh& ground,
    SolarPanel& solarPanel,
    SunCalculator& calculator,
    int year,
    int month,
    int day,
    const WeatherData& weatherData,
    bool weatherAvailable
)
{
    std::cout << "\nStarting rooftop heat-map baking..."
              << std::endl;

    // Clear previous accumulation.
    heatmapBaker.Clear();

    // Calculate solar information for noon.
    SolarPosition noon = calculator.Calculate(
        year,
        month,
        day,
        12.0
    );

    if (!noon.validSunriseSunset)
    {
        std::cout << "No valid sunrise/sunset."
                  << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    const double sunrise = noon.sunrise;
    const double sunset = noon.sunset;

    const double intervalHours =
        HEATMAP_INTERVAL / 60.0;

    int samples = 0;

    // ------------------------------------------
    // SIMULATE THE SUN THROUGHOUT THE DAY
    // ------------------------------------------

    for (
        double time = sunrise;
        time < sunset;
        time += intervalHours
    )
    {
        // Handle the final partial interval.
        const float dt = static_cast<float>(
            std::min(intervalHours, sunset - time)
        );

        // Sample at the midpoint of this interval.
        const double sampleTime = time + dt * 0.5;

        // Use the date passed to BakeHeatmap and sample the entire day.
        SolarPosition sun = calculator.Calculate(
            year,
            month,
            day,
            sampleTime
        );

        if (!sun.sunAboveHorizon)
            continue;

        // Calculate the Sun's light-space matrix.
        glm::mat4 lightSpaceMatrix =
            CalculateLightSpaceMatrix(sun.direction);

        // --------------------------------------
        // PASS 1: GENERATE SHADOW MAP
        // --------------------------------------

        glEnable(GL_DEPTH_TEST);

        shadowMap.BindForWriting();

        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);

        shadowShader.Use();

        GLint matrixLocation =
            glGetUniformLocation(
                shadowShader.ID,
                "lightSpaceMatrix"
            );

        glUniformMatrix4fv(
            matrixLocation,
            1,
            GL_FALSE,
            &lightSpaceMatrix[0][0]
        );

        // All buildings and the ground participate
        // in shadow generation.
        RenderScene(
            building,
            ground,
            shadowShader,
            true
        );
        solarPanel.Draw(shadowShader);

        glDisable(GL_POLYGON_OFFSET_FILL);

        shadowMap.Unbind();

        // --------------------------------------
        // PASS 2: ACCUMULATE ROOF SUNLIGHT
        // --------------------------------------

        // M9: Use interpolated hourly radiation for this simulated time.
        // The forecast profile belongs to the API's current forecast day;
        // it is a representative profile when baking another seasonal date.
        const float cloudFraction = weatherAvailable
            ? std::clamp(
                static_cast<float>(weatherData.cloudCover) / 100.0f,
                0.0f,
                1.0f
              )
            : 0.0f;

        const float estimatedDirect =
            850.0f * (1.0f - 0.75f * cloudFraction);
        const float estimatedDiffuse =
            100.0f + 200.0f * cloudFraction;

        double hourlyShortwave = 0.0;
        double hourlyDirect = 0.0;
        double hourlyDiffuse = 0.0;
        const bool hasHourlyRadiation = weatherAvailable &&
            weatherData.GetRadiationAtHour(
                sampleTime,
                hourlyShortwave,
                hourlyDirect,
                hourlyDiffuse
            );

        const float directRadiation =
            hasHourlyRadiation && hourlyDirect > 1.0
            ? static_cast<float>(hourlyDirect)
            : estimatedDirect;

        const float diffuseRadiation =
            hasHourlyRadiation && hourlyDiffuse > 1.0
            ? static_cast<float>(hourlyDiffuse)
            : estimatedDiffuse;

        heatmapBaker.Accumulate(
            heatmapBakeShader,
            shadowMap,
            lightSpaceMatrix,
            sun.direction,
            dt,
            directRadiation,
            diffuseRadiation
        );

        samples++;

        if (samples % 8 == 0)
        {
            std::cout
                << "Heat-map samples: "
                << samples
                << " | Simulated time: "
                << FormatTime(sampleTime)
                << std::endl;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::cout
        << "Heat-map baking completed."
        << std::endl;

    std::cout
        << "Total samples: "
        << samples
        << std::endl;

    return samples > 0;
}

// --------------------------------------------------
// TIME FORMATTING
// --------------------------------------------------

std::string FormatTime(double decimalHours)
{
    if (!std::isfinite(decimalHours))
        return "--:--";

    int totalMinutes = static_cast<int>(
        std::round(decimalHours * 60.0)
    );

    totalMinutes = std::clamp(
        totalMinutes, 0, 1439
    );

    char buffer[16];

    std::snprintf(
        buffer,
        sizeof(buffer),
        "%02d:%02d",
        totalMinutes / 60,
        totalMinutes % 60
    );

    return std::string(buffer);
}

// Return the date represented by the selected heat-map mode.
// 0 = user-selected date, 1 = summer solstice, 2 = equinox,
// 3 = winter solstice. Dates are representative calendar dates.
void GetHeatmapDate(
    int mode,
    int selectedYear,
    int selectedMonth,
    int selectedDay,
    int& year,
    int& month,
    int& day
)
{
    year = selectedYear;

    switch (mode)
    {
        case 1: month = 6;  day = 21; break;
        case 2: month = 3;  day = 20; break;
        case 3: month = 12; day = 21; break;
        default:
            month = selectedMonth;
            day = selectedDay;
            break;
    }
}

// M10: Append the currently selected seasonal simulation to a CSV report.
// One row is written each time the user presses the export button.
bool ExportSeasonReportCSV(
    int year,
    int month,
    int day,
    double localHour,
    const SolarPosition& sun,
    const SolarPanel& solarPanel,
    const WeatherData& weatherData,
    bool weatherAvailable,
    float irradianceWm2,
    float instantaneousPowerW,
    float dailyEnergyKWh,
    const std::vector<float>& panelPowerValues
)
{
    const std::string filename = "solar_seasonal_comparison.csv";
    const bool writeHeader =
        !std::filesystem::exists(filename) ||
        std::filesystem::file_size(filename) == 0;

    std::ofstream file(filename, std::ios::app);
    if (!file.is_open())
        return false;

    if (writeHeader)
    {
        file << "date,local_time,sun_elevation_deg,sun_azimuth_deg,"
             << "declination_deg,sunrise_ist,sunset_ist,"
             << "sunlit_panels,shaded_panels,average_exposure_percent,"
             << "irradiance_w_m2,instantaneous_power_w,daily_energy_kwh,"
             << "weather_available,temperature_c,cloud_cover_percent";
        for (std::size_t i = 0; i < panelPowerValues.size(); ++i)
            file << ",panel_" << (i + 1) << "_power_w";
        file << '\n';
    }

    file << year << '-' << (month < 10 ? "0" : "") << month
         << '-' << (day < 10 ? "0" : "") << day << ','
         << std::fixed << std::setprecision(2)
         << localHour << ','
         << sun.elevation << ','
         << sun.azimuth << ','
         << sun.declination << ','
         << sun.sunrise << ','
         << sun.sunset << ','
         << solarPanel.GetSunlitPanelCount() << ','
         << solarPanel.GetShadedPanelCount() << ','
         << solarPanel.GetAverageSunlightPercent() << ','
         << irradianceWm2 << ','
         << instantaneousPowerW << ','
         << dailyEnergyKWh << ','
         << (weatherAvailable ? 1 : 0) << ','
         << (weatherAvailable ? weatherData.temperature : 0.0) << ','
         << (weatherAvailable ? weatherData.cloudCover : 0.0);

    for (float power : panelPowerValues)
        file << ',' << power;

    file << '\n';
    return file.good();
}

// --------------------------------------------------
// MAIN
// --------------------------------------------------

int main()
{
    try
    {
        // ------------------------------------------
        // 1. CREATE WINDOW
        // ------------------------------------------

        Window window(
            1280,
            720,
            "Rooftop Solar Shadow Analyzer - M10"
        );

        GLFWwindow* nativeWindow =
            window.getNativeWindow();

        glfwSetWindowUserPointer(
            nativeWindow,
            &camera
        );

        // ------------------------------------------
        // 2. INITIALIZE IMGUI
        // ------------------------------------------

        IMGUI_CHECKVERSION();

        ImGui::CreateContext();

        ImGui::StyleColorsDark();

        ImGui::GetStyle().WindowRounding = 6.0f;

        ImGui_ImplGlfw_InitForOpenGL(
            nativeWindow,
            true
        );

        ImGui_ImplOpenGL3_Init("#version 330");

        // ------------------------------------------
        // 3. LOAD SHADERS
        // ------------------------------------------

        std::string root = PROJECT_ROOT;

        Shader lightingShader(
            root + "/shaders/lighting.vert",
            root + "/shaders/lighting.frag"
        );

        Shader shadowShader(
            root + "/shaders/shadow.vert",
            root + "/shaders/shadow.frag"
        );

        // ------------------------------------------
        // 4. LOAD MODEL
        // ------------------------------------------

        Model building(
            root + "/assets/models/building.obj"
        );

        // ------------------------------------------
        // 5. CREATE GROUND
        // ------------------------------------------

        Mesh ground = CreateGround();

        // ------------------------------------------
        // 6. CREATE SHADOW MAP
        // ------------------------------------------

        ShadowMap shadowMap;

        // ------------------------------------------
        // 6B. M5 HEAT-MAP RESOURCES
        // ------------------------------------------

        Shader heatmapBakeShader(
            root + "/shaders/heatmap_bake.vert",
            root + "/shaders/heatmap_bake.frag"
        );

        Shader heatmapDisplayShader(
            root + "/shaders/heatmap_display.vert",
            root + "/shaders/heatmap_display.frag"
        );

        HeatmapBaker heatmapBaker;

        // ------------------------------------------
        // M6: SOLAR PANEL SYSTEM
        // ------------------------------------------
        SolarPanel solarPanel;
        WeatherClient weatherClient;
        WeatherData weatherData;

        const bool weatherAvailable = weatherClient.FetchWeather(
            28.6139,
            77.2090,
            weatherData
        );

        if (weatherAvailable)
        {
            std::cout << "\n===== LIVE WEATHER DATA =====\n";
            std::cout << "Temperature: "
                    << weatherData.temperature << " C\n";
            std::cout << "Humidity: "
                    << weatherData.humidity << " %\n";
            std::cout << "Cloud cover: "
                    << weatherData.cloudCover << " %\n";
            std::cout << "Shortwave radiation: "
                    << weatherData.shortwaveRadiation << " W/m2\n";
            std::cout << "Direct radiation: "
                    << weatherData.directRadiation << " W/m2\n";
            std::cout << "Diffuse radiation: "
                    << weatherData.diffuseRadiation << " W/m2\n";
        }
        else
        {
            std::cerr << "Weather API error: "
                    << weatherClient.GetLastError()
                    << std::endl;
        }
        solarPanel.GenerateLayout(
            2.0f,   // Rooftop width
            2.0f,   // Rooftop depth
            0.40f,  // Panel width
            0.60f,  // Panel depth
            0.08f   // Panel spacing
        );


        // ------------------------------------------
        // 7. OPENGL SETTINGS
        // ------------------------------------------

        glEnable(GL_DEPTH_TEST);

        glEnable(GL_POLYGON_OFFSET_FILL);

        float deltaTime = 0.0f;
        float lastFrame = 0.0f;

        // Set shadow texture sampler to texture unit 1.
        lightingShader.Use();

        GLint shadowLocation = glGetUniformLocation(
            lightingShader.ID,
            "shadowMap"
        );

        glUniform1i(shadowLocation, 1);

        // ------------------------------------------
        // 8. MAIN LOOP
        // ------------------------------------------

        while (!window.shouldClose())
        {
            float currentFrame =
                static_cast<float>(glfwGetTime());

            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            deltaTime = std::min(deltaTime, 0.1f);

            window.pollEvents();

            processInput(
                nativeWindow,
                deltaTime
            );

            // --------------------------------------
            // 9. UPDATE SIMULATION TIME
            // --------------------------------------

            if (animationPlaying)
            {
                localTime +=
                    deltaTime * animationSpeed / 60.0f;

                if (localTime >= 23.99f)
                {
                    localTime = 23.99f;
                    animationPlaying = false;
                }
            }

            // --------------------------------------
            // 10. CALCULATE SUN
            // --------------------------------------

            // Use the selected seasonal date for the live panel simulation.
            int simulationYear = selectedYear;
            int simulationMonth = selectedMonth;
            int simulationDay = selectedDay;

            GetHeatmapDate(
                heatmapMode,
                selectedYear,
                selectedMonth,
                selectedDay,
                simulationYear,
                simulationMonth,
                simulationDay
            );

            SolarPosition sun =
                sunCalculator.Calculate(
                    simulationYear,
                    simulationMonth,
                    simulationDay,
                    localTime
                );

            // M6.4: Evaluate direct sunlight and neighbour obstruction
            // for each panel using the current Sun direction and tilt.
            solarPanel.UpdateSunExposure(
                sun.direction,
                sun.sunAboveHorizon
            );

            // M7: Weather-adjusted solar power and energy estimation.
            const float exposureFactor = std::clamp(
                solarPanel.GetAverageSunlightPercent() / 100.0f,
                0.0f,
                1.0f
            );

            const float effectiveArea = solarPanel.GetTotalArea();
            const float ratedPowerW =
                effectiveArea * STANDARD_IRRADIANCE * panelEfficiency;

            
// M9: Interpolate the forecast radiation at the current simulation time.
// If the API reports zero/no data, use the documented cloud-cover fallback.
const float cloudFraction = weatherAvailable
    ? std::clamp(
        static_cast<float>(weatherData.cloudCover) / 100.0f,
        0.0f,
        1.0f
      )
    : 0.0f;

const float estimatedIrradiance =
    850.0f * (1.0f - 0.75f * cloudFraction);



double hourlyShortwaveNow = 0.0;
double hourlyDirectNow = 0.0;
double hourlyDiffuseNow = 0.0;
const bool hasCurrentHourlyRadiation = weatherAvailable &&
    weatherData.GetRadiationAtHour(
        localTime,
        hourlyShortwaveNow,
        hourlyDirectNow,
        hourlyDiffuseNow
    );

const float actualIrradiance =
    hasCurrentHourlyRadiation && hourlyShortwaveNow > 1.0
    ? static_cast<float>(hourlyShortwaveNow)
    : estimatedIrradiance;


const float safeIrradiance = std::clamp(
    actualIrradiance,
    0.0f,
    1200.0f
);


            const float ambientTemperature = weatherAvailable
                ? static_cast<float>(weatherData.temperature)
                : 25.0f;

            // Simplified NOCT-based panel temperature estimate.
            const float estimatedPanelTemperature =
                ambientTemperature +
                ((45.0f - 20.0f) / 800.0f) * safeIrradiance;

            // Approximate module power coefficient: -0.4% per degree C
            // above 25 C. This is a simplified model assumption.
            const float temperatureFactor = std::clamp(
                1.0f - 0.004f *
                    (estimatedPanelTemperature - 25.0f),
                0.70f, 1.05f
            );

            const float instantaneousPowerW =
                sun.sunAboveHorizon
                ? effectiveArea * safeIrradiance *
                    panelEfficiency * SYSTEM_PERFORMANCE_RATIO *
                    exposureFactor * temperatureFactor
                : 0.0f;

            // Daily energy is a weather-adjusted projection using the
            // current irradiance and selected peak-sun-hours assumption.
            const float irradianceFactor =
                safeIrradiance / STANDARD_IRRADIANCE;

            const float estimatedDailyEnergyKWh =
                (ratedPowerW / 1000.0f) *
                SYSTEM_PERFORMANCE_RATIO *
                peakSunHours *
                irradianceFactor *
                exposureFactor *
                temperatureFactor;

            const float estimatedMonthlyEnergyKWh =
                estimatedDailyEnergyKWh * 30.0f;

            const float estimatedYearlyEnergyKWh =
                estimatedDailyEnergyKWh * 365.0f;

            float elevation =
                static_cast<float>(sun.elevation);

            float daylightIntensity = std::clamp(
                std::sin(glm::radians(elevation)),
                0.0f,
                1.0f
            );

            glm::vec3 sunriseColor(
                1.0f, 0.55f, 0.28f
            );

            glm::vec3 middayColor(
                1.0f, 0.96f, 0.85f
            );

            float colorMix = std::clamp(
                elevation / 40.0f,
                0.0f,
                1.0f
            );

            glm::vec3 sunColor = glm::mix(
                sunriseColor,
                middayColor,
                colorMix
            );

            float lightIntensity =
                sun.sunAboveHorizon
                ? 0.15f + 0.85f * daylightIntensity
                : 0.025f;

            sunColor *= lightIntensity;

            // --------------------------------------
            // 11. CALCULATE LIGHT SPACE MATRIX
            // --------------------------------------

            glm::mat4 lightSpaceMatrix =
                CalculateLightSpaceMatrix(
                    sun.direction
                );

            // --------------------------------------
            // 12. FIRST PASS: SHADOW DEPTH
            // --------------------------------------

            bool renderShadows =
                shadowsEnabled &&
                sun.sunAboveHorizon;

            if (renderShadows)
            {
                shadowMap.BindForWriting();
                glEnable(GL_POLYGON_OFFSET_FILL);


                // Reduce depth fighting on shadow surfaces.
                glPolygonOffset(2.0f, 4.0f);

                shadowShader.Use();

                GLint lightMatrixLocation =
                    glGetUniformLocation(
                        shadowShader.ID,
                        "lightSpaceMatrix"
                    );

                glUniformMatrix4fv(
                    lightMatrixLocation,
                    1,
                    GL_FALSE,
                    &lightSpaceMatrix[0][0]
                );

                // Render all shadow-casting geometry.
                RenderScene(
                    building,
                    ground,
                    shadowShader,
                    true
                );

                glDisable(GL_POLYGON_OFFSET_FILL);

                shadowMap.Unbind();
            }

            // --------------------------------------
            // 13. RESTORE NORMAL VIEWPORT
            // --------------------------------------

            int width, height;

            glfwGetFramebufferSize(
                nativeWindow,
                &width,
                &height
            );

            if (width == 0 || height == 0)
                continue;

            glViewport(0, 0, width, height);

            // --------------------------------------
            // 14. SKY COLOUR
            // --------------------------------------

            glm::vec3 nightSky(
                0.025f, 0.035f, 0.10f
            );

            glm::vec3 daySky(
                0.48f, 0.68f, 0.86f
            );

            glm::vec3 skyColor = glm::mix(
                nightSky,
                daySky,
                daylightIntensity
            );

            glClearColor(
                skyColor.r,
                skyColor.g,
                skyColor.b,
                1.0f
            );

            glClear(
                GL_COLOR_BUFFER_BIT |
                GL_DEPTH_BUFFER_BIT
            );

            // --------------------------------------
            // 15. CAMERA MATRICES
            // --------------------------------------

            glm::mat4 projection =
                glm::perspective(
                    glm::radians(camera.Zoom),
                    static_cast<float>(width) /
                        static_cast<float>(height),
                    0.1f,
                    100.0f
                );

            glm::mat4 view =
                camera.GetViewMatrix();

            // --------------------------------------
            // 16. SECOND PASS: NORMAL RENDERING
            // --------------------------------------

            lightingShader.Use();

            lightingShader.SetMat4(
                "view",
                view
            );

            lightingShader.SetMat4(
                "projection",
                projection
            );

            lightingShader.SetMat4(
                "lightSpaceMatrix",
                lightSpaceMatrix
            );

            lightingShader.SetVec3(
                "viewPos",
                camera.Position
            );

            lightingShader.SetVec3(
                "lightDir",
                sun.direction
            );

            lightingShader.SetVec3(
                "lightColor",
                sunColor
            );

            lightingShader.SetFloat(
                "ambientStrength",
                0.28f
            );

            lightingShader.SetFloat(
                "specularStrength",
                0.35f
            );

            lightingShader.SetFloat(
                "shininess",
                32.0f
            );

            // Enable shadow comparison only in daylight.
            GLint shadowToggleLocation =
                glGetUniformLocation(
                    lightingShader.ID,
                    "shadowsEnabled"
                );

            glUniform1i(
                shadowToggleLocation,
                renderShadows ? GL_TRUE : GL_FALSE
            );

            GLint pcfLocation =
                glGetUniformLocation(
                    lightingShader.ID,
                    "pcfEnabled"
                );

            glUniform1i(
                pcfLocation,
                pcfEnabled ? GL_TRUE : GL_FALSE
            );

            // Bind depth texture to texture unit 1.
            shadowMap.BindForReading(1);

            // --------------------------------------
            // M6 DIAGNOSTICS: NORMAL SCENE + PANELS
            // --------------------------------------

            // Discard errors from earlier stages so this test
            // identifies errors generated by the scene pass.
            ClearOpenGLErrors();

            RenderScene(
                building,
                ground,
                lightingShader,
                false
            );

            CheckOpenGLError("After RenderScene");

            // M6: Draw rooftop solar panels.
            

            // M5: Draw the heatmap directly above the building roof.
// M5: Draw the heatmap overlay on the rooftop.
if (heatmapEnabled && heatmapBaked)
{
    glEnable(GL_DEPTH_TEST);

    heatmapBaker.Draw(
        heatmapDisplayShader,
        view,
        projection
    );

    CheckOpenGLError("After HeatmapBaker Draw");
}

// M6: Draw solar panels only when enabled.
// Keep panels above the heatmap.
if (showSolarPanels)
{
    solarPanel.Draw(lightingShader);
    CheckOpenGLError("After SolarPanel Draw");
}

// --------------------------------------
// 17. IMGUI
// --------------------------------------

            // --------------------------------------
            // 17. IMGUI
            // --------------------------------------

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();

            ImGui::NewFrame();

            ImGui::SetNextWindowSize(
                ImVec2(390.0f, 600.0f),
                ImGuiCond_FirstUseEver
            );

            ImGui::Begin(
                "ROOFTOP SOLAR ANALYZER"
            );

            ImGui::Text("DELHI SOLAR SIMULATION");

            ImGui::Separator();

            ImGui::Text(
                "Location: Delhi, India"
            );

            ImGui::Text(
                "Latitude: %.4f N",
                SunCalculator::LATITUDE
            );

            ImGui::Text(
                "Longitude: %.4f E",
                SunCalculator::LONGITUDE
            );

            ImGui::Text("Timezone: IST UTC+05:30");

            ImGui::Separator();

            // --------------------------------------
            // DATE
            // --------------------------------------

            ImGui::Text("DATE");

            ImGui::InputInt("Year", &selectedYear);
            ImGui::InputInt("Month", &selectedMonth);
            ImGui::InputInt("Day", &selectedDay);

            selectedYear = std::clamp(
                selectedYear, 1900, 2100
            );

            selectedMonth = std::clamp(
                selectedMonth, 1, 12
            );

            int maxDay = 31;

            if (selectedMonth == 2)
            {
                bool leap =
                    (selectedYear % 400 == 0) ||
                    (selectedYear % 4 == 0 &&
                     selectedYear % 100 != 0);

                maxDay = leap ? 29 : 28;
            }
            else if (
                selectedMonth == 4 ||
                selectedMonth == 6 ||
                selectedMonth == 9 ||
                selectedMonth == 11
            )
            {
                maxDay = 30;
            }

            selectedDay = std::clamp(
                selectedDay, 1, maxDay
            );

            ImGui::Separator();

            // --------------------------------------
            // TIME
            // --------------------------------------

            ImGui::Text("LOCAL TIME (IST)");

            ImGui::SliderFloat(
                "Time of day",
                &localTime,
                0.0f,
                23.99f,
                "%.2f hours"
            );

            ImGui::Text(
                "Clock: %s IST",
                FormatTime(localTime).c_str()
            );

            if (ImGui::Button(
                animationPlaying ? "PAUSE" : "PLAY",
                ImVec2(100, 35)
            ))
            {
                animationPlaying = !animationPlaying;
            }

            ImGui::SameLine();

            if (ImGui::Button("RESET"))
            {
                localTime = 12.0f;
                animationPlaying = false;
            }

            ImGui::SliderFloat(
                "Speed",
                &animationSpeed,
                1.0f,
                120.0f,
                "%.0f min/sec"
            );

            ImGui::Separator();

            // --------------------------------------
            // SHADOW CONTROLS
            // --------------------------------------

            ImGui::Text("SHADOW SETTINGS");

            ImGui::Checkbox(
                "Enable shadows",
                &shadowsEnabled
            );
            ImGui::SameLine();

ImGui::Checkbox(
    "Show Solar Panels",
    &showSolarPanels
);
            ImGui::Checkbox(
                "Enable PCF soft shadows",
                &pcfEnabled
            );

            ImGui::Text(
                "Shadow resolution: %u x %u",
                ShadowMap::WIDTH,
                ShadowMap::HEIGHT
            );

            ImGui::Text(
                "Shadow status: %s",
                renderShadows ? "ACTIVE" : "INACTIVE"
            );

            ImGui::Separator();

            // --------------------------------------
            // M6.3: SOLAR PANEL TILT CONTROLS
            // --------------------------------------

            ImGui::Text("SOLAR PANEL SETTINGS");

            float panelTiltAngle = solarPanel.GetTiltAngle();

            if (ImGui::SliderFloat(
                "Panel tilt angle",
                &panelTiltAngle,
                0.0f,
                45.0f,
                "%.1f degrees"
            ))
            {
                solarPanel.SetTiltAngle(panelTiltAngle);
            }

            ImGui::Text(
                "Panel count: %d",
                solarPanel.GetPanelCount()
            );

            ImGui::Text(
                "Total panel area: %.2f square units",
                solarPanel.GetTotalArea()
            );

            // M6.4: live panel-wise direct sunlight analysis.
            ImGui::Text("PANEL SHADOW ANALYSIS");

            ImGui::Text(
                "Sunlit panels: %d / %d",
                solarPanel.GetSunlitPanelCount(),
                solarPanel.GetPanelCount()
            );

            ImGui::Text(
                "Shaded panels: %d",
                solarPanel.GetShadedPanelCount()
            );

            ImGui::Text(
                "Average direct exposure: %.1f%%",
                solarPanel.GetAverageSunlightPercent()
            );

            const auto& panelResults = solarPanel.GetPanelExposure();

            if (ImGui::TreeNode("Individual panel results"))
            {
                for (size_t i = 0; i < panelResults.size(); ++i)
                {
                    ImGui::Text(
                        "Panel %02zu: %s | %.1f%%",
                        i + 1,
                        panelResults[i].shaded ? "SHADED" : "SUNLIT",
                        panelResults[i].sunlightPercent
                    );
                }
                ImGui::TreePop();
            }

            ImGui::Separator();

            // --------------------------------------
            // M7: LIVE WEATHER CONDITIONS
            // --------------------------------------

            ImGui::Text("LIVE WEATHER CONDITIONS");

            if (weatherAvailable)
            {
                ImGui::Text("Temperature: %.1f C", weatherData.temperature);
                ImGui::Text("Humidity: %.0f%%", weatherData.humidity);
                ImGui::Text("Cloud cover: %.0f%%", weatherData.cloudCover);

ImGui::Text(
    "Hourly forecast shortwave: %.1f W/m2",
    hourlyShortwaveNow
);

ImGui::Text(
    "Current model irradiance: %.1f W/m2",
    safeIrradiance
);

ImGui::Text(
    "Direct radiation: %.1f W/m2",
    weatherData.directRadiation
);

ImGui::Text(
    "Diffuse radiation: %.1f W/m2",
    weatherData.diffuseRadiation
);
                ImGui::Text(
                    "Estimated panel temperature: %.1f C",
                    estimatedPanelTemperature
                );
            }
            else
            {
                ImGui::TextColored(
                    ImVec4(1.0f, 0.5f, 0.2f, 1.0f),
                    "Live weather unavailable - using fallback model."
                );
            }

            ImGui::Separator();

            // M6.5 / M7: SOLAR ENERGY GENERATION
            // --------------------------------------

            ImGui::Text("SOLAR ENERGY GENERATION");

            // Store efficiency internally as a fraction (0.20),
            // but show the slider to the user as a percentage (20).
            float panelEfficiencyPercent = panelEfficiency * 100.0f;

            if (ImGui::SliderFloat(
                "Panel efficiency",
                &panelEfficiencyPercent,
                10.0f,
                25.0f,
                "%.0f%%"
            ))
            {
                panelEfficiency = panelEfficiencyPercent / 100.0f;
            }

            ImGui::SliderFloat(
                "Peak sun hours/day",
                &peakSunHours,
                3.0f,
                7.0f,
                "%.1f hours"
            );

            ImGui::Text(
                "System performance ratio: %.0f%%",
                SYSTEM_PERFORMANCE_RATIO * 100.0f
            );

            ImGui::Text(
                "Rated DC capacity: %.1f W",
                ratedPowerW
            );

            ImGui::Text(
                "Instantaneous estimated power: %.1f W",
                instantaneousPowerW
            );

            ImGui::Separator();

            ImGui::Text("ESTIMATED ENERGY");

            ImGui::Text(
                "Daily: %.3f kWh",
                estimatedDailyEnergyKWh
            );

            ImGui::Text(
                "Monthly (30 days): %.2f kWh",
                estimatedMonthlyEnergyKWh
            );

            ImGui::Text(
                "Yearly (365 days): %.2f kWh",
                estimatedYearlyEnergyKWh
            );

            ImGui::TextWrapped(
                "Energy estimates use available API solar irradiance, "
                "panel efficiency, estimated panel temperature, panel "
                "exposure, and an 80%% system performance ratio. Daily "
                "energy is a weather-adjusted projection based on the "
                "selected peak-sun-hours assumption, not measured production."
            );

            ImGui::Separator();

            // --------------------------------------
            // M6.6: SOLAR PERFORMANCE DASHBOARD
            // --------------------------------------

            ImGui::Text("SOLAR PERFORMANCE DASHBOARD");

            const float capacityUtilization = ratedPowerW > 0.0f
                ? std::clamp(instantaneousPowerW / ratedPowerW, 0.0f, 1.0f)
                : 0.0f;

            ImGui::Text(
                "Current capacity utilization: %.1f%%",
                capacityUtilization * 100.0f
            );

            ImGui::ProgressBar(
                capacityUtilization,
                ImVec2(-1.0f, 14.0f),
                "Current output / rated DC capacity"
            );

            ImGui::Text("PANEL-WISE PERFORMANCE");

            const auto& dashboardPanelResults =
                solarPanel.GetPanelExposure();

            std::vector<float> panelPowerValues;
            panelPowerValues.reserve(dashboardPanelResults.size());

            const float panelArea =
                solarPanel.GetPanelCount() > 0
                ? solarPanel.GetTotalArea() /
                    static_cast<float>(solarPanel.GetPanelCount())
                : 0.0f;

            for (const auto& panelResult : dashboardPanelResults)
            {
                const float panelPower =
                    panelArea *
                    safeIrradiance *
                    panelEfficiency *
                    SYSTEM_PERFORMANCE_RATIO *
                    temperatureFactor *
                    (panelResult.sunlightPercent / 100.0f);

                panelPowerValues.push_back(panelPower);
            }

            if (!panelPowerValues.empty())
            {
                ImGui::PlotHistogram(
                    "Estimated panel output (W)",
                    panelPowerValues.data(),
                    static_cast<int>(panelPowerValues.size()),
                    0,
                    nullptr,
                    0.0f,
                    panelArea * safeIrradiance * panelEfficiency *
                        SYSTEM_PERFORMANCE_RATIO * temperatureFactor,
                    ImVec2(-1.0f, 90.0f)
                );

                ImGui::Text(
                    "Panel output range: %.1f W to %.1f W",
                    *std::min_element(
                        panelPowerValues.begin(),
                        panelPowerValues.end()
                    ),
                    *std::max_element(
                        panelPowerValues.begin(),
                        panelPowerValues.end()
                    )
                );
            }
            else
            {
                ImGui::Text("Panel performance data unavailable.");
            }

            if (ImGui::Button("EXPORT SEASONAL RESULT TO CSV"))
            {
                const bool exported = ExportSeasonReportCSV(
                    simulationYear,
                    simulationMonth,
                    simulationDay,
                    localTime,
                    sun,
                    solarPanel,
                    weatherData,
                    weatherAvailable,
                    safeIrradiance,
                    instantaneousPowerW,
                    estimatedDailyEnergyKWh,
                    panelPowerValues
                );

                if (exported)
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f),
                        "Saved: solar_seasonal_comparison.csv");
                else
                    ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.3f, 1.0f),
                        "CSV export failed. Check folder permissions.");
            }

            ImGui::Separator();

            ImGui::Text("ENERGY SUMMARY");

            ImGui::BulletText(
                "Daily estimate: %.3f kWh",
                estimatedDailyEnergyKWh
            );

            ImGui::BulletText(
                "30-day estimate: %.2f kWh",
                estimatedMonthlyEnergyKWh
            );

            ImGui::BulletText(
                "365-day estimate: %.2f kWh",
                estimatedYearlyEnergyKWh
            );

            ImGui::TextWrapped(
                "Dashboard chart shows estimated panel output adjusted for "
                "current weather irradiance, panel temperature and direct "
                "exposure. It is not a measured time-series or historical chart."
            );

            ImGui::Separator();

            // --------------------------------------
            // SUN INFORMATION
            // --------------------------------------

            ImGui::Text("SUN POSITION");

            ImGui::Text(
                "Elevation: %.2f degrees",
                sun.elevation
            );

            ImGui::Text(
                "Azimuth: %.2f degrees",
                sun.azimuth
            );

            ImGui::Text(
                "Declination: %.2f degrees",
                sun.declination
            );

            ImGui::Text(
                "Hour angle: %.2f degrees",
                sun.hourAngle
            );

            ImGui::Text(
                "Equation of time: %.2f min",
                sun.equationOfTime
            );

            ImGui::Separator();

            // --------------------------------------
            // SUNRISE AND SUNSET
            // --------------------------------------

            ImGui::Text("SUNRISE / SUNSET");

            if (sun.validSunriseSunset)
            {
                ImGui::Text(
                    "Sunrise: %s IST",
                    FormatTime(sun.sunrise).c_str()
                );

                ImGui::Text(
                    "Sunset: %s IST",
                    FormatTime(sun.sunset).c_str()
                );
            }
            else
            {
                ImGui::Text("No sunrise/sunset data");
            }

            ImGui::Text(
                "Sun above horizon: %s",
                sun.sunAboveHorizon ? "YES" : "NO"
            );

            ImGui::Separator();

            // --------------------------------------
            // M5: ROOFTOP SOLAR HEAT-MAP
            // --------------------------------------

            ImGui::Text("ROOFTOP SOLAR HEAT-MAP");

            const char* heatmapModes[] =
            {
                "Selected date",
                "Summer solstice (21 June)",
                "Equinox (20 March)",
                "Winter solstice (21 December)"
            };

            ImGui::Combo(
                "Seasonal analysis",
                &heatmapMode,
                heatmapModes,
                IM_ARRAYSIZE(heatmapModes)
            );

            ImGui::Text(
                "Sampling interval: %.0f minutes",
                HEATMAP_INTERVAL
            );

            int bakeYear = selectedYear;
            int bakeMonth = selectedMonth;
            int bakeDay = selectedDay;

            GetHeatmapDate(
                heatmapMode,
                selectedYear,
                selectedMonth,
                selectedDay,
                bakeYear,
                bakeMonth,
                bakeDay
            );

            ImGui::Text(
                "Bake date: %04d-%02d-%02d",
                bakeYear,
                bakeMonth,
                bakeDay
            );

            if (ImGui::Button(
                "BAKE SOLAR HEAT-MAP",
                ImVec2(220.0f, 36.0f)
            ))
            {
                animationPlaying = false;

             heatmapBaked = BakeHeatmap(
    heatmapBaker,
    shadowMap,
    shadowShader,
    heatmapBakeShader,
    building,
    ground,
    solarPanel,
    sunCalculator,
    bakeYear,
    bakeMonth,
    bakeDay,
    weatherData,
    weatherAvailable
);
if (heatmapBaked)
{
    heatmapBaker.PrintStatistics();
}
            }

            ImGui::SameLine();

            ImGui::Checkbox(
                "Show overlay",
                &heatmapEnabled
            );

            ImGui::Text(
                "Heat-map status: %s",
                heatmapBaked ? "BAKED" : "NOT BAKED"
            );

            ImGui::TextWrapped(
                "Colour scale: blue = lower exposure, "
                "red = higher exposure. Re-bake after "
                "changing the date or season."
            );

            ImGui::Separator();

            // --------------------------------------
            // PERFORMANCE
            // --------------------------------------

            float fps = deltaTime > 0.00001f
                ? 1.0f / deltaTime
                : 0.0f;

            ImGui::Text("FPS: %.1f", fps);

            ImGui::Text(
                "Camera mode: %s",
                cameraMode ? "3D NAVIGATION" : "UI CONTROL"
            );

            ImGui::Text("TAB toggles camera control.");

            ImGui::End();

            // --------------------------------------
            // 18. RENDER GUI
            // --------------------------------------

            ImGui::Render();

            ImGui_ImplOpenGL3_RenderDrawData(
                ImGui::GetDrawData()
            );

            // --------------------------------------
            // 19. DISPLAY
            // --------------------------------------

            window.swapBuffers();

#ifndef NDEBUG
            CheckOpenGLError("End of frame (unattributed)");
#endif
        }

        // ------------------------------------------
        // 20. CLEANUP
        // ------------------------------------------

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        ImGui::DestroyContext();
    }
    catch (const std::exception& error)
    {
        std::cerr << "\nAPPLICATION ERROR: "
                  << error.what() << std::endl;

        return -1;
    }

    return 0;
}
