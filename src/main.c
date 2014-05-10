#include <pebble.h>

#define MAX_NOTIFICATIONS 25
#define LOADING_NOTIFICATIONS -1
#define NO_NOTIFICATIONS -2
#define COMM_ERROR -3
	
#define MSG_ASK_FOR_DATA 0
#define MSG_DISMISS_NOTIFICATION 1
	
#define MSG_NOTIFICATIONS_CHANGED 500
#define MSG_NO_NOTIFICATIONS 700
#define MSG_LOAD_NOTIFICATION_ID 300
#define MSG_NOTIFICATION_ICON_1 0
#define MSG_NOTIFICATION_ICON_2 1
#define MSG_NOTIFICATION_ICON_3 2
#define MSG_NOTIFICATION_TITLE 200
#define MSG_NOTIFICATION_DETAILS 100

Window *window;
Layer *layer;
ActionBarLayer *action_bar;

GBitmap *button_up;
GBitmap *button_down;
GBitmap *button_cross;

typedef struct Notification {
	uint8_t icon[384];
	char title[20];
	char details[60];
} Notification;

Notification notifications[MAX_NOTIFICATIONS];

int8_t loadingNotification = 0;
int8_t atNotification = LOADING_NOTIFICATIONS;

// Drawing display

void update_layer_callback(Layer *me, GContext *ctx) {
	graphics_context_set_text_color(ctx, GColorBlack);
	
	if (atNotification < 0) {
		char * message;
		switch (atNotification) {
			case LOADING_NOTIFICATIONS:
				message = "Loading...";
				break;
			case NO_NOTIFICATIONS:
				message = "No notifications";
				break;
			case COMM_ERROR:
				message = "Error: Could not connect to Android";
				break;
			default:
				message = "Error";
				break;
		}
		graphics_draw_text(ctx,
		     message,
		     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
		     GRect(5, 53, 144 - ACTION_BAR_WIDTH - 10, 80),
		     GTextOverflowModeWordWrap,
		     GTextAlignmentCenter,
		     NULL);
		
		action_bar_layer_clear_icon(action_bar, BUTTON_ID_UP);
		action_bar_layer_clear_icon(action_bar, BUTTON_ID_SELECT);
		action_bar_layer_clear_icon(action_bar, BUTTON_ID_DOWN);
	} else {
		GBitmap bitmap = {
			.addr = &notifications[atNotification].icon,
			.bounds = GRect(0, 0, 48, 48),
			.info_flags = 0x1000,
			.row_size_bytes = 8
		};
		graphics_context_set_compositing_mode(ctx, GCompOpAssignInverted);
		graphics_draw_bitmap_in_rect(ctx, &bitmap, GRect(38, 5, 48, 48));
		
		graphics_draw_text(ctx,
						   notifications[atNotification].title,
						   fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
						   GRect(5, 53, 144 - ACTION_BAR_WIDTH - 10, 24),
						   GTextOverflowModeTrailingEllipsis,
						   GTextAlignmentCenter,
						   NULL);
		graphics_draw_text(ctx,
						   notifications[atNotification].details,
						   fonts_get_system_font(FONT_KEY_GOTHIC_18),
						   GRect(5, 82, 144 - ACTION_BAR_WIDTH - 10, 81),
						   GTextOverflowModeTrailingEllipsis,
						   GTextAlignmentCenter,
						   NULL);
		
		if (atNotification == 0) {
			action_bar_layer_clear_icon(action_bar, BUTTON_ID_UP);
		} else {
			action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, button_up);
		}
		action_bar_layer_set_icon(action_bar, BUTTON_ID_SELECT, button_cross);
		if (atNotification == loadingNotification) {
			action_bar_layer_clear_icon(action_bar, BUTTON_ID_DOWN);
		} else {
			action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, button_down);
		}
	}
}

// Button presses

void up_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (atNotification > 0) {
		atNotification--;
		layer_mark_dirty((Layer*)layer);
	}
}
void select_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (atNotification > -1) {
		DictionaryIterator *dict;
		if (app_message_outbox_begin(&dict) == APP_MSG_OK) {
			dict_write_int8(dict, MSG_DISMISS_NOTIFICATION, (int8_t)atNotification);
			app_message_outbox_send();
		}
	}
}
void down_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (atNotification > -1 && atNotification < loadingNotification) {
		atNotification++;
		layer_mark_dirty((Layer*)layer);
	}
}
void click_config_provider(void *context) {
	window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) up_click_handler);
	window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) select_click_handler);
	window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) down_click_handler);
}

