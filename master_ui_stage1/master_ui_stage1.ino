/*******************************************************************************
 * Speed ​​Slalom Timer - Master UI Stage 1 (Official Port)
 * Board     : VIEWE UEDX80480070E-WB-A
 * Library   : ESP32_Display_Panel v1.0.4
 * LVGL      : v8.4.x
 * Date      : 2026-09-22
 ******************************************************************************/

#include <Arduino.h>
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include "lvgl_v8_port.h"  // 官方 port header（simple_port 入面有）

// 中文字體
LV_FONT_DECLARE(cn_ui_16);
LV_FONT_DECLARE(cn_ui_32);

using namespace esp_panel::board;
using namespace esp_panel::drivers;

/* ======================== 賽道資料 ======================== */
struct LaneData {
  bool     running   = false;
  uint32_t start_ms  = 0;
  float    pure_time = 0.0f;   // 衝線時間
  float    split_sf  = 0.0f;
  float    split_fe  = 0.0f;
  bool     has_s     = false;
  bool     has_f     = false;
  bool     has_e     = false;
  uint8_t  penalty   = 0;
  char     status[32]= "準備中";
};

LaneData laneA;
LaneData laneB;

LaneData* get_lane(char id) {
  return (id == 'B') ? &laneB : &laneA;
}

/* ======================== 全域 ======================== */
Board *board = nullptr;

lv_obj_t *scr_home = nullptr;
lv_obj_t *scr_event = nullptr;
lv_obj_t *scr_timer = nullptr;
lv_obj_t *scr_penalty = nullptr;
lv_obj_t *scr_history = nullptr;
lv_obj_t *scr_settings = nullptr;
lv_obj_t *scr_help = nullptr;
lv_obj_t *btn_beep = nullptr;

// 計時畫面物件指標（方便之後更新）
// lv_obj_t *label_main_time = nullptr;
// lv_obj_t *label_split_sf = nullptr;
// lv_obj_t *label_split_fe = nullptr;
// lv_obj_t *label_split_se = nullptr;
// lv_obj_t *label_status = nullptr;
lv_obj_t *label_mode_info = nullptr;
lv_obj_t *label_bib = nullptr;

// 罰分畫面用變數
lv_obj_t *label_penalty_count = nullptr;
lv_obj_t *label_penalty_display = nullptr;  // 計時畫面顯示罰分用
lv_obj_t *label_total_time = nullptr;
lv_obj_t *label_pure_time = nullptr;
int current_penalty = 0;

// 賽道 A 顯示
lv_obj_t *label_time_A     = nullptr;
lv_obj_t *label_total_A    = nullptr;
lv_obj_t *label_status_A   = nullptr;

// 賽道 B 顯示（雙賽道時用）
lv_obj_t *label_time_B     = nullptr;
lv_obj_t *label_total_B    = nullptr;
lv_obj_t *label_status_B   = nullptr;

// 容器（方便顯示/隱藏）
lv_obj_t *box_lane_A       = nullptr;
lv_obj_t *box_lane_B       = nullptr;

enum class TimerMode {
  COMP_PRELIM = 0,
  TRAIN_PRELIM,
  COMP_FINAL,
  TRAIN_FINAL
};

TimerMode selectedMode = TimerMode::COMP_PRELIM;
bool dualLane = false;

const char *get_mode_name(int mode) {
  switch (mode) {
    case 0: return "比賽-預賽";   // COMP_PRELIM
    case 1: return "練習-預賽";   // TRAIN_PRELIM
    case 2: return "比賽-決賽";   // COMP_FINAL
    case 3: return "練習-決賽";   // TRAIN_FINAL
    default: return "未知";
  }
}

/* ======================== 計時畫面用變數 ======================== */

/* ======================== 前向宣告 ======================== */
void create_home_screen();
void create_placeholder_screen(lv_obj_t **scr, const char *title);
void switch_to_screen(lv_obj_t *target);
void refresh_timer_mode_info();
void create_penalty_screen();
void update_penalty_display();
void update_timer_buttons_visibility();

// 更新主跑錶顯示（之後由計時引擎呼叫）
// void update_timer_display() {
//   if (!label_main_time) return;

//   char buf[32];

//   // 主時間（大字體）
//   if (laneA.running) {
//     float elapsed = (millis() - laneA.start_ms) / 1000.0f; 
//     snprintf(buf, sizeof(buf), "%.3f", elapsed);
//   } else {
//     snprintf(buf, sizeof(buf), "%.3f", laneA.pure_time);
//   }
//   lv_label_set_text(label_main_time, buf);

//   // 分段
//   if (label_split_sf) {
//     if (laneA.has_s && laneA.has_f)
//       snprintf(buf, sizeof(buf), "S->F: %.3f s", laneA.split_sf);
//     else
//       snprintf(buf, sizeof(buf), "S->F: ---");
//     lv_label_set_text(label_split_sf, buf);
//   }

