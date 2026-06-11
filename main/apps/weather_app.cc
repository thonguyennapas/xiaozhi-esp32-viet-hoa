#include "weather_app.h"
#include "app_http_client.h"
#include "board.h"
#include "display.h"

#include <cJSON.h>
#include <cstdio>
#include <cstring>
#include <esp_log.h>

#define TAG "WeatherApp"

extern const lv_font_t lv_font_montserrat_24;
extern const lv_font_t lv_font_montserrat_48;

void WeatherApp::OnEnter(lv_obj_t* parent) {
    {
        auto display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        CreateRoot(parent);
        BuildUI();
    }
    tick_counter_ = 0;
    StartFetchTask();
}

void WeatherApp::OnExit() {
    StopFetchTask();

    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    temp_label_ = desc_label_ = detail_label_ = city_label_ = nullptr;
    forecast_container_ = loading_label_ = nullptr;
    DestroyRoot();
}

void WeatherApp::OnTick() {
    tick_counter_++;

    if (has_new_data_.exchange(false)) {
        auto display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        UpdateUI();
    }

    if (tick_counter_ % FETCH_INTERVAL_SEC == 0 && !fetch_running_) {
        StartFetchTask();
    }
}

void WeatherApp::BuildUI() {
    // City name (top)
    city_label_ = lv_label_create(app_root_);
    lv_label_set_text(city_label_, LV_SYMBOL_GPS " Ha Noi");
    lv_obj_set_style_text_color(city_label_, lv_color_make(150, 150, 150), 0);
    lv_obj_set_style_text_font(city_label_, &lv_font_montserrat_24, 0);
    lv_obj_align(city_label_, LV_ALIGN_TOP_MID, 0, 30);

    // Temperature (Huge center)
    temp_label_ = lv_label_create(app_root_);
    lv_label_set_text(temp_label_, "--°C");
    lv_obj_set_style_text_color(temp_label_, lv_color_white(), 0);
    lv_obj_set_style_text_font(temp_label_, &lv_font_montserrat_48, 0);
    lv_obj_align(temp_label_, LV_ALIGN_TOP_MID, 0, 80);

    // Description (pill badge)
    lv_obj_t* desc_pill = lv_obj_create(app_root_);
    lv_obj_remove_style_all(desc_pill);
    lv_obj_set_size(desc_pill, 160, 36);
    lv_obj_set_style_radius(desc_pill, 18, 0);
    lv_obj_set_style_bg_opa(desc_pill, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(desc_pill, lv_color_make(255, 165, 0), 0);
    lv_obj_align(desc_pill, LV_ALIGN_TOP_MID, 0, 150);

    desc_label_ = lv_label_create(desc_pill);
    lv_label_set_text(desc_label_, "Dang tai...");
    lv_obj_set_style_text_color(desc_label_, lv_color_make(255, 200, 80), 0);
    lv_obj_set_style_text_font(desc_label_, &lv_font_montserrat_24, 0);
    lv_obj_align(desc_label_, LV_ALIGN_CENTER, 0, 0);

    // Details (humidity + wind)
    detail_label_ = lv_label_create(app_root_);
    lv_label_set_text(detail_label_, "");
    lv_obj_set_style_text_color(detail_label_, lv_color_make(140, 180, 220), 0);
    lv_obj_align(detail_label_, LV_ALIGN_TOP_MID, 0, 200);

    // Forecast container (bottom cards)
    forecast_container_ = lv_obj_create(app_root_);
    lv_obj_remove_style_all(forecast_container_);
    lv_obj_set_size(forecast_container_, LV_PCT(95), 110);
    lv_obj_align(forecast_container_, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_flex_flow(forecast_container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(forecast_container_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(forecast_container_, LV_SCROLLBAR_MODE_OFF);

    // Loading indicator
    loading_label_ = lv_label_create(app_root_);
    lv_label_set_text(loading_label_, LV_SYMBOL_REFRESH " Loading...");
    lv_obj_set_style_text_color(loading_label_, lv_color_make(100, 100, 100), 0);
    lv_obj_align(loading_label_, LV_ALIGN_CENTER, 0, 0);
}

void WeatherApp::UpdateUI() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    if (!data_.valid || !temp_label_) return;

    if (loading_label_) lv_obj_add_flag(loading_label_, LV_OBJ_FLAG_HIDDEN);

    // Temperature
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f°C", data_.temp);
    lv_label_set_text(temp_label_, buf);

    // Description
    lv_label_set_text(desc_label_, WeatherDescription(data_.weather_code));

    // Details
    snprintf(buf, sizeof(buf), LV_SYMBOL_TINT " %d%%   ~   %.0f km/h", data_.humidity, data_.wind);
    lv_label_set_text(detail_label_, buf);

    // Update forecast
    if (forecast_container_) {
        lv_obj_clean(forecast_container_);
        const char* day_names[] = {"HN", "+1", "+2", "+3", "+4"};
        for (int i = 0; i < 5; i++) {
            lv_obj_t* card = lv_obj_create(forecast_container_);
            lv_obj_remove_style_all(card);
            lv_obj_set_size(card, 65, 95);
            lv_obj_set_style_radius(card, 12, 0);
            lv_obj_set_style_bg_opa(card, LV_OPA_20, 0);
            lv_obj_set_style_bg_color(card, lv_color_make(60, 60, 60), 0);
            lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_row(card, 8, 0);

            lv_obj_t* dl = lv_label_create(card);
            lv_label_set_text(dl, day_names[i]);
            lv_obj_set_style_text_color(dl, lv_color_make(180, 180, 180), 0);

            lv_obj_t* wl = lv_label_create(card);
            lv_label_set_text(wl, WeatherDescription(data_.daily_code[i]));
            lv_obj_set_style_text_color(wl, lv_color_make(255, 200, 80), 0);

            char mb[16];
            snprintf(mb, sizeof(mb), "%.0f/%.0f", data_.daily_max[i], data_.daily_min[i]);
            lv_obj_t* ml = lv_label_create(card);
            lv_label_set_text(ml, mb);
            lv_obj_set_style_text_color(ml, lv_color_white(), 0);
        }
    }
}

void WeatherApp::StartFetchTask() {
    if (fetch_running_) return;
    fetch_running_ = true;
    xTaskCreate(FetchTaskFunc, "weather_fetch", 8192, this, 2, &fetch_task_);
}

void WeatherApp::StopFetchTask() {
    fetch_running_ = false;
    if (fetch_task_) {
        vTaskDelay(pdMS_TO_TICKS(100));
        fetch_task_ = nullptr;
    }
}

void WeatherApp::FetchTaskFunc(void* arg) {
    auto* app = static_cast<WeatherApp*>(arg);

    char url[512];
    snprintf(url, sizeof(url),
        "https://api.open-meteo.com/v1/forecast?"
        "latitude=%.2f&longitude=%.2f"
        "&current=temperature_2m,relative_humidity_2m,wind_speed_10m,weather_code"
        "&daily=weather_code,temperature_2m_max,temperature_2m_min"
        "&timezone=Asia%%2FHo_Chi_Minh&forecast_days=5",
        LAT, LON);

    std::string response;
    if (AppHttpClient::Get(url, response)) {
        if (app->ParseResponse(response)) {
            app->has_new_data_ = true;
        }
    }

    app->fetch_running_ = false;
    app->fetch_task_ = nullptr;
    vTaskDelete(nullptr);
}

bool WeatherApp::ParseResponse(const std::string& json) {
    cJSON* root = cJSON_Parse(json.c_str());
    if (!root) {
        ESP_LOGE(TAG, "JSON parse failed");
        return false;
    }

    std::lock_guard<std::mutex> lock(data_mutex_);

    cJSON* current = cJSON_GetObjectItem(root, "current");
    if (current) {
        cJSON* t = cJSON_GetObjectItem(current, "temperature_2m");
        cJSON* h = cJSON_GetObjectItem(current, "relative_humidity_2m");
        cJSON* w = cJSON_GetObjectItem(current, "wind_speed_10m");
        cJSON* c = cJSON_GetObjectItem(current, "weather_code");
        if (t) data_.temp = (float)t->valuedouble;
        if (h) data_.humidity = h->valueint;
        if (w) data_.wind = (float)w->valuedouble;
        if (c) data_.weather_code = c->valueint;
    }

    cJSON* daily = cJSON_GetObjectItem(root, "daily");
    if (daily) {
        cJSON* codes = cJSON_GetObjectItem(daily, "weather_code");
        cJSON* maxs = cJSON_GetObjectItem(daily, "temperature_2m_max");
        cJSON* mins = cJSON_GetObjectItem(daily, "temperature_2m_min");
        for (int i = 0; i < 5; i++) {
            if (codes && i < cJSON_GetArraySize(codes))
                data_.daily_code[i] = cJSON_GetArrayItem(codes, i)->valueint;
            if (maxs && i < cJSON_GetArraySize(maxs))
                data_.daily_max[i] = (float)cJSON_GetArrayItem(maxs, i)->valuedouble;
            if (mins && i < cJSON_GetArraySize(mins))
                data_.daily_min[i] = (float)cJSON_GetArrayItem(mins, i)->valuedouble;
        }
    }

    data_.valid = true;
    cJSON_Delete(root);
    ESP_LOGI(TAG, "Weather: %.1f°C, humidity %d%%, code %d", data_.temp, data_.humidity, data_.weather_code);
    return true;
}

const char* WeatherApp::WeatherDescription(int code) {
    if (code == 0) return "Nang";
    if (code <= 3) return "It may";
    if (code <= 48) return "Suong mu";
    if (code <= 57) return "Mua phun";
    if (code <= 67) return "Mua";
    if (code <= 77) return "Tuyet";
    if (code <= 82) return "Mua rao";
    if (code <= 99) return "Dong";
    return "?";
}