// Communication with phone

void ask_for_data() {
	DictionaryIterator *dict;
	if (app_message_outbox_begin(&dict) == APP_MSG_OK) {
		dict_write_uint8(dict, MSG_ASK_FOR_DATA, 1);
		app_message_outbox_send();
	}
}
void my_out_sent_handler(DictionaryIterator *sent, void *context) {
	// All good, no need to do anything
}
void my_out_fail_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
	atNotification = COMM_ERROR;
	layer_mark_dirty((Layer*)layer);
}
void my_in_rcv_handler(DictionaryIterator *received, void *context) {
	Tuple* cmd_tuple = dict_find(received, MSG_NOTIFICATIONS_CHANGED);
	if (cmd_tuple != NULL) {
		ask_for_data();
	}
	
	cmd_tuple = dict_find(received, MSG_NO_NOTIFICATIONS);
	if (cmd_tuple != NULL) {
		atNotification = NO_NOTIFICATIONS;
	}
	
	cmd_tuple = dict_find(received, MSG_LOAD_NOTIFICATION_ID);
	if (cmd_tuple != NULL) {
		loadingNotification = cmd_tuple->value->int8;
		if (loadingNotification == 0) {
			atNotification = 0;
			vibes_short_pulse();
		}
	}
	
	cmd_tuple = dict_find(received, MSG_NOTIFICATION_ICON_1);
	if (cmd_tuple != NULL) {
		for (int i = 0; i < 116; i++) {
			notifications[loadingNotification].icon[i + (i / 6) * 2] = cmd_tuple->value->data[i];
		}
	}

	cmd_tuple = dict_find(received, MSG_NOTIFICATION_ICON_2);	
	if (cmd_tuple != NULL) {
		for (int i = 116; i < 116*2; i++) {
			notifications[loadingNotification].icon[i + (i / 6) * 2] = cmd_tuple->value->data[i-116];
		}
	}
	
	cmd_tuple = dict_find(received, MSG_NOTIFICATION_ICON_3);
	if (cmd_tuple != NULL) {
		for (int i = 116*2; i < 116*2+56; i++) {
			notifications[loadingNotification].icon[i + (i / 6) * 2] = cmd_tuple->value->data[i-116*2];
		}
	}	
		
	cmd_tuple = dict_find(received, MSG_NOTIFICATION_TITLE);
	if (cmd_tuple != NULL) {
		strcpy(&notifications[loadingNotification].title[0], cmd_tuple->value->cstring);
	}

	cmd_tuple = dict_find(received, MSG_NOTIFICATION_DETAILS);
	if (cmd_tuple != NULL) {
		strcpy(&notifications[loadingNotification].details[0], cmd_tuple->value->cstring);
	}
	
  	layer_mark_dirty((Layer*)layer);
}
void my_in_drp_handler(AppMessageResult reason, void *context) {
	// Java will handle NACKs, not sure if anything useful can be done here
}

// App lifecycle

void handle_init(void) {
	// Load resources
	button_up = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_UP);
	button_down = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DOWN);
	button_cross = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CROSS);
	
	// Setup the window
	window = window_create();
	window_set_fullscreen(window, true);
	window_stack_push(window, true /* Animated */);

	Layer *window_root_layer = window_get_root_layer(window);
	
	// Setup the layer that will display the notifications
	layer = layer_create((GRect){ .origin = GPointZero, .size = layer_get_bounds(window_root_layer).size });
	layer_set_update_proc(layer, update_layer_callback);
	layer_add_child(window_root_layer, layer);
	
	// Setup action bar
	action_bar = action_bar_layer_create();
	action_bar_layer_add_to_window(action_bar, window);
	action_bar_layer_set_click_config_provider(action_bar, click_config_provider);

	// Trigger a layer update
	layer_mark_dirty(layer);

	//Setup AppMessage
	app_message_register_inbox_received(my_in_rcv_handler);
	app_message_register_inbox_dropped(my_in_drp_handler);
	app_message_register_outbox_failed(my_out_fail_handler);
	app_message_register_outbox_sent(my_out_sent_handler);
	app_message_open(128, 32);
	
	ask_for_data();
}
void handle_deinit(void) {
  	gbitmap_destroy(button_up);
	gbitmap_destroy(button_down);
	gbitmap_destroy(button_cross);

	layer_destroy(layer);
	window_destroy(window);
}

int main(void) {
	handle_init();
	app_event_loop();
	handle_deinit();
}
