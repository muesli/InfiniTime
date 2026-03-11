#include "displayapp/screens/WatchFaceCleanDigital.h"

#include <lvgl/lvgl.h>
#include <cstdint>
#include <cstdio>
#include <array>

#include "displayapp/screens/NotificationIcon.h"
#include "displayapp/screens/Symbols.h"
#include "displayapp/screens/WeatherSymbols.h"
#include "components/battery/BatteryController.h"
#include "components/ble/BleController.h"
#include "components/ble/NotificationManager.h"
#include "components/heartrate/HeartRateController.h"
#include "components/motion/MotionController.h"
#include "components/ble/SimpleWeatherService.h"
#include "components/settings/Settings.h"

using namespace Pinetime::Applications::Screens;

namespace {
  void event_handler(lv_obj_t* obj, lv_event_t event) {
    auto* screen = static_cast<WatchFaceCleanDigital*>(obj->user_data);
    screen->UpdateSelected(obj, event);
  }

  constexpr int nCleanDigitalColors = 8;
  constexpr std::array<lv_color_t, nCleanDigitalColors> cleanDigitalHighlightColors = {
    LV_COLOR_MAKE(0x90, 0x50, 0xB0), // purple
    LV_COLOR_MAKE(0xE0, 0x20, 0x7A), // pink
    LV_COLOR_MAKE(0x2E, 0x80, 0xD8), // blue
    LV_COLOR_MAKE(0x00, 0xB8, 0xD0), // cyan
    LV_COLOR_MAKE(0xC9, 0x34, 0x1C), // red
    LV_COLOR_MAKE(0xE6, 0x80, 0x1A), // orange
    LV_COLOR_MAKE(0xF0, 0xC0, 0x20), // yellow
    LV_COLOR_MAKE(0x2A, 0xBB, 0x55), // green
  };
}

lv_color_t WatchFaceCleanDigital::colorProgressFilled;
lv_color_t WatchFaceCleanDigital::colorProgressEmpty;
WatchFaceCleanDigital::BarMode WatchFaceCleanDigital::barMode = WatchFaceCleanDigital::BarMode::Steps;
uint8_t WatchFaceCleanDigital::hrMin = 0;
uint8_t WatchFaceCleanDigital::hrMax = 0;
uint8_t WatchFaceCleanDigital::hrCached = 0;
bool WatchFaceCleanDigital::hrCacheValid = false;
std::chrono::time_point<std::chrono::system_clock, std::chrono::minutes> WatchFaceCleanDigital::hrCacheTime {};

