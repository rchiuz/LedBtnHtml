/*
===============================================================================
  ESP32 - 4 LEDs + 4 Buttons (pull-up) - Web Control (All-in-One)
  - GPIOs:
      LED1=17, LED2=5, LED3=18, LED4=19 (OUTPUT)
      BTN1=13, BTN2=12, BTN3=14, BTN4=27 (INPUT, PULL-UP)
  - Wi-Fi SoftAP: SSID=ESP32_LAB  PASS=12345678
  - HTTP endpoints:
      GET  /           -> UI (HTML+CSS+JS embebido)
      GET  /state      -> JSON con {leds[], btns[]}
      GET  /set?led=N&state=0|1  -> setea LEDN
      GET  /all?state=0|1        -> apaga/enciende todos
===============================================================================
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "driver/gpio.h"

/* === DEFINICIONES DE PINES === */
#define LED1    GPIO_NUM_17
#define LED2    GPIO_NUM_5
#define LED3    GPIO_NUM_18
#define LED4    GPIO_NUM_19

#define BTN1    GPIO_NUM_13
#define BTN2    GPIO_NUM_12
#define BTN3    GPIO_NUM_14
#define BTN4    GPIO_NUM_27

/* === Wi-Fi SoftAP fijo (plantilla permanente) === */
#define AP_SSID         "ESP32_LAB"
#define AP_PASS         "12345678"   // >=8 chars (vacío -> OPEN)
#define AP_CHANNEL      1
#define AP_MAX_CONN     4

static const char *TAG = "web_io";

/* === Estado global (atómico) para 4 LEDs === */
static _Atomic int g_led[4] = {0,0,0,0};  // LED1..LED4 (0=OFF,1=ON)

/* === Utilidad: mapea índice [1..4] a GPIO === */
static gpio_num_t led_idx_to_gpio(int idx) {
    switch (idx) {
        case 1: return LED1;
        case 2: return LED2;
        case 3: return LED3;
        case 4: return LED4;
        default: return -1;
    }
}
static gpio_num_t btn_idx_to_gpio(int idx) {
    switch (idx) {
        case 1: return BTN1;
        case 2: return BTN2;
        case 3: return BTN3;
        case 4: return BTN4;
        default: return -1;
    }
}

/* === HTML + CSS + JS embebidos (todo en uno) === */
static const char index_html[] =
"<!doctype html><html><head>"
"<meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>ESP32 I/O Control</title>"
"<style>"
"body{font-family:system-ui,Arial;margin:20px;background:#f5f7fb}"
".wrap{max-width:620px;margin:0 auto}"
"h2{color:#223;padding:6px 0;margin:0 0 10px}"
".card{background:#fff;border-radius:12px;box-shadow:0 6px 16px rgba(0,0,0,.08);padding:16px}"
".grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}"
".section h3{margin:0 0 8px;color:#334}"
".row{display:flex;align-items:center;justify-content:space-between;padding:8px 0;border-bottom:1px solid #eef}"
".row:last-child{border-bottom:none}"
".led{font-weight:600}"
"button{padding:8px 12px;border:1px solid #ccd;border-radius:8px;background:#eef;cursor:pointer}"
"button:hover{background:#e5eefc}"
".pill{display:inline-block;padding:4px 10px;border-radius:999px;font-size:.9rem}"
".on{background:#d7f5db;color:#075}"
".off{background:#ffe2e2;color:#922}"
"small{color:#667}"
".toolbar{display:flex;gap:10px;justify-content:flex-end;margin-top:10px}"
"input[type=checkbox]{transform:scale(1.2)}"
"</style>"
"</head><body><div class='wrap'>"
"<h2>ESP32 I/O Control</h2>"
"<div class='card section'>"
"<h3>LEDs</h3>"
"<div class='grid'>"
"  <div class='row'><span class='led'>LED1</span><span><button onclick='setLed(1,1)'>ON</button> <button onclick='setLed(1,0)'>OFF</button> <span id='l1' class='pill off'>OFF</span></span></div>"
"  <div class='row'><span class='led'>LED2</span><span><button onclick='setLed(2,1)'>ON</button> <button onclick='setLed(2,0)'>OFF</button> <span id='l2' class='pill off'>OFF</span></span></div>"
"  <div class='row'><span class='led'>LED3</span><span><button onclick='setLed(3,1)'>ON</button> <button onclick='setLed(3,0)'>OFF</button> <span id='l3' class='pill off'>OFF</span></span></div>"
"  <div class='row'><span class='led'>LED4</span><span><button onclick='setLed(4,1)'>ON</button> <button onclick='setLed(4,0)'>OFF</button> <span id='l4' class='pill off'>OFF</span></span></div>"
"</div>"
"<div class='toolbar'>"
"  <button onclick='allSet(1)'>ALL ON</button>"
"  <button onclick='allSet(0)'>ALL OFF</button>"
"</div>"
"</div>"

