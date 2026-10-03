#ifndef WEATHERPLUGIN_H
#define WEATHERPLUGIN_H

#include <Arduino.h>
#include <display/core/Plugin.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mutex>

struct Event;

// Fetches the current conditions from OpenWeather every 15 minutes and publishes them as a `weather:update` event.
// The standby screen (DefaultUI) renders the result below the clock.
class WeatherPlugin : public Plugin {
  public:
    void setup(Controller *controller, PluginManager *pluginManager) override;
    void loop() override {};

  private:
    struct Config {
        bool active = false;
        String apiKey;
        String location;
        int units = 0; // 0 = metric (°C), 1 = imperial (°F)
    };

    struct Reading {
        float temp = 0.0f;
        String unit;
        String condition;
        String description;
        String icon;
        String city;
    };

    static void taskFunction(void *arg);
    void run();
    void copyConfig();
    Config getConfig();
    bool fetch(const Config &config, Reading &reading);
    String buildUrl(const Config &config) const;
    void publish(bool valid, const Reading &reading);

    Controller *controller = nullptr;
    PluginManager *pluginManager = nullptr;

    std::mutex configMutex;
    Config config;          // copy of the settings, only ever read by the task
    bool lastValid = false; // whether the last published event carried data

    TaskHandle_t taskHandle = nullptr;
};

#endif // WEATHERPLUGIN_H
