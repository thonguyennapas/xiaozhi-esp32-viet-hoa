#include "pomodoro_app.h"
#include "board.h"
#include "display.h"
#include "application.h"
#include "assets/lang_config.h"

#include <cstdio>
#include <esp_log.h>

#define TAG "PomodoroApp"

extern const lv_font_t lv_font_montserrat_14;
extern const lv_font_t lv_font_montserrat_24;
extern const lv_font_t lv_font_montserrat_48;

void PomodoroApp::OnEnter(lv_obj_t* parent) {
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    CreateRoot(parent);
    state_ = IDLE;
    remaining_secs_ = WORK_SECS;
    total_secs_ = WORK_SECS;
    running_ = false;
    sessions_done_ = 0;

    BuildUI();
    UpdateUI();
}

void PomodoroApp::OnExit() {
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    arc_ = time_label_ = state_label_ = session_label_ = hint_label_ = nullptr;
    DestroyRoot();
}

void PomodoroApp::OnTick() {
    if (!running_ || !active_) return;

    remaining_secs_--;
    if (remaining_secs_ <= 0) {
        Application::GetInstance().Schedule([]() {
            Application::GetInstance().PlaySound(Lang::Sounds::OGG_SUCCESS);
        });
        TransitionToNext();
    }

    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);
    UpdateUI();
}

