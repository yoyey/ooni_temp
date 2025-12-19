#pragma once

/* Configuration minimale LVGL 9.x pour ESP32 + RGB565 */

/* Couleur en 16-bit (RGB565) */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/* Désactive les logs pour gagner de la place */
#define LV_USE_LOG 0

/* Widgets utilisés */
#define LV_USE_LABEL 1
#define LV_USE_BUTTON 1
#define LV_USE_LINE 1
#define LV_USE_SCALE 1

/* Thème par défaut (optionnel) */
#define LV_USE_THEME_DEFAULT 1

/* Polices minimales */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* ===== Mémoire : IMPORTANT =====
   On évite un gros buffer statique en BSS (.dram0.bss) en demandant à LVGL
   d'utiliser malloc/free (CLIB) au lieu du pool builtin.
*/
#define LV_USE_STDLIB_MALLOC  LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING  LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
