#include <furi.h>
#include <gui/gui.h>
#include <furi_hal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_CALIBER_LENGTH 2
#define MAX_DISPLAY_LENGTH 128
#define INCH_TO_MM 25.4
#define PISTOL_MULTIPLIER 0.1
#define RIFLE_MULTIPLIER 0.25
#define PISTOL_INCREMENT 0.4
#define RIFLE_INCREMENT 0.6
#define GRAM_TO_GRAINS 15.4

typedef struct {
    char caliber[MAX_CALIBER_LENGTH + 1]; // +1 for null terminator
    int current_position;
    bool input_mode;
    char display_buffer[MAX_DISPLAY_LENGTH];
    char display_buffer_rifle[MAX_DISPLAY_LENGTH]; // Buffer for Max Rifle
    bool exit; // Flag to indicate exit
} AppData;

void draw_callback(Canvas* canvas, void* ctx) {
    AppData* app_data = (AppData*)ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Caliber Input:");

    // Create a buffer to hold the full string with the dot
    char display_caliber[MAX_CALIBER_LENGTH + 2]; // +2 to accommodate '.' and '\0'
    display_caliber[0] = '.';
    display_caliber[1] = '\0';
    strcat(display_caliber, app_data->caliber);

    canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignTop, display_caliber);
    if (!app_data->input_mode) {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, app_data->display_buffer);
        canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignTop, app_data->display_buffer_rifle); // Display Max Rifle on a new line
    }
}

void calculate_powder(const char* caliber, float* max_pistol, float* max_rifle) {
    char full_caliber[MAX_CALIBER_LENGTH + 2]; // +2 to accommodate '.' and '\0'
    if (caliber[0] != '.') {
        full_caliber[0] = '.';
        strncpy(&full_caliber[1], caliber, MAX_CALIBER_LENGTH);
    } else {
        strncpy(full_caliber, caliber, MAX_CALIBER_LENGTH + 1);
    }

    float cal = strtof(full_caliber, NULL); // Convert string to float
    float bore_diameter_mm = cal * INCH_TO_MM;

    // Calculate the powder amounts in grams
    float pistol_powder_grams = (bore_diameter_mm * PISTOL_MULTIPLIER) + PISTOL_INCREMENT;
    float rifle_powder_grams = (bore_diameter_mm * RIFLE_MULTIPLIER) + RIFLE_INCREMENT;

    // Convert the powder amounts to grains
    *max_pistol = pistol_powder_grams * GRAM_TO_GRAINS;
    *max_rifle = rifle_powder_grams * GRAM_TO_GRAINS;
}

void input_callback(InputEvent* input_event, void* ctx) {
    AppData* app_data = (AppData*)ctx;
    if(input_event->type == InputTypeShort) {
        if(app_data->input_mode) {
            switch(input_event->key) {
                case InputKeyDown:
                    if(app_data->caliber[app_data->current_position] < '9') {
                        app_data->caliber[app_data->current_position]++;
                    }
                    break;
                case InputKeyUp:
                    if(app_data->caliber[app_data->current_position] > '0') {
                        app_data->caliber[app_data->current_position]--;
                    }
                    break;
                case InputKeyLeft:
                    if(app_data->current_position > 0) {
                        app_data->current_position--;
                    }
                    break;
                case InputKeyRight:
                    if(app_data->current_position < MAX_CALIBER_LENGTH - 1) {
                        app_data->caliber[++app_data->current_position] = '0';
                    }
                    break;
                case InputKeyOk:
                    app_data->input_mode = false;
                    float max_pistol, max_rifle;
                    calculate_powder(app_data->caliber, &max_pistol, &max_rifle);
                    snprintf(app_data->display_buffer, MAX_DISPLAY_LENGTH, "Max Pistol: %.2f grains", (double)max_pistol);
                    snprintf(app_data->display_buffer_rifle, MAX_DISPLAY_LENGTH, "Max Rifle: %.2f grains", (double)max_rifle); // Separate buffer for Max Rifle
                    break;
                case InputKeyBack:
                    // Set the exit flag to true
                    app_data->exit = true;
                    break;
                default:
                    break;
            }
        } else {
            if(input_event->key == InputKeyOk) {
                memset(app_data->caliber, '0', MAX_CALIBER_LENGTH);
                app_data->caliber[MAX_CALIBER_LENGTH] = '\0';
                app_data->current_position = 0;
                app_data->input_mode = true;
            }
            if(input_event->key == InputKeyBack) {
                // Set the exit flag to true
                app_data->exit = true;
            }
        }
    }
}

int32_t muzzleloader_app(void* p) {
    UNUSED(p);

    AppData app_data = {
        .caliber = "00", // Changed initial caliber to "00"
        .current_position = 0,
        .input_mode = true,
        .display_buffer = "",
        .display_buffer_rifle = "", // Initialize the buffer for Max Rifle
        .exit = false, // Initialize the exit flag
    };

    ViewPort* viewport = view_port_alloc();
    view_port_draw_callback_set(viewport, draw_callback, &app_data);
    view_port_input_callback_set(viewport, input_callback, &app_data);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, viewport, GuiLayerFullscreen);

    while(!app_data.exit) {
        view_port_update(viewport);
        furi_delay_ms(100);
    }

    gui_remove_view_port(gui, viewport);
    furi_record_close(RECORD_GUI);
    view_port_free(viewport);

    return 0;
}
