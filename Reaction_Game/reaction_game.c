#include <furi.h>
#include <gui/gui.h>
#include <furi_hal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

// Notendefinitionen
#define NOTE_UP 587.33f
#define NOTE_DOWN 349.23f
#define NOTE_LEFT 493.88f
#define NOTE_RIGHT 440.00f
#define NOTE_OK 261.63f // Ton für die OK-Taste

// Funktion für das Zeichnen auf der GUI
void draw_callback(Canvas* canvas, void* ctx) {
    const char* text = ctx ? (const char*)ctx : "Reaktionsspiel!";
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    // Erstelle eine modifizierbare Kopie des Textes
    char* mutable_text = strdup(text); // Erstelle eine Kopie
    if (mutable_text == NULL) {
        // Fehlerbehandlung, falls strdup fehlschlägt
        return;
    }

    // Zeilen aufteilen und zeichnen
    char* line = strtok(mutable_text, "\n");
    int y = 0; // Startposition für die erste Zeile (ganz oben)
    while (line != NULL) {
        canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignTop, line); // AlignTop für obere Ausrichtung
        y += 16; // Erhöhe die Y-Position für die nächste Zeile (16 ist die Höhe der Schrift)
        line = strtok(NULL, "\n");
    }

    free(mutable_text); // Speicher freigeben
}

// Funktion zum Abspielen von Sounds
void play_sound(float frequency, uint32_t duration_ms) {
    if(furi_hal_speaker_acquire(1000)) {
        furi_hal_speaker_start(frequency, 100);
        furi_delay_ms(duration_ms);
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

// Funktion zur Überprüfung, ob eine Taste gedrückt wurde
bool is_button_pressed(const GpioPin* button) {
    return furi_hal_gpio_read(button) == false; // Aktiv niedrig
}

// Funktion zur Auswahl der richtigen Richtung für das Reaktionsspiel
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

// Hauptfunktion der App
int32_t reaction_game_app(void* p) {
    int lifes = 3;
    uint32_t best_time = 9999;
    UNUSED(p);

    // Zufallszahlengenerator initialisieren
    srand(furi_get_tick());  // Seed für den Zufallsgenerator setzen

    // GUI und Viewport initialisieren
    ViewPort* viewport = view_port_alloc();
    gui_add_view_port(furi_record_open("gui"), viewport, GuiLayerFullscreen);

    // 1. Intro anzeigen und auf OK-Taste loslassen warten
    char intro_text[] = "Druecke OK zum Starten!"; // Erstelle eine Kopie des Textes
    view_port_draw_callback_set(viewport, draw_callback, intro_text);

    // Warten, bis die OK-Taste losgelassen wird
    while (is_button_pressed(&gpio_button_ok)) {
        furi_delay_ms(10); // Warte auf das Loslassen der OK-Taste
    }

    // Bestätigungston für das Starten des Spiels
    

    // 2. Jetzt das eigentliche Spiel starten
    char text[32];
    char reaction_text[128];  // Für die Anzeige der Reaktionszeit
    bool game_running = true;

    // Mögliche Richtungen
    const char* directions[] = {"Up", "Down", "Left", "Right"};
    const uint32_t num_directions = 4;
    
    while(game_running) {
        play_sound(NOTE_OK, 200);
        // 1. Zeige "Warte..." an und warte eine zufällige Zeit
        snprintf(text, sizeof(text), "\n\nWarte...");
        view_port_draw_callback_set(viewport, draw_callback, text);
        furi_delay_ms(500 + (furi_get_tick() % 1500)); // Zufällige Verzögerung zwischen 500-2000 ms

        // 2. Wähle zufällig eine Richtung aus
        const char* direction = directions[rand() % num_directions]; // Zufallsauswahl der Richtung
        snprintf(text, sizeof(text), "\n\nDruecke %s!", direction);
        view_port_draw_callback_set(viewport, draw_callback, text);

        // 3. Starte Zeitmessung
        uint32_t start_time = furi_get_tick();

        // 4. Warte auf die korrekte Taste (Up, Down, Left oder Right)
        bool reaction_captured = false; // Wird verwendet, um die Runde zu beenden
        
while(!reaction_captured) {
    const char* directions[] = {"Up", "Down", "Left", "Right"};
    const GpioPin* buttons[] = {&gpio_button_up, &gpio_button_down, &gpio_button_left, &gpio_button_right};

    for (int i = 0; i < 4; i++) {
        if (strcmp(direction, directions[i]) == 0 && is_button_pressed(buttons[i])) {
            uint32_t reaction_time = (furi_get_tick() - start_time) * 1000 / furi_kernel_get_tick_frequency();
            if (reaction_time < best_time && reaction_time > 50) {
                best_time = reaction_time;
                }
            snprintf(reaction_text, sizeof(reaction_text), "Deine Leben: %d\nReaktionszeit: %lu ms\nBeste Reaktionszeit:\n%lu ms", lifes,reaction_time,best_time);
            play_reaction_sound(directions[i]);
            reaction_captured = true;
            break; // Beende die Schleife, wenn die richtige Taste gedrückt wurde
        } else if (strcmp(direction, directions[i]) != 0 && is_button_pressed(buttons[i])) {
            lifes -=1;
            snprintf(reaction_text, sizeof(reaction_text), "Deine Leben: %d\nFalsche Taste gedrueckt\nBeste Reaktionszeit:\n%lu ms",lifes,best_time);
            reaction_captured = true;
            break; // Beende die Schleife, wenn eine falsche Taste gedrückt wurde
        }
    }

    if (lifes == -1) {
        snprintf(reaction_text, sizeof(reaction_text), "Alle Leben weg!\nBeste Reaktionszeit:\n%lu ms",best_time);
        game_running = false;
    }

    // Überwache die BACK-Taste, um das Spiel zu beenden
    if (is_button_pressed(&gpio_button_back)) { 
        game_running = false;
        reaction_captured = true;
    }

    furi_delay_ms(10); // Verzögerung für CPU-Entlastung
}

        // Zeige die Reaktionszeit oder den Fehlertext an
        view_port_draw_callback_set(viewport, draw_callback, reaction_text);

        // 5. Überwache die OK-Taste, um die Runde zu starten
        while (!is_button_pressed(&gpio_button_ok)) {
            furi_delay_ms(10); // Warte auf OK-Taste loslassen (keine Verzögerung nach der Anzeige)
        }

        // Bestätigungston für OK-Taste
        //play_sound(NOTE_OK, 200);

        // Warte auf das Loslassen der OK-Taste, bevor das Spiel fortgesetzt wird
        while (is_button_pressed(&gpio_button_ok)) {
            furi_delay_ms(10); // Warte, bis OK-Taste losgelassen wird
        }

        // Die Runde ist abgeschlossen und eine neue Runde kann beginnen
    }

    // Beende das Spiel und räume auf
    snprintf(text, sizeof(text), "Spiel beendet!");
    view_port_draw_callback_set(viewport, draw_callback, text);
    furi_delay_ms(2000);

    gui_remove_view_port(furi_record_open("gui"), viewport);
    view_port_free(viewport);
    furi_record_close("gui");
    return 0;
}
