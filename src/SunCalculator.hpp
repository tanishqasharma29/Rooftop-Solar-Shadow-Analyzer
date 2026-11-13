
#pragma once

#include <glm/glm.hpp>

struct SolarPosition
{
    double elevation = 0.0;
    double azimuth = 0.0;
    double declination = 0.0;
    double equationOfTime = 0.0;
    double hourAngle = 0.0;

    double sunrise = 0.0;
    double sunset = 0.0;

    bool sunAboveHorizon = false;
    bool validSunriseSunset = false;

    glm::vec3 direction = glm::vec3(0.0f, 1.0f, 0.0f);
};

class SunCalculator
{
public:

    // Delhi coordinates.
    static constexpr double LATITUDE = 28.6139;
    static constexpr double LONGITUDE = 77.2090;

    // Indian Standard Time UTC offset.
    static constexpr double TIMEZONE = 5.5;

    SolarPosition Calculate(
        int year,
        int month,
        int day,
        double localTime
    ) const;

private:

    int DayOfYear(
        int year,
        int month,
        int day
    ) const;

    bool IsLeapYear(int year) const;

    int DaysInMonth(
        int year,
        int month
    ) const;

    double NormalizeDegrees(double angle) const;
};