void PomodoroApp::BuildUI() {
    lv_obj_update_layout(app_root_);

    lv_coord_t w = lv_obj_get_width(app_root_);
    lv_coord_t h = lv_obj_get_height(app_root_);
    if (w <= 0) w = LV_HOR_RES;
    if (h <= 0) h = LV_VER_RES;

    int arc_size = (w < h ? w : h) - 80;

    // ═══ Outer decorative ring ═══
    lv_obj_t* outer_ring = lv_arc_create(app_root_);
    lv_obj_set_size(outer_ring, arc_size + 16, arc_size + 16);
    lv_obj_align(outer_ring, LV_ALIGN_CENTER, 0, -20);
    lv_arc_set_rotation(outer_ring, 0);
    lv_arc_set_bg_angles(outer_ring, 0, 360);
    lv_arc_set_range(outer_ring, 0, 100);
    lv_arc_set_value(outer_ring, 0);
    lv_obj_remove_flag(outer_ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(outer_ring, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_color(outer_ring, lv_color_make(40, 40, 40), LV_PART_MAIN);
    lv_obj_set_style_arc_width(outer_ring, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(outer_ring, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(outer_ring, 0, LV_PART_KNOB);

    // ═══ Main progress arc ═══
    arc_ = lv_arc_create(app_root_);
    lv_obj_set_size(arc_, arc_size, arc_size);
    lv_obj_align(arc_, LV_ALIGN_CENTER, 0, -20);
    lv_arc_set_rotation(arc_, 270);
    lv_arc_set_bg_angles(arc_, 0, 360);
    lv_arc_set_range(arc_, 0, 100);
    lv_arc_set_value(arc_, 100);
    lv_obj_remove_flag(arc_, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_arc_width(arc_, 22, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_, lv_color_make(25, 25, 30), LV_PART_MAIN);

    lv_obj_set_style_arc_width(arc_, 22, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc_, true, LV_PART_INDICATOR);

    lv_obj_set_style_bg_opa(arc_, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc_, 0, LV_PART_KNOB);

    // ═══ State label (inside arc, top) ═══
    state_label_ = lv_label_create(app_root_);
    lv_obj_align(state_label_, LV_ALIGN_CENTER, 0, -55);
    lv_obj_set_style_text_color(state_label_, lv_color_make(160, 160, 170), 0);
    lv_obj_set_style_text_font(state_label_, &lv_font_montserrat_24, 0);

    // ═══ Time (hero, center) ═══
    time_label_ = lv_label_create(app_root_);
    lv_obj_align(time_label_, LV_ALIGN_CENTER, 0, -15);
    lv_obj_set_style_text_color(time_label_, lv_color_white(), 0);
    lv_obj_set_style_text_font(time_label_, &lv_font_montserrat_48, 0);

    // ═══ Session dots ═══
    session_label_ = lv_label_create(app_root_);
    lv_obj_align(session_label_, LV_ALIGN_CENTER, 0, 35);
    lv_obj_set_style_text_color(session_label_, lv_color_make(100, 100, 110), 0);
    lv_obj_set_style_text_font(session_label_, &lv_font_montserrat_24, 0);

    // ═══ Hint pill (bottom) ═══
    lv_obj_t* hint_pill = lv_obj_create(app_root_);
    lv_obj_remove_style_all(hint_pill);
    lv_obj_set_size(hint_pill, 220, 50);
    lv_obj_set_style_radius(hint_pill, 25, 0);
    lv_obj_set_style_bg_opa(hint_pill, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(hint_pill, lv_color_make(20, 25, 35), 0);
    lv_obj_set_style_bg_grad_color(hint_pill, lv_color_make(25, 30, 45), 0);
    lv_obj_set_style_bg_grad_dir(hint_pill, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(hint_pill, 1, 0);
    lv_obj_set_style_border_color(hint_pill, lv_color_make(50, 55, 70), 0);
    lv_obj_align(hint_pill, LV_ALIGN_BOTTOM_MID, 0, -32);

    hint_label_ = lv_label_create(hint_pill);
    lv_label_set_text(hint_label_, LV_SYMBOL_PLAY "  Bat dau");
    lv_obj_align(hint_label_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(hint_label_, lv_color_make(180, 190, 220), 0);
    lv_obj_set_style_text_font(hint_label_, &lv_font_montserrat_24, 0);
}

void PomodoroApp::UpdateUI() {
    if (!arc_ || !time_label_) return;

    // Update time
    int mins = remaining_secs_ / 60;
    int secs = remaining_secs_ % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
    lv_label_set_text(time_label_, buf);

    // Update arc progress
    int progress = (total_secs_ > 0) ? (remaining_secs_ * 100 / total_secs_) : 0;
    lv_arc_set_value(arc_, progress);
    
    // Style active elements with state color
    lv_color_t sc = StateColor();
    lv_obj_set_style_arc_color(arc_, sc, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(state_label_, sc, 0);

    // State text
    lv_label_set_text(state_label_, StateText());

    // Session dots — use simple ASCII that works with any font
    char dots[64] = {};
    int pos = 0;
    for (int i = 0; i < SESSIONS_BEFORE_LONG; i++) {
        if (i < sessions_done_) {
            dots[pos++] = '*';  // Completed session
        } else {
            dots[pos++] = '-';  // Pending session
        }
        if (i < SESSIONS_BEFORE_LONG - 1) {
            dots[pos++] = ' ';
            dots[pos++] = ' ';
        }
    }
    dots[pos] = '\0';
    lv_label_set_text(session_label_, dots);

    // Hint
    if (hint_label_) {
        if (state_ == IDLE) {
            lv_label_set_text(hint_label_, LV_SYMBOL_PLAY "  Bat dau");
            lv_obj_set_style_text_color(hint_label_, lv_color_white(), 0);
        } else if (running_) {
            lv_label_set_text(hint_label_, LV_SYMBOL_PAUSE "  Tam dung");
            lv_obj_set_style_text_color(hint_label_, lv_color_make(255, 100, 100), 0);
        } else {
            lv_label_set_text(hint_label_, LV_SYMBOL_PLAY "  Tiep tuc");
            lv_obj_set_style_text_color(hint_label_, lv_color_make(100, 255, 100), 0);
        }
    }
}

void PomodoroApp::TransitionToNext() {
    if (state_ == WORK) {
        sessions_done_++;
        if (sessions_done_ >= SESSIONS_BEFORE_LONG) {
            state_ = LONG_BREAK;
            remaining_secs_ = total_secs_ = LONG_BREAK_SECS;
            sessions_done_ = 0;
        } else {
            state_ = SHORT_BREAK;
            remaining_secs_ = total_secs_ = SHORT_BREAK_SECS;
        }
    } else {
        state_ = WORK;
        remaining_secs_ = total_secs_ = WORK_SECS;
    }
    running_ = true;
}

const char* PomodoroApp::StateText() const {
    switch (state_) {
        case IDLE: return "SAN SANG";
        case WORK: return "LAM VIEC";
        case SHORT_BREAK: return "NGHI NGAN";
        case LONG_BREAK: return "NGHI DAI";
    }
    return "";
}

lv_color_t PomodoroApp::StateColor() const {
    switch (state_) {
        case IDLE: return lv_color_make(150, 150, 150);
        case WORK: return lv_color_make(255, 80, 80);       // Red/Orange
        case SHORT_BREAK: return lv_color_make(60, 220, 100); // Mint Green
        case LONG_BREAK: return lv_color_make(80, 160, 255);  // Blue
    }
    return lv_color_white();
}

void PomodoroApp::OnTap() {
    if (state_ == IDLE) {
        state_ = WORK;
        remaining_secs_ = total_secs_ = WORK_SECS;
        running_ = true;
    } else {
        running_ = !running_;  // toggle pause
    }

    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);
    UpdateUI();
}
