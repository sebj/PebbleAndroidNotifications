#define MAX_NOTIFICATIONS 25

// Internal statuses
#define LOADING_NOTIFICATIONS -1
#define NO_NOTIFICATIONS -2
#define COMM_ERROR -3

// Sent
#define MSG_ASK_FOR_DATA 0
#define MSG_DISMISS_NOTIFICATION 1
#define MSG_NOTIFICATION_ACTION 2

#define MSG_GET_PHONE_BATTERY 4

// Received
#define MSG_NOTIFICATION_ICON_1 0
#define MSG_NOTIFICATION_ICON_2 1
#define MSG_NOTIFICATION_ICON_3 2
#define MSG_NOTIFICATION_DETAILS 100
#define MSG_NOTIFICATION_TITLE 200
#define MSG_LOAD_NOTIFICATION_ID 300

#define MSG_NOTIFICATIONS_CHANGED 500
#define MSG_NO_NOTIFICATIONS 700

#define MSG_PHONE_BATTERY 800

#define PERSIST_VIBRATION 0
#define PERSIST_INVERT_COLORS 1

GBitmap *button_up;
GBitmap *button_down;
GBitmap *button_cross;

bool action_bar_visible;
ActionBarLayer *action_bar;

bool notification_visible;
Window *notifications_window;
// TODO: Use ScrollLayer (?)
Layer *notification_layer;

Window *window;
MenuLayer *menu;

typedef struct Notification {
	uint8_t icon[384];
	char title[20];
	char details[60];
} Notification;

Notification notifications[MAX_NOTIFICATIONS];

int8_t loadingNotification;
int8_t atNotification = LOADING_NOTIFICATIONS;

char pebble_status[16] = "%%";
char phone_status[16] = "Unavailable";
int phone_battery = 50;