WatchFaceCleanDigital::WatchFaceCleanDigital(Controllers::DateTime& dateTimeController,
                                             const Controllers::Battery& batteryController,
                                             const Controllers::Ble& bleController,
                                             const Controllers::AlarmController& alarmController,
                                             Controllers::NotificationManager& notificationManager,
                                             Controllers::Settings& settingsController,
                                             Controllers::HeartRateController& heartRateController,
                                             Controllers::MotionController& motionController,
                                             Controllers::SimpleWeatherService& weatherService)
  : currentDateTime {{}},
    dateTimeController {dateTimeController},
    batteryController {batteryController},
    notificationManager {notificationManager},
    settingsController {settingsController},
    heartRateController {heartRateController},
    motionController {motionController},
    weatherService {weatherService},
    statusIcons(batteryController, bleController, alarmController) {

  statusIcons.Create();

  // old progressbar: 0xC9341C lv_color_hex(0xD93D24) : lv_color_hex(0x2B070A);
  auto brightGray = lv_color_hex(0xFDFBFE);
  auto midGray = lv_color_hex(0xA8B2E1);
  auto darkGray = lv_color_hex(0x414563);
  auto brightHighlight = cleanDigitalHighlightColors[settingsController.GetCleanDigitalColorIndex()];
  auto darkHighlight = lv_color_mix(brightHighlight, LV_COLOR_BLACK, 50);

  auto colorDate = midGray;
  auto colorTemp = midGray;
  auto colorHour = brightGray;
  auto colorMinute = brightHighlight;
  auto colorProgressValue = midGray;
  auto colorProgressLabel = darkGray;
  auto colorProgressMinMax = darkGray;
  WatchFaceCleanDigital::colorProgressFilled = brightHighlight;
  WatchFaceCleanDigital::colorProgressEmpty = darkHighlight;

  // notification icon
  notificationIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(notificationIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
  lv_label_set_text_static(notificationIcon, NotificationIcon::GetIcon(true));
  lv_obj_align(notificationIcon, nullptr, LV_ALIGN_IN_TOP_LEFT, 0, 0);

  // weather icon + temp
  weatherIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(weatherIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, colorTemp);
  lv_obj_set_style_local_text_font(weatherIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &fontawesome_weathericons);
  lv_label_set_text(weatherIcon, "");
  lv_obj_align(weatherIcon, nullptr, LV_ALIGN_IN_TOP_MID, 72, 34);
  lv_obj_set_auto_realign(weatherIcon, true);

  temperature = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(temperature, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &roboto_20);
  lv_obj_set_style_local_text_color(temperature, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, colorTemp);
  lv_label_set_text(temperature, "");
  lv_obj_align(temperature, nullptr, LV_ALIGN_IN_TOP_MID, 72, 64);

  // date
  label_date = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(label_date, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &roboto_20);
  lv_obj_align(label_date, lv_scr_act(), LV_ALIGN_CENTER, -60, 22);
  lv_obj_set_style_local_text_color(label_date, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, colorDate);

  // time: hour + minute
  label_hour = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(label_hour, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &roboto_italic_120);
  lv_obj_align(label_hour, lv_scr_act(), LV_ALIGN_IN_RIGHT_MID, 0, 0);
  lv_obj_set_style_local_text_color(label_hour, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, colorHour);

  label_minute = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(label_minute, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &roboto_96);
  lv_obj_align(label_minute, lv_scr_act(), LV_ALIGN_IN_RIGHT_MID, 0, 0);
  lv_obj_set_style_local_text_color(label_minute, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, colorMinute);

  // progress bar + label
  progressValue = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(progressValue, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &roboto_italic_32);
  lv_obj_set_style_local_text_color(progressValue, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, colorProgressValue);
  lv_label_set_text_static(progressValue, "0");
  lv_obj_align(progressValue, lv_scr_act(), LV_ALIGN_IN_BOTTOM_MID, 0, -5);

  // Progress bar: custom lv_obj with a design callback drawing parallelogram tiles
  constexpr int barW = (progressTileCount - 1) * (progressTileW + progressGap) + progressTileW + progressSlant;
  progressBar = lv_obj_create(lv_scr_act(), nullptr);
  lv_obj_set_size(progressBar, barW, progressTileH);
  lv_obj_set_style_local_bg_opa(progressBar, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
  lv_obj_set_style_local_border_width(progressBar, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_obj_set_click(progressBar, false);
  lv_obj_set_user_data(progressBar, reinterpret_cast<lv_obj_user_data_t>(0));
  lv_obj_set_design_cb(progressBar, [](lv_obj_t* obj, const lv_area_t* clip, lv_design_mode_t mode) -> lv_design_res_t {
    if (mode != LV_DESIGN_DRAW_MAIN)
      return LV_DESIGN_RES_OK;

    const int filledTiles = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(obj)));
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    const int ox = coords.x1;
    const int oy = coords.y1;
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;

    for (int i = 0; i < WatchFaceCleanDigital::progressTileCount; ++i) {
      const int bx = ox + i * (WatchFaceCleanDigital::progressTileW + WatchFaceCleanDigital::progressGap);
      lv_point_t pts[4];
      pts[0].x = bx + WatchFaceCleanDigital::progressSlant;
      pts[0].y = oy;
      pts[1].x = bx;
      pts[1].y = oy + WatchFaceCleanDigital::progressTileH - 1;
      pts[2].x = bx + WatchFaceCleanDigital::progressTileW;
      pts[2].y = oy + WatchFaceCleanDigital::progressTileH - 1;
      pts[3].x = bx + WatchFaceCleanDigital::progressTileW + WatchFaceCleanDigital::progressSlant;
      pts[3].y = oy;
      dsc.bg_color = (i < filledTiles) ? WatchFaceCleanDigital::colorProgressFilled : WatchFaceCleanDigital::colorProgressEmpty;
      lv_draw_polygon(pts, 4, clip, &dsc);
    }
    return LV_DESIGN_RES_OK;
  });

  lv_obj_align(progressBar, progressValue, LV_ALIGN_OUT_TOP_MID, 0, -4);
  UpdateProgressBar(0);

  progressLabel = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(progressLabel, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &roboto_italic_20);
  lv_obj_set_style_local_text_color(progressLabel, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, colorProgressLabel);
  lv_label_set_text_static(progressLabel, "STEPS");
  lv_obj_align(progressLabel, progressBar, LV_ALIGN_OUT_TOP_LEFT, 2, -4);

  progressMinLabel = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(progressMinLabel, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &roboto_italic_16);
  lv_obj_set_style_local_text_color(progressMinLabel, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, colorProgressMinMax);
  lv_label_set_text_static(progressMinLabel, "0");
  lv_obj_align(progressMinLabel, progressBar, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);

  progressMaxLabel = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(progressMaxLabel, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &roboto_italic_16);
  lv_obj_set_style_local_text_color(progressMaxLabel, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, colorProgressMinMax);
  lv_label_set_text_static(progressMaxLabel, "100");
  lv_obj_align(progressMaxLabel, progressBar, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 4);

  taskRefresh = lv_task_create(RefreshTaskCallback, LV_DISP_DEF_REFR_PERIOD, LV_TASK_PRIO_MID, this);
  Refresh();
}

WatchFaceCleanDigital::~WatchFaceCleanDigital() {
  lv_task_del(taskRefresh);
  lv_obj_clean(lv_scr_act());
}

void WatchFaceCleanDigital::UpdateProgressBar(uint8_t targetTiles) {
  if (!progressBar)
    return;
  const uint8_t currentDisplayed = static_cast<uint8_t>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(progressBar)));
  const int diff = static_cast<int>(targetTiles) - static_cast<int>(currentDisplayed);
  if (diff >= -1 && diff <= 1) {
    // Small change (<=1 tile): skip animation, apply immediately
    barAnimating = false;
    barAnimTarget = targetTiles;
    if (diff != 0) {
      lv_obj_set_user_data(progressBar, reinterpret_cast<lv_obj_user_data_t>(static_cast<intptr_t>(targetTiles)));
      lv_obj_invalidate(progressBar);
    }
    return;
  }
  // Already animating toward this target — don't reset the clock.
  if (barAnimating && targetTiles == barAnimTarget)
    return;
  barAnimFrom = currentDisplayed;
  barAnimTarget = targetTiles;
  barAnimStartTick = xTaskGetTickCount();
  barAnimating = true;
}

