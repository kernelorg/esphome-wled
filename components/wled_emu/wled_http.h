#pragma once

// WLED-compatible HTTP API on top of ESPHome's web_server_base.
//
//   /win[&T=0|1|2][&A=0..255]  legacy XML (WLED master Nodes-UI toggle)
//   /json, /json/state         state in WLED shape; POST applies a delta
//   /json/info                 node identity (Android app + master)
//   /json/si                   state+info+effects+palettes (app bootstrap)
//   /json/nodes                discovered peers
//   /json/effects, /json/palettes, /json/cfg
//   /json/sync                 GET {"send":m,"recv":m}; POST same → masks
//   /sync                      HTML: 2×8 sync-group checkbox matrix
//   /nodes                     HTML: discovered peers with Toggle buttons
//   /ws                        WebSocket (ESP32): pushes {"state","info"} on
//                              connect, on change and every 30 s; accepts
//                              state JSON like POST /json/state (WLED apps
//                              use only this for online status and RSSI)
//
// Works on both web_server_base backends: ESPAsyncWebServer (ESP8266 /
// RP2040 Arduino) and ESPHome's web_server_idf shim (all ESP32 builds).

#include "esphome/core/defines.h"

#if defined(USE_WLED_EMU_HTTP) && defined(USE_LIGHT)

#include "esphome/components/web_server_base/web_server_base.h"
#include "wled_emu.h"

#include <string>
#ifdef USE_ESP32
#include <esp_http_server.h>
#include <mutex>
#include <vector>
#endif

namespace esphome {
namespace wled_emu {

class WledHttpHandler : public AsyncWebHandler {
 public:
  explicit WledHttpHandler(WledEmu *parent) : parent_(parent) {}

  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;
  void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override;

  static void install(WledEmu *parent);

#ifdef USE_ESP32
  /// Main-loop work for the WebSocket: attach to the server, apply received
  /// commands, push the state to the clients.
  void ws_loop();
  size_t ws_client_count();
#endif

 protected:
#ifdef USE_ESP32
  static bool uri_match_(const char *tmpl, const char *uri, size_t len);
  static esp_err_t ws_on_connect_(httpd_req_t *req);
  static esp_err_t ws_handler_(httpd_req_t *req);
  /// Send `text` to every connected WebSocket client (from the httpd task).
  void ws_broadcast_(std::string text);

  httpd_handle_t ws_hd_{nullptr};
  std::mutex ws_mutex_;              // guards the fields below (httpd task ↔ main loop)
  std::vector<int> ws_fds_;          // connected clients
  std::vector<std::string> ws_rx_;   // received text frames, applied in the main loop
  bool ws_new_client_{false};
  std::string ws_last_state_;
  uint32_t ws_last_check_{0};
  uint32_t ws_last_push_{0};
#endif
  void handle_win_(AsyncWebServerRequest *request, const std::string &url);
  void send_(AsyncWebServerRequest *request, int code, const char *type, const std::string &body, bool cors);
  std::string state_json_();
  std::string info_json_();
  std::string full_json_();  // {"state":..,"info":..}
  std::string effects_json_();
  std::string nodes_json_();
  void apply_state_(const char *json);
  void apply_sync_(const char *json);

  WledEmu *parent_;
};

}  // namespace wled_emu
}  // namespace esphome

#endif
