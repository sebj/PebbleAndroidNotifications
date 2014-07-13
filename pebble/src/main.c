#include <pebble.h>
#include "elements.h"

//Animated action bar from https://github.com/gregoiresage/pebble_animated_actionbar
//by Grégoire Sage
#include "animated_ab.h"

// Drawing

void update_layer_callback(Layer *me, GContext *ctx) {
    graphics_context_set_text_color(ctx, GColorBlack);
    
    if (atNotification < 0) {
        char *message;
        switch (atNotification) {
            case LOADING_NOTIFICATIONS:
                message = "Loading...";
                break;
            case COMM_ERROR:
                message = "Could not connect to Android";
                break;
            default:
                message = "Error";
                break;
        }

        action_bar_layer_clear_icon(action_bar, BUTTON_ID_UP);
        action_bar_layer_clear_icon(action_bar, BUTTON_ID_SELECT);
        action_bar_layer_clear_icon(action_bar, BUTTON_ID_DOWN);

        if (atNotification == NO_NOTIFICATIONS) {
            BatteryChargeState state = battery_state_service_peek();
            bool bluetooth_connected = bluetooth_connection_service_peek();

            char phone_status[16] = " disconnected";
            if (bluetooth_connected) {
                snprintf(phone_status, sizeof(phone_status), "at %d%%", phone_battery);
            }

            char info_msg[64];
            snprintf(info_msg, sizeof(info_msg), "No notifications.\n\n\nBattery at %d%%\nAndroid %s", state.charge_percent, phone_status);

            graphics_draw_text(ctx,
                               info_msg,
                               fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                               GRect(0, 8, 144, 100),
                               GTextOverflowModeWordWrap,
                               GTextAlignmentCenter,
                               NULL);
        
        } else {
            graphics_draw_text(ctx,
                           message,
                           fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                           GRect(5, (atNotification==COMM_ERROR)? 36 : 58, 144 - 10, 80),
                           GTextOverflowModeWordWrap,
                           GTextAlignmentCenter,
                           NULL);
        }
    } else {
        GBitmap bitmap = {
            .addr = &notifications[atNotification].icon,
            .bounds = GRect(0, 0, 48, 48),
            .info_flags = 0x1000,
            .row_size_bytes = 8
        };
        graphics_context_set_compositing_mode(ctx, GCompOpAssignInverted);
        graphics_draw_bitmap_in_rect(ctx, &bitmap, GRect(95, 2, 48, 48));


        // Calculating title text position..
        // Get length of title
        size_t title_length = strlen(notifications[atNotification].title);

        // Get content size based on some rough position guesses
        GSize title_size = graphics_text_layout_get_content_size(notifications[atNotification].title,
                                                                 fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                                                                 GRect(title_length>9? 5 : 6, title_length>10? 0 : 9, 144-4-3-48, 48),
                                                                 GTextOverflowModeWordWrap,
                                                                 GTextAlignmentLeft);
        
        // Refine y position based on actual content size
        GRect title_box = GRect(title_length>9? 5 : 6, 0, 144-4-3-48, 48);
        if (title_size.h < 48) {
            title_box.origin.y = (int)((48-title_size.h)/2);
        }
        //

        graphics_draw_text(ctx,
                           notifications[atNotification].title,
                           fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                           title_box,
                           GTextOverflowModeWordWrap,
                           GTextAlignmentLeft,
                           NULL);

        graphics_draw_text(ctx,
                           notifications[atNotification].details,
                           fonts_get_system_font(FONT_KEY_GOTHIC_24),
                           GRect(4, 50, 144-8, 90),
                           GTextOverflowModeFill,
                           GTextAlignmentLeft,
                           NULL);

        // Update action bar
        bool should_hide_action_bar = true;

        if (atNotification == 0) {
            action_bar_layer_clear_icon(action_bar, BUTTON_ID_UP);
        } else {
            should_hide_action_bar = false;
            action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, button_up);
        }

        action_bar_layer_set_icon(action_bar, BUTTON_ID_SELECT, button_cross);

        if (atNotification == loadingNotification) {
            action_bar_layer_clear_icon(action_bar, BUTTON_ID_DOWN);
        } else {
            should_hide_action_bar = false;
            action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, button_down);
        }

        if (should_hide_action_bar && action_bar_visible) {
            hide_actionbar(action_bar);
            action_bar_visible = false;

        } else if (!should_hide_action_bar && !action_bar_visible) {
            show_actionbar(action_bar);
            action_bar_visible = true;
        }
    }
}

