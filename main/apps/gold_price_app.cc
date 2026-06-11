#include "gold_price_app.h"
#include "app_http_client.h"
#include "board.h"
#include "display.h"

#include <cJSON.h>
#include <cstdio>
#include <ctime>
#include <esp_log.h>

#define TAG "GoldPriceApp"

extern const lv_font_t lv_font_montserrat_24;
extern const lv_font_t lv_font_montserrat_36;
extern const lv_font_t lv_font_montserrat_48;

void GoldPriceApp::OnEnter(lv_obj_t* parent) {
    {
        auto display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        CreateRoot(parent);
        BuildUI();
    }
    tick_counter_ = 0;
    StartFetchTask();
}

void GoldPriceApp::OnExit() {
    StopFetchTask();

    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    title_label_ = price_usd_label_ = price_vnd_label_ = nullptr;
    change_label_ = unit_label_ = update_label_ = loading_label_ = nullptr;
    DestroyRoot();
}

void GoldPriceApp::OnTick() {
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

void GoldPriceApp::BuildUI() {
    // Header background (Finance style)
    lv_obj_t* header = lv_obj_create(app_root_);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, LV_PCT(100), 50);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(header, lv_color_make(30, 30, 30), 0);
    lv_obj_set_style_border_width(header, 2, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(header, lv_color_make(255, 215, 0), 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    // Title
    title_label_ = lv_label_create(header);
    lv_label_set_text(title_label_, "GIA VANG SJC 9999");
    lv_obj_set_style_text_color(title_label_, lv_color_make(255, 215, 0), 0);
    lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_24, 0);
    lv_obj_align(title_label_, LV_ALIGN_CENTER, 0, 0);

    // Main VND Price Card (MUA VAO)
    lv_obj_t* vnd_card = lv_obj_create(app_root_);
    lv_obj_remove_style_all(vnd_card);
    lv_obj_set_size(vnd_card, 300, 120);
    lv_obj_set_style_radius(vnd_card, 15, 0);
    lv_obj_set_style_bg_opa(vnd_card, LV_OPA_20, 0);
    lv_obj_set_style_bg_color(vnd_card, lv_color_make(255, 215, 0), 0);
    lv_obj_align(vnd_card, LV_ALIGN_TOP_MID, 0, 80);

    lv_obj_t* mua_label = lv_label_create(vnd_card);
    lv_label_set_text(mua_label, "MUA VAO:");
    lv_obj_set_style_text_color(mua_label, lv_color_make(180, 180, 180), 0);
    lv_obj_align(mua_label, LV_ALIGN_TOP_LEFT, 15, 10);

    price_vnd_label_ = lv_label_create(vnd_card);
    lv_label_set_text(price_vnd_label_, "---");
    lv_obj_set_style_text_color(price_vnd_label_, lv_color_white(), 0);
    lv_obj_set_style_text_font(price_vnd_label_, &lv_font_montserrat_48, 0);
    lv_obj_align(price_vnd_label_, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* vnd_unit = lv_label_create(vnd_card);
    lv_label_set_text(vnd_unit, "Trieu VND / Luong");
    lv_obj_set_style_text_color(vnd_unit, lv_color_make(180, 180, 180), 0);
    lv_obj_align(vnd_unit, LV_ALIGN_BOTTOM_MID, 0, -10);

    // SELL Price Details (BAN RA)
    price_usd_label_ = lv_label_create(app_root_); // Re-using variable name for Sell Price
    lv_label_set_text(price_usd_label_, "BAN RA: ---");
    lv_obj_set_style_text_color(price_usd_label_, lv_color_make(255, 150, 150), 0);
    lv_obj_set_style_text_font(price_usd_label_, &lv_font_montserrat_36, 0);
    lv_obj_align(price_usd_label_, LV_ALIGN_TOP_MID, 0, 220);

    // Change indicator
    change_label_ = lv_label_create(app_root_);
    lv_label_set_text(change_label_, "--");
    lv_obj_set_style_text_font(change_label_, &lv_font_montserrat_24, 0);
    lv_obj_align(change_label_, LV_ALIGN_TOP_MID, 0, 270);

    // Last update time
    update_label_ = lv_label_create(app_root_);
    lv_label_set_text(update_label_, "Dang tai du lieu...");
    lv_obj_set_style_text_color(update_label_, lv_color_make(100, 100, 100), 0);
    lv_obj_align(update_label_, LV_ALIGN_BOTTOM_MID, 0, -40);

    // Loading overlay
    loading_label_ = lv_label_create(app_root_);
    lv_label_set_text(loading_label_, LV_SYMBOL_REFRESH " Loading...");
    lv_obj_set_style_text_color(loading_label_, lv_color_make(150, 150, 150), 0);
    lv_obj_align(loading_label_, LV_ALIGN_CENTER, 0, 0);
}

void GoldPriceApp::UpdateUI() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    if (!data_.valid || !price_vnd_label_) return;

    if (loading_label_) lv_obj_add_flag(loading_label_, LV_OBJ_FLAG_HIDDEN);

    char buf[64];

    // Format Buy price (MUA VAO)
    float buy_mil = data_.buy / 1000000.0f;
    snprintf(buf, sizeof(buf), "%.2f", buy_mil);
    lv_label_set_text(price_vnd_label_, buf);

    // Format Sell price (BAN RA)
    float sell_mil = data_.sell / 1000000.0f;
    snprintf(buf, sizeof(buf), "BAN RA: %.2f", sell_mil);
    lv_label_set_text(price_usd_label_, buf);

    // Change indicator
    if (data_.change_buy != 0 || data_.change_sell != 0) {
        float chg = (data_.change_buy != 0) ? data_.change_buy : data_.change_sell;
        float chg_mil = chg / 1000000.0f;
        
        if (chg > 0) {
            snprintf(buf, sizeof(buf), LV_SYMBOL_UP " TANG %.2f Tr", chg_mil);
            lv_obj_set_style_text_color(change_label_, lv_color_make(0, 255, 100), 0);
        } else {
            snprintf(buf, sizeof(buf), LV_SYMBOL_DOWN " GIAM %.2f Tr", -chg_mil);
            lv_obj_set_style_text_color(change_label_, lv_color_make(255, 80, 80), 0);
        }
        lv_label_set_text(change_label_, buf);
    } else {
        lv_label_set_text(change_label_, LV_SYMBOL_MINUS " GIA ON DINH");
        lv_obj_set_style_text_color(change_label_, lv_color_make(150, 150, 150), 0);
    }

    // Update time
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    if (t && t->tm_year >= (2025 - 1900)) {
        strftime(buf, sizeof(buf), "Cap nhat: %H:%M", t);
        lv_label_set_text(update_label_, buf);
    }
}

void GoldPriceApp::StartFetchTask() {
    if (fetch_running_) return;
    fetch_running_ = true;
    xTaskCreate(FetchTaskFunc, "gold_fetch", 8192, this, 2, &fetch_task_);
}

void GoldPriceApp::StopFetchTask() {
    fetch_running_ = false;
    if (fetch_task_) {
        vTaskDelay(pdMS_TO_TICKS(100));
        fetch_task_ = nullptr;
    }
}

void GoldPriceApp::FetchTaskFunc(void* arg) {
    auto* app = static_cast<GoldPriceApp*>(arg);

    // Vietnam Gold Price API
    const char* url = "https://www.vang.today/api/prices?type=SJL1L10";

    std::string response;
    if (AppHttpClient::Get(url, response)) {
        if (app->ParseResponse(response)) {
            app->has_new_data_ = true;
        }
    } else {
        ESP_LOGW(TAG, "Gold price fetch failed");
    }

    app->fetch_running_ = false;
    app->fetch_task_ = nullptr;
    vTaskDelete(nullptr);
}

bool GoldPriceApp::ParseResponse(const std::string& json) {
    cJSON* root = cJSON_Parse(json.c_str());
    if (!root) return false;

    std::lock_guard<std::mutex> lock(data_mutex_);
    
    cJSON* buy_json = cJSON_GetObjectItem(root, "buy");
    cJSON* sell_json = cJSON_GetObjectItem(root, "sell");
    cJSON* chg_buy = cJSON_GetObjectItem(root, "change_buy");
    cJSON* chg_sell = cJSON_GetObjectItem(root, "change_sell");

    if (buy_json && sell_json && cJSON_IsNumber(buy_json)) {
        data_.buy = buy_json->valuedouble;
        data_.sell = sell_json->valuedouble;
        if (chg_buy && cJSON_IsNumber(chg_buy)) data_.change_buy = chg_buy->valuedouble;
        if (chg_sell && cJSON_IsNumber(chg_sell)) data_.change_sell = chg_sell->valuedouble;
        
        data_.valid = true;
        ESP_LOGI(TAG, "VN Gold: MUA %.0f, BAN %.0f", data_.buy, data_.sell);
        cJSON_Delete(root);
        return true;
    }

    cJSON_Delete(root);
    return false;
}
