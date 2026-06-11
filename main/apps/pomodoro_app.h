#pragma once
#include "app_base.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>

/**
 * Pomodoro Timer App.
 * 25 min work → 5 min break → repeat 4x → 15 min long break.
 * Touch to start/pause. Animated arc shows progress.
 * Plays sound when timer expires (via board speaker).
 */
class PomodoroApp : public AppBase {
public:
    void OnEnter(lv_obj_t* parent) override;
    void OnExit() override;
    void OnTick() override;
    void OnTap() override;
    const char* GetName() const override { return "Pomodoro"; }

private:
    enum PomState { IDLE, WORK, SHORT_BREAK, LONG_BREAK };

    static constexpr int WORK_SECS = 25 * 60;
    static constexpr int SHORT_BREAK_SECS = 5 * 60;
    static constexpr int LONG_BREAK_SECS = 15 * 60;
    static constexpr int SESSIONS_BEFORE_LONG = 4;

    PomState state_ = IDLE;
    int remaining_secs_ = WORK_SECS;
    int total_secs_ = WORK_SECS;
    int sessions_done_ = 0;
    bool running_ = false;

    // LVGL objects
    lv_obj_t* arc_ = nullptr;
    lv_obj_t* time_label_ = nullptr;
    lv_obj_t* state_label_ = nullptr;
    lv_obj_t* session_label_ = nullptr;
    lv_obj_t* hint_label_ = nullptr;

    void BuildUI();
    void UpdateUI();
    void TransitionToNext();
    const char* StateText() const;
    lv_color_t StateColor() const;
};