// Button presses

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
    if (atNotification > 0) {
        atNotification--;
    }
    layer_mark_dirty((Layer*)layer);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
    if (atNotification > -1) {
        if (action_bar_visible) {
            DictionaryIterator *dict;
            if (app_message_outbox_begin(&dict) == APP_MSG_OK) {
                dict_write_int8(dict, MSG_DISMISS_NOTIFICATION, (int8_t)atNotification);
                app_message_outbox_send();
            }
        } else {
            show_actionbar(action_bar);
            action_bar_visible = true;
        }
    } else {
        // Show options
        window_stack_push(options_window, true);
        options_visible = true;
    }
}

static void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
    if (atNotification > -1) {
        DictionaryIterator *dict;
        if (app_message_outbox_begin(&dict) == APP_MSG_OK) {
            dict_write_int8(dict, MSG_NOTIFICATION_ACTION, (int8_t)atNotification);
            app_message_outbox_send();
        }
    }
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
    if (atNotification > -1 && atNotification < loadingNotification) {
        atNotification++;
    }
    layer_mark_dirty((Layer*)layer);
}

static void click_config_provider(void *context) {
    window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
    window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
    window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);

    // Specify down handler but no up, with the default delay (0 meaning 500ms)
    window_long_click_subscribe(BUTTON_ID_SELECT, 0, select_long_click_handler, NULL);
}

// Phone communication

void ask_for_data() {
    DictionaryIterator *dict;
    if (app_message_outbox_begin(&dict) == APP_MSG_OK) {
        dict_write_uint8(dict, MSG_ASK_FOR_DATA, 1);
        app_message_outbox_send();
    }
}

void ask_for_phone_battery() {
    DictionaryIterator *dict;
    if (app_message_outbox_begin(&dict) == APP_MSG_OK) {
        dict_write_uint8(dict, MSG_GET_PHONE_BATTERY, 1);
        app_message_outbox_send();
    }
}

static void out_fail_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
    atNotification = COMM_ERROR;
    layer_mark_dirty((Layer*)layer);
}

