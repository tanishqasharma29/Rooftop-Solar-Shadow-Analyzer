#include "WeatherClient.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <stdexcept>

using json = nlohmann::json;

namespace
{
    double ReadNumberOrZero(const json& values, std::size_t index)
    {
        if (index >= values.size() || values.at(index).is_null())
            return 0.0;

        return values.at(index).get<double>();
    }

    double ParseLocalHour(const std::string& timestamp)
    {
        // Open-Meteo local timestamps use YYYY-MM-DDTHH:MM.
        if (timestamp.size() < 16)
            return -1.0;

        try
        {
            const int hour = std::stoi(timestamp.substr(11, 2));
            const int minute = std::stoi(timestamp.substr(14, 2));

            if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
                return -1.0;

            return static_cast<double>(hour) +
                   static_cast<double>(minute) / 60.0;
        }
        catch (...)
        {
            return -1.0;
        }
    }
}

bool WeatherData::GetRadiationAtHour(
    double localHour,
    double& shortwave,
    double& direct,
    double& diffuse
) const
{
    if (hourlyRadiation.empty() || !std::isfinite(localHour))
        return false;

    const double requested = std::clamp(localHour, 0.0, 23.999);

    if (requested <= hourlyRadiation.front().hour)
    {
        shortwave = hourlyRadiation.front().shortwave;
        direct = hourlyRadiation.front().direct;
        diffuse = hourlyRadiation.front().diffuse;
        return true;
    }

    if (requested >= hourlyRadiation.back().hour)
    {
        shortwave = hourlyRadiation.back().shortwave;
        direct = hourlyRadiation.back().direct;
        diffuse = hourlyRadiation.back().diffuse;
        return true;
    }

    for (std::size_t i = 1; i < hourlyRadiation.size(); ++i)
    {
        const HourlyRadiation& right = hourlyRadiation[i];

        if (requested <= right.hour)
        {
            const HourlyRadiation& left = hourlyRadiation[i - 1];
            const double span = right.hour - left.hour;
            const double t = span > 1e-9
                ? (requested - left.hour) / span
                : 0.0;

            shortwave = left.shortwave +
                (right.shortwave - left.shortwave) * t;
            direct = left.direct +
                (right.direct - left.direct) * t;
            diffuse = left.diffuse +
                (right.diffuse - left.diffuse) * t;

            return true;
        }
    }

    return false;
}

WeatherClient::WeatherClient()
{
    const CURLcode result = curl_global_init(CURL_GLOBAL_DEFAULT);

    if (result != CURLE_OK)
        lastError = "Failed to initialize CURL.";
}

size_t WeatherClient::WriteCallback(
    void* contents,
    size_t size,
    size_t nmemb,
    void* userData
)
{
    const size_t totalSize = size * nmemb;
    std::string* response = static_cast<std::string*>(userData);

    response->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

bool WeatherClient::FetchWeather(
    double latitude,
    double longitude,
    WeatherData& result
)
{
    result = WeatherData{};
    lastError.clear();

    CURL* curl = curl_easy_init();

    if (!curl)
    {
        lastError = "Could not create CURL request.";
        return false;
    }

    std::ostringstream url;
    url << std::fixed << std::setprecision(4);

    url
        << "https://api.open-meteo.com/v1/forecast?"
        << "latitude=" << latitude
        << "&longitude=" << longitude
        << "&current=temperature_2m,relative_humidity_2m,cloud_cover"
        << "&hourly=temperature_2m,relative_humidity_2m,"
        << "cloud_cover,shortwave_radiation,direct_radiation,"
        << "diffuse_radiation"
        << "&forecast_days=1"
        << "&timezone=Asia%2FKolkata";

    const std::string requestUrl = url.str();
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, requestUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "RooftopSolarShadowAnalyzer/1.0");

    const CURLcode curlResult = curl_easy_perform(curl);

    long httpStatus = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
    curl_easy_cleanup(curl);

    if (curlResult != CURLE_OK || httpStatus != 200)
    {
        lastError =
            "Weather request failed: " +
            std::string(curl_easy_strerror(curlResult));

        if (httpStatus != 200)
            lastError += " | HTTP status: " + std::to_string(httpStatus);

        return false;
    }

    try
    {
        const json data = json::parse(response);

        if (!data.contains("current") || !data.contains("hourly"))
        {
            lastError = "Invalid weather API response.";
            return false;
        }

        const json& current = data.at("current");
        const json& hourly = data.at("hourly");

        result.temperature = current.at("temperature_2m").get<double>();
        result.humidity = current.at("relative_humidity_2m").get<double>();
        result.cloudCover = current.at("cloud_cover").get<double>();
        result.observationTime = current.at("time").get<std::string>();

        const auto& times = hourly.at("time");
        const auto& shortwaveValues = hourly.at("shortwave_radiation");
        const auto& directValues = hourly.at("direct_radiation");
        const auto& diffuseValues = hourly.at("diffuse_radiation");

        const std::size_t count = std::min({
            times.size(),
            shortwaveValues.size(),
            directValues.size(),
            diffuseValues.size()
        });

        result.hourlyRadiation.reserve(count);

        for (std::size_t i = 0; i < count; ++i)
        {
            const std::string timestamp = times.at(i).get<std::string>();
            const double hour = ParseLocalHour(timestamp);

            if (hour < 0.0)
                continue;

            HourlyRadiation sample;
            sample.hour = hour;
            sample.shortwave = ReadNumberOrZero(shortwaveValues, i);
            sample.direct = ReadNumberOrZero(directValues, i);
            sample.diffuse = ReadNumberOrZero(diffuseValues, i);
            if (i < 24)
{
    std::cout
        << "Hour: " << sample.hour
        << " | Shortwave: " << sample.shortwave
        << " | Direct: " << sample.direct
        << " | Diffuse: " << sample.diffuse
        << std::endl;
}
            result.hourlyRadiation.push_back(sample);
        }

        std::sort(
            result.hourlyRadiation.begin(),
            result.hourlyRadiation.end(),
            [](const HourlyRadiation& a, const HourlyRadiation& b)
            {
                return a.hour < b.hour;
            }
        );

        double currentShortwave = 0.0;
        double currentDirect = 0.0;
        double currentDiffuse = 0.0;

        const double currentHour = ParseLocalHour(result.observationTime);

        if (currentHour >= 0.0 &&
            result.GetRadiationAtHour(
                currentHour,
                currentShortwave,
                currentDirect,
                currentDiffuse
            ))
        {
            result.shortwaveRadiation = currentShortwave;
            result.directRadiation = currentDirect;
            result.diffuseRadiation = currentDiffuse;
        }

        result.valid = !result.hourlyRadiation.empty();

        if (!result.valid)
        {
            lastError = "Weather response contained no usable hourly radiation data.";
            return false;
        }

        std::cout
            << "\nWeather data fetched successfully!"
            << "\nTemperature: " << result.temperature << " C"
            << "\nHumidity: " << result.humidity << "%"
            << "\nCloud cover: " << result.cloudCover << "%"
            << "\nCurrent shortwave radiation: "
            << result.shortwaveRadiation << " W/m2"
            << "\nHourly radiation samples: "
            << result.hourlyRadiation.size()
            << std::endl;

        return true;
    }
    catch (const std::exception& exception)
    {
        lastError = "JSON parsing error: " + std::string(exception.what());
        return false;
    }
}

const std::string& WeatherClient::GetLastError() const
{
    return lastError;
}
