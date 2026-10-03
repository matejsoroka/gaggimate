#include "WeatherPlugin.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <cmath>
#include <display/core/Controller.h>
#include <display/core/Event.h>
#include <esp_log.h>

static const char *WEATHER_TAG = "WeatherPlugin";

constexpr uint32_t WEATHER_REFRESH_MS = 15 * 60 * 1000; // successful fetch
constexpr uint32_t WEATHER_RETRY_MS = 60 * 1000;        // failed fetch
constexpr uint32_t WEATHER_WIFI_WAIT_MS = 5 * 1000;     // no Wi-Fi yet
constexpr uint16_t WEATHER_HTTP_TIMEOUT_MS = 10 * 1000;

static String urlEncode(const String &value) {
    String encoded;
    encoded.reserve(value.length() * 3);
    for (size_t i = 0; i < value.length(); i++) {
        const char c = value[i];
        if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~' || c == ',') {
            encoded += c;
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", static_cast<unsigned char>(c));
            encoded += buf;
        }
    }
    return encoded;
}

// Splits "<number>,<number>" (digits, '.', '+', '-', optional spaces) into lat/lon; anything else is a place name.
static bool parseCoordinates(const String &location, String &lat, String &lon) {
    const int comma = location.indexOf(',');
    if (comma < 0 || location.indexOf(',', comma + 1) >= 0)
        return false;
    String parts[2] = {location.substring(0, comma), location.substring(comma + 1)};
    for (auto &part : parts) {
        part.trim();
        if (part.isEmpty())
            return false;
        bool digit = false;
        for (size_t i = 0; i < part.length(); i++) {
            const char c = part[i];
            if (isdigit(static_cast<unsigned char>(c)))
                digit = true;
            else if (c != '.' && c != '+' && c != '-')
                return false;
        }
        if (!digit)
            return false;
    }
    lat = parts[0];
    lon = parts[1];
    return true;
}

void WeatherPlugin::setup(Controller *controller, PluginManager *pluginManager) {
    this->controller = controller;
    this->pluginManager = pluginManager;
    copyConfig();
    // settings:changed fires on the web server task; copy there and wake the task, so it never reads Settings itself.
    pluginManager->on("settings:changed", [this](Event const &) {
        copyConfig();
        if (taskHandle != nullptr)
            xTaskNotifyGive(taskHandle);
    });
    xTaskCreatePinnedToCore(taskFunction, "WeatherPlugin", 8192, this, 1, &taskHandle, 0);
}

void WeatherPlugin::taskFunction(void *arg) {
    auto *plugin = static_cast<WeatherPlugin *>(arg);
    plugin->run();
}

void WeatherPlugin::copyConfig() {
    const Settings &settings = controller->getSettings();
    Config fresh;
    fresh.active = settings.isWeatherActive();
    fresh.apiKey = settings.getWeatherApiKey();
    fresh.location = settings.getWeatherLocation();
    fresh.units = settings.getWeatherUnits();
    std::lock_guard<std::mutex> guard(configMutex);
    config = fresh;
}

WeatherPlugin::Config WeatherPlugin::getConfig() {
    std::lock_guard<std::mutex> guard(configMutex);
    return config;
}

void WeatherPlugin::run() {
    while (true) {
        const Config current = getConfig();
        TickType_t wait;
        if (!current.active || current.apiKey.isEmpty() || current.location.isEmpty()) {
            if (lastValid)
                publish(false, Reading{});
            wait = portMAX_DELAY;
        } else if (WiFi.status() != WL_CONNECTED) {
            wait = pdMS_TO_TICKS(WEATHER_WIFI_WAIT_MS);
        } else {
            Reading reading;
            if (fetch(current, reading)) {
                publish(true, reading);
                wait = pdMS_TO_TICKS(WEATHER_REFRESH_MS);
            } else {
                // Keep the last good reading on screen and try again shortly.
                wait = pdMS_TO_TICKS(WEATHER_RETRY_MS);
            }
        }
        ulTaskNotifyTake(pdTRUE, wait);
    }
}

String WeatherPlugin::buildUrl(const Config &config) const {
    String url = "https://api.openweathermap.org/data/2.5/weather?";
    String lat, lon;
    if (parseCoordinates(config.location, lat, lon)) {
        url += "lat=" + urlEncode(lat) + "&lon=" + urlEncode(lon);
    } else {
        url += "q=" + urlEncode(config.location);
    }
    url += config.units == 1 ? "&units=imperial" : "&units=metric";
    url += "&appid=" + urlEncode(config.apiKey);
    return url;
}

bool WeatherPlugin::fetch(const Config &config, Reading &reading) {
    WiFiClientSecure client;
    client.setInsecure(); // TODO: the OpenWeather CA could be pinned here instead of skipping verification
    client.setTimeout(WEATHER_HTTP_TIMEOUT_MS / 1000);

    HTTPClient http;
    http.setTimeout(WEATHER_HTTP_TIMEOUT_MS);
    http.setConnectTimeout(WEATHER_HTTP_TIMEOUT_MS);
    if (!http.begin(client, buildUrl(config))) {
        ESP_LOGW(WEATHER_TAG, "Failed to start weather request");
        return false;
    }

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        if (code == HTTP_CODE_UNAUTHORIZED)
            ESP_LOGW(WEATHER_TAG, "HTTP 401: invalid or not yet activated API key");
        else if (code == HTTP_CODE_NOT_FOUND)
            ESP_LOGW(WEATHER_TAG, "HTTP 404: unknown location '%s'", config.location.c_str());
        else
            ESP_LOGW(WEATHER_TAG, "Weather request failed with HTTP %d", code);
        http.end();
        return false;
    }

    JsonDocument filter;
    filter["name"] = true;
    filter["main"]["temp"] = true;
    filter["weather"][0]["main"] = true;
    filter["weather"][0]["description"] = true;
    filter["weather"][0]["icon"] = true;

    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (error) {
        ESP_LOGW(WEATHER_TAG, "Failed to parse weather response: %s", error.c_str());
        return false;
    }
    if (!doc["main"]["temp"].is<float>()) {
        ESP_LOGW(WEATHER_TAG, "Weather response is missing main.temp");
        return false;
    }

    reading.temp = doc["main"]["temp"].as<float>();
    reading.unit = config.units == 1 ? "F" : "C";
    reading.condition = doc["weather"][0]["main"] | "";
    reading.description = doc["weather"][0]["description"] | "";
    reading.icon = doc["weather"][0]["icon"] | "";
    reading.city = doc["name"] | "";
    ESP_LOGI(WEATHER_TAG, "Weather in %s: %.1f°%s, %s", reading.city.c_str(), reading.temp, reading.unit.c_str(),
             reading.description.c_str());
    return true;
}

// Every key is always present so consumers never fall back to Event's defaults by accident.
void WeatherPlugin::publish(bool valid, const Reading &reading) {
    Event event;
    event.id = "weather:update";
    event.setInt("valid", valid ? 1 : 0);
    event.setFloat("temp", reading.temp);
    event.setString("unit", reading.unit);
    event.setString("condition", reading.condition);
    event.setString("desc", reading.description);
    event.setString("icon", reading.icon);
    event.setString("city", reading.city);
    pluginManager->trigger(event);
    lastValid = valid;
}
