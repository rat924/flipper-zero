#include <furi.h>
#include <gui/gui.h>
#include <furi_hal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

// Note definitions
#define NOTE_UP 587.33f
#define NOTE_DOWN 349.23f
#define NOTE_LEFT 493.88f
#define NOTE_RIGHT 440.00f
#define NOTE_OK 261.63f // Sound for the OK button

// Function for drawing on the GUI
void draw_callback(Canvas* canvas, void* ctx) {
    const char* text = ctx ? (const char*)ctx : "Reaction Game!";
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    // Create a modifiable copy of the text
    char* mutable_text = strdup(text); // Create a copy
    if (mutable_text == NULL) {
        // Error handling if strdup fails
        return;
    }

    // Split and draw lines
    char* line = strtok(mutable_text, "\n");
    int y = 0; // Start position for the first line (at the top)
    while (line != NULL) {
        canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignTop, line); // AlignTop for top alignment
        y += 16; // Increase the Y position for the next line (16 is the height of the font)
        line = strtok(NULL, "\n");
    }

    free(mutable_text); // Free memory
}

// Function for playing sounds
void play_sound(float frequency, uint32_t duration_ms) {
    if(furi_hal_speaker_acquire(1000)) {
        furi_hal_speaker_start(frequency, 100);
        furi_delay_ms(duration_ms);
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

// Function to check if a button is pressed
bool is_button_pressed(const GpioPin* button) {
    return furi_hal_gpio_read(button) == false; // Active low
}

// Function to select the correct direction for the reaction game
void play_reaction_sound(const char* direction) {
    if (strcmp(direction, "Up") == 0) {
        play_sound(NOTE_UP, 200);
    } else if (strcmp(direction, "Down") == 0) {
        play_sound(NOTE_DOWN, 200);
    } else if (strcmp(direction, "Left") == 0) {
        play_sound(NOTE_LEFT, 200);
    } else if (strcmp(direction, "Right") == 0) {
        play_sound(NOTE_RIGHT, 200);
    }
}

// Main function of the app
int32_t reaction_game_app(void* p) {
    int lifes = 3;
    uint32_t best_time = 9999;
    UNUSED(p);

    // Initialize random number generator
    srand(furi_get_tick());  // Seed the random number generator

    // Initialize GUI and Viewport
    ViewPort* viewport = view_port_alloc();
    gui_add_view_port(furi_record_open("gui"), viewport, GuiLayerFullscreen);

    // 1. Show intro and wait for OK button release
    char intro_text[] = "Press OK to start!";
    view_port_draw_callback_set(viewport, draw_callback, intro_text);

    // Wait until the OK button is released
    while (is_button_pressed(&gpio_button_ok)) {
        furi_delay_ms(10); // Wait for OK button release
    }

    // Confirmation sound for starting the game
    play_sound(NOTE_OK, 200);

    // 2. Start the actual game
    char text[32];
    char reaction_text[128];  // For displaying reaction time
    bool game_running = true;

    // Possible directions
    const char* directions[] = {"Up", "Down", "Left", "Right"};
    const uint32_t num_directions = 4;
    
    while(game_running) {
        // 1. Show "Wait..." and wait a random time
        snprintf(text, sizeof(text), "\n\nWait...");
        view_port_draw_callback_set(viewport, draw_callback, text);
        furi_delay_ms(500 + (furi_get_tick() % 1500)); // Random delay between 500-2000 ms

        // 2. Randomly select a direction
        const char* direction = directions[rand() % num_directions]; // Randomly select a direction
        snprintf(text, sizeof(text), "\n\nPress %s!", direction);
        view_port_draw_callback_set(viewport, draw_callback, text);

        // 3. Start time measurement
        uint32_t start_time = furi_get_tick();

        // 4. Wait for the correct button (Up, Down, Left, or Right)
        bool reaction_captured = false; // Used to end the round
        
        while(!reaction_captured) {
            const GpioPin* buttons[] = {&gpio_button_up, &gpio_button_down, &gpio_button_left, &gpio_button_right};

            for (int i = 0; i < 4; i++) {
                if (strcmp(direction, directions[i]) == 0 && is_button_pressed(buttons[i])) {
                    uint32_t reaction_time = (furi_get_tick() - start_time) * 1000 / furi_kernel_get_tick_frequency();
                    if (reaction_time < best_time && reaction_time > 50) {
                        best_time = reaction_time;
                    }
                    snprintf(reaction_text, sizeof(reaction_text), "Lives: %d\nReaction time: %lu ms\nBest reaction time:\n%lu ms", lifes, reaction_time, best_time);
                    play_reaction_sound(directions[i]);
                    reaction_captured = true;
                    break; // End the loop if the correct button is pressed
                } else if (strcmp(direction, directions[i]) != 0 && is_button_pressed(buttons[i])) {
                    lifes -= 1;
                    snprintf(reaction_text, sizeof(reaction_text), "Lives: %d\nWrong button pressed\nBest reaction time:\n%lu ms", lifes, best_time);
                    reaction_captured = true;
                    break; // End the loop if a wrong button is pressed
                }
            }

            if (lifes == 0) {
                snprintf(reaction_text, sizeof(reaction_text), "No lives left!\nBest reaction time:\n%lu ms", best_time);
                game_running = false;
            }

            // Monitor the BACK button to end the game
            if (is_button_pressed(&gpio_button_back)) { 
                game_running = false;
                reaction_captured = true;
            }

            furi_delay_ms(10); // Delay for CPU relief
        }

        // Show the reaction time or error message
        view_port_draw_callback_set(viewport, draw_callback, reaction_text);

        // 5. Monitor the OK button to start the round
        while (!is_button_pressed(&gpio_button_ok)) {
            furi_delay_ms(10); // Wait for OK button release (no delay after display)
        }

        // Confirmation sound for OK button
        play_sound(NOTE_OK, 200);

        // Wait for the OK button to be released before continuing the game
        while (is_button_pressed(&gpio_button_ok)) {
            furi_delay_ms(10); // Wait until OK button is released
        }

        // The round is complete and a new round can begin
    }

    // End the game and clean up
    snprintf(text, sizeof(text), "Game over!");
    view_port_draw_callback_set(viewport, draw_callback, text);
    furi_delay_ms(2000);

    gui_remove_view_port(furi_record_open("gui"), viewport);
    view_port_free(viewport);
    furi_record_close("gui");
    return 0;
}
