#pragma once
#include "app_base.h"
#include <vector>
#include <memory>
#include <lvgl.h>

/**
 * Manages multiple mini-apps with swipe navigation.
 * Singleton pattern — one instance shared across the firmware.
 * 
 * Touch detection: Registers event callbacks directly on the LVGL
 * touch input device (lv_indev_t), which fires BEFORE any widget
 * event processing. This bypasses all object-level event routing issues.
 */
class AppManager {
public:
    static AppManager& GetInstance() {
        static AppManager instance;
        return instance;
    }

    void RegisterApp(std::unique_ptr<AppBase> app);
    void Initialize(lv_obj_t* screen);

    void NextApp();
    void PrevApp();

    void ShowCurrentApp();
    void HideCurrentApp();
    void Tick();

    bool IsVisible() const { return app_visible_; }
    int GetCurrentIndex() const { return current_index_; }
    int GetAppCount() const { return (int)apps_.size(); }

private:
    AppManager() = default;

    std::vector<std::unique_ptr<AppBase>> apps_;
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* app_layer_ = nullptr;
    lv_obj_t* indicator_ = nullptr;
    int current_index_ = 0;
    bool initialized_ = false;
    bool app_visible_ = false;

    // Touch tracking for manual swipe detection
    bool touch_active_ = false;
    lv_coord_t touch_start_x_ = 0;
    lv_coord_t touch_start_y_ = 0;
    uint32_t touch_start_time_ = 0;

    static constexpr int SWIPE_MIN_DISTANCE = 30;   // minimum pixels for swipe (lowered for small screen)
    static constexpr int SWIPE_MAX_TIME_MS = 1000;   // maximum ms for swipe gesture
    static constexpr int SWIPE_MAX_Y_DRIFT = 100;    // max vertical drift allowed

    void ActivateApp(int index);
    void DeactivateCurrentApp();
    void UpdateIndicator();

    // LVGL indev event callbacks (fires for ALL touches, before widget processing)
    static void OnIndevEvent(lv_event_t* e);
};
