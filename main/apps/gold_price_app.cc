#include "gold_price_app.h"
#include "app_http_client.h"
#include "board.h"
#include "display.h"

#include <cJSON.h>
#include <cstdio>
#include <ctime>
#include <esp_log.h>

#define TAG "GoldPriceApp"

extern const lv_font_t lv_font_montserrat_14;
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
    lv_obj_set_flex_flow(app_root_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(app_root_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(app_root_, 0, 0);
    lv_obj_set_style_pad_bottom(app_root_, 32, 0);
    lv_obj_set_style_pad_row(app_root_, 0, 0);

    // ═══ Gold accent bar (top) ═══
    lv_obj_t* accent = lv_obj_create(app_root_);
    lv_obj_remove_style_all(accent);
    lv_obj_set_size(accent, LV_PCT(100), 4);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(accent, lv_color_make(255, 195, 0), 0);
    lv_obj_set_style_bg_grad_color(accent, lv_color_make(200, 150, 0), 0);
    lv_obj_set_style_bg_grad_dir(accent, LV_GRAD_DIR_HOR, 0);

    // ═══ Title ═══
    title_label_ = lv_label_create(app_root_);
    lv_label_set_text(title_label_, "VANG SJC 9999");
    lv_obj_set_style_text_color(title_label_, lv_color_make(255, 215, 0), 0);
    lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_24, 0);
    lv_obj_set_style_pad_top(title_label_, 14, 0);
    lv_obj_set_style_pad_bottom(title_label_, 10, 0);

    // ═══ BUY CARD (hero) ═══
    lv_obj_t* buy_card = lv_obj_create(app_root_);
    lv_obj_remove_style_all(buy_card);
    lv_obj_set_size(buy_card, LV_PCT(88), LV_SIZE_CONTENT);
    lv_obj_set_style_radius(buy_card, 20, 0);
    lv_obj_set_style_bg_opa(buy_card, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(buy_card, lv_color_make(25, 22, 10), 0);
    lv_obj_set_style_bg_grad_color(buy_card, lv_color_make(40, 35, 15), 0);
    lv_obj_set_style_bg_grad_dir(buy_card, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(buy_card, 1, 0);
    lv_obj_set_style_border_color(buy_card, lv_color_make(80, 65, 0), 0);
    lv_obj_set_flex_flow(buy_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(buy_card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(buy_card, 16, 0);
    lv_obj_set_style_pad_row(buy_card, 4, 0);
    lv_obj_set_scrollbar_mode(buy_card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(buy_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* mua_lbl = lv_label_create(buy_card);
    lv_label_set_text(mua_lbl, "MUA VAO");
    lv_obj_set_style_text_color(mua_lbl, lv_color_make(180, 160, 80), 0);
    lv_obj_set_style_text_font(mua_lbl, &lv_font_montserrat_24, 0);

    price_vnd_label_ = lv_label_create(buy_card);
    lv_label_set_text(price_vnd_label_, "---");
    lv_obj_set_style_text_color(price_vnd_label_, lv_color_make(255, 225, 100), 0);
    lv_obj_set_style_text_font(price_vnd_label_, &lv_font_montserrat_48, 0);

    lv_obj_t* unit_lbl = lv_label_create(buy_card);
    lv_label_set_text(unit_lbl, "Trieu VND / Luong");
    lv_obj_set_style_text_color(unit_lbl, lv_color_make(120, 110, 60), 0);
    lv_obj_set_style_text_font(unit_lbl, &lv_font_montserrat_24, 0);

    // ═══ Separator ═══
    lv_obj_t* sep = lv_obj_create(app_root_);
    lv_obj_remove_style_all(sep);
    lv_obj_set_size(sep, LV_PCT(50), 1);
    lv_obj_set_style_bg_opa(sep, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(sep, lv_color_make(255, 215, 0), 0);
    lv_obj_set_style_margin_top(sep, 10, 0);
    lv_obj_set_style_margin_bottom(sep, 10, 0);

    // ═══ SELL CARD ═══
    lv_obj_t* sell_card = lv_obj_create(app_root_);
    lv_obj_remove_style_all(sell_card);
    lv_obj_set_size(sell_card, LV_PCT(88), LV_SIZE_CONTENT);
    lv_obj_set_style_radius(sell_card, 14, 0);
    lv_obj_set_style_bg_opa(sell_card, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(sell_card, lv_color_make(30, 15, 15), 0);
    lv_obj_set_style_border_width(sell_card, 2, 0);
    lv_obj_set_style_border_color(sell_card, lv_color_make(180, 60, 60), 0);
    lv_obj_set_style_border_side(sell_card, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_pad_all(sell_card, 14, 0);
    lv_obj_set_scrollbar_mode(sell_card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(sell_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* sell_title = lv_label_create(sell_card);
    lv_label_set_text(sell_title, "BAN RA");
    lv_obj_set_style_text_color(sell_title, lv_color_make(180, 100, 100), 0);
    lv_obj_set_style_text_font(sell_title, &lv_font_montserrat_24, 0);
    lv_obj_align(sell_title, LV_ALIGN_TOP_LEFT, 0, 0);

    price_usd_label_ = lv_label_create(sell_card);
    lv_label_set_text(price_usd_label_, "---");
    lv_obj_set_style_text_color(price_usd_label_, lv_color_make(255, 130, 130), 0);
    lv_obj_set_style_text_font(price_usd_label_, &lv_font_montserrat_36, 0);
    lv_obj_align(price_usd_label_, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    // ═══ Change pill ═══
    change_label_ = lv_label_create(app_root_);
    lv_label_set_text(change_label_, "--");
    lv_obj_set_style_text_font(change_label_, &lv_font_montserrat_24, 0);
    lv_obj_set_style_margin_top(change_label_, 8, 0);

    // ═══ Loading ═══
    loading_label_ = lv_label_create(app_root_);
    lv_label_set_text(loading_label_, LV_SYMBOL_REFRESH " Dang tai...");
    lv_obj_set_style_text_color(loading_label_, lv_color_make(120, 100, 50), 0);
    lv_obj_set_style_text_font(loading_label_, &lv_font_montserrat_24, 0);

    // ═══ Spacer ═══
    lv_obj_t* spacer = lv_obj_create(app_root_);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, 1, 1);
    lv_obj_set_flex_grow(spacer, 1);

    // ═══ Update time ═══
    update_label_ = lv_label_create(app_root_);
    lv_label_set_text(update_label_, "");
    lv_obj_set_style_text_color(update_label_, lv_color_make(80, 70, 40), 0);
    lv_obj_set_style_text_font(update_label_, &lv_font_montserrat_24, 0);
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
