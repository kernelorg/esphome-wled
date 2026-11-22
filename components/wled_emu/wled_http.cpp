#include "wled_http.h"

#if defined(USE_WLED_EMU_HTTP) && defined(USE_LIGHT)

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/json/json_util.h"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif
#ifdef USE_ESP32
#include <esp_heap_caps.h>
#endif
#ifdef USE_ESP8266
#include <Esp.h>
#endif

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace esphome {
namespace wled_emu {

static const char *const TAG = "wled_emu.http";

// ── portability helpers ────────────────────────────────────────────────

static std::string request_url(AsyncWebServerRequest *req) {
#ifdef USE_ESP32
  char buf[AsyncWebServerRequest::URL_BUF_SIZE];
  return req->url_to(buf).str();
#else
  return std::string(req->url().c_str());
#endif
}

static bool starts_with(const std::string &s, const char *p) { return s.rfind(p, 0) == 0; }

static std::string fmt(const char *f, ...) __attribute__((format(printf, 1, 2)));
static std::string fmt(const char *f, ...) {
  va_list ap;
  va_start(ap, f);
  char small[256];
  int n = vsnprintf(small, sizeof(small), f, ap);
  va_end(ap);
  if (n < 0)
    return {};
  if ((size_t) n < sizeof(small))
    return std::string(small, n);
  std::string out(n, '\0');
  va_start(ap, f);
  vsnprintf(&out[0], n + 1, f, ap);
  va_end(ap);
  return out;
}

static std::string json_escape(const std::string &s) {
  std::string o;
  o.reserve(s.size() + 2);
  for (char c : s) {
    if (c == '"' || c == '\\') {
      o += '\\';
      o += c;
    } else if ((uint8_t) c < 0x20) {
      o += ' ';
    } else {
      o += c;
    }
  }
  return o;
}

static uint32_t free_heap() {
#ifdef USE_ESP32
  return heap_caps_get_free_size(MALLOC_CAP_8BIT);
#elif defined(USE_ESP8266)
  return ESP.getFreeHeap();
#else
  return 0;
#endif
}

static const char *arch_name() {
#ifdef USE_ESP8266
  return "esp8266";
#elif defined(USE_ESP32)
  return "esp32";
#else
  return "unknown";
#endif
}

// WLED "release" (build environment) name.
static const char *release_name() {
#ifdef USE_ESP8266
  return "ESP8266";
#elif defined(USE_ESP32)
  return "ESP32";
#else
  return "unknown";
#endif
}

// Only one request body is in flight at a time on both server backends.
static std::string g_body;

// ── install / dispatch ─────────────────────────────────────────────────

void WledHttpHandler::install(WledEmu *parent) {
  auto *base = web_server_base::global_web_server_base;
  if (base == nullptr) {
    ESP_LOGW(TAG, "web_server_base not available — HTTP API disabled");
    return;
  }
  auto *handler = new WledHttpHandler(parent);  // NOLINT
  base->add_handler(handler);
#ifdef USE_ESP32
  parent->set_http_loop([handler]() { handler->ws_loop(); });
  ESP_LOGI(TAG, "WLED HTTP API installed (/win, /json/*, /sync, /nodes, /ws)");
#else
  ESP_LOGI(TAG, "WLED HTTP API installed (/win, /json/*, /sync, /nodes)");
#endif
}

bool WledHttpHandler::canHandle(AsyncWebServerRequest *request) const {
  std::string url = request_url(request);
  ESP_LOGD(TAG, "%s %s", request->method() == HTTP_POST ? "POST" : request->method() == HTTP_OPTIONS ? "OPTIONS" : "GET",
           url.c_str());
  return starts_with(url, "/win") || starts_with(url, "/json") || url == "/sync" || url == "/nodes";
}

void WledHttpHandler::send_(AsyncWebServerRequest *request, int code, const char *type, const std::string &body,
                            bool cors) {
  auto *resp = request->beginResponse(code, type, body.c_str());
  resp->addHeader("Cache-Control", "no-cache");
  if (cors)
    resp->addHeader("Access-Control-Allow-Origin", "*");
  request->send(resp);
}

static const char SYNC_PAGE[] = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Sync Groups</title>
<style>
body{font-family:sans-serif;background:#222;color:#eee;padding:18px}
table{border-collapse:collapse;margin:8px 0}
th,td{padding:8px 14px;text-align:center;border:1px solid #444}
th{background:#333}input{transform:scale(1.6);cursor:pointer}
.note{font-size:.85em;color:#888;margin-top:14px}a{color:#7af}
</style></head><body>
<h2>WLED Sync Groups</h2>
<table id="t"><tr><th></th><th>1</th><th>2</th><th>3</th><th>4</th><th>5</th><th>6</th><th>7</th><th>8</th></tr></table>
<p class="note">Changes apply instantly.</p>
<p><a href="/">&larr; Main</a> &nbsp; <a href="/nodes">Network nodes &rarr;</a></p>
<script>
const t=document.getElementById('t');let st={send:0,recv:0};
for(const k of['send','recv']){const tr=document.createElement('tr');
tr.innerHTML='<th>'+(k=='send'?'Send':'Receive')+'</th>';
for(let g=0;g<8;g++){const td=document.createElement('td');const cb=document.createElement('input');
cb.type='checkbox';cb.dataset.k=k;cb.dataset.g=g;cb.onchange=save;td.appendChild(cb);tr.appendChild(td);}
t.appendChild(tr);}
function paint(){document.querySelectorAll('input').forEach(c=>c.checked=!!(st[c.dataset.k]&(1<<c.dataset.g)));}
async function load(){try{st=await(await fetch('/json/sync')).json();paint();}catch(e){}}
async function save(){const m={send:0,recv:0};document.querySelectorAll('input').forEach(c=>{if(c.checked)m[c.dataset.k]|=1<<c.dataset.g;});
await fetch('/json/sync',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(m)});load();}
load();
</script></body></html>)HTML";

static const char NODES_PAGE[] = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>WLED nodes</title>
<style>
body{font-family:sans-serif;background:#222;color:#eee;padding:18px}
table{border-collapse:collapse;width:100%;max-width:700px}
th,td{padding:8px 12px;border-bottom:1px solid #444;text-align:left}th{background:#333}
button{padding:6px 14px;background:#444;color:#fff;border:1px solid #666;cursor:pointer}
.on{color:#5f5}.off{color:#888}a{color:#7af}.note{font-size:.85em;color:#888;margin-top:14px}
</style></head><body>
<h2>Discovered WLED Nodes</h2>
<table id="t"><thead><tr><th>Name</th><th>IP</th><th>Type</th><th>vid</th><th>Age</th><th>State</th><th></th></tr></thead><tbody></tbody></table>
<p class="note">Populated from sysInfo broadcasts on the info port. Toggle sends <code>/win&amp;T=2</code> to the peer.</p>
<p><a href="/">&larr; Main</a> &nbsp; <a href="/sync">Sync groups &rarr;</a></p>
<script>
function esc(s){return String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));}
async function load(){let d=[];try{d=await(await fetch('/json/nodes')).json();}catch(e){return;}
const tb=document.querySelector('#t tbody');tb.innerHTML='';
if(!d.length){tb.innerHTML='<tr><td colspan=7 style="color:#888;text-align:center;padding:18px">No nodes yet&hellip;</td></tr>';return;}
for(const n of d){const on=!!(n.type&128);const tr=document.createElement('tr');
tr.innerHTML=`<td><a href="http://${esc(n.ip)}" target="_blank">${esc(n.name||n.ip)}</a></td><td>${esc(n.ip)}</td><td>${n.type&127}</td><td>${n.vid}</td><td>${n.age}s</td><td class="${on?'on':'off'}">${on?'ON':'OFF'}</td><td><button data-ip="${esc(n.ip)}">Toggle</button></td>`;
tb.appendChild(tr);}
document.querySelectorAll('button[data-ip]').forEach(b=>b.onclick=async()=>{try{await fetch('http://'+b.dataset.ip+'/win&T=2',{mode:'no-cors'});}catch(e){}setTimeout(load,300);});}
load();setInterval(load,5000);
</script></body></html>)HTML";

void WledHttpHandler::handleRequest(AsyncWebServerRequest *request) {
  std::string url = request_url(request);
  if (starts_with(url, "/win")) {
    this->handle_win_(request, url);
    return;
  }
  if (url == "/sync") {
    this->send_(request, 200, "text/html", SYNC_PAGE, false);
    return;
  }
  if (url == "/nodes") {
    this->send_(request, 200, "text/html", NODES_PAGE, false);
    return;
  }
  if (request->method() == HTTP_OPTIONS) {
    auto *resp = request->beginResponse(200, "text/plain", "");
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(resp);
    return;
  }
  if (request->method() == HTTP_POST) {
    // Body already applied in handleBody(); reply with the new state.
    if (url == "/json/sync") {
      this->send_(request, 200, "application/json",
                  fmt("{\"send\":%u,\"recv\":%u}", this->parent_->get_send_groups(),
                      this->parent_->get_receive_groups()),
                  true);
    } else {
      this->send_(request, 200, "application/json", this->state_json_(), true);
    }
    return;
  }
  if (url == "/json/info") {
    this->send_(request, 200, "application/json", this->info_json_(), true);
  } else if (url == "/json/state") {
    this->send_(request, 200, "application/json", this->state_json_(), true);
  } else if (url == "/json/effects") {
    this->send_(request, 200, "application/json", this->effects_json_(), true);
  } else if (url == "/json/palettes") {
    this->send_(request, 200, "application/json", "[\"Default\"]", true);
  } else if (url == "/json/cfg") {
    std::string n = json_escape(App.get_name());
    this->send_(request, 200, "application/json", fmt("{\"id\":{\"mdns\":\"%s\",\"name\":\"%s\"}}", n.c_str(), n.c_str()),
                true);
  } else if (url == "/json/nodes") {
    this->send_(request, 200, "application/json", this->nodes_json_(), true);
  } else if (url == "/json/sync") {
    this->send_(request, 200, "application/json",
                fmt("{\"send\":%u,\"recv\":%u}", this->parent_->get_send_groups(),
                    this->parent_->get_receive_groups()),
                true);
  } else {
    // /json, /json/si → full bootstrap document
    std::string body = this->full_json_();
    body.pop_back();
    body += ",\"effects\":" + this->effects_json_() + ",\"palettes\":[\"Default\"]}";
    this->send_(request, 200, "application/json", body, true);
  }
}

void WledHttpHandler::handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index,
                                 size_t total) {
  if (request->method() != HTTP_POST || total == 0 || total > 1024)
    return;
  if (index == 0)
    g_body.clear();
  g_body.append(reinterpret_cast<const char *>(data), len);
  if (index + len < total)
    return;
  std::string url = request_url(request);
  if (url == "/json/sync")
    this->apply_sync_(g_body.c_str());
  else if (starts_with(url, "/json"))
    this->apply_state_(g_body.c_str());
  g_body.clear();
}

// ── /win ───────────────────────────────────────────────────────────────

void WledHttpHandler::handle_win_(AsyncWebServerRequest *request, const std::string &url) {
  int t = -1, a = -1;
  // Path form: /win&T=2&A=128 (no '?', so it survives in the URL).
  std::string args = url.size() > 4 ? url.substr(4) : std::string();
  size_t pos = 0;
  while (pos < args.size()) {
    if (args[pos] == '&' || args[pos] == '?') {
      pos++;
      continue;
    }
    size_t amp = args.find('&', pos);
    std::string pair = args.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
    size_t eq = pair.find('=');
    if (eq != std::string::npos) {
      std::string k = pair.substr(0, eq);
      int v = atoi(pair.c_str() + eq + 1);
      if (k == "T")
        t = v;
      else if (k == "A")
        a = v;
    }
    if (amp == std::string::npos)
      break;
    pos = amp + 1;
  }
  // Query form: /win?T=2&A=128
  if (t < 0 && request->hasArg("T"))
    t = atoi(request->arg("T").c_str());
  if (a < 0 && request->hasArg("A"))
    a = atoi(request->arg("A").c_str());

  auto *light = this->parent_->get_light();
  bool was_on = light->remote_values.is_on();
  auto call = light->make_call();
  bool any = false;
  bool on_now = was_on;
  if (t == 0) {
    call.set_state(false);
    on_now = false;
    any = true;
  } else if (t == 1) {
    call.set_state(true);
    on_now = true;
    any = true;
  } else if (t == 2) {
    call.set_state(!was_on);
    on_now = !was_on;
    any = true;
  }
  if (a >= 0 && a <= 255) {
    call.set_state(a > 0);
    on_now = a > 0;
    if (a > 0)
      call.set_brightness(a / 255.0f);
    any = true;
  }
  if (any)
    call.perform();
  int bri = on_now ? (int) (light->remote_values.get_brightness() * 255.0f + 0.5f) : 0;
  if (on_now && a > 0)
    bri = a;
  if (on_now && bri == 0)
    bri = 255;
  std::string name = json_escape(App.get_name());
  this->send_(request, 200, "text/xml",
              fmt("<?xml version=\"1.0\" ?><vs><ac>%d</ac><cl>%d</cl><cl>%d</cl><cl>%d</cl>"
                  "<ix>128</ix><fx>0</fx><sx>128</sx><wv>0</wv><ds>%s</ds></vs>",
                  bri, bri, bri, bri, name.c_str()),
              true);
}

// ── JSON documents ─────────────────────────────────────────────────────

std::string WledHttpHandler::state_json_() {
  auto *light = this->parent_->get_light();
  auto &st = light->remote_values;
  bool on = st.is_on();
  int bri = on ? (int) (st.get_brightness() * 255.0f + 0.5f) : 0;
  // Index into our /json/effects list: 0 Solid, then the light's effects.
  unsigned fx_index = on ? (unsigned) light->get_current_effect_index() : 0;
  // RGB light: its colour; monochrome: white at the current brightness.
  int cr = bri, cg = bri, cb = bri;
  if (light->get_traits().supports_color_capability(light::ColorCapability::RGB)) {
    cr = (int) (st.get_red() * 255.0f + 0.5f);
    cg = (int) (st.get_green() * 255.0f + 0.5f);
    cb = (int) (st.get_blue() * 255.0f + 0.5f);
  }
  return fmt("{\"on\":%s,\"bri\":%d,\"transition\":7,\"ps\":-1,\"pl\":-1,"
             "\"nl\":{\"on\":false,\"dur\":60,\"mode\":1,\"tbri\":0,\"rem\":-1},"
             "\"udpn\":{\"send\":%s,\"recv\":%s,\"sgrp\":%u,\"rgrp\":%u},"
             "\"lor\":0,\"mainseg\":0,"
             "\"seg\":[{\"id\":0,\"start\":0,\"stop\":1,\"len\":1,\"grp\":1,"
             "\"spc\":0,\"of\":0,\"on\":%s,\"frz\":false,\"bri\":255,\"cct\":127,"
             "\"set\":0,\"col\":[[%d,%d,%d],[0,0,0],[0,0,0]],"
             "\"fx\":%u,\"sx\":128,\"ix\":128,\"pal\":0,"
             "\"c1\":128,\"c2\":128,\"c3\":16,\"sel\":true,"
             "\"rev\":false,\"mi\":false,\"o1\":false,\"o2\":false,\"o3\":false,"
             "\"si\":0,\"m12\":0}]}",
             on ? "true" : "false", bri, this->parent_->get_sync_send() ? "true" : "false",
             this->parent_->get_sync_receive() ? "true" : "false", this->parent_->get_send_groups(),
             this->parent_->get_receive_groups(), on ? "true" : "false", cr, cg, cb,
             fx_index);
}

// "Solid" + the bound light's effects; the index is the HTTP/app effect id.
std::string WledHttpHandler::effects_json_() {
  std::string out = "[\"Solid\"";
  for (auto *e : this->parent_->get_light()->get_effects()) {
    out += ",\"";
    out += json_escape(e->get_name().str());
    out += "\"";
  }
  out += "]";
  return out;
}

std::string WledHttpHandler::full_json_() {
  std::string body = "{\"state\":";
  body += this->state_json_();
  body += ",\"info\":";
  body += this->info_json_();
  body += "}";
  return body;
}

std::string WledHttpHandler::info_json_() {
  uint8_t ip[4] = {0, 0, 0, 0};
  WledEmu::local_ip4(ip);
  uint8_t mac[6];
  get_mac_address_raw(mac);
  int rssi = 0, channel = 0;
  uint8_t bssid[6] = {0, 0, 0, 0, 0, 0};
#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_connected()) {
    rssi = wifi::global_wifi_component->wifi_rssi();
    channel = (int) wifi::global_wifi_component->get_wifi_channel();
    auto b = wifi::global_wifi_component->wifi_bssid();
    memcpy(bssid, b.data(), 6);
  }
#endif
  // WLED's getSignalQuality(): 0 % at -100 dBm, 100 % at -50 dBm.
  int signal = rssi == 0 ? 0 : rssi <= -100 ? 0 : rssi >= -50 ? 100 : 2 * (rssi + 100);
  std::string name = json_escape(App.get_name());
  // Stable per-device id in the WLED 16 format (42 hex chars), from the MAC.
  char device_id[43];
  uint32_t h = fnv1_hash(std::string((const char *) mac, 6));
  for (int i = 0; i < 6; i++) {
    snprintf(device_id + i * 7, 8, "%07x", (unsigned) (h & 0x0FFFFFFF));
    h = h * 16777619u ^ (uint32_t) mac[i];
  }
  device_id[42] = 0;
  return fmt("{\"ver\":\"%s\",\"vid\":%u,\"cn\":\"Niji\",\"release\":\"%s\",\"repo\":\"wled/WLED\","
             "\"deviceId\":\"%s\","
             "\"leds\":{\"count\":1,\"pwr\":0,\"fps\":50,\"maxpwr\":850,\"maxseg\":1,\"bootps\":0,"
             "\"seglc\":[1],\"lc\":1,\"rgbw\":false,\"wv\":0,\"cct\":0},"
             "\"str\":false,\"name\":\"%s\",\"simplifiedui\":false,"
             "\"udpport\":%u,\"live\":false,\"liveseg\":-1,"
             "\"lm\":\"\",\"lip\":\"\",\"ws\":-1,\"fxcount\":%u,\"palcount\":1,"
             "\"cpalcount\":0,\"maps\":[],"
             "\"wifi\":{\"bssid\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"rssi\":%d,\"signal\":%d,"
             "\"channel\":%d,\"ap\":false},"
             "\"fs\":{\"u\":0,\"t\":256,\"pmt\":0},\"ndc\":%u,"
             "\"arch\":\"%s\",\"core\":\"esphome\",\"clock\":%u,\"flash\":4,\"lwip\":2,"
             "\"freeheap\":%u,\"uptime\":%u,\"opt\":15,"
             "\"brand\":\"WLED\",\"product\":\"%s\","
             "\"mac\":\"%02x%02x%02x%02x%02x%02x\",\"ip\":\"%u.%u.%u.%u\"}",
             proto::BUILD_VER, (unsigned) proto::BUILD_VID, release_name(), device_id, name.c_str(),
             this->parent_->get_port(), (unsigned) (this->parent_->get_light()->get_effect_count() + 1), bssid[0], bssid[1], bssid[2],
             bssid[3], bssid[4], bssid[5], rssi, signal, channel,
             (unsigned) this->parent_->node_count(), arch_name(),
             (unsigned) (arch_get_cpu_freq_hz() / 1000000u), (unsigned) free_heap(),
             (unsigned) (millis() / 1000), name.c_str(), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], ip[0],
             ip[1], ip[2], ip[3]);
}

std::string WledHttpHandler::nodes_json_() {
  std::string out = "[";
  uint32_t now = millis();
  bool first = true;
  for (auto &n : this->parent_->nodes()) {
    if (!first)
      out += ',';
    first = false;
    out += fmt("{\"name\":\"%s\",\"ip\":\"%s\",\"type\":%u,\"vid\":%u,\"age\":%u}", json_escape(n.name).c_str(),
               n.ip.c_str(), (unsigned) n.type, (unsigned) n.vid, (unsigned) ((now - n.last_seen) / 1000));
  }
  out += ']';
  return out;
}

// ── POST handlers ──────────────────────────────────────────────────────

void WledHttpHandler::apply_sync_(const char *body) {
  json::parse_json(body, [this](JsonObject root) -> bool {
    if (root["send"].is<int>())
      this->parent_->set_send_groups((uint8_t) root["send"].as<int>());
    if (root["recv"].is<int>())
      this->parent_->set_receive_groups((uint8_t) root["recv"].as<int>());
    return true;
  });
}

void WledHttpHandler::apply_state_(const char *body) {
  auto *light = this->parent_->get_light();
  auto *parent = this->parent_;
  json::parse_json(body, [light, parent](JsonObject root) -> bool {
    auto call = light->make_call();
    bool any = false;
    auto apply_bri = [&](int b) {
      if (b < 0 || b > 255)
        return;
      call.set_state(b > 0);
      if (b > 0)
        call.set_brightness(b / 255.0f);
      any = true;
    };
    if (root["on"].is<bool>()) {
      call.set_state(root["on"].as<bool>());
      any = true;
    } else if (root["on"].is<const char *>() && strcmp(root["on"].as<const char *>(), "t") == 0) {
      call.set_state(!light->remote_values.is_on());
      any = true;
    }
    if (root["bri"].is<int>())
      apply_bri(root["bri"].as<int>());
    JsonArray seg = root["seg"].as<JsonArray>();
    if (!seg.isNull() && seg.size() > 0) {
      JsonObject s0 = seg[0].as<JsonObject>();
      if (s0["on"].is<bool>()) {
        call.set_state(s0["on"].as<bool>());
        any = true;
      }
      if (s0["bri"].is<int>())
        apply_bri(s0["bri"].as<int>());
      JsonArray cols = s0["col"].as<JsonArray>();
      if (!cols.isNull() && cols.size() > 0) {
        JsonArray c0 = cols[0].as<JsonArray>();
        bool rgb = light->get_traits().supports_color_capability(light::ColorCapability::RGB);
        if (!c0.isNull() && rgb && c0.size() >= 3) {
          // WLED colour is independent of the master brightness.
          int r = c0[0].as<int>(), g = c0[1].as<int>(), b = c0[2].as<int>();
          if (r | g | b) {
            call.set_state(true);
            call.set_rgb(r / 255.0f, g / 255.0f, b / 255.0f);
            any = true;
          }
        } else if (!c0.isNull()) {
          int v = 0;
          for (JsonVariant x : c0) {
            int n = x.as<int>();
            if (n > v)
              v = n;
          }
          if (v > 0) {
            call.set_state(true);
            call.set_brightness(v / 255.0f);
            any = true;
          }
        }
      }
      if (s0["fx"].is<int>()) {
        // Index into our /json/effects list: 0 Solid, then the light's effects.
        int i = s0["fx"].as<int>();
        if (i >= 0 && (size_t) i <= light->get_effect_count())
          call.set_effect((uint32_t) i);
        any = true;
      }
    }
    if (any)
      call.perform();
    return true;
  });
}

// ── WebSocket /ws (ESP32, esp_http_server) ────────────────────────────
#ifdef USE_ESP32

static constexpr uint32_t WS_CHECK_MS = 250;      // look for state changes
static constexpr uint32_t WS_REFRESH_MS = 30000;  // re-send info (RSSI) anyway
static constexpr size_t WS_MAX_CLIENTS = 4;
static constexpr size_t WS_MAX_FRAME = 1024;

// ESPHome registers catch-all handlers ("" + a match function that accepts
// every URI). Matching runs in registration order, so "/ws" is excluded from
// the catch-alls and only matches our handler.
bool WledHttpHandler::uri_match_(const char *tmpl, const char *uri, size_t len) {
  size_t n = 0;
  while (n < len && uri[n] != '?')
    n++;
  bool is_ws = n == 3 && memcmp(uri, "/ws", 3) == 0;
  if (strcmp(tmpl, "/ws") == 0)
    return is_ws;
  return !is_ws;
}

esp_err_t WledHttpHandler::ws_on_connect_(httpd_req_t *req) {
  // Handshake done: register the client; the main loop sends the state.
  auto *self = static_cast<WledHttpHandler *>(req->user_ctx);
  int fd = httpd_req_to_sockfd(req);
  {
    std::lock_guard<std::mutex> lock(self->ws_mutex_);
    if (std::find(self->ws_fds_.begin(), self->ws_fds_.end(), fd) == self->ws_fds_.end()) {
      if (self->ws_fds_.size() >= WS_MAX_CLIENTS)
        self->ws_fds_.erase(self->ws_fds_.begin());  // oldest client loses its slot
      self->ws_fds_.push_back(fd);
    }
    self->ws_new_client_ = true;
    ESP_LOGD(TAG, "ws: client %d connected (%u)", fd, (unsigned) self->ws_fds_.size());
  }
  return ESP_OK;
}

esp_err_t WledHttpHandler::ws_handler_(httpd_req_t *req) {
  auto *self = static_cast<WledHttpHandler *>(req->user_ctx);
  httpd_ws_frame_t frame{};
  frame.type = HTTPD_WS_TYPE_TEXT;
  esp_err_t err = httpd_ws_recv_frame(req, &frame, 0);  // length only
  if (err != ESP_OK)
    return err;
  if (frame.len == 0 || frame.len > WS_MAX_FRAME)
    return ESP_OK;
  std::string buf(frame.len, '\0');
  frame.payload = reinterpret_cast<uint8_t *>(&buf[0]);
  err = httpd_ws_recv_frame(req, &frame, frame.len);
  if (err != ESP_OK)
    return err;
  if (frame.type == HTTPD_WS_TYPE_TEXT) {
    std::lock_guard<std::mutex> lock(self->ws_mutex_);
    if (self->ws_rx_.size() < 8)
      self->ws_rx_.push_back(std::move(buf));
  }
  return ESP_OK;
}

namespace {
struct WsSendJob {
  WledHttpHandler *self;
  std::string text;
};
}  // namespace

void WledHttpHandler::ws_broadcast_(std::string text) {
  if (this->ws_hd_ == nullptr)
    return;
  auto *job = new WsSendJob{this, std::move(text)};  // NOLINT
  // Socket writes belong to the httpd task.
  esp_err_t err = httpd_queue_work(
      this->ws_hd_,
      [](void *arg) {
        auto *job = static_cast<WsSendJob *>(arg);
        auto *self = job->self;
        std::vector<int> fds;
        {
          std::lock_guard<std::mutex> lock(self->ws_mutex_);
          fds = self->ws_fds_;
        }
        httpd_ws_frame_t frame{};
        frame.type = HTTPD_WS_TYPE_TEXT;
        frame.final = true;
        frame.payload = reinterpret_cast<uint8_t *>(&job->text[0]);
        frame.len = job->text.size();
        std::vector<int> gone;
        for (int fd : fds) {
          if (httpd_ws_get_fd_info(self->ws_hd_, fd) != HTTPD_WS_CLIENT_WEBSOCKET ||
              httpd_ws_send_frame_async(self->ws_hd_, fd, &frame) != ESP_OK)
            gone.push_back(fd);
        }
        if (!gone.empty()) {
          std::lock_guard<std::mutex> lock(self->ws_mutex_);
          for (int fd : gone) {
            self->ws_fds_.erase(std::remove(self->ws_fds_.begin(), self->ws_fds_.end(), fd), self->ws_fds_.end());
            ESP_LOGD(TAG, "ws: client %d gone", fd);
          }
        }
        delete job;  // NOLINT
      },
      job);
  if (err != ESP_OK)
    delete job;  // NOLINT
}

size_t WledHttpHandler::ws_client_count() {
  std::lock_guard<std::mutex> lock(this->ws_mutex_);
  return this->ws_fds_.size();
}

void WledHttpHandler::ws_loop() {
  // Attach to the running server (again after a restart of the listener).
  auto *srv = web_server_base::global_web_server_base->get_server();
  httpd_handle_t hd = srv != nullptr ? srv->get_server() : nullptr;
  if (hd == nullptr)
    return;
  if (hd != this->ws_hd_) {
    // httpd_data starts with its httpd_config_t: install our matcher.
    reinterpret_cast<httpd_config_t *>(hd)->uri_match_fn = WledHttpHandler::uri_match_;
    httpd_uri_t ws_uri{};
    ws_uri.uri = "/ws";
    ws_uri.method = HTTP_GET;
    ws_uri.handler = WledHttpHandler::ws_handler_;
    ws_uri.user_ctx = this;
    ws_uri.is_websocket = true;
    ws_uri.ws_post_handshake_cb = WledHttpHandler::ws_on_connect_;
    esp_err_t err = httpd_register_uri_handler(hd, &ws_uri);
    ESP_LOGD(TAG, "ws: /ws handler %s", err == ESP_OK ? "registered" : esp_err_to_name(err));
    std::lock_guard<std::mutex> lock(this->ws_mutex_);
    this->ws_fds_.clear();
    this->ws_hd_ = hd;
  }

  // Commands from the clients (same JSON as POST /json/state).
  std::vector<std::string> rx;
  bool new_client;
  {
    std::lock_guard<std::mutex> lock(this->ws_mutex_);
    rx.swap(this->ws_rx_);
    new_client = this->ws_new_client_;
    this->ws_new_client_ = false;
    if (this->ws_fds_.empty())
      return;
  }
  for (auto &msg : rx) {
    ESP_LOGD(TAG, "ws: rx %s", msg.c_str());
    this->apply_state_(msg.c_str());
  }

  uint32_t now = millis();
  if (!new_client && rx.empty() && now - this->ws_last_check_ < WS_CHECK_MS)
    return;
  this->ws_last_check_ = now;
  std::string state = this->state_json_();
  if (!new_client && rx.empty() && state == this->ws_last_state_ && now - this->ws_last_push_ < WS_REFRESH_MS)
    return;
  this->ws_last_state_ = state;
  this->ws_last_push_ = now;
  this->ws_broadcast_(this->full_json_());
}

#endif  // USE_ESP32

}  // namespace wled_emu
}  // namespace esphome

#endif