void WatchFaceCleanDigital::RefreshBar() {
  switch (barMode) {
    case BarMode::Bat: {
      lv_label_set_text_static(progressLabel, "BAT");
      const uint8_t pct = batteryPercent.Get();
      lv_label_set_text_fmt(progressValue, "%d%%", pct);
      lv_label_set_text_static(progressMinLabel, "0");
      lv_label_set_text_static(progressMaxLabel, "100");
      lv_obj_realign(progressValue);
      lv_obj_align(progressBar, progressValue, LV_ALIGN_OUT_TOP_MID, 0, -4);
      lv_obj_realign(progressLabel);
      lv_obj_realign(progressMinLabel);
      lv_obj_realign(progressMaxLabel);
      const uint8_t filled = static_cast<uint8_t>((static_cast<uint32_t>(pct) * progressTileCount + 50) / 100);
      UpdateProgressBar(filled);
      break;
    }

    case BarMode::Steps: {
      lv_label_set_text_static(progressLabel, "STEPS");
      const uint32_t steps = stepCount.Get();
      lv_label_set_text_fmt(progressValue, "%lu", steps);
      lv_obj_realign(progressValue);
      lv_obj_align(progressBar, progressValue, LV_ALIGN_OUT_TOP_MID, 0, -4);
      lv_label_set_text_static(progressMinLabel, "0");
      lv_label_set_text_static(progressMaxLabel, "100");
      lv_obj_realign(progressLabel);
      lv_obj_realign(progressMinLabel);
      lv_obj_realign(progressMaxLabel);
      const uint32_t goal = settingsController.GetStepsGoal();
      uint8_t filled = 0;
      if (goal > 0) {
        uint32_t pct = (steps * 100) / goal;
        if (pct > 100)
          pct = 100;
        filled = static_cast<uint8_t>((pct * progressTileCount + 50) / 100);
      }
      UpdateProgressBar(filled);
      break;
    }

    case BarMode::Weather: {
      lv_label_set_text_static(progressLabel, "TEMP");
      const auto optWeather = currentWeather.Get();
      if (optWeather) {
        const bool imperial = settingsController.GetWeatherFormat() == Controllers::Settings::WeatherFormat::Imperial;
        const int16_t cur = imperial ? optWeather->temperature.Fahrenheit() : optWeather->temperature.Celsius();
        const int16_t minT = imperial ? optWeather->minTemperature.Fahrenheit() : optWeather->minTemperature.Celsius();
        const int16_t maxT = imperial ? optWeather->maxTemperature.Fahrenheit() : optWeather->maxTemperature.Celsius();
        lv_label_set_text_fmt(progressValue, "%d\xc2\xb0", cur);
        lv_label_set_text_fmt(progressMinLabel, "%d\xc2\xb0", minT);
        lv_label_set_text_fmt(progressMaxLabel, "%d\xc2\xb0", maxT);
        uint8_t filled = 0;
        if (maxT > minT) {
          const int32_t range = maxT - minT;
          int32_t pos = cur - minT;
          if (pos < 0)
            pos = 0;
          if (pos > range)
            pos = range;
          filled = static_cast<uint8_t>((pos * progressTileCount + range / 2) / range);
        }
        UpdateProgressBar(filled);
      } else {
        lv_label_set_text_static(progressValue, "--");
        lv_label_set_text_static(progressMinLabel, "--");
        lv_label_set_text_static(progressMaxLabel, "--");
        UpdateProgressBar(0);
      }
      lv_obj_realign(progressValue);
      lv_obj_align(progressBar, progressValue, LV_ALIGN_OUT_TOP_MID, 0, -4);
      lv_obj_realign(progressLabel);
      lv_obj_realign(progressMinLabel);
      lv_obj_realign(progressMaxLabel);
      break;
    }

    case BarMode::Secs: {
      lv_label_set_text_static(progressLabel, "SECS");
      const uint8_t secs = currentSeconds.Get();
      lv_label_set_text_fmt(progressValue, "%d", secs);
      lv_label_set_text_static(progressMinLabel, "0");
      lv_label_set_text_static(progressMaxLabel, "59");
      lv_obj_realign(progressValue);
      lv_obj_align(progressBar, progressValue, LV_ALIGN_OUT_TOP_MID, 0, -4);
      lv_obj_realign(progressLabel);
      lv_obj_realign(progressMinLabel);
      lv_obj_realign(progressMaxLabel);
      const uint8_t filled = static_cast<uint8_t>((static_cast<uint32_t>(secs) * progressTileCount + 29) / 59);
      UpdateProgressBar(filled);
      break;
    }

    case BarMode::HeartRate: {
      lv_label_set_text_static(progressLabel, "HEART");
      const uint8_t hr = hrCached;
      if (hrCacheValid) {
        lv_label_set_text_fmt(progressValue, "%d", hr);
        lv_label_set_text_fmt(progressMinLabel, "%d", hrMin > 0 ? hrMin : hr);
        lv_label_set_text_fmt(progressMaxLabel, "%d", hrMax);
        uint8_t filled = 0;
        if (hrMax > hrMin) {
          const int32_t range = hrMax - hrMin;
          const int32_t pos = hr - hrMin;
          filled = static_cast<uint8_t>((pos * progressTileCount + range / 2) / range);
        } else {
          filled = progressTileCount / 2;
        }
        UpdateProgressBar(filled);
      } else {
        lv_label_set_text_static(progressValue, "--");
        if (hrMin > 0) {
          lv_label_set_text_fmt(progressMinLabel, "%d", hrMin);
          lv_label_set_text_fmt(progressMaxLabel, "%d", hrMax);
        } else {
          lv_label_set_text_static(progressMinLabel, "--");
          lv_label_set_text_static(progressMaxLabel, "--");
        }
        UpdateProgressBar(0);
      }
      lv_obj_realign(progressValue);
      lv_obj_align(progressBar, progressValue, LV_ALIGN_OUT_TOP_MID, 0, -4);
      lv_obj_realign(progressLabel);
      lv_obj_realign(progressMinLabel);
      lv_obj_realign(progressMaxLabel);
      break;
    }
  }
}