//   if (label_split_fe) {
//     if (laneA.has_f && laneA.has_e)
//       snprintf(buf, sizeof(buf), "F->E: %.3f s", laneA.split_fe);
//     else
//       snprintf(buf, sizeof(buf), "F->E: ---");
//     lv_label_set_text(label_split_fe, buf);
//   }

//   if (label_split_se) {
//     if (laneA.has_e)
//       snprintf(buf, sizeof(buf), "總時間: %.3f s", laneA.split_se);
//     else
//       snprintf(buf, sizeof(buf), "總時間: ---");
//     lv_label_set_text(label_split_se, buf);
//   }

//   // ★ 罰分顯示
//   if (label_penalty_display) {
//     float penalty_sec = laneA.penalty * 0.2f;
//     snprintf(buf, sizeof(buf), "罰分: %d (+%.2fs)", laneA.penalty, penalty_sec);
//     lv_label_set_text(label_penalty_display, buf);
//   }

//   if (label_status) {
//     lv_label_set_text(label_status, laneA.status);
//   }
// }

void update_penalty_display() {
  if (!label_penalty_count) return;

  char buf[64];
  snprintf(buf, sizeof(buf), "%d", current_penalty);
  lv_label_set_text(label_penalty_count, buf);

  float penalty_sec = current_penalty * 0.2f;
  float total = laneA.pure_time + penalty_sec;

  if (label_pure_time) {
    snprintf(buf, sizeof(buf), "純時間: %.3f s", laneA.pure_time);
    lv_label_set_text(label_pure_time, buf);
  }
  if (label_total_time) {
    snprintf(buf, sizeof(buf), "總成績: %.3f s", total);
    lv_label_set_text(label_total_time, buf);
  }
}

void reset_lane(LaneData &lane) {
  lane.running = false;
  lane.start_ms = 0;
  lane.pure_time = 0;
  lane.split_sf = 0;
  lane.split_fe = 0;
  lane.has_s = lane.has_f = lane.has_e = false;
  lane.penalty = 0;
  strcpy(lane.status, "準備中");
}

void reset_all_lanes() {
  reset_lane(laneA);
  reset_lane(laneB);
}

/* ======================== 回調 ======================== */
static void mode_btn_event_cb(lv_event_t *e) {
  TimerMode mode = (TimerMode)(intptr_t)lv_event_get_user_data(e);
  selectedMode = mode;
  Serial.printf("[UI] Selected mode: %d -> go to Timer\n", (int)mode);

  refresh_timer_mode_info();
  update_timer_buttons_visibility();  // ★ 新增

  if (scr_timer) {
    switch_to_screen(scr_timer);
  }
}

static void lane_btn_event_cb(lv_event_t *e) {
  dualLane = (bool)(intptr_t)lv_event_get_user_data(e);
  Serial.printf("[UI] Dual lane: %s\n", dualLane ? "YES" : "NO");
}

static void start_btn_event_cb(lv_event_t *e) {
  Serial.printf("[UI] Start button -> Timer  Mode=%d  Dual=%d\n",
                (int)selectedMode, dualLane);
  refresh_timer_mode_info();
  update_timer_buttons_visibility();  // ★ 新增
  if (scr_timer) switch_to_screen(scr_timer);
}

static void nav_btn_event_cb(lv_event_t *e) {
  lv_obj_t *target = (lv_obj_t *)lv_event_get_user_data(e);
  if (target) switch_to_screen(target);
}

