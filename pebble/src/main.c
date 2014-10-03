#include <pebble.h>
#include "elements.h"

//Animated action bar from https://github.com/gregoiresage/pebble_animated_actionbar
//by Grégoire Sage
#include "animated_ab.h"

// Drawing

void update_layer_callback(Layer *me, GContext *ctx) {
    bool invert_colors = (persist_exists(PERSIST_INVERT_COLORS) == false || persist_read_bool(PERSIST_INVERT_COLORS) == true);
    if (invert_colors) {
        //Invert colors to white on black
        graphics_context_set_text_color(ctx, GColorWhite);
        window_set_background_color(notifications_window, GColorBlack);

        action_bar_layer_set_background_color(action_bar, GColorWhite);

    } else {
        graphics_context_set_text_color(ctx, GColorBlack);
        window_set_background_color(notifications_window, GColorWhite);

        action_bar_layer_set_background_color(action_bar, GColorBlack);
    }
    
    if (atNotification >= 0) {
        GBitmap bitmap = {
            .addr = &notifications[atNotification].icon,
            .bounds = GRect(0, 0, 48, 48),
            .info_flags = 0x1000,
            .row_size_bytes = 8
        };
        graphics_context_set_compositing_mode(ctx, invert_colors? GCompOpAssign : GCompOpAssignInverted);
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

static void refreshInformation() {
    BatteryChargeState charge_state = battery_state_service_peek();
    bool bluetooth_connected = bluetooth_connection_service_peek();

    if (phone_battery == 0) {
        snprintf(phone_status, sizeof(phone_status), "Unavailable");
        
    } else if (bluetooth_connected && atNotification != COMM_ERROR) {
        snprintf(phone_status, sizeof(phone_status), "%d%% charged", phone_battery);
    }

    if (charge_state.is_charging) {
        snprintf(pebble_status, sizeof(pebble_status), "Charging (%d%%)", charge_state.charge_percent);
    } else {
        snprintf(pebble_status, sizeof(pebble_status), "%d%% charged", charge_state.charge_percent);
    }

    menu_layer_reload_data(menu);
}

// Button presses

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
    if (atNotification > 0) {
        atNotification--;
    }
    layer_mark_dirty((Layer*)notification_layer);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
    if (atNotification > LOADING_NOTIFICATIONS) {

        DictionaryIterator *dict;
        if (app_message_outbox_begin(&dict) == APP_MSG_OK) {
            dict_write_int8(dict, MSG_DISMISS_NOTIFICATION, (int8_t)atNotification);
            app_message_outbox_send();

            if (atNotification == 0 && atNotification == loadingNotification) {
                //Remove notifications window and main menu window if only one notification
                //and it was dismissed
                window_stack_remove(window, false);
                window_stack_pop(true);
            }
        }
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
    if (atNotification > LOADING_NOTIFICATIONS && atNotification < loadingNotification) {
        atNotification++;
    }
    layer_mark_dirty((Layer*)notification_layer);
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
    layer_mark_dirty((Layer*)notification_layer);
}

static void in_rcv_handler(DictionaryIterator *received, void *context) {
    Tuple* cmd_tuple = dict_find(received, MSG_NOTIFICATIONS_CHANGED);
    if (cmd_tuple != NULL) {
        ask_for_data();
    }

    cmd_tuple = dict_find(received, MSG_NO_NOTIFICATIONS);
    if (cmd_tuple != NULL) {
        ask_for_phone_battery();

        if (action_bar_visible) {
            hide_actionbar(action_bar);
            action_bar_visible = false;
        }

        atNotification = NO_NOTIFICATIONS;
    }

    cmd_tuple = dict_find(received, MSG_PHONE_BATTERY);
    if (cmd_tuple != NULL) {
        phone_battery = cmd_tuple->value->int8;
        refreshInformation();
    }
    
    cmd_tuple = dict_find(received, MSG_LOAD_NOTIFICATION_ID);
    if (cmd_tuple != NULL) {
        loadingNotification = cmd_tuple->value->int8;
        if (loadingNotification == 0) {
            atNotification = 0;

            if (!notification_visible) {
                window_stack_push(notifications_window, false);
                notification_visible = true;
            }
            
            if (persist_exists(PERSIST_VIBRATION) == false || persist_read_bool(PERSIST_VIBRATION) == true) {
                vibes_short_pulse();
            }

            if (!action_bar_visible) {
                show_actionbar(action_bar);
                action_bar_visible = true;
            }
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
    
    layer_mark_dirty((Layer*)notification_layer);
}

static void draw_row_callback(GContext *ctx, Layer *cell_layer, MenuIndex *cell_index, void *callback_context) {
    switch(cell_index->section) {
        //First section
        case 0:
            switch (cell_index->row) {
                //Pebble battery
                case 0:
                    menu_cell_basic_draw(ctx, cell_layer, "Battery", pebble_status, NULL);
                    break;

                //Pebble battery
                case 1:
                    menu_cell_basic_draw(ctx, cell_layer, "Phone battery", phone_status, NULL);
                    break;
            }
            break;

        //Second section
        case 1:
            switch (cell_index->row) {
                case 0:
                    menu_cell_basic_draw(ctx, cell_layer, "Vibration", (persist_exists(PERSIST_VIBRATION) == false || persist_read_bool(PERSIST_VIBRATION) == true)? "Enabled":"Disabled", NULL);
                    break;

                case 1:
                    menu_cell_basic_draw(ctx, cell_layer, "Colors", (persist_exists(PERSIST_INVERT_COLORS) == false || persist_read_bool(PERSIST_INVERT_COLORS) == true)? "Inverted (W on B)":"Regular (B on W)", NULL);
                    break;
            }
            break;
    }
}

static void draw_header_callback(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *callback_context) {
    switch (section_index) {
        case 0:
            menu_cell_basic_header_draw(ctx, cell_layer, "Information");
            break;

        case 1:
            menu_cell_basic_header_draw(ctx, cell_layer, "Options");
            break;
    }
}

static int16_t header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static uint16_t num_sections_callback(MenuLayer *menu_layer, void *data) {
    return 2;
}

static uint16_t num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *callback_context) {
    return 2;
}

static void select_click_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
    //Options section
    bool need_to_redraw = false;
    if (cell_index->section == 1) {

        if (cell_index->row == 0) {
            persist_write_bool(PERSIST_VIBRATION, !(persist_exists(PERSIST_VIBRATION) == false || persist_read_bool(PERSIST_VIBRATION) == true));
            need_to_redraw = true;

        } else if (cell_index->row == 1) {
            persist_write_bool(PERSIST_INVERT_COLORS, !(persist_exists(PERSIST_INVERT_COLORS) == false || persist_read_bool(PERSIST_INVERT_COLORS) == true));
            need_to_redraw = true;
        }
    }

    if (need_to_redraw) {
        layer_mark_dirty(menu_layer_get_layer(menu));
    }
}

void notifications_disappeared(Window *window) {
    notification_visible = false;
}

// Pebble Bluetooth/battery status
static void handle_bluetooth(bool connected) {
    layer_mark_dirty((Layer*)notification_layer);
}

static void handle_battery(BatteryChargeState charge_state) {
    layer_mark_dirty((Layer*)notification_layer);
}

// App lifecycle

void handle_init(void) {
    // Load resources
    button_up = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_UP);
    button_down = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DOWN);
    button_cross = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CROSS);

    // Variables
    loadingNotification = 0;
    notification_visible = false;
    
    // Setup the notifications window
    notifications_window = window_create();
    window_set_window_handlers(notifications_window, (WindowHandlers){
        .disappear = notifications_disappeared
    });
    //window_set_click_config_provider(window, click_config_provider);

    Layer *window_root_layer = window_get_root_layer(notifications_window);
    GRect frame = layer_get_bounds(window_root_layer);
    
    // Setup the layer that will display the notifications
    notification_layer = layer_create((GRect){ .origin = GPointZero, .size = frame.size });
    layer_set_update_proc(notification_layer, update_layer_callback);
    layer_add_child(window_root_layer, notification_layer);


    // Setup action bar
    action_bar = action_bar_layer_create();
    action_bar_layer_add_to_window(action_bar, notifications_window);
    action_bar_layer_set_click_config_provider(action_bar, click_config_provider);
    hide_actionbar(action_bar);
    action_bar_visible = false;


    // Setup main window
    window = window_create();

    // Setup menu
    menu = menu_layer_create(frame);
    menu_layer_set_callbacks(menu, NULL, (MenuLayerCallbacks){
        .get_num_sections = (MenuLayerGetNumberOfSectionsCallback) num_sections_callback,
        .get_num_rows = (MenuLayerGetNumberOfRowsInSectionsCallback) num_rows_callback,
        .get_header_height = (MenuLayerGetHeaderHeightCallback) header_height_callback,
        .draw_header = (MenuLayerDrawHeaderCallback) draw_header_callback,
        .draw_row = (MenuLayerDrawRowCallback)draw_row_callback,
        .select_click = (MenuLayerSelectCallback)select_click_callback
    });
    menu_layer_set_click_config_onto_window(menu, window);

    layer_add_child(window_get_root_layer(window), menu_layer_get_layer(menu));

    // Battery/Bluetooth status
    battery_state_service_subscribe(handle_battery);
    bluetooth_connection_service_subscribe(handle_bluetooth);


    window_stack_push(window, true);

    refreshInformation();

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

    layer_destroy(notification_layer);
    window_destroy(notifications_window);
    action_bar_layer_destroy(action_bar);
    menu_layer_destroy(menu);
    window_destroy(window);
}

int main(void) {
    handle_init();
    app_event_loop();
    handle_deinit();
}