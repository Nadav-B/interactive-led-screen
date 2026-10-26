#include "SkyEvents.h"

#include <math.h>

namespace sky {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;
constexpr long kDaySeconds = 86400;
constexpr long kScanStepSeconds = 600;  // coarse scan; crossings are then bisected
constexpr int kBisectSteps = 8;         // 600s / 2^8 ~ 2s

// Standard altitude of the sun's upper limb at rise/set, refraction
// included.
constexpr double kSunHorizonDeg = -0.833;

struct Equatorial {
  double ra;   // radians
  double dec;  // radians
  double parallaxDeg;
};

struct Ecliptic {
  double lon;  // degrees
  double lat;  // degrees
  double parallaxDeg;
};

double daysSinceJ2000(time_t t) { return t / 86400.0 + 2440587.5 - 2451545.0; }

double normDeg(double a) {
  a = fmod(a, 360.0);
  return a < 0 ? a + 360.0 : a;
}

double sunEclipticLon(double d) {
  const double g = (357.529 + 0.98560028 * d) * kDeg;
  const double q = 280.459 + 0.98564736 * d;
  return normDeg(q + 1.915 * sin(g) + 0.020 * sin(2 * g));
}

// Astronomical Almanac low-precision lunar theory (~0.3 deg).
Ecliptic moonEcliptic(double d) {
  const double T = d / 36525.0;
  auto s = [](double deg) { return sin(deg * kDeg); };
  auto c = [](double deg) { return cos(deg * kDeg); };
  Ecliptic e;
  e.lon = normDeg(218.32 + 481267.881 * T + 6.29 * s(135.0 + 477198.87 * T) -
                  1.27 * s(259.3 - 413335.36 * T) + 0.66 * s(235.7 + 890534.22 * T) +
                  0.21 * s(269.9 + 954397.74 * T) - 0.19 * s(357.5 + 35999.05 * T) -
                  0.11 * s(186.5 + 966404.03 * T));
  e.lat = 5.13 * s(93.3 + 483202.02 * T) + 0.28 * s(228.2 + 960400.89 * T) -
          0.28 * s(318.3 + 6003.15 * T) - 0.17 * s(217.6 - 407332.21 * T);
  e.parallaxDeg = 0.9508 + 0.0518 * c(135.0 + 477198.87 * T) +
                  0.0095 * c(259.3 - 413335.36 * T) + 0.0078 * c(235.7 + 890534.22 * T) +
                  0.0028 * c(269.9 + 954397.74 * T);
  return e;
}

Equatorial toEquatorial(double lonDeg, double latDeg, double d) {
  const double eps = (23.439 - 0.00000036 * d) * kDeg;
  const double l = lonDeg * kDeg;
  const double b = latDeg * kDeg;
  const double x = cos(b) * cos(l);
  const double y = cos(eps) * cos(b) * sin(l) - sin(eps) * sin(b);
  const double z = sin(eps) * cos(b) * sin(l) + cos(eps) * sin(b);
  return {atan2(y, x), asin(z), 0.0};
}

Equatorial sunPosition(double d) { return toEquatorial(sunEclipticLon(d), 0.0, d); }

Equatorial moonPosition(double d) {
  const Ecliptic e = moonEcliptic(d);
  Equatorial q = toEquatorial(e.lon, e.lat, d);
  q.parallaxDeg = e.parallaxDeg;
  return q;
}

// Geocentric altitude (degrees) of a body at RA/dec seen from lat/lon.
double altitudeDeg(const Equatorial &p, double d, double latDeg, double lonDeg) {
  const double gmst = normDeg(280.46061837 + 360.98564736629 * d);
  const double hourAngle = (gmst + lonDeg) * kDeg - p.ra;
  const double lat = latDeg * kDeg;
  return asin(sin(lat) * sin(p.dec) + cos(lat) * cos(p.dec) * cos(hourAngle)) / kDeg;
}

// Altitude above the body's own rise/set threshold; positive = up.
double sunAbove(time_t t, double lat, double lon) {
  const double d = daysSinceJ2000(t);
  return altitudeDeg(sunPosition(d), d, lat, lon) - kSunHorizonDeg;
}

double moonAbove(time_t t, double lat, double lon) {
  const double d = daysSinceJ2000(t);
  const Equatorial p = moonPosition(d);
  // Parallax, refraction and semi-diameter folded into one threshold.
  const double horizon = 0.7275 * p.parallaxDeg - 0.5667;
  return altitudeDeg(p, d, lat, lon) - horizon;
}

template <typename AboveFn>
RiseSet scanDay(time_t dayStart, double lat, double lon, AboveFn above) {
  RiseSet out;
  time_t t0 = dayStart;
  double a0 = above(t0, lat, lon);
  for (long step = kScanStepSeconds; step <= kDaySeconds; step += kScanStepSeconds) {
    const time_t t1 = dayStart + step;
    const double a1 = above(t1, lat, lon);
    if ((a0 < 0) != (a1 < 0)) {
      const bool rising = a1 >= 0;
      time_t lo = t0, hi = t1;
      for (int i = 0; i < kBisectSteps; ++i) {
        const time_t mid = lo + (hi - lo) / 2;
        if ((above(mid, lat, lon) >= 0) == rising) {
          hi = mid;
        } else {
          lo = mid;
        }
      }
      const time_t when = lo + (hi - lo) / 2;
      if (when < dayStart + kDaySeconds) {
        if (rising && !out.hasRise) {
          out.hasRise = true;
          out.rise = when;
        } else if (!rising && !out.hasSet) {
          out.hasSet = true;
          out.set = when;
        }
      }
    }
    t0 = t1;
    a0 = a1;
  }
  return out;
}

}  // namespace

RiseSet sunRiseSet(time_t dayStart, double latDeg, double lonDeg) {
  return scanDay(dayStart, latDeg, lonDeg, sunAbove);
}

RiseSet moonRiseSet(time_t dayStart, double latDeg, double lonDeg) {
  return scanDay(dayStart, latDeg, lonDeg, moonAbove);
}

double sunAltitudeDeg(time_t t, double latDeg, double lonDeg) {
  const double d = daysSinceJ2000(t);
  return altitudeDeg(sunPosition(d), d, latDeg, lonDeg);
}

MoonPhase moonPhase(time_t t) {
  const double d = daysSinceJ2000(t);
  const Ecliptic m = moonEcliptic(d);
  const double elongation = normDeg(m.lon - sunEclipticLon(d));
  const double cosPsi = cos(m.lat * kDeg) * cos(elongation * kDeg);
  MoonPhase p;
  p.age = elongation / 360.0;
  p.illumination = (1.0 - cosPsi) / 2.0;
  return p;
}

}  // namespace sky
