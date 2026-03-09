#pragma once

#include <lvgl/src/lv_core/lv_obj.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include "displayapp/screens/Screen.h"
#include "components/datetime/DateTimeController.h"
#include "components/battery/BatteryController.h"
#include "components/ble/SimpleWeatherService.h"
#include "components/ble/BleController.h"
#include "displayapp/widgets/StatusIcons.h"
#include "utility/DirtyValue.h"
#include "displayapp/apps/Apps.h"

namespace Pinetime {
  namespace Controllers {
    class Settings;
    class Battery;
    class Ble;
    class AlarmController;
    class NotificationManager;
    class HeartRateController;
    class MotionController;
  }

  namespace Applications {
    namespace Screens {

      class WatchFaceCleanDigital : public Screen {
      public:
        WatchFaceCleanDigital(Controllers::DateTime& dateTimeController,
                              const Controllers::Battery& batteryController,
                              const Controllers::Ble& bleController,
                              const Controllers::AlarmController& alarmController,
                              Controllers::NotificationManager& notificationManager,
                              Controllers::Settings& settingsController,
                              Controllers::HeartRateController& heartRateController,
                              Controllers::MotionController& motionController,
                              Controllers::FS& fs,
                              Controllers::SimpleWeatherService& weather);
        ~WatchFaceCleanDigital() override;

        void Refresh() override;
        bool OnTouchEvent(TouchEvents event) override;
        bool OnButtonPushed() override;
        void UpdateSelected(lv_obj_t* object, lv_event_t event);
        void CloseMenu();
        void UpdateColors();
        void CreateSettingsMenu();

      private:
        uint8_t displayedHour = -1;
        uint8_t displayedMinute = -1;

        Utility::DirtyValue<std::chrono::time_point<std::chrono::system_clock, std::chrono::minutes>> currentDateTime {};
        Utility::DirtyValue<std::chrono::time_point<std::chrono::system_clock, std::chrono::days>> currentDate;
        Utility::DirtyValue<uint32_t> stepCount {};
        Utility::DirtyValue<uint8_t> heartbeat {};
        Utility::DirtyValue<bool> heartbeatRunning {};
        Utility::DirtyValue<uint8_t> currentSeconds {};
        Utility::DirtyValue<uint8_t> batteryPercent {};
        Utility::DirtyValue<bool> notificationState {};
        Utility::DirtyValue<std::optional<Pinetime::Controllers::SimpleWeatherService::CurrentWeather>> currentWeather {};

        lv_obj_t* label_hour;
        lv_obj_t* label_minute;
        lv_obj_t* label_time_ampm;
        lv_obj_t* label_date;
        lv_obj_t* progressValue;
        lv_obj_t* progressBar = nullptr;
        lv_obj_t* progressLabel = nullptr;
        lv_obj_t* progressMinLabel = nullptr;
        lv_obj_t* progressMaxLabel = nullptr;
        lv_obj_t* notificationIcon;
        lv_obj_t* weatherIcon;
        lv_obj_t* temperature;
        lv_obj_t* btnClose = nullptr;
        lv_obj_t* btnNextColor = nullptr;
        lv_obj_t* btnPrevColor = nullptr;
        lv_obj_t* btnSettings = nullptr;

        static constexpr int progressTileCount = 24;
        static constexpr int progressTileW = 6;
        static constexpr int progressTileH = 16;
        static constexpr int progressGap = 2;
        static constexpr int progressSlant = 4; // px top-edge shift right

        void UpdateProgressBar(uint8_t filledTiles);
        void RefreshBar();
        enum class BarMode : uint8_t { Steps, Weather, HeartRate, Secs, Bat };
        static BarMode barMode;

        uint8_t hrMin = 0;
        uint8_t hrMax = 0;
        uint8_t hrCached = 0;
        bool hrCacheValid = false;
        std::chrono::time_point<std::chrono::system_clock, std::chrono::minutes> hrCacheTime {};
        TickType_t savedTick = 0;

        Controllers::DateTime& dateTimeController;
        const Controllers::Battery& batteryController;
        Controllers::NotificationManager& notificationManager;
        Controllers::Settings& settingsController;
        Controllers::HeartRateController& heartRateController;
        Controllers::MotionController& motionController;
        Controllers::SimpleWeatherService& weatherService;

        lv_task_t* taskRefresh;
        Widgets::StatusIcons statusIcons;
        static lv_color_t colorProgressFilled;
        static lv_color_t colorProgressEmpty;

        lv_font_t* font_roboto_20 = nullptr;
        lv_font_t* font_roboto_italic_16 = nullptr;
        lv_font_t* font_roboto_italic_20 = nullptr;
        lv_font_t* font_roboto_italic_32 = nullptr;
        lv_font_t* font_roboto_96 = nullptr;
        lv_font_t* font_roboto_120 = nullptr;
      };
    }

    template <>
    struct WatchFaceTraits<WatchFace::CleanDigital> {
      static constexpr WatchFace watchFace = WatchFace::CleanDigital;
      static constexpr const char* name = "muesli";

      static Screens::Screen* Create(AppControllers& controllers) {
        return new Screens::WatchFaceCleanDigital(controllers.dateTimeController,
                                                  controllers.batteryController,
                                                  controllers.bleController,
                                                  controllers.alarmController,
                                                  controllers.notificationManager,
                                                  controllers.settingsController,
                                                  controllers.heartRateController,
                                                  controllers.motionController,
                                                  controllers.filesystem,
                                                  *controllers.weatherController);
      };

      static bool IsAvailable(Pinetime::Controllers::FS& /*filesystem*/) {
        return true;
      }
    };
  }
}
