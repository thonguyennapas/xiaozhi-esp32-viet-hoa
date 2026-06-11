#include "watch_face_app.h"
#include "board.h"
#include "display.h"

#include <ctime>
#include <cstdio>
#include <esp_log.h>

#define TAG "WatchFace"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Declare external LVGL fonts
extern const lv_font_t lv_font_montserrat_24;
extern const lv_font_t lv_font_montserrat_36;

void WatchFaceApp::OnEnter(lv_obj_t* parent) {
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    CreateRoot(parent);

    // Force layout update so sizes are available
    lv_obj_update_layout(app_root_);

    // Get actual resolved size
    lv_coord_t w = lv_obj_get_width(app_root_);
    lv_coord_t h = lv_obj_get_height(app_root_);

    if (w <= 0) w = LV_HOR_RES;
    if (h <= 0) h = LV_VER_RES;

    ESP_LOGI(TAG, "Screen: %dx%d", w, h);

    // Watch radius and center
    radius_ = (w < h ? w : h) / 2 - 35; // Leave 35px margin
    cx_ = w / 2;
    cy_ = h / 2 - 10; 

    DrawClockFace();
    UpdateHands();
}

void WatchFaceApp::OnExit() {
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    hour_hand_ = min_hand_ = sec_hand_ = nullptr;
    center_dot_ = date_label_ = time_label_ = nullptr;
    DestroyRoot();
}

void WatchFaceApp::OnTick() {
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);
    UpdateHands();
}

