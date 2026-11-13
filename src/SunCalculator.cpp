
#include "SunCalculator.hpp"

#include <cmath>
#include <algorithm>

namespace
{
    constexpr double PI = 3.14159265358979323846;

    double ToRadians(double degrees)
    {
        return degrees * PI / 180.0;
    }

    double ToDegrees(double radians)
    {
        return radians * 180.0 / PI;
    }
}

bool SunCalculator::IsLeapYear(int year) const
{
    return (year % 400 == 0) ||
           (year % 4 == 0 && year % 100 != 0);
}

int SunCalculator::DaysInMonth(
    int year,
    int month
) const
{
    const int days[] =
    {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    if (month < 1 || month > 12)
        return 30;

    if (month == 2 && IsLeapYear(year))
        return 29;

    return days[month - 1];
}

int SunCalculator::DayOfYear(
    int year,
    int month,
    int day
) const
{
    int result = day;

    for (int m = 1; m < month; m++)
    {
        result += DaysInMonth(year, m);
    }

    return result;
}

double SunCalculator::NormalizeDegrees(
    double angle
) const
{
    angle = std::fmod(angle, 360.0);

    if (angle < 0.0)
        angle += 360.0;

    return angle;
}

SolarPosition SunCalculator::Calculate(
    int year,
    int month,
    int day,
    double localTime
) const
{
    SolarPosition result;

    // Validate input date.
    if (year < 1900 || year > 2100 ||
        month < 1 || month > 12 ||
        day < 1 || day > DaysInMonth(year, month) ||
        !std::isfinite(localTime) ||
        localTime < 0.0 || localTime >= 24.0)
    {
        return result;
    }

    const int N = DayOfYear(year, month, day);

    // ------------------------------------------
    // 1. FRACTIONAL YEAR ANGLE
    // ------------------------------------------

    // Include fractional day for better accuracy.
    double gamma =
        (2.0 * PI / 365.0) *
        (N - 1 + (localTime - 12.0) / 24.0);

    // ------------------------------------------
    // 2. SOLAR DECLINATION
    // ------------------------------------------

    // NOAA fractional-year approximation.
    double declination =
          0.006918
        - 0.399912 * std::cos(gamma)
        + 0.070257 * std::sin(gamma)
        - 0.006758 * std::cos(2.0 * gamma)
        + 0.000907 * std::sin(2.0 * gamma)
        - 0.002697 * std::cos(3.0 * gamma)
        + 0.001480 * std::sin(3.0 * gamma);

    // ------------------------------------------
    // 3. EQUATION OF TIME
    // ------------------------------------------

    double equationOfTime =
        229.18 * (
              0.000075
            + 0.001868 * std::cos(gamma)
            - 0.032077 * std::sin(gamma)
            - 0.014615 * std::cos(2.0 * gamma)
            - 0.040849 * std::sin(2.0 * gamma)
        );

    // ------------------------------------------
    // 4. TRUE SOLAR TIME
    // ------------------------------------------

    double localMinutes = localTime * 60.0;

    // 82.5 degrees east is the IST standard meridian.
    double timeCorrection =
        equationOfTime +
        4.0 * (LONGITUDE - 82.5);

    double trueSolarTime =
        localMinutes + timeCorrection;

    // Keep time within one day.
    trueSolarTime = std::fmod(
        trueSolarTime + 1440.0,
        1440.0
    );

    // ------------------------------------------
    // 5. HOUR ANGLE
    // ------------------------------------------

    double hourAngle =
        trueSolarTime / 4.0 - 180.0;

    double latitudeRad =
        ToRadians(LATITUDE);

    double hourAngleRad =
        ToRadians(hourAngle);

    // ------------------------------------------
    // 6. SOLAR ELEVATION
    // ------------------------------------------

    double sinElevation =
          std::sin(latitudeRad) * std::sin(declination)
        + std::cos(latitudeRad) * std::cos(declination)
        * std::cos(hourAngleRad);

    sinElevation = std::clamp(
        sinElevation,
        -1.0,
        1.0
    );

    double elevation =
        std::asin(sinElevation);

    // ------------------------------------------
    // 7. SOLAR AZIMUTH
    // ------------------------------------------

    double azimuthRad = std::atan2(
        std::sin(hourAngleRad),
        std::cos(hourAngleRad) * std::sin(latitudeRad)
        - std::tan(declination) * std::cos(latitudeRad)
    );

    // Convert to clockwise degrees from north.
    double azimuth =
        NormalizeDegrees(ToDegrees(azimuthRad) + 180.0);

    // ------------------------------------------
    // 8. SUNRISE AND SUNSET
    // ------------------------------------------

    double cosHourAngle =
        -std::tan(latitudeRad) *
        std::tan(declination);

    if (cosHourAngle >= -1.0 &&
        cosHourAngle <= 1.0)
    {
        double sunriseHourAngle =
            ToDegrees(std::acos(cosHourAngle));

        // Convert apparent solar time to local IST.
        double sunriseMinutes =
            720.0
            - 4.0 * sunriseHourAngle
            - timeCorrection;

        double sunsetMinutes =
            720.0
            + 4.0 * sunriseHourAngle
            - timeCorrection;

        result.sunrise = sunriseMinutes / 60.0;
        result.sunset = sunsetMinutes / 60.0;

        result.validSunriseSunset = true;
    }

    // ------------------------------------------
    // 9. CONVERT SUN POSITION TO 3D VECTOR
    // ------------------------------------------

    double elevationDegrees =
        ToDegrees(elevation);

    double elevationForVector =
        std::max(elevation, 0.0);

    double azimuthForVector = ToRadians(azimuth);

    // World coordinates:
    // X = East, Y = Up, Z = North.
    //
    // Azimuth is measured clockwise from North.

    glm::vec3 sunDirection;

    sunDirection.x =
        static_cast<float>(
            std::cos(elevationForVector) *
            std::sin(azimuthForVector)
        );

    sunDirection.y =
        static_cast<float>(
            std::sin(elevationForVector)
        );

    sunDirection.z =
        static_cast<float>(
            std::cos(elevationForVector) *
            std::cos(azimuthForVector)
        );

    result.direction = glm::normalize(sunDirection);

    // ------------------------------------------
    // 10. STORE RESULTS
    // ------------------------------------------

    result.elevation = elevationDegrees;

    result.azimuth = azimuth;

    result.declination =
        ToDegrees(declination);

    result.equationOfTime =
        equationOfTime;

    result.hourAngle =
        hourAngle;

    result.sunAboveHorizon =
        elevationDegrees > 0.0;

    return result;
}
