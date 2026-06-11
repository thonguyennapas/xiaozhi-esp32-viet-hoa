#pragma once
#include "app_base.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <string>
#include <mutex>

/**
 * Gold price tracker app.
 * Fetches international gold spot price (XAU/USD) from free API.
 * Displays price in USD and estimated VND per luong (tael).
 * Updates every 30 minutes via background task.
 */
class GoldPriceApp : public AppBase {
public:
    void OnEnter(lv_obj_t* parent) override;
    void OnExit() override;
    void OnTick() override;
    const char* GetName() const override { return "Gold"; }

private:
    static constexpr int FETCH_INTERVAL_SEC = 30 * 60;
    struct GoldData {
        double buy = 0;
        double sell = 0;
        double change_buy = 0;
        double change_sell = 0;
        bool valid = false;
    };

    std::mutex data_mutex_;
    GoldData data_;
    std::atomic<bool> has_new_data_{false};
    std::atomic<bool> fetch_running_{false};
    TaskHandle_t fetch_task_ = nullptr;
    int tick_counter_ = 0;

    // LVGL objects
    lv_obj_t* title_label_ = nullptr;
    lv_obj_t* price_usd_label_ = nullptr;
    lv_obj_t* price_vnd_label_ = nullptr;
    lv_obj_t* change_label_ = nullptr;
    lv_obj_t* unit_label_ = nullptr;
    lv_obj_t* update_label_ = nullptr;
    lv_obj_t* loading_label_ = nullptr;

    void BuildUI();
    void UpdateUI();
    void StartFetchTask();
    void StopFetchTask();
    static void FetchTaskFunc(void* arg);
    bool ParseResponse(const std::string& json);
};
