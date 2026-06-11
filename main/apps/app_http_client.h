#pragma once
#include <string>

/**
 * Simple HTTP client wrapper for mini apps.
 * Uses Board::GetNetwork()->CreateHttp() under the hood.
 * MUST be called from a background FreeRTOS task, not the main loop.
 */
class AppHttpClient {
public:
    /**
     * Perform a GET request and return the response body.
     * @param url Full URL (http or https)
     * @param response Output string for response body
     * @param timeout_retry Retry count for network creation (default 3)
     * @return true on success (HTTP 200), false otherwise
     */
    static bool Get(const char* url, std::string& response, int timeout_retry = 3);
};
