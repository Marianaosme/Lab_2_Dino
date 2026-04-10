/*
 * Dino Game - laboratorio 2
 * 
 * Colores:
 *   Verde   -> Dino
 *   Rojo    -> Obstaculo
 * 
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

static void mostrar_frame(void);

//  PINES 
static const int ROW_PINS[8]  = {13, 14, 27, 26, 25, 33, 32, 12};
static const int COLR_PINS[8] = {23, 22, 21, 19, 18,  5, 17, 16};
static const int COLG_PINS[8] = {-1,  2, 15,  0,  1,  3,  4, -1};

#define BTN_JUMP 34

//  FRAMEBUFFER 
static uint8_t fb_red[8];
static uint8_t fb_green[8];

//  DINO 
static int dino_x = 1;
static int dino_y = 6;

static int jump_state = 0;
static int jump_count = 0;

static int obs_x = 7;

//  GPIO INIT 
static void gpio_init_all(void)
{
    gpio_config_t cfg;
    cfg.intr_type    = GPIO_INTR_DISABLE;
    cfg.mode         = GPIO_MODE_OUTPUT;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.pull_up_en   = GPIO_PULLUP_DISABLE;

    // Filas (apagadas = HIGH)
    for (int i = 0; i < 8; i++) {
        cfg.pin_bit_mask = (1ULL << ROW_PINS[i]);
        gpio_config(&cfg);
        gpio_set_level(ROW_PINS[i], 1);
    }

    // Columnas rojas
    for (int i = 0; i < 8; i++) {
        cfg.pin_bit_mask = (1ULL << COLR_PINS[i]);
        gpio_config(&cfg);
        gpio_set_level(COLR_PINS[i], 1);
    }

    // Columnas verdes
    for (int i = 0; i < 8; i++) {
        if (COLG_PINS[i] == -1) continue;
        cfg.pin_bit_mask = (1ULL << COLG_PINS[i]);
        gpio_config(&cfg);
        gpio_set_level(COLG_PINS[i], 1);
    }

    // Botón
    gpio_config_t btn_cfg;
    btn_cfg.intr_type    = GPIO_INTR_DISABLE;
    btn_cfg.mode         = GPIO_MODE_INPUT;
    btn_cfg.pull_up_en   = GPIO_PULLUP_DISABLE;
    btn_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;

    btn_cfg.pin_bit_mask = (1ULL << BTN_JUMP);
    gpio_config(&btn_cfg);
}

//  FRAMEBUFFER 
static void fb_clear(void){
    memset(fb_red,   0, sizeof(fb_red));
    memset(fb_green, 0, sizeof(fb_green));
}

static void fb_set_red(int col, int row){
    if (col < 0 || col > 7 || row < 0 || row > 7) return;
    fb_red[row] |= (uint8_t)(1u << (7 - col));
}

static void fb_set_green(int col, int row){
    if (col < 0 || col > 7 || row < 0 || row > 7) return;
    fb_green[row] |= (uint8_t)(1u << (7 - col));
}

// muerte del dino
static void animacion_muerte(void)
{
    // Encender TODO en rojo
    memset(fb_red, 0xFF, sizeof(fb_red));
    memset(fb_green, 0x00, sizeof(fb_green));

    // Mostrar varios frames para que se vea
    for (int i = 0; i < 40; i++) {
        mostrar_frame();
    }
}

//  DISPLAY 
static void mostrar_frame(void){
    for (int fila = 0; fila < 8; fila++){

        // Apagar todo
        for (int i = 0; i < 8; i++) {
            gpio_set_level(ROW_PINS[i],  1);
            gpio_set_level(COLR_PINS[i], 1);
            if (COLG_PINS[i] != -1) {
                gpio_set_level(COLG_PINS[i], 1);
            }
        }

        // Cargar columnas
        for (int col = 0; col < 8; col++) {
            int bit_r = (fb_red[fila]   >> (7 - col)) & 1;
            int bit_g = (fb_green[fila] >> (7 - col)) & 1;

            gpio_set_level(COLR_PINS[col], bit_r ? 0 : 1);

            if (COLG_PINS[col] != -1) {
                gpio_set_level(COLG_PINS[col], bit_g ? 0 : 1);
            }
        }

        // Activar fila (LOW = activa)
        gpio_set_level(ROW_PINS[fila], 0);

        esp_rom_delay_us(2000);
    }
}

//  INPUT 
static int btn_salto(void){
    static int last = 1;
    int cur = gpio_get_level(BTN_JUMP);

    if (last == 1 && cur == 0){
        last = 0;
        return 1;
    }
    if (cur == 1) last = 1;

    return 0;
}

//  SALTO 
static void actualizar_salto(void){
    if (jump_state == 1){
        dino_y--;
        jump_count++;

        if (jump_count >= 3){
            jump_state = 2;
        }
    }
    else if (jump_state == 2){
        dino_y++;

        if (dino_y >= 6){
            dino_y = 6;
            jump_state = 0;
        }
    }
}

//  COLISION 
static int hay_colision(void){
    if (obs_x == dino_x){
        if (dino_y >= 5) return 1;
    }
    return 0;
}

//  LOGICA 
static void actualizar_juego(void){

    if (btn_salto() && jump_state == 0){
        jump_state = 1;
        jump_count = 0;
    }

    actualizar_salto();

    obs_x--;

    if (obs_x < 0){
        obs_x = 7;
    }

    if (hay_colision()){

    animacion_muerte(); 

    // Reinicio
    dino_y = 6;
    obs_x = 7;
    jump_state = 0;
}
}

//  RENDER 
static void construir_frame(void){

    fb_clear();

    // suelo
    for (int i = 0; i < 8; i++){
        fb_set_red(i, 7);
    }

    // obstáculo
    if (obs_x >= 0 && obs_x < 8){
        fb_set_red(obs_x, 7);
        fb_set_red(obs_x, 6);
    }

    // jugador
    fb_set_green(dino_x, dino_y);
}

//  MAIN 
void app_main(void)
{
    gpio_init_all();

    int64_t last_update = 0;
    const int GAME_SPEED_MS = 120; // ajusta esto

    while (1)
    {
        int64_t now = esp_timer_get_time() / 1000; // ms

        // actualizar juego cada cierto tiempo
        if (now - last_update > GAME_SPEED_MS) {
            actualizar_juego();
            last_update = now;
        }

        // dibujar SIEMPRE
        construir_frame();

        // refresco constante
        mostrar_frame();
    }
}