void switch_to_screen(lv_obj_t *target) {
  if (target) {
    lv_scr_load_anim(target, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
  }
}

void refresh_timer_mode_info() {
  if (!label_mode_info) return;

  char buf[64];
  snprintf(buf, sizeof(buf), "模式: %s | %s",
           get_mode_name((int)selectedMode),
           dualLane ? "雙賽道" : "單賽道");
  lv_label_set_text(label_mode_info, buf);
}

void update_timer_buttons_visibility() {
  // Beep 只決賽顯示
  if (btn_beep) {
    if (selectedMode == TimerMode::COMP_FINAL || selectedMode == TimerMode::TRAIN_FINAL) {
      lv_obj_clear_flag(btn_beep, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(btn_beep, LV_OBJ_FLAG_HIDDEN);
    }
  }

  // 單/雙賽道顯示
  if (box_lane_B) {
    if (dualLane) {
      lv_obj_clear_flag(box_lane_B, LV_OBJ_FLAG_HIDDEN);
      // 單賽道時 A 置中可之後再優化
    } else {
      lv_obj_add_flag(box_lane_B, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void update_lane_display(char id) {
  LaneData *lane = get_lane(id);
  lv_obj_t *lbl_time   = (id == 'A') ? label_time_A   : label_time_B;
  lv_obj_t *lbl_total  = (id == 'A') ? label_total_A  : label_total_B;
  lv_obj_t *lbl_status = (id == 'A') ? label_status_A : label_status_B;

  if (!lbl_time) return;

  char buf[32];

  // 即時 / 衝線時間
  if (lane->running) {
    float elapsed = (millis() - lane->start_ms) / 1000.0f;
    snprintf(buf, sizeof(buf), "%.3f", elapsed);
  } else {
    snprintf(buf, sizeof(buf), "%.3f", lane->pure_time);
  }
  lv_label_set_text(lbl_time, buf);

  // 總時間 = 衝線 + 罰分×0.2
  float total = lane->pure_time + lane->penalty * 0.2f;
  snprintf(buf, sizeof(buf), "總: %.3f", total);
  if (lbl_total) lv_label_set_text(lbl_total, buf);

  if (lbl_status) lv_label_set_text(lbl_status, lane->status);
}

void update_timer_display() {
  update_lane_display('A');
  if (dualLane) {
    update_lane_display('B');
  }

  // 更新頂部模式資訊
  refresh_timer_mode_info();
}

////////////////////
// Create screen ///
////////////////////

/* ======================== 主頁 ======================== */
void create_home_screen() {
  scr_home = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_home, lv_color_hex(0x1a1a2e), 0);

  // 標題
  lv_obj_t *title = lv_label_create(scr_home);
  lv_label_set_text(title, "速度過樁計時系統");
  lv_obj_set_style_text_font(title, &cn_ui_32, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0xeeeeee), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

  lv_obj_t *subtitle = lv_label_create(scr_home);
  lv_label_set_text(subtitle, "Speed Slalom Timer  v0.1");
  lv_obj_set_style_text_color(subtitle, lv_color_hex(0xaaaaaa), 0);
  lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 55);

  // 模式選擇
  lv_obj_t *mode_label = lv_label_create(scr_home);
  lv_obj_set_style_text_font(mode_label, &cn_ui_16, 0);
  lv_label_set_text(mode_label, "選擇模式");
  lv_obj_set_style_text_color(mode_label, lv_color_hex(0x00d4ff), 0);
  lv_obj_align(mode_label, LV_ALIGN_TOP_LEFT, 30, 100);

  // 注意：唔好用 ①②③④，字體冇呢啲字元
  const char *mode_names[4] = {
    "1. 比賽-預賽",
    "2. 練習-預賽",
    "3. 比賽-決賽",
    "4. 練習-決賽"
  };

  for (int i = 0; i < 4; i++) {
    lv_obj_t *btn = lv_btn_create(scr_home);
    lv_obj_set_size(btn, 340, 70);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 30, 140 + i * 85);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_radius(btn, 12, 0);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, mode_names[i]);
    lv_obj_set_style_text_font(label, &cn_ui_16, 0);
    lv_obj_center(label);

    lv_obj_add_event_cb(btn, mode_btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  // 賽道設定
  lv_obj_t *lane_label = lv_label_create(scr_home);
  lv_obj_set_style_text_font(lane_label, &cn_ui_16, 0);
  lv_label_set_text(lane_label, "賽道設定");
  lv_obj_set_style_text_color(lane_label, lv_color_hex(0x00d4ff), 0);
  lv_obj_align(lane_label, LV_ALIGN_TOP_RIGHT, -180, 100);

  lv_obj_t *btn_single = lv_btn_create(scr_home);
  lv_obj_set_size(btn_single, 160, 70);
  lv_obj_align(btn_single, LV_ALIGN_TOP_RIGHT, -30, 140);
  lv_obj_set_style_bg_color(btn_single, lv_color_hex(0x0f3460), 0);
  lv_obj_t *lbl1 = lv_label_create(btn_single);
  lv_obj_set_style_text_font(lbl1, &cn_ui_16, 0);
  lv_label_set_text(lbl1, "單賽道");
  lv_obj_center(lbl1);
  lv_obj_add_event_cb(btn_single, lane_btn_event_cb, LV_EVENT_CLICKED, (void *)false);

  lv_obj_t *btn_dual = lv_btn_create(scr_home);
  lv_obj_set_size(btn_dual, 160, 70);
  lv_obj_align(btn_dual, LV_ALIGN_TOP_RIGHT, -30, 225);
  lv_obj_set_style_bg_color(btn_dual, lv_color_hex(0x0f3460), 0);
  lv_obj_t *lbl2 = lv_label_create(btn_dual);
  lv_obj_set_style_text_font(lbl2, &cn_ui_16, 0);
  lv_label_set_text(lbl2, "雙賽道");
  lv_obj_center(lbl2);
  lv_obj_add_event_cb(btn_dual, lane_btn_event_cb, LV_EVENT_CLICKED, (void *)true);

  // 底部按鈕
  lv_obj_t *btn_start = lv_btn_create(scr_home);
  lv_obj_set_size(btn_start, 200, 70);
  lv_obj_align(btn_start, LV_ALIGN_BOTTOM_MID, 0, -25);
  lv_obj_set_style_bg_color(btn_start, lv_color_hex(0xe94560), 0);
  lv_obj_set_style_radius(btn_start, 12, 0);
  lv_obj_t *lbl_start = lv_label_create(btn_start);
  lv_label_set_text(lbl_start, "開始");
  lv_obj_set_style_text_font(lbl_start, &cn_ui_16, 0);
  lv_obj_center(lbl_start);
  lv_obj_add_event_cb(btn_start, start_btn_event_cb, LV_EVENT_CLICKED, NULL);

  // 歷史按鈕
  lv_obj_t *btn_hist = lv_btn_create(scr_home);
  lv_obj_set_size(btn_hist, 140, 55);
  lv_obj_align(btn_hist, LV_ALIGN_BOTTOM_LEFT, 30, -30);
  lv_obj_set_style_bg_color(btn_hist, lv_color_hex(0x533483), 0);
  lv_obj_t *lbl_hist = lv_label_create(btn_hist);
  lv_obj_set_style_text_font(lbl_hist, &cn_ui_16, 0);
  lv_label_set_text(lbl_hist, "歷史記錄");
  lv_obj_center(lbl_hist);
  lv_obj_add_event_cb(btn_hist, nav_btn_event_cb, LV_EVENT_CLICKED, scr_history);  // ← 加呢行

  // 設定按鈕
  lv_obj_t *btn_set = lv_btn_create(scr_home);
  lv_obj_set_size(btn_set, 140, 55);
  lv_obj_align(btn_set, LV_ALIGN_BOTTOM_RIGHT, -30, -30);
  lv_obj_set_style_bg_color(btn_set, lv_color_hex(0x533483), 0);
  lv_obj_t *lbl_set = lv_label_create(btn_set);
  lv_obj_set_style_text_font(lbl_set, &cn_ui_16, 0);
  lv_label_set_text(lbl_set, "系統設定");
  lv_obj_center(lbl_set);
  lv_obj_add_event_cb(btn_set, nav_btn_event_cb, LV_EVENT_CLICKED, scr_settings);  // ← 加呢行
}

/* ======================== Timer screen ======================== */
void create_timer_screen() {
  scr_timer = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_timer, lv_color_hex(0x0f0f1a), 0);

  // ===== 頂部 =====
  label_mode_info = lv_label_create(scr_timer);
  lv_obj_set_style_text_font(label_mode_info, &cn_ui_16, 0);
  lv_obj_set_style_text_color(label_mode_info, lv_color_hex(0x00d4ff), 0);
  lv_label_set_text(label_mode_info, "模式: --- | ---");
  lv_obj_align(label_mode_info, LV_ALIGN_TOP_LEFT, 20, 10);

  label_bib = lv_label_create(scr_timer);
  lv_obj_set_style_text_font(label_bib, &cn_ui_16, 0);
  lv_obj_set_style_text_color(label_bib, lv_color_hex(0xffcc00), 0);
  lv_label_set_text(label_bib, "Bib: ---");
  lv_obj_align(label_bib, LV_ALIGN_TOP_RIGHT, -20, 10);

  // ===== 賽道 A 容器 =====
  box_lane_A = lv_obj_create(scr_timer);
  lv_obj_set_size(box_lane_A, 360, 200);
  lv_obj_align(box_lane_A, LV_ALIGN_TOP_LEFT, 20, 50);
  lv_obj_set_style_bg_color(box_lane_A, lv_color_hex(0x1a1a2e), 0);
  lv_obj_set_style_radius(box_lane_A, 10, 0);
  lv_obj_set_style_border_width(box_lane_A, 0, 0);
  lv_obj_clear_flag(box_lane_A, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title_A = lv_label_create(box_lane_A);
  lv_obj_set_style_text_font(title_A, &cn_ui_16, 0);
  lv_obj_set_style_text_color(title_A, lv_color_hex(0x00d4ff), 0);
  lv_label_set_text(title_A, "賽道 A");
  lv_obj_align(title_A, LV_ALIGN_TOP_MID, 0, 8);

  label_time_A = lv_label_create(box_lane_A);
  lv_obj_set_style_text_font(label_time_A, &cn_ui_32, 0);
  lv_obj_set_style_text_color(label_time_A, lv_color_hex(0xffffff), 0);
  lv_label_set_text(label_time_A, "0.000");
  lv_obj_align(label_time_A, LV_ALIGN_TOP_MID, 0, 45);

  label_total_A = lv_label_create(box_lane_A);
  lv_obj_set_style_text_font(label_total_A, &cn_ui_16, 0);
  lv_obj_set_style_text_color(label_total_A, lv_color_hex(0x00ff99), 0);
  lv_label_set_text(label_total_A, "總: 0.000");
  lv_obj_align(label_total_A, LV_ALIGN_TOP_MID, 0, 100);

  label_status_A = lv_label_create(box_lane_A);
  lv_obj_set_style_text_font(label_status_A, &cn_ui_16, 0);
  lv_obj_set_style_text_color(label_status_A, lv_color_hex(0xffaa00), 0);
  lv_label_set_text(label_status_A, "準備中");
  lv_obj_align(label_status_A, LV_ALIGN_BOTTOM_MID, 0, -15);

  // ===== 賽道 B 容器 =====
  box_lane_B = lv_obj_create(scr_timer);
  lv_obj_set_size(box_lane_B, 360, 200);
  lv_obj_align(box_lane_B, LV_ALIGN_TOP_RIGHT, -20, 50);
  lv_obj_set_style_bg_color(box_lane_B, lv_color_hex(0x1a1a2e), 0);
  lv_obj_set_style_radius(box_lane_B, 10, 0);
  lv_obj_set_style_border_width(box_lane_B, 0, 0);
  lv_obj_clear_flag(box_lane_B, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title_B = lv_label_create(box_lane_B);
  lv_obj_set_style_text_font(title_B, &cn_ui_16, 0);
  lv_obj_set_style_text_color(title_B, lv_color_hex(0x00d4ff), 0);
  lv_label_set_text(title_B, "賽道 B");
  lv_obj_align(title_B, LV_ALIGN_TOP_MID, 0, 8);

  label_time_B = lv_label_create(box_lane_B);
  lv_obj_set_style_text_font(label_time_B, &cn_ui_32, 0);
  lv_obj_set_style_text_color(label_time_B, lv_color_hex(0xffffff), 0);
  lv_label_set_text(label_time_B, "0.000");
  lv_obj_align(label_time_B, LV_ALIGN_TOP_MID, 0, 45);

  label_total_B = lv_label_create(box_lane_B);
  lv_obj_set_style_text_font(label_total_B, &cn_ui_16, 0);
  lv_obj_set_style_text_color(label_total_B, lv_color_hex(0x00ff99), 0);
  lv_label_set_text(label_total_B, "總: 0.000");
  lv_obj_align(label_total_B, LV_ALIGN_TOP_MID, 0, 100);

  label_status_B = lv_label_create(box_lane_B);
  lv_obj_set_style_text_font(label_status_B, &cn_ui_16, 0);
  lv_obj_set_style_text_color(label_status_B, lv_color_hex(0xffaa00), 0);
  lv_label_set_text(label_status_B, "準備中");
  lv_obj_align(label_status_B, LV_ALIGN_BOTTOM_MID, 0, -15);

  // ===== 模擬按鈕 =====
  // S
  lv_obj_t *btn_s = lv_btn_create(scr_timer);
  lv_obj_set_size(btn_s, 120, 45);
  lv_obj_align(btn_s, LV_ALIGN_TOP_LEFT, 20, 270);
  lv_obj_set_style_bg_color(btn_s, lv_color_hex(0x0f3460), 0);
  lv_obj_t *lbl_s = lv_label_create(btn_s);
  lv_obj_set_style_text_font(lbl_s, &cn_ui_16, 0);
  lv_label_set_text(lbl_s, "模擬 S");
  lv_obj_center(lbl_s);
  lv_obj_add_event_cb(btn_s, [](lv_event_t *e) {
    // 預設操作賽道 A（之後可擴充選邊條）
    if (selectedMode == TimerMode::COMP_FINAL) return;
    if (selectedMode == TimerMode::TRAIN_FINAL && !laneA.running) return;

    if (!laneA.running) {
      laneA.running = true;
      laneA.start_ms = millis();
    }
    laneA.has_s = true;
    strcpy(laneA.status, "已過S");
    Serial.println("[SIM] S → Lane A");
  }, LV_EVENT_CLICKED, NULL);

  // F
  lv_obj_t *btn_f = lv_btn_create(scr_timer);
  lv_obj_set_size(btn_f, 120, 45);
  lv_obj_align(btn_f, LV_ALIGN_TOP_LEFT, 150, 270);
  lv_obj_set_style_bg_color(btn_f, lv_color_hex(0x0f3460), 0);
  lv_obj_t *lbl_f = lv_label_create(btn_f);
  lv_obj_set_style_text_font(lbl_f, &cn_ui_16, 0);
  lv_label_set_text(lbl_f, "模擬 F");
  lv_obj_center(lbl_f);
  lv_obj_add_event_cb(btn_f, [](lv_event_t *e) {
    if (selectedMode == TimerMode::COMP_PRELIM || selectedMode == TimerMode::COMP_FINAL) return;
    if (!laneA.running || !laneA.has_s) return;
    laneA.has_f = true;
    laneA.split_sf = (millis() - laneA.start_ms) / 1000.0f;
    strcpy(laneA.status, "已過F");
    Serial.printf("[SIM] F  S→F=%.3f\n", laneA.split_sf);
  }, LV_EVENT_CLICKED, NULL);

  // E（唔再自動跳罰分）
  lv_obj_t *btn_e = lv_btn_create(scr_timer);
  lv_obj_set_size(btn_e, 120, 45);
  lv_obj_align(btn_e, LV_ALIGN_TOP_LEFT, 280, 270);
  lv_obj_set_style_bg_color(btn_e, lv_color_hex(0x0f3460), 0);
  lv_obj_t *lbl_e = lv_label_create(btn_e);
  lv_obj_set_style_text_font(lbl_e, &cn_ui_16, 0);
  lv_label_set_text(lbl_e, "模擬 E");
  lv_obj_center(lbl_e);
  lv_obj_add_event_cb(btn_e, [](lv_event_t *e) {
    if (!laneA.running && selectedMode != TimerMode::COMP_FINAL) return;

    if (selectedMode == TimerMode::COMP_FINAL && !laneA.running) {
      laneA.running = true;
      laneA.start_ms = millis();
    }

    laneA.running = false;
    laneA.has_e = true;
    laneA.pure_time = (millis() - laneA.start_ms) / 1000.0f;
    if (laneA.has_f) {
      laneA.split_fe = laneA.pure_time - laneA.split_sf;
    }
    strcpy(laneA.status, "已衝線");
    Serial.printf("[SIM] E → STOP  Pure=%.3f\n", laneA.pure_time);
    // ★ 唔再自動跳罰分畫面
  }, LV_EVENT_CLICKED, NULL);

  // Beep（只決賽顯示）
  btn_beep = lv_btn_create(scr_timer);
  lv_obj_set_size(btn_beep, 120, 45);
  lv_obj_align(btn_beep, LV_ALIGN_TOP_LEFT, 410, 270);
  lv_obj_set_style_bg_color(btn_beep, lv_color_hex(0x8b0000), 0);
  lv_obj_t *lbl_beep = lv_label_create(btn_beep);
  lv_obj_set_style_text_font(lbl_beep, &cn_ui_16, 0);
  lv_label_set_text(lbl_beep, "Beep");
  lv_obj_center(lbl_beep);
lv_obj_add_event_cb(btn_beep, [](lv_event_t *e) {
  if (selectedMode != TimerMode::COMP_FINAL && selectedMode != TimerMode::TRAIN_FINAL) return;

  // 先清除舊資料
  reset_lane(laneA);
  laneA.running = true;
  laneA.start_ms = millis();
  strcpy(laneA.status, "Beep!");

  if (dualLane) {
    reset_lane(laneB);
    laneB.running = true;
    laneB.start_ms = millis();
    strcpy(laneB.status, "Beep!");
  }
  Serial.println("[SIM] BEEP → both lanes started");
}, LV_EVENT_CLICKED, NULL);

  // ===== 底部按鈕 =====
  lv_obj_t *btn_back = lv_btn_create(scr_timer);
  lv_obj_set_size(btn_back, 120, 45);
  lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 20, -15);
  lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x533483), 0);
  lv_obj_t *lbl_back = lv_label_create(btn_back);
  lv_obj_set_style_text_font(lbl_back, &cn_ui_16, 0);
  lv_label_set_text(lbl_back, "返回");
  lv_obj_center(lbl_back);
  lv_obj_add_event_cb(btn_back, nav_btn_event_cb, LV_EVENT_CLICKED, scr_home);

  lv_obj_t *btn_reset = lv_btn_create(scr_timer);
  lv_obj_set_size(btn_reset, 120, 45);
  lv_obj_align(btn_reset, LV_ALIGN_BOTTOM_LEFT, 150, -15);
  lv_obj_set_style_bg_color(btn_reset, lv_color_hex(0x555555), 0);
  lv_obj_t *lbl_reset = lv_label_create(btn_reset);
  lv_obj_set_style_text_font(lbl_reset, &cn_ui_16, 0);
  lv_label_set_text(lbl_reset, "重置");
  lv_obj_center(lbl_reset);
  lv_obj_add_event_cb(btn_reset, [](lv_event_t *e) {
    reset_all_lanes();
    Serial.println("[SIM] Reset all");
  }, LV_EVENT_CLICKED, NULL);

  lv_obj_t *btn_pen = lv_btn_create(scr_timer);
  lv_obj_set_size(btn_pen, 120, 45);
  lv_obj_align(btn_pen, LV_ALIGN_BOTTOM_LEFT, 280, -15);
  lv_obj_set_style_bg_color(btn_pen, lv_color_hex(0xe94560), 0);
  lv_obj_t *lbl_pen = lv_label_create(btn_pen);
  lv_obj_set_style_text_font(lbl_pen, &cn_ui_16, 0);
  lv_label_set_text(lbl_pen, "罰分");
  lv_obj_center(lbl_pen);
  lv_obj_add_event_cb(btn_pen, [](lv_event_t *e) {
    update_penalty_display();
    if (scr_penalty) switch_to_screen(scr_penalty);
  }, LV_EVENT_CLICKED, NULL);

  lv_obj_t *btn_save = lv_btn_create(scr_timer);
  lv_obj_set_size(btn_save, 140, 45);
  lv_obj_align(btn_save, LV_ALIGN_BOTTOM_RIGHT, -20, -15);
  lv_obj_set_style_bg_color(btn_save, lv_color_hex(0x00aa55), 0);
  lv_obj_t *lbl_save = lv_label_create(btn_save);
  lv_obj_set_style_text_font(lbl_save, &cn_ui_16, 0);
  lv_label_set_text(lbl_save, "保存記錄");
  lv_obj_center(lbl_save);
  lv_obj_add_event_cb(btn_save, [](lv_event_t *e) {
    float totalA = laneA.pure_time + laneA.penalty * 0.2f;
    Serial.printf("[SAVE] LaneA Pure:%.3f Pen:%d Total:%.3f\n",
                  laneA.pure_time, laneA.penalty, totalA);
    if (dualLane) {
      float totalB = laneB.pure_time + laneB.penalty * 0.2f;
      Serial.printf("[SAVE] LaneB Pure:%.3f Pen:%d Total:%.3f\n",
                    laneB.pure_time, laneB.penalty, totalB);
    }
    strcpy(laneA.status, "已保存");
    if (dualLane) strcpy(laneB.status, "已保存");
  }, LV_EVENT_CLICKED, NULL);
}

void create_penalty_screen() {
  scr_penalty = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_penalty, lv_color_hex(0x0f0f1a), 0);

  // 標題
  lv_obj_t *title = lv_label_create(scr_penalty);
  lv_label_set_text(title, "罰分輸入");
  lv_obj_set_style_text_font(title, &cn_ui_32, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0xeeeeee), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

  // 純時間
  label_pure_time = lv_label_create(scr_penalty);
  lv_obj_set_style_text_font(label_pure_time, &cn_ui_16, 0);
  lv_obj_set_style_text_color(label_pure_time, lv_color_hex(0xaaaaaa), 0);
  lv_label_set_text(label_pure_time, "純時間: 0.000 s");
  lv_obj_align(label_pure_time, LV_ALIGN_TOP_MID, 0, 80);

  // 罰分數量（大字）
  lv_obj_t *pen_label = lv_label_create(scr_penalty);
  lv_obj_set_style_text_font(pen_label, &cn_ui_16, 0);
  lv_obj_set_style_text_color(pen_label, lv_color_hex(0x00d4ff), 0);
  lv_label_set_text(pen_label, "罰分點數");
  lv_obj_align(pen_label, LV_ALIGN_TOP_MID, 0, 130);

  label_penalty_count = lv_label_create(scr_penalty);
  lv_obj_set_style_text_font(label_penalty_count, &cn_ui_32, 0);
  lv_obj_set_style_text_color(label_penalty_count, lv_color_hex(0xffffff), 0);
  lv_label_set_text(label_penalty_count, "0");
  lv_obj_align(label_penalty_count, LV_ALIGN_TOP_MID, 0, 165);

  // +/- 按鈕
  lv_obj_t *btn_minus = lv_btn_create(scr_penalty);
  lv_obj_set_size(btn_minus, 100, 70);
  lv_obj_align(btn_minus, LV_ALIGN_TOP_MID, -120, 230);
  lv_obj_set_style_bg_color(btn_minus, lv_color_hex(0xcc3300), 0);
  lv_obj_t *lbl_m = lv_label_create(btn_minus);
  lv_obj_set_style_text_font(lbl_m, &cn_ui_32, 0);
  lv_label_set_text(lbl_m, "-");
  lv_obj_center(lbl_m);
  lv_obj_add_event_cb(
    btn_minus, [](lv_event_t *e) {
      if (current_penalty > 0) current_penalty--;
      update_penalty_display();
    },
    LV_EVENT_CLICKED, NULL);

  lv_obj_t *btn_plus = lv_btn_create(scr_penalty);
  lv_obj_set_size(btn_plus, 100, 70);
  lv_obj_align(btn_plus, LV_ALIGN_TOP_MID, 120, 230);
  lv_obj_set_style_bg_color(btn_plus, lv_color_hex(0x00aa55), 0);
  lv_obj_t *lbl_p = lv_label_create(btn_plus);
  lv_obj_set_style_text_font(lbl_p, &cn_ui_32, 0);
  lv_label_set_text(lbl_p, "+");
  lv_obj_center(lbl_p);
  lv_obj_add_event_cb(
    btn_plus, [](lv_event_t *e) {
      if (current_penalty < 20) current_penalty++;  // 上限保護
      update_penalty_display();
    },
    LV_EVENT_CLICKED, NULL);

  // 總成績
  label_total_time = lv_label_create(scr_penalty);
  lv_obj_set_style_text_font(label_total_time, &cn_ui_32, 0);
  lv_obj_set_style_text_color(label_total_time, lv_color_hex(0x00ff99), 0);
  lv_label_set_text(label_total_time, "總成績: 0.000 s");
  lv_obj_align(label_total_time, LV_ALIGN_TOP_MID, 0, 330);

  // 底部按鈕
  lv_obj_t *btn_back = lv_btn_create(scr_penalty);
  lv_obj_set_size(btn_back, 160, 55);
  lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 30, -20);
  lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x533483), 0);
  lv_obj_t *lbl_back = lv_label_create(btn_back);
  lv_obj_set_style_text_font(lbl_back, &cn_ui_16, 0);
  lv_label_set_text(lbl_back, "返回計時");
  lv_obj_center(lbl_back);
  lv_obj_add_event_cb(btn_back, nav_btn_event_cb, LV_EVENT_CLICKED, scr_timer);

  lv_obj_t *btn_confirm = lv_btn_create(scr_penalty);
  lv_obj_set_size(btn_confirm, 200, 55);
  lv_obj_align(btn_confirm, LV_ALIGN_BOTTOM_RIGHT, -30, -20);
  lv_obj_set_style_bg_color(btn_confirm, lv_color_hex(0xe94560), 0);
  lv_obj_t *lbl_c = lv_label_create(btn_confirm);
  lv_obj_set_style_text_font(lbl_c, &cn_ui_16, 0);
  lv_label_set_text(lbl_c, "確認寫入");
  lv_obj_center(lbl_c);
  lv_obj_add_event_cb(
    btn_confirm, [](lv_event_t *e) {
      float total = laneA.pure_time + current_penalty * 0.2f;
      Serial.printf("[RECORD] Pure: %.3f  Penalty: %d  Total: %.3f\n",
                    laneA.pure_time, current_penalty, total);

      laneA.penalty = current_penalty;

      // 之後喺呢度寫入 SD 卡
      strcpy(laneA.status, "已記錄，可開始下一輪");
      current_penalty = 0;
      update_penalty_display();

      // ★ 寫入後跳返計時畫面
      if (scr_timer) switch_to_screen(scr_timer);
    },
    LV_EVENT_CLICKED, NULL);
}

