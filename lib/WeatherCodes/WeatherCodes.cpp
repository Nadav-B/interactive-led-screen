#include "WeatherCodes.h"

namespace weather {

Condition conditionFromCode(int code) {
  // WMO weather interpretation codes, as used by Open-Meteo.
  if (code == 0) return Condition::Clear;
  if (code >= 1 && code <= 3) return Condition::PartlyCloudy;
  if (code == 45 || code == 48) return Condition::Fog;
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return Condition::Rain;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return Condition::Snow;
  if (code >= 95 && code <= 99) return Condition::Thunder;
  return Condition::Unknown;
}

}  // namespace weather