void WatchFaceApp::DrawClockFace() {
    // 1. Subtle minute/hour dots around the edge
    for (int i = 0; i < 60; i++) {
        float angle = i * 6.0f;
        float rad = angle * M_PI / 180.0f;
        bool is_hour = (i % 5 == 0);
        bool is_major = (i % 15 == 0);

        if (is_major) continue; // Skip 12, 3, 6, 9 (will put numbers)

        int dist = radius_;
        int dot_size = is_hour ? 4 : 2;

        lv_obj_t* dot = lv_obj_create(app_root_);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, dot_size, dot_size);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        
        if (is_hour) {
            lv_obj_set_style_bg_color(dot, lv_color_make(180, 180, 180), 0);
        } else {
            lv_obj_set_style_bg_color(dot, lv_color_make(80, 80, 80), 0);
        }

        int nx = cx_ + (int)(sinf(rad) * dist);
        int ny = cy_ - (int)(cosf(rad) * dist);
        lv_obj_set_pos(dot, nx - dot_size / 2, ny - dot_size / 2);
    }

    // 2. Premium large numbers for 12, 3, 6, 9
    const char* nums[] = {"12", "3", "6", "9"};
    float angles[] = {0, 90, 180, 270};
    for (int i = 0; i < 4; i++) {
        float rad = angles[i] * M_PI / 180.0f;
        int nr = radius_ - 10;
        int nx = cx_ + (int)(sinf(rad) * nr);
        int ny = cy_ - (int)(cosf(rad) * nr);

        lv_obj_t* lbl = lv_label_create(app_root_);
        lv_label_set_text(lbl, nums[i]);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        // Use large font
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_36, 0);
        
        lv_obj_refr_size(lbl);
        lv_coord_t lw = lv_obj_get_width(lbl);
        lv_coord_t lh = lv_obj_get_height(lbl);
        lv_obj_set_pos(lbl, nx - lw / 2, ny - lh / 2);
    }

    // 3. Date & Time Pill Widgets
    lv_obj_t* date_pill = lv_obj_create(app_root_);
    lv_obj_remove_style_all(date_pill);
    lv_obj_set_size(date_pill, 80, 30);
    lv_obj_set_style_radius(date_pill, 15, 0);
    lv_obj_set_style_bg_opa(date_pill, LV_OPA_40, 0);
    lv_obj_set_style_bg_color(date_pill, lv_color_make(40, 40, 40), 0);
    lv_obj_align(date_pill, LV_ALIGN_CENTER, 65, 0);

    date_label_ = lv_label_create(date_pill);
    lv_obj_set_style_text_color(date_label_, lv_color_make(200, 200, 200), 0);
    lv_obj_set_style_text_font(date_label_, &lv_font_montserrat_24, 0);
    lv_obj_align(date_label_, LV_ALIGN_CENTER, 0, 0);

    // Digital time pill (top)
    lv_obj_t* time_pill = lv_obj_create(app_root_);
    lv_obj_remove_style_all(time_pill);
    lv_obj_set_size(time_pill, 100, 36);
    lv_obj_set_style_radius(time_pill, 18, 0);
    lv_obj_set_style_bg_opa(time_pill, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(time_pill, lv_color_make(30, 30, 30), 0);
    lv_obj_align(time_pill, LV_ALIGN_TOP_MID, 0, 20);

    time_label_ = lv_label_create(time_pill);
    lv_obj_set_style_text_color(time_label_, lv_color_make(180, 180, 180), 0);
    lv_obj_set_style_text_font(time_label_, &lv_font_montserrat_24, 0);
    lv_obj_align(time_label_, LV_ALIGN_CENTER, 0, 0);

    // 4. Clock hands (Hour, Minute, Second)
    hour_hand_ = lv_line_create(app_root_);
    lv_obj_set_style_line_width(hour_hand_, 8, 0);
    lv_obj_set_style_line_color(hour_hand_, lv_color_white(), 0);
    lv_obj_set_style_line_rounded(hour_hand_, true, 0);

    min_hand_ = lv_line_create(app_root_);
    lv_obj_set_style_line_width(min_hand_, 5, 0);
    lv_obj_set_style_line_color(min_hand_, lv_color_white(), 0);
    lv_obj_set_style_line_rounded(min_hand_, true, 0);

    sec_hand_ = lv_line_create(app_root_);
    lv_obj_set_style_line_width(sec_hand_, 2, 0);
    lv_obj_set_style_line_color(sec_hand_, lv_color_make(255, 60, 60), 0);
    lv_obj_set_style_line_rounded(sec_hand_, true, 0);

    // 5. Center Hub
    center_dot_ = lv_obj_create(app_root_);
    lv_obj_remove_style_all(center_dot_);
    lv_obj_set_size(center_dot_, 14, 14);
    lv_obj_set_style_radius(center_dot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(center_dot_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(center_dot_, lv_color_white(), 0);
    lv_obj_set_style_border_width(center_dot_, 3, 0);
    lv_obj_set_style_border_color(center_dot_, lv_color_make(255, 60, 60), 0);
    lv_obj_set_pos(center_dot_, cx_ - 7, cy_ - 7);
    lv_obj_clear_flag(center_dot_, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
}

void WatchFaceApp::CalcHandPoint(float angle_deg, int length, lv_point_precise_t& out) {
    float rad = angle_deg * M_PI / 180.0f;
    out.x = cx_ + (int)(sinf(rad) * length);
    out.y = cy_ - (int)(cosf(rad) * length);
}

void WatchFaceApp::UpdateHands() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    if (!t || t->tm_year < (2025 - 1900)) return;

    int hour = t->tm_hour % 12;
    int min = t->tm_min;
    int sec = t->tm_sec;

    // Hour hand: 50% length
    float hour_angle = hour * 30.0f + min * 0.5f;
    hour_pts_[0] = {(lv_value_precise_t)cx_, (lv_value_precise_t)cy_};
    CalcHandPoint(hour_angle, radius_ * 50 / 100, hour_pts_[1]);
    lv_line_set_points(hour_hand_, hour_pts_, 2);

    // Minute hand: 75% length
    float min_angle = min * 6.0f + sec * 0.1f;
    min_pts_[0] = {(lv_value_precise_t)cx_, (lv_value_precise_t)cy_};
    CalcHandPoint(min_angle, radius_ * 75 / 100, min_pts_[1]);
    lv_line_set_points(min_hand_, min_pts_, 2);

    // Second hand: 90% forward, 15% backward (counter-weight)
    float sec_angle = sec * 6.0f;
    CalcHandPoint(sec_angle + 180.0f, radius_ * 15 / 100, sec_pts_[0]);
    CalcHandPoint(sec_angle, radius_ * 90 / 100, sec_pts_[1]);
    lv_line_set_points(sec_hand_, sec_pts_, 2);

    // Update date
    char buf[32];
    strftime(buf, sizeof(buf), "%d/%m", t);
    lv_label_set_text(date_label_, buf);

    // Update digital time
    strftime(buf, sizeof(buf), "%H:%M", t); // No seconds needed here
    lv_label_set_text(time_label_, buf);
}
