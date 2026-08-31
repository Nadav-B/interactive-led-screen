#include "DepartureParsing.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace bvg {

namespace {
void stripPrefix(std::string &s, const char *prefix) {
  const size_t n = std::strlen(prefix);
  if (s.compare(0, n, prefix) == 0) {
    s.erase(0, n);
  }
}

void trim(std::string &s) {
  size_t start = 0;
  while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
  size_t end = s.size();
  while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
  s = s.substr(start, end - start);
}
}  // namespace

std::string shortenName(const char *name) {
  std::string s(name ? name : "");

  const size_t berlin = s.find("(Berlin)");
  if (berlin != std::string::npos) {
    s.erase(berlin, std::strlen("(Berlin)"));
  }
  trim(s);

  // Only one of these can match (they're mutually exclusive prefixes).
  if (s.compare(0, 4, "S+U ") == 0) {
    stripPrefix(s, "S+U ");
  } else if (s.compare(0, 2, "S ") == 0) {
    stripPrefix(s, "S ");
  } else if (s.compare(0, 2, "U ") == 0) {
    stripPrefix(s, "U ");
  }
  trim(s);
  return s;
}

bool parseIsoHourMinute(const char *iso, int &hourOut, int &minOut, char *outHHMM, size_t outSize) {
  // iso looks like "2026-08-31T19:38:00+02:00"; the wall-clock HH:MM is a
  // fixed slice (positions 11..15) and is already in the local timezone.
  const bool shaped = iso && std::strlen(iso) >= 16 && iso[13] == ':' &&
                      std::isdigit(static_cast<unsigned char>(iso[11])) &&
                      std::isdigit(static_cast<unsigned char>(iso[12])) &&
                      std::isdigit(static_cast<unsigned char>(iso[14])) &&
                      std::isdigit(static_cast<unsigned char>(iso[15]));
  if (!shaped) {
    if (outSize > 0) outHHMM[0] = '\0';
    return false;
  }
  hourOut = (iso[11] - '0') * 10 + (iso[12] - '0');
  minOut = (iso[14] - '0') * 10 + (iso[15] - '0');
  if (outSize > 0) {
    std::snprintf(outHHMM, outSize, "%02d:%02d", hourOut, minOut);
  }
  return true;
}

int minutesFromNow(int nowHour, int nowMin, int whenHour, int whenMin) {
  int diff = (whenHour * 60 + whenMin) - (nowHour * 60 + nowMin);
  if (diff < -60) diff += 24 * 60;  // Crossed midnight within the lookahead window.
  if (diff < 0) diff = 0;
  return diff;
}

}  // namespace bvg
