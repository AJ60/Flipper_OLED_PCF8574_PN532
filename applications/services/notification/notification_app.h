#include <furi.h>
#include <furi_hal.h>
#include "notification.h"
#include "notification_messages.h"
#include "notification_settings_filename.h"

#define NOTIFICATION_LED_COUNT      3
#define NOTIFICATION_EVENT_COMPLETE 0x00000001U

typedef enum {
    NotificationLayerMessage,
    InternalLayerMessage,
    SaveSettingsMessage,
    LoadSettingsMessage,
} NotificationAppMessageType;

typedef struct {
    const NotificationSequence* sequence;
    NotificationAppMessageType type;
    FuriEventFlag* back_event;
} NotificationAppMessage;

typedef enum {
    LayerInternal = 0,
    LayerNotification = 1,
    LayerMAX = 2,
} NotificationLedLayerIndex;

typedef struct {
    uint8_t value_last[LayerMAX];
    uint8_t value[LayerMAX];
    NotificationLedLayerIndex index;
    Light light;
} NotificationLedLayer;

#define NOTIFICATION_SETTINGS_VERSION 0x03
#define NOTIFICATION_SETTINGS_MAGIC   0x16

typedef enum {
    NotificationOledDriverSSD1306 = 0,
    NotificationOledDriverSH1106 = 1,
} NotificationOledDriver;

typedef struct {
    uint8_t version;
    float display_brightness;
	uint8_t oled_driver;
    float led_brightness;
    float speaker_volume;
    uint32_t display_off_delay_ms;
    int8_t contrast;
    bool vibro_on;
} NotificationSettings;

struct NotificationApp {
    FuriMessageQueue* queue;
    FuriPubSub* event_record;
    FuriTimer* display_timer;

    NotificationLedLayer display;
    NotificationLedLayer led[NOTIFICATION_LED_COUNT];
    uint8_t display_led_lock;

    NotificationSettings settings;

    FuriPubSub* ascii_record;
};

#ifdef __cplusplus
extern "C" {
#endif

void notification_message_save_settings(NotificationApp* app);

#ifdef __cplusplus
}
#endif