/* ======================== Placeholder ======================== */
void create_placeholder_screen(lv_obj_t **scr, const char *title) {
  *scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(*scr, lv_color_hex(0x1a1a2e), 0);

  lv_obj_t *label = lv_label_create(*scr);
  lv_label_set_text(label, title);
  lv_obj_set_style_text_font(label, &cn_ui_32, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xeeeeee), 0);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, -40);

  lv_obj_t *btn_back = lv_btn_create(*scr);
  lv_obj_set_size(btn_back, 160, 60);
  lv_obj_align(btn_back, LV_ALIGN_CENTER, 0, 40);
  lv_obj_set_style_bg_color(btn_back, lv_color_hex(0xe94560), 0);
  lv_obj_t *lbl = lv_label_create(btn_back);
  lv_label_set_text(lbl, "返回主頁");
  lv_obj_set_style_text_font(lbl, &cn_ui_16, 0);
  lv_obj_center(lbl);
  lv_obj_add_event_cb(btn_back, nav_btn_event_cb, LV_EVENT_CLICKED, scr_home);
}

/* ======================== Setup ======================== */
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Speed Slalom Timer UI Stage 1 ===");

  Serial.println("Initializing board");
  board = new Board();
  board->init();

  // 可選：防撕裂設定（如果需要可以打開）