"<div class='card section' style='margin-top:14px'>"
"<h3>Buttons <small>(pull-up, activo en LOW)</small></h3>"
"<div class='grid'>"
"  <div class='row'><span>BTN1</span><span id='b1' class='pill off'>HIGH</span></div>"
"  <div class='row'><span>BTN2</span><span id='b2' class='pill off'>HIGH</span></div>"
"  <div class='row'><span>BTN3</span><span id='b3' class='pill off'>HIGH</span></div>"
"  <div class='row'><span>BTN4</span><span id='b4' class='pill off'>HIGH</span></div>"
"</div>"
"</div>"

"<p style='text-align:right;color:#667'><small>Auto-refresh cada 1.2s</small></p>"
"</div>"

"<script>"
"function cls(el,on){el.classList.toggle('on',on);el.classList.toggle('off',!on)}"
"function setLed(n,st){fetch(`/set?led=${n}&state=${st}`).then(r=>r.json()).then(update)}"
"function allSet(st){fetch(`/all?state=${st}`).then(r=>r.json()).then(update)}"
"function update(s){"
"  const L=[,'l1','l2','l3','l4'];"
"  for(let i=1;i<=4;i++){const el=document.getElementById(L[i]);const on=!!s.leds[i-1];"
"    cls(el,on);el.textContent=on?'ON':'OFF';}"
"  const B=[,'b1','b2','b3','b4'];"
"  for(let i=1;i<=4;i++){const el=document.getElementById(B[i]);const low=(s.btns[i-1]===0);"
"    cls(el,low);el.textContent=low?'LOW(pressed)':'HIGH';}"
"}"
"function poll(){fetch('/state').then(r=>r.json()).then(update)}"
"poll(); setInterval(poll,1200);"
"</script>"
"</body></html>";

/* === Inicialización de GPIOs: LEDs salida, BTN entrada con pull-up === */
static void io_init(void)
{
    // LEDs como salida
    gpio_config_t io_led = {
        .pin_bit_mask = (1ULL<<LED1) | (1ULL<<LED2) | (1ULL<<LED3) | (1ULL<<LED4),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_led);
    gpio_set_level(LED1, 0);
    gpio_set_level(LED2, 0);
    gpio_set_level(LED3, 0);
    gpio_set_level(LED4, 0);

    // Botones como entrada con pull-up
    gpio_config_t io_btn = {
        .pin_bit_mask = (1ULL<<BTN1) | (1ULL<<BTN2) | (1ULL<<BTN3) | (1ULL<<BTN4),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,          // <-- pull-up habilitado
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_btn);
}

/* === Wi-Fi SoftAP permanente === */
static void wifi_init_softap(void)
{
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t ap = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = sizeof(AP_SSID)-1,
            .channel = AP_CHANNEL,
            .password = AP_PASS,
            .max_connection = AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };
    if (strlen(AP_PASS) == 0) ap.ap.authmode = WIFI_AUTH_OPEN;

    // (Opcional) Cambiar IP del AP:
    // esp_netif_t *netif = esp_netif_create_default_wifi_ap();
    // esp_netif_ip_info_t ip;
    // IP4_ADDR(&ip.ip, 192,168,10,1); IP4_ADDR(&ip.gw, 192,168,10,1); IP4_ADDR(&ip.netmask,255,255,255,0);
    // esp_netif_dhcps_stop(netif); esp_netif_set_ip_info(netif,&ip); esp_netif_dhcps_start(netif);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "SoftAP listo — SSID:%s PASS:%s  URL: http://192.168.4.1/", AP_SSID, AP_PASS);
}

/* === Utilidad: lee botones (retorna 1=HIGH, 0=LOW) === */
static void read_buttons(int btns[4])
{
    btns[0] = gpio_get_level(BTN1);
    btns[1] = gpio_get_level(BTN2);
    btns[2] = gpio_get_level(BTN3);
    btns[3] = gpio_get_level(BTN4);
}

/* === Respuesta JSON /state === */
static void send_state(httpd_req_t *req)
{
    int btns[4];
    read_buttons(btns);

    // Actualiza niveles físicos de LEDs según g_led (por si algo desfasó)
    for (int i=0;i<4;i++){
        gpio_set_level(led_idx_to_gpio(i+1), atomic_load(&g_led[i]) ? 1 : 0);
    }

    char buf[160];
    int n = snprintf(buf, sizeof(buf),
        "{\"leds\":[%d,%d,%d,%d],\"btns\":[%d,%d,%d,%d]}",
        atomic_load(&g_led[0]), atomic_load(&g_led[1]),
        atomic_load(&g_led[2]), atomic_load(&g_led[3]),
        btns[0], btns[1], btns[2], btns[3]);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, n);
}

/* === Handlers HTTP === */
static esp_err_t root_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html, strlen(index_html));
    return ESP_OK;
}

static esp_err_t state_get(httpd_req_t *req)
{
    send_state(req);
    return ESP_OK;
}

// /set?led=N&state=0|1
static esp_err_t set_get(httpd_req_t *req)
{
    char q[64];
    int ledn = 0, st = -1;

    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK) {
        char v[16];
        if (httpd_query_key_value(q, "led", v, sizeof(v)) == ESP_OK)  ledn = atoi(v);
        if (httpd_query_key_value(q, "state", v, sizeof(v)) == ESP_OK) st = atoi(v);
    }

    if (ledn >= 1 && ledn <= 4 && (st == 0 || st == 1)) {
        atomic_store(&g_led[ledn-1], st);
        gpio_set_level(led_idx_to_gpio(ledn), st);
    }

    send_state(req);
    return ESP_OK;
}

// /all?state=0|1
static esp_err_t all_get(httpd_req_t *req)
{
    char q[32], v[8];
    int st = -1;
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK) {
        if (httpd_query_key_value(q, "state", v, sizeof(v)) == ESP_OK) st = atoi(v);
    }
    if (st==0 || st==1) {
        for (int i=0;i<4;i++){
            atomic_store(&g_led[i], st);
            gpio_set_level(led_idx_to_gpio(i+1), st);
        }
    }
    send_state(req);
    return ESP_OK;
}

/* === Inicio del servidor HTTP === */
static httpd_handle_t start_server(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t s = NULL;

    if (httpd_start(&s, &cfg) == ESP_OK) {
        httpd_uri_t root =  { .uri="/",      .method=HTTP_GET, .handler=root_get };
        httpd_uri_t st   =  { .uri="/state", .method=HTTP_GET, .handler=state_get };
        httpd_uri_t set  =  { .uri="/set",   .method=HTTP_GET, .handler=set_get };
        httpd_uri_t all  =  { .uri="/all",   .method=HTTP_GET, .handler=all_get };

        httpd_register_uri_handler(s, &root);
        httpd_register_uri_handler(s, &st);
        httpd_register_uri_handler(s, &set);
        httpd_register_uri_handler(s, &all);
    }
    return s;
}

/* === app_main === */
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());

    io_init();           // LEDs como salida, botones input pull-up
    wifi_init_softap();  // AP listo (192.168.4.1)
    start_server();      // HTTP arriba

    ESP_LOGI(TAG, "Conéctate a %s y abre http://192.168.4.1/", AP_SSID);
}