void WatchFaceCleanDigital::CreateSettingsMenu() {
  btnSettings = lv_btn_create(lv_scr_act(), nullptr);
  btnSettings->user_data = this;
  lv_obj_set_size(btnSettings, 150, 150);
  lv_obj_align(btnSettings, lv_scr_act(), LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_local_radius(btnSettings, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, 30);
  lv_obj_set_style_local_bg_opa(btnSettings, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_70);
  lv_obj_set_event_cb(btnSettings, event_handler);
  lv_obj_t* labelBtnSettings = lv_label_create(btnSettings, nullptr);
  lv_obj_set_style_local_text_font(labelBtnSettings, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &lv_font_sys_48);
  lv_label_set_text_static(labelBtnSettings, Symbols::settings);

  btnClose = lv_btn_create(lv_scr_act(), nullptr);
  btnClose->user_data = this;
  lv_obj_set_size(btnClose, 60, 60);
  lv_obj_align(btnClose, lv_scr_act(), LV_ALIGN_CENTER, 0, -80);
  lv_obj_set_style_local_bg_opa(btnClose, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_70);
  lv_obj_t* lblClose = lv_label_create(btnClose, nullptr);
  lv_label_set_text_static(lblClose, "X");
  lv_obj_set_event_cb(btnClose, event_handler);
  lv_obj_set_hidden(btnClose, true);

  btnNextColor = lv_btn_create(lv_scr_act(), nullptr);
  btnNextColor->user_data = this;
  lv_obj_set_size(btnNextColor, 60, 60);
  lv_obj_align(btnNextColor, lv_scr_act(), LV_ALIGN_IN_RIGHT_MID, -15, 0);
  lv_obj_set_style_local_bg_opa(btnNextColor, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_70);
  lv_obj_t* lblNextColor = lv_label_create(btnNextColor, nullptr);
  lv_label_set_text_static(lblNextColor, ">");
  lv_obj_set_event_cb(btnNextColor, event_handler);
  lv_obj_set_hidden(btnNextColor, true);

  btnPrevColor = lv_btn_create(lv_scr_act(), nullptr);
  btnPrevColor->user_data = this;
  lv_obj_set_size(btnPrevColor, 60, 60);
  lv_obj_align(btnPrevColor, lv_scr_act(), LV_ALIGN_IN_LEFT_MID, 15, 0);
  lv_obj_set_style_local_bg_opa(btnPrevColor, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_70);
  lv_obj_t* lblPrevColor = lv_label_create(btnPrevColor, nullptr);
  lv_label_set_text_static(lblPrevColor, "<");
  lv_obj_set_event_cb(btnPrevColor, event_handler);
  lv_obj_set_hidden(btnPrevColor, true);
}

bool WatchFaceCleanDigital::OnTouchEvent(TouchEvents event) {
  if (event == TouchEvents::SwipeLeft) {
    barMode = static_cast<BarMode>((static_cast<uint8_t>(barMode) + 1) % 5);
    RefreshBar();
    return true;
  }
  if (event == TouchEvents::LongTap && btnSettings == nullptr) {
    CreateSettingsMenu();
    savedTick = xTaskGetTickCount();
    return true;
  }
  if (event == TouchEvents::DoubleTap && btnClose != nullptr && !lv_obj_get_hidden(btnClose)) {
    return true;
  }
  return false;
}

void WatchFaceCleanDigital::CloseMenu() {
  settingsController.SaveSettings();
  lv_obj_del(btnSettings);
  lv_obj_del(btnClose);
  lv_obj_del(btnNextColor);
  lv_obj_del(btnPrevColor);
  btnSettings = nullptr;
  btnClose = nullptr;
  btnNextColor = nullptr;
  btnPrevColor = nullptr;
  savedTick = 0;
}

bool WatchFaceCleanDigital::OnButtonPushed() {
  if (btnClose != nullptr && !lv_obj_get_hidden(btnClose)) {
    CloseMenu();
    return true;
  }
  return false;
}

void WatchFaceCleanDigital::UpdateColors() {
  const lv_color_t brightHighlight = cleanDigitalHighlightColors[settingsController.GetCleanDigitalColorIndex()];
  const lv_color_t darkHighlight = lv_color_mix(brightHighlight, LV_COLOR_BLACK, 50);
  lv_obj_set_style_local_text_color(label_minute, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, brightHighlight);
  WatchFaceCleanDigital::colorProgressFilled = brightHighlight;
  WatchFaceCleanDigital::colorProgressEmpty = darkHighlight;
  lv_obj_invalidate(progressBar);
}

void WatchFaceCleanDigital::UpdateSelected(lv_obj_t* object, lv_event_t event) {
  if (event == LV_EVENT_CLICKED) {
    int colorIndex = settingsController.GetCleanDigitalColorIndex();
    if (object == btnSettings) {
      lv_obj_set_hidden(btnSettings, true);
      lv_obj_set_hidden(btnClose, false);
      lv_obj_set_hidden(btnNextColor, false);
      lv_obj_set_hidden(btnPrevColor, false);
      savedTick = 0; // don't auto-hide now that the sub-menu is open
    }
    if (object == btnClose) {
      CloseMenu();
    }
    if (object == btnNextColor) {
      colorIndex = (colorIndex + 1) % nCleanDigitalColors;
      settingsController.SetCleanDigitalColorIndex(colorIndex);
      UpdateColors();
    }
    if (object == btnPrevColor) {
      colorIndex -= 1;
      if (colorIndex < 0)
        colorIndex = nCleanDigitalColors - 1;
      settingsController.SetCleanDigitalColorIndex(colorIndex);
      UpdateColors();
    }
  }
}

void WatchFaceCleanDigital::Refresh() {
  statusIcons.Update();

  if (barAnimating && progressBar) {
    const TickType_t elapsed = xTaskGetTickCount() - barAnimStartTick;
    const TickType_t durationTicks = pdMS_TO_TICKS(barAnimDurationMs);
    uint8_t displayTiles;
    if (elapsed >= durationTicks) {
      displayTiles = barAnimTarget;
      barAnimating = false;
    } else {
      const int32_t delta = static_cast<int32_t>(barAnimTarget) - static_cast<int32_t>(barAnimFrom);
      int32_t interpolated =
        static_cast<int32_t>(barAnimFrom) + (delta * static_cast<int32_t>(elapsed)) / static_cast<int32_t>(durationTicks);
      if (interpolated < 0)
        interpolated = 0;
      if (interpolated > progressTileCount)
        interpolated = progressTileCount;
      displayTiles = static_cast<uint8_t>(interpolated);
    }
    const uint8_t current = static_cast<uint8_t>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(progressBar)));
    if (displayTiles != current) {
      lv_obj_set_user_data(progressBar, reinterpret_cast<lv_obj_user_data_t>(static_cast<intptr_t>(displayTiles)));
      lv_obj_invalidate(progressBar);
    }
  }

  notificationState = notificationManager.AreNewNotificationsAvailable();
  if (notificationState.IsUpdated()) {
    lv_label_set_text_static(notificationIcon, NotificationIcon::GetIcon(notificationState.Get()));
  }

  currentDateTime = std::chrono::time_point_cast<std::chrono::minutes>(dateTimeController.CurrentDateTime());
  if (currentDateTime.IsUpdated()) {
    uint8_t hour = dateTimeController.Hours();
    uint8_t minute = dateTimeController.Minutes();

    if (settingsController.GetClockType() == Controllers::Settings::ClockType::H12) {
      if (hour == 0) {
        hour = 12;
      } else if (hour > 12) {
        hour = hour - 12;
      }
      lv_label_set_text_fmt(label_hour, "%02d", hour);
      lv_obj_align(label_hour, lv_scr_act(), LV_ALIGN_CENTER, -28, -48);
      lv_label_set_text_fmt(label_minute, "%02d", minute);
      lv_obj_align(label_minute, lv_scr_act(), LV_ALIGN_CENTER, 40, 12);
    } else {
      lv_label_set_text_fmt(label_hour, "%02d", hour);
      lv_obj_align(label_hour, lv_scr_act(), LV_ALIGN_CENTER, -28, -48);
      lv_label_set_text_fmt(label_minute, "%02d", minute);
      lv_obj_align(label_minute, lv_scr_act(), LV_ALIGN_CENTER, 40, 12);
    }

    currentDate = std::chrono::time_point_cast<std::chrono::days>(currentDateTime.Get());
    if (currentDate.IsUpdated()) {
      uint8_t day = dateTimeController.Day();
      lv_label_set_text_fmt(label_date, "%s %d", dateTimeController.DayOfWeekShortToString(), day);
      lv_obj_realign(label_date);
    }
  }

  heartbeat = heartRateController.HeartRate();
  heartbeatRunning = heartRateController.State() != Controllers::HeartRateController::States::Stopped;
  if (heartbeat.IsUpdated() || heartbeatRunning.IsUpdated()) {
    const uint8_t hr = heartbeat.Get();
    if (heartbeatRunning.Get() && hr > 0) {
      if (hrMin == 0 || hr < hrMin)
        hrMin = hr;
      if (hr > hrMax)
        hrMax = hr;
      hrCached = hr;
      hrCacheTime = currentDateTime.Get();
      hrCacheValid = true;
    }
    if (barMode == BarMode::HeartRate) {
      RefreshBar();
    }
  }
  if (hrCacheValid && (currentDateTime.Get() - hrCacheTime > std::chrono::minutes(5))) {
    hrCacheValid = false;
    if (barMode == BarMode::HeartRate) {
      RefreshBar();
    }
  }

  stepCount = motionController.NbSteps();
  if (stepCount.IsUpdated() && barMode == BarMode::Steps) {
    RefreshBar();
  }

  currentSeconds = dateTimeController.Seconds();
  if (currentSeconds.IsUpdated() && barMode == BarMode::Secs) {
    RefreshBar();
  }

  batteryPercent = batteryController.PercentRemaining();
  if (batteryPercent.IsUpdated() && barMode == BarMode::Bat) {
    RefreshBar();
  }

  currentWeather = weatherService.Current();
  if (currentWeather.IsUpdated()) {
    auto optCurrentWeather = currentWeather.Get();
    if (optCurrentWeather) {
      int16_t temp = optCurrentWeather->temperature.Celsius();
      // char tempUnit = 'C';
      if (settingsController.GetWeatherFormat() == Controllers::Settings::WeatherFormat::Imperial) {
        temp = optCurrentWeather->temperature.Fahrenheit();
        // tempUnit = 'F';
      }
      // lv_label_set_text_fmt(temperature, "%d°%c", temp, tempUnit);
      lv_label_set_text_fmt(temperature, "%d°", temp);
      lv_label_set_text(weatherIcon, Symbols::GetSymbol(optCurrentWeather->iconId, weatherService.IsNight()));
    } else {
      lv_label_set_text_static(temperature, "");
      lv_label_set_text(weatherIcon, "");
    }
    lv_obj_realign(temperature);
    lv_obj_realign(weatherIcon);
    if (barMode == BarMode::Weather) {
      RefreshBar();
    }
  }

  if (btnSettings != nullptr && !lv_obj_get_hidden(btnSettings)) {
    if ((savedTick > 0) && (xTaskGetTickCount() - savedTick > pdMS_TO_TICKS(3000))) {
      CloseMenu();
    }
  }
}