static void in_rcv_handler(DictionaryIterator *received, void *context) {
    Tuple* cmd_tuple = dict_find(received, MSG_NOTIFICATIONS_CHANGED);
    if (cmd_tuple != NULL) {
        ask_for_data();
    }

    cmd_tuple = dict_find(received, MSG_NO_NOTIFICATIONS);
    if (cmd_tuple != NULL) {
        ask_for_phone_battery();

        hide_actionbar(action_bar);
        action_bar_visible = false;

        atNotification = NO_NOTIFICATIONS;
    }

    cmd_tuple = dict_find(received, MSG_PHONE_BATTERY);
    if (cmd_tuple != NULL) {
        phone_battery = cmd_tuple->value->int8;
        layer_mark_dirty((Layer*)layer);
    }
    
    cmd_tuple = dict_find(received, MSG_LOAD_NOTIFICATION_ID);
    if (cmd_tuple != NULL) {
        loadingNotification = cmd_tuple->value->int8;
        if (loadingNotification == 0) {
            atNotification = 0;

            if (options_visible) {
                window_stack_pop(true);
                options_visible = false;
            }
            
            if (persist_exists(PERSIST_VIBRATION) == false || persist_read_bool(PERSIST_VIBRATION) == true) {
                vibes_short_pulse();
            }

            // Only show action bar if multiple notifications
            // if one, user can just press select button to dismiss/action
            show_actionbar(action_bar);
            action_bar_visible = true;
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

// Options menu
void draw_row_callback(GContext *ctx, Layer *cell_layer, MenuIndex *cell_index, void *callback_context) {
    switch(cell_index->row) {
        case 0:
            menu_cell_basic_draw(ctx, cell_layer, "Vibration", (persist_exists(PERSIST_VIBRATION) == false || persist_read_bool(PERSIST_VIBRATION) == true)? "Enabled":"Disabled", NULL);
            break;
    }
}

uint16_t num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *callback_context) {
    return 1;
}

void select_click_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
    switch (cell_index->row) {
        case 0:
            persist_write_bool(PERSIST_VIBRATION, !(persist_exists(PERSIST_VIBRATION) == false || persist_read_bool(PERSIST_VIBRATION) == true));

            layer_mark_dirty(menu_layer_get_layer(options));

            break;
    }
}

void options_disappeared(Window *window) {
    options_visible = false;
}

// Pebble Bluetooth/battery status
static void handle_bluetooth(bool connected) {
    layer_mark_dirty((Layer*)layer);
}

static void handle_battery(BatteryChargeState charge_state) {
    layer_mark_dirty((Layer*)layer);
}

// App lifecycle

void handle_init(void) {
    // Load resources
    button_up = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_UP);
    button_down = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DOWN);
    button_cross = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CROSS);

    // Variables
    loadingNotification = 0;
    options_visible = false;
    
    // Setup the window
    window = window_create();
    window_set_fullscreen(window, false);
    //window_set_click_config_provider(window, click_config_provider);

    Layer *window_root_layer = window_get_root_layer(window);
    GRect frame = layer_get_bounds(window_root_layer);
    
    // Setup the layer that will display the notifications
    layer = layer_create((GRect){ .origin = GPointZero, .size = frame.size });
    layer_set_update_proc(layer, update_layer_callback);
    layer_add_child(window_root_layer, layer);


    // Setup action bar
    action_bar = action_bar_layer_create();
    action_bar_layer_add_to_window(action_bar, window);
    action_bar_layer_set_click_config_provider(action_bar, click_config_provider);
    hide_actionbar(action_bar);
    action_bar_visible = false;


    // Setup options window
    options_window = window_create();
    window_set_fullscreen(options_window, true);
    window_set_window_handlers(options_window, (WindowHandlers){
        .disappear = options_disappeared
    });

    // Setup options menu
    options = menu_layer_create(frame);
    menu_layer_set_callbacks(options, NULL, (MenuLayerCallbacks){
        .draw_row = (MenuLayerDrawRowCallback)draw_row_callback,
        .get_num_rows = (MenuLayerGetNumberOfRowsInSectionsCallback)num_rows_callback,
        .select_click = (MenuLayerSelectCallback)select_click_callback
    });
    menu_layer_set_click_config_onto_window(options, options_window);

    layer_add_child(window_get_root_layer(options_window), menu_layer_get_layer(options));

    // Battery/Bluetooth status
    battery_state_service_subscribe(handle_battery);
    bluetooth_connection_service_subscribe(handle_bluetooth);

    // Trigger a layer update
    layer_mark_dirty((Layer*)layer);

    window_stack_push(window, true /* Animated */);

    // Setup AppMessage
    app_message_register_inbox_received(in_rcv_handler);
    app_message_register_outbox_failed(out_fail_handler);
    app_message_open(128, 32);
    
    ask_for_data();
}

void handle_deinit(void) {
    gbitmap_destroy(button_up);
    gbitmap_destroy(button_down);
    gbitmap_destroy(button_cross);

    menu_layer_destroy(options);
    window_destroy(options_window);
    action_bar_layer_destroy(action_bar);
    layer_destroy(layer);
    window_destroy(window);
}

int main(void) {
    handle_init();
    app_event_loop();
    handle_deinit();
}