#pragma once

#include <string>
#include <vector>
#include <cstddef>

struct HourlyRadiation
{
    double hour = 0.0; // Local decimal hour, e.g. 13.5 = 13:30
    double shortwave = 0.0;
    double direct = 0.0;
    double diffuse = 0.0;
};

struct WeatherData
{
    double temperature = 0.0;
    double humidity = 0.0;
    double cloudCover = 0.0;

    double shortwaveRadiation = 0.0;
    double directRadiation = 0.0;
    double diffuseRadiation = 0.0;

    std::string observationTime;
    std::vector<HourlyRadiation> hourlyRadiation;

    bool valid = false;

    // Linearly interpolates the current forecast day's hourly radiation.
    // Returns false if no usable hourly samples exist.
    bool GetRadiationAtHour(
        double localHour,
        double& shortwave,
        double& direct,
        double& diffuse
    ) const;
};

class WeatherClient
{
public:
    WeatherClient();

    bool FetchWeather(
        double latitude,
        double longitude,
        WeatherData& result
    );

    const std::string& GetLastError() const;

private:
    std::string lastError;

    static size_t WriteCallback(
        void* contents,
        size_t size,
        size_t nmemb,
        void* userData
    );
};
