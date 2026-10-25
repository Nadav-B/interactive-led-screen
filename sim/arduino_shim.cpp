// Implementations behind the host stand-ins in sim/arduino/.

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include <random>
#include <string>

namespace sim {
unsigned long nowMs = 0;
time_t epochAtBoot = 0;
}  // namespace sim

HardwareSerial Serial;
WiFiClass WiFi;

namespace {
std::mt19937 &rng() {
  static std::mt19937 engine(1);
  return engine;
}
}  // namespace

void randomSeed(unsigned long seed) { rng().seed(static_cast<std::mt19937::result_type>(seed)); }

long random(long howBig) { return howBig <= 0 ? 0 : random(0, howBig); }

long random(long howSmall, long howBig) {
  if (howSmall >= howBig) return howSmall;
  std::uniform_int_distribution<long> dist(howSmall, howBig - 1);
  return dist(rng());
}

int HTTPClient::GET() {
  // Single-quote the URL for the shell; any embedded quote becomes '\''.
  std::string url = url_.c_str();
  std::string quoted = "'";
  for (char c : url) quoted += c == '\'' ? std::string("'\\''") : std::string(1, c);
  quoted += "'";

  const std::string cmd = "curl -s -L --max-time 20 -w '\\n%{http_code}' " + quoted;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) return -1;
  std::string out;
  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0) out.append(buf, n);
  pclose(pipe);

  // curl appends "\n<status>" after the body (see -w above).
  const size_t nl = out.rfind('\n');
  if (nl == std::string::npos) return -1;
  const int status = atoi(out.c_str() + nl + 1);
  body_ = out.substr(0, nl);
  return status == 0 ? -1 : status;
}
