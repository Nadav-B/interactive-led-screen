#pragma once

// WMO weather-interpretation code -> display condition. Pure and free of
// Arduino/matrix dependencies so it builds and runs on the host (see the
// `native` env in platformio.ini and test/test_weather_codes).
// WeatherDisplay re-exports Condition and conditionFromCode() unchanged.
namespace weather {

enum class Condition { Clear, PartlyCloudy, Fog, Rain, Snow, Thunder, Unknown };

// Maps an Open-Meteo/WMO weather code to a Condition.
Condition conditionFromCode(int code);

}  // namespace weather
