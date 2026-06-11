#include "app_manager.h"
#include "display.h"
#include "board.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <cstdlib>

#define TAG "AppManager"

void AppManager::RegisterApp(std::unique_ptr<AppBase> app) {
    ESP_LOGI(TAG, "Registered app: %s", app->GetName());
    apps_.push_back(std::move(app));
}

void AppManager::Initialize(lv_obj_t* screen) {
    if (initialized_ || apps_.empty()) return;
    screen_ = screen;

    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    // Create a full-screen layer for apps (initially hidden)
    app_layer_ = lv_obj_create(screen_);
    lv_obj_remove_style_all(app_layer_);
    lv_obj_set_size(app_layer_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(app_layer_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(app_layer_, lv_color_black(), 0);
    lv_obj_set_scrollbar_mode(app_layer_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(app_layer_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(app_layer_, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));

    // === KEY FIX: Register on the touch INPUT DEVICE directly ===
    // This fires for EVERY touch, before any widget event processing.
    // No more worrying about clickable objects blocking events.
    lv_indev_t* indev = nullptr;
    while ((indev = lv_indev_get_next(indev)) != nullptr) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            lv_indev_add_event_cb(indev, OnIndevEvent, LV_EVENT_ALL, this);
            ESP_LOGI(TAG, "Registered touch event on indev pointer");
            break;
        }
    }

    // Create page indicator at bottom of app_layer_
    indicator_ = lv_obj_create(app_layer_);
    lv_obj_remove_style_all(indicator_);
    lv_obj_set_size(indicator_, LV_PCT(100), 20);
    lv_obj_align(indicator_, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_flex_flow(indicator_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(indicator_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(indicator_, 8, 0);
    lv_obj_set_scrollbar_mode(indicator_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(indicator_, LV_OBJ_FLAG_CLICKABLE);

    // Create indicator dots (one per app)
    for (int i = 0; i < (int)apps_.size(); i++) {
        lv_obj_t* dot = lv_obj_create(indicator_);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(dot, lv_color_make(80, 80, 80), 0);
    }

    initialized_ = true;
    ESP_LOGI(TAG, "Initialized with %d apps", (int)apps_.size());
}

void AppManager::OnIndevEvent(lv_event_t* e) {
    auto* mgr = static_cast<AppManager*>(lv_event_get_user_data(e));
    if (!mgr->app_visible_) return;  // only process when apps are visible

    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t* indev = (lv_indev_t*)lv_event_get_target(e);

    if (code == LV_EVENT_PRESSED) {
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        mgr->touch_start_x_ = point.x;
        mgr->touch_start_y_ = point.y;
        mgr->touch_start_time_ = (uint32_t)(esp_timer_get_time() / 1000);
        mgr->touch_active_ = true;
        ESP_LOGD(TAG, "INDEV press: x=%d y=%d", point.x, point.y);
    }
    else if (code == LV_EVENT_RELEASED && mgr->touch_active_) {
        mgr->touch_active_ = false;

        lv_point_t point;
        lv_indev_get_point(indev, &point);
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        uint32_t dt = now_ms - mgr->touch_start_time_;

        int dx = point.x - mgr->touch_start_x_;
        int dy = point.y - mgr->touch_start_y_;
        int abs_dx = abs(dx);
        int abs_dy = abs(dy);

        ESP_LOGI(TAG, "INDEV release: dx=%d dy=%d dt=%lu ms", dx, dy, (unsigned long)dt);

        // Check swipe conditions
        if (abs_dx >= SWIPE_MIN_DISTANCE && abs_dx > abs_dy &&
            dt <= SWIPE_MAX_TIME_MS && abs_dy <= SWIPE_MAX_Y_DRIFT) {
            if (dx < 0) {
                ESP_LOGI(TAG, ">>> Swipe LEFT -> NextApp");
                mgr->NextApp();
            } else {
                ESP_LOGI(TAG, ">>> Swipe RIGHT -> PrevApp");
                mgr->PrevApp();
            }
            return;
        }

        // Short tap detection
        if (abs_dx < 20 && abs_dy < 20 && dt < 500) {
            ESP_LOGI(TAG, ">>> Tap detected");
            int idx = mgr->current_index_;
            if (idx >= 0 && idx < (int)mgr->apps_.size() && mgr->apps_[idx]->IsActive()) {
                mgr->apps_[idx]->OnTap();
            }
        }
    }
}

void AppManager::NextApp() {
    if (apps_.size() <= 1) return;
    int next = (current_index_ + 1) % apps_.size();
    DeactivateCurrentApp();
    current_index_ = next;
    ActivateApp(current_index_);
}

void AppManager::PrevApp() {
    if (apps_.size() <= 1) return;
    int prev = (current_index_ - 1 + apps_.size()) % apps_.size();
    DeactivateCurrentApp();
    current_index_ = prev;
    ActivateApp(current_index_);
}

void AppManager::ActivateApp(int index) {
    if (index < 0 || index >= (int)apps_.size()) return;
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    apps_[index]->OnEnter(app_layer_);
    UpdateIndicator();
    ESP_LOGI(TAG, "Activated: %s", apps_[index]->GetName());
}

void AppManager::DeactivateCurrentApp() {
    if (current_index_ < 0 || current_index_ >= (int)apps_.size()) return;
    auto& app = apps_[current_index_];
    if (app->IsActive()) {
        auto display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        app->OnExit();
    }
}

void AppManager::ShowCurrentApp() {
    if (!initialized_ || apps_.empty()) return;

    {
        auto display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        lv_obj_remove_flag(app_layer_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_to_index(app_layer_, -1);  // bring to front
    }
    app_visible_ = true;

    if (!apps_[current_index_]->IsActive()) {
        ActivateApp(current_index_);
    }
}

void AppManager::HideCurrentApp() {
    if (!initialized_) return;

    DeactivateCurrentApp();

    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    lv_obj_add_flag(app_layer_, LV_OBJ_FLAG_HIDDEN);
    app_visible_ = false;
}

void AppManager::Tick() {
    if (!app_visible_ || !initialized_) return;
    if (current_index_ >= 0 && current_index_ < (int)apps_.size()) {
        auto& app = apps_[current_index_];
        if (app->IsActive()) {
            app->OnTick();
        }
    }
}

void AppManager::UpdateIndicator() {
    if (!indicator_) return;
    int count = lv_obj_get_child_count(indicator_);
    for (int i = 0; i < count; i++) {
        lv_obj_t* dot = lv_obj_get_child(indicator_, i);
        if (i == current_index_) {
            lv_obj_set_style_bg_color(dot, lv_color_white(), 0);
            lv_obj_set_size(dot, 10, 10);
        } else {
            lv_obj_set_style_bg_color(dot, lv_color_make(80, 80, 80), 0);
            lv_obj_set_size(dot, 8, 8);
        }
    }
}