#if LVGL_PORT_AVOID_TEARING_MODE
  auto lcd = board->getLCD();
  lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
  auto lcd_bus = lcd->getBus();
  if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
    static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 10);
  }
#endif
#endif


  assert(board->begin());

  Serial.println("Initializing LVGL");
  lvgl_port_init(board->getLCD(), board->getTouch());  // ★ 官方正確方式

  Serial.println("Creating UI");
  /* LVGL API 唔係 thread-safe，一定要 lock */
  lvgl_port_lock(-1);

  create_home_screen();
  create_placeholder_screen(&scr_event, "賽事資料畫面\n(下一階段實作)");
  create_timer_screen();
  create_penalty_screen();
  create_placeholder_screen(&scr_history, "歷史記錄畫面");
  create_placeholder_screen(&scr_settings, "系統設定畫面");
  create_placeholder_screen(&scr_help, "說明畫面");

  lv_scr_load(scr_home);

  // 建立一次更新 timer（只建立一次！）
  lv_timer_create([](lv_timer_t *t) {
    update_timer_display();
  },
                  50, NULL);

  lvgl_port_unlock();

  Serial.println("[UI] Home screen loaded");
}

void loop() {
  // 官方 port 已經處理咗 tick 同 timer task
  lv_timer_handler();
  delay(5);
}