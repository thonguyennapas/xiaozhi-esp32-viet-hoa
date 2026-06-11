#pragma once
#include "app_base.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <string>
#include <mutex>

/**
 * Weather app using Open-Meteo API (free, no key required).
 * Displays current temperature, humidity, wind, and 5-day forecast.
 * Data is fetched every 15 minutes in a background task.
 */
class WeatherApp : public AppBase {
public:
    void OnEnter(lv_obj_t* parent) override;
    void OnExit() override;
    void OnTick() override;
    const char* GetName() const override { return "Weather"; }

private:
    // Hanoi coordinates
    static constexpr float LAT = 21.03f;
    static constexpr float LON = 105.85f;
    static constexpr int FETCH_INTERVAL_SEC = 15 * 60;

    struct WeatherData {
        float temp = 0;
        int humidity = 0;
        float wind = 0;
        int weather_code = 0;
        // Daily forecast (5 days)
        float daily_max[5] = {};
        float daily_min[5] = {};
        int daily_code[5] = {};
        bool valid = false;
    };

    std::mutex data_mutex_;
    WeatherData data_;
    std::atomic<bool> has_new_data_{false};
    std::atomic<bool> fetch_running_{false};
    TaskHandle_t fetch_task_ = nullptr;
    int tick_counter_ = 0;

    // LVGL objects
    lv_obj_t* temp_label_ = nullptr;
    lv_obj_t* desc_label_ = nullptr;
    lv_obj_t* detail_label_ = nullptr;
    lv_obj_t* city_label_ = nullptr;
    lv_obj_t* forecast_container_ = nullptr;
    lv_obj_t* loading_label_ = nullptr;

    void BuildUI();
    void UpdateUI();
    void StartFetchTask();
    void StopFetchTask();
    static void FetchTaskFunc(void* arg);
    bool ParseResponse(const std::string& json);
    static const char* WeatherDescription(int code);
};
