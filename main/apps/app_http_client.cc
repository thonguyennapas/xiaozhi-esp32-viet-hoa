#include "app_http_client.h"
#include "board.h"

#include <esp_log.h>

#define TAG "AppHttp"

bool AppHttpClient::Get(const char* url, std::string& response, int timeout_retry) {
    response.clear();

    auto network = Board::GetInstance().GetNetwork();
    if (!network) {
        ESP_LOGE(TAG, "Network not available");
        return false;
    }

    auto http = network->CreateHttp(timeout_retry);
    if (!http) {
        ESP_LOGE(TAG, "Failed to create HTTP client");
        return false;
    }

    if (!http->Open("GET", url)) {
        ESP_LOGE(TAG, "Failed to open: %s", url);
        return false;
    }

    int status = http->GetStatusCode();
    if (status != 200) {
        ESP_LOGW(TAG, "HTTP %d from %s", status, url);
        http->Close();
        return false;
    }

    response = http->ReadAll();
    http->Close();

    ESP_LOGI(TAG, "HTTP 200 from %s (%d bytes)", url, (int)response.size());
    return true;
}
