#pragma once
#include "app_base.h"
#include <cmath>

/**
 * Analog watch face app for AMOLED display.
 * - Circular analog clock with hour/minute/second hands
 * - Date display
 * - Pure black background (AMOLED power saving)
 * - Updates every second
 */
class WatchFaceApp : public AppBase {
public:
    void OnEnter(lv_obj_t* parent) override;
    void OnExit() override;
    void OnTick() override;
    const char* GetName() const override { return "Watch"; }

private:
    // Clock geometry
    int cx_ = 0, cy_ = 0, radius_ = 0;

    // LVGL objects
    lv_obj_t* hour_hand_ = nullptr;
    lv_obj_t* min_hand_ = nullptr;
    lv_obj_t* sec_hand_ = nullptr;
    lv_obj_t* center_dot_ = nullptr;
    lv_obj_t* date_label_ = nullptr;
    lv_obj_t* time_label_ = nullptr;

    // Line point arrays (must persist while lines exist)
    lv_point_precise_t hour_pts_[2] = {};
    lv_point_precise_t min_pts_[2] = {};
    lv_point_precise_t sec_pts_[2] = {};

    void DrawClockFace();
    void UpdateHands();
    void CalcHandPoint(float angle_deg, int length, lv_point_precise_t& out);
};
