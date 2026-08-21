#include "SettingsServer.h"

#include <WiFi.h>

#include "WeatherSettings.h"

namespace {
constexpr int kPort = 80;

// Matches an optional leading '-' followed by exactly 2 digits, a '.',
// then exactly 2 more digits - e.g. "52.52" or "-13.40".
bool isValidCoordinate(const String &value) {
  int i = 0;
  if (i < static_cast<int>(value.length()) && value[i] == '-') {
    ++i;
  }
  int digitsBefore = 0;
  while (i < static_cast<int>(value.length()) && isDigit(value[i])) {
    ++i;
    ++digitsBefore;
  }
  if (digitsBefore != 2 || i >= static_cast<int>(value.length()) || value[i] != '.') {
    return false;
  }
  ++i;
  int digitsAfter = 0;
  while (i < static_cast<int>(value.length()) && isDigit(value[i])) {
    ++i;
    ++digitsAfter;
  }
  return digitsAfter == 2 && i == static_cast<int>(value.length());
}
}  // namespace

SettingsServer::SettingsServer(WeatherDisplay *weatherDisplay)
    : server_(kPort), weatherDisplay_(weatherDisplay) {}

void SettingsServer::begin() {
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  server_.begin();
  Serial.printf("[SettingsServer] listening at http://%s/\n",
                WiFi.localIP().toString().c_str());
}

void SettingsServer::handleClient() { server_.handleClient(); }

void SettingsServer::handleRoot() { server_.send(200, "text/html", renderPage("")); }

void SettingsServer::handleSave() {
  const String lat = server_.arg("lat");
  const String lon = server_.arg("lon");
  const String name = server_.arg("name");

  if (lat.isEmpty() || lon.isEmpty() || name.isEmpty()) {
    server_.send(400, "text/html", renderPage("All fields are required."));
    return;
  }
  if (!isValidCoordinate(lat) || !isValidCoordinate(lon)) {
    server_.send(400, "text/html",
                 renderPage("Latitude/longitude must look like 12.34 - 2 digits, a dot, "
                            "2 digits."));
    return;
  }

  WeatherSettings::save(lat, lon, name);
  Serial.printf("[SettingsServer] saved location: %s, %s (\"%s\")\n", lat.c_str(), lon.c_str(),
                name.c_str());
  weatherDisplay_->fetchWeather();

  server_.send(200, "text/html", renderPage("Saved and refreshed."));
}

String SettingsServer::renderPage(const String &message) const {
  String html;
  html += "<!DOCTYPE html><html><head>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<title>LED Display Settings</title><style>";
  html += "body{font-family:sans-serif;max-width:420px;margin:2rem auto;padding:0 1rem}";
  html += "label{display:block;margin-top:1rem}";
  html += "input{width:100%;padding:.5rem;box-sizing:border-box;font-size:1rem}";
  html += "button{margin-top:1.5rem;padding:.6rem 1.2rem;font-size:1rem}";
  html += "</style></head><body>";
  html += "<h2>Weather Location</h2>";
  if (!message.isEmpty()) {
    html += "<p><strong>" + message + "</strong></p>";
  }
  html += "<form method=\"POST\" action=\"/save\">";
  const String coordPattern = "-?[0-9]{2}\\.[0-9]{2}";
  const String coordTitle = "title=\"Format: 12.34 - 2 digits, a dot, 2 digits\"";
  html += "<label>Latitude<input name=\"lat\" value=\"" + WeatherSettings::latitude() +
          "\" pattern=\"" + coordPattern + "\" " + coordTitle + " required></label>";
  html += "<label>Longitude<input name=\"lon\" value=\"" + WeatherSettings::longitude() +
          "\" pattern=\"" + coordPattern + "\" " + coordTitle + " required></label>";
  html += "<label>Display name<input name=\"name\" value=\"" + WeatherSettings::locationName() +
          "\" maxlength=\"10\"></label>";
  html += "<button type=\"submit\">Save</button>";
  html += "</form></body></html>";
  return html;
}
