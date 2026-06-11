#pragma once

#include <lvgl.h>
#include <string>
#include <esp_log.h>

/**
 * Abstract base class for mini apps on the AMOLED display.
 * Each app creates its own LVGL objects when entering and destroys them when exiting.
 * Only one app's objects exist at a time to conserve ESP32 RAM.
 */
class AppBase {
public:
    virtual ~AppBase() = default;

    /**
     * Called when this app becomes the active app.
     * Create all LVGL objects here, parented to the given container.
     */
    virtual void OnEnter(lv_obj_t* parent) = 0;

    /**
     * Called when leaving this app (switching to another app or chatbot).
     * Clean up all LVGL objects here.
     */
    virtual void OnExit() = 0;

    /**
     * Called every second from the main event loop (CLOCK_TICK).
     * Update time, animation, data display etc.
     */
    virtual void OnTick() = 0;

    /**
     * Called when user taps the screen (short touch without swipe).
     * Override in apps that need tap interaction (e.g., Pomodoro start/pause).
     */
    virtual void OnTap() {}

    /** Human-readable app name */
    virtual const char* GetName() const = 0;

    bool IsActive() const { return active_; }

protected:
    bool active_ = false;
    lv_obj_t* app_root_ = nullptr;

    /**
     * Helper: create a full-size black container inside the parent.
     * All app UI should be built inside app_root_.
     */
    void CreateRoot(lv_obj_t* parent) {
        app_root_ = lv_obj_create(parent);
        lv_obj_remove_style_all(app_root_);
        lv_obj_set_size(app_root_, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_opa(app_root_, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(app_root_, lv_color_black(), 0);
        lv_obj_set_style_pad_all(app_root_, 0, 0);
        lv_obj_set_scrollbar_mode(app_root_, LV_SCROLLBAR_MODE_OFF);
        // Make transparent to touch so swipe gestures reach app_layer_
        lv_obj_clear_flag(app_root_, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        lv_obj_add_flag(app_root_, LV_OBJ_FLAG_EVENT_BUBBLE);
        active_ = true;
    }

    /** Helper: destroy the root container and all children */
    void DestroyRoot() {
        active_ = false;
        if (app_root_) {
            lv_obj_del(app_root_);
            app_root_ = nullptr;
        }
    }
};
