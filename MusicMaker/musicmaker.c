#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <furi_hal.h>
#include <stdlib.h>

// Enumeration für die Notenwerte und Pausen
typedef enum {
    NoteWhole,    // Ganze Note
    NoteHalf,     // Halbe Note
    NoteQuarter,  // Viertelnote
    NoteEighth,   // Achtelnote
    NoteSixteenth, // Sechzehntelnote
    RestWhole,    // Ganze Pause
    RestHalf,     // Halbe Pause
    RestQuarter,  // Viertelpause
    RestEighth,   // Achtelpause
    RestSixteenth // Sechzehntelpause
} NoteValue;

// Enumeration für den Anzeigemodus
typedef enum {
    ModeNotes,
    ModeMenu,
    ModeExit,
    ModePlay  // Neuer Modus zum Abspielen
} DisplayMode;

// Struktur zur Verwaltung der Noten
typedef struct {
    int32_t x_position;
    int32_t y_position;
    NoteValue value;
} Note;

// Maximale Anzahl der Noten
#define MAX_NOTES 16

// Struktur zur Verwaltung des Notenblattes
typedef struct {
    Note notes[MAX_NOTES];
    int current_note_index;
    int total_notes;
    int scroll_offset;
    DisplayMode mode;
    int menu_index;
} NoteSheet;

// Menüoptionen
const char* menu_options[] = {
    "1. Abspielen",
    "2. Laden",
    "3. Speichern",
    "4. Beenden"
};
#define MENU_OPTIONS_COUNT (sizeof(menu_options) / sizeof(menu_options[0]))

// Frequenzen für die Noten (B3 bis F5)
const float note_frequencies[] = {
    246.94,  // B3 (H in deutsch)
    261.63,  // C4
    293.66,  // D4
    329.63,  // E4
    349.23,  // F4
    392.00,  // G4
    440.00,  // A4
    493.88,  // B4
    523.25,  // C5
    587.33,  // D5
    659.25,  // E5
    698.46   // F5
};

// Dauer für die Notenwerte (in Millisekunden)
const int note_durations[] = {
    1000, // Ganze Note
    500,  // Halbe Note
    250,  // Viertelnote
    125,  // Achtelnote
    62    // Sechzehntelnote
};

// Funktion zum Abspielen eines Tons
void play_sound(float frequency, uint32_t duration_ms) {
    if(furi_hal_speaker_acquire(1000)) {
        furi_hal_speaker_start(frequency, 100);
        furi_delay_ms(duration_ms);
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

// Zeichnen der fünf Linien auf dem Canvas
void draw_music_lines(Canvas* canvas, void* ctx) {
    NoteSheet* sheet = (NoteSheet*)ctx;

    canvas_clear(canvas);

    // Überprüfen, ob das Menü angezeigt werden soll
    if(sheet->mode == ModeMenu) {
        // Zeichnen des Menüs
        for(int i = 0; i < (int)MENU_OPTIONS_COUNT; i++) {
            if(i == sheet->menu_index) {
                // Markiere die aktuelle Auswahl
                canvas_draw_str(canvas, 10, 10 + i * 10, ">");
            }
            canvas_draw_str(canvas, 20, 10 + i * 10, menu_options[i]);
        }
    } else {
        // Zentrierte Position und kleinerer Abstand zwischen den Linien
        int start_y = 10; // Startposition der ersten Linie
        int line_spacing = 6; // Abstand zwischen den Linien

        // Zeichnen der fünf Linien
        for(int i = 0; i < 5; i++) {
            canvas_draw_line(canvas, 0, start_y + i * line_spacing, 128, start_y + i * line_spacing);
        }

        // Zeichnen der Noten
        for(int i = 0; i < sheet->total_notes; i++) {
            Note* note = &sheet->notes[i];
            int x_position = note->x_position - sheet->scroll_offset;

            if(x_position >= 0 && x_position < 128) { // Zeichne nur sichtbare Noten
                switch(note->value) {
                    case NoteWhole:
                        // Ganze Note: Unausgefüllter Kreis
                        canvas_draw_circle(canvas, x_position, note->y_position, 3);
                        break;
                    case NoteHalf:
                        // Halbe Note: Unausgefüllter Kreis mit Notenstrich
                        canvas_draw_circle(canvas, x_position, note->y_position, 3);
                        canvas_draw_line(canvas, x_position + 3, note->y_position, x_position + 3, note->y_position - 10);
                        break;
                    case NoteQuarter:
                        // Viertelnote: Ausgemalter Kreis mit Notenstrich
                        canvas_draw_box(canvas, x_position - 3, note->y_position - 3, 6, 6);
                        canvas_draw_line(canvas, x_position + 3, note->y_position, x_position + 3, note->y_position - 10);
                        break;
                    case NoteEighth:
                        // Achtelnote: Ausgemalter Kreis mit Notenstrich und Häckchen
                        canvas_draw_box(canvas, x_position - 3, note->y_position - 3, 6, 6);
                        canvas_draw_line(canvas, x_position + 3, note->y_position, x_position + 3, note->y_position - 10);
                        canvas_draw_line(canvas, x_position + 3, note->y_position - 10, x_position + 6, note->y_position - 13);
                        break;
                    case NoteSixteenth:
                        // Sechzehntelnote: Ausgemalter Kreis mit Notenstrich und zwei Häckchen
                        canvas_draw_box(canvas, x_position - 3, note->y_position - 3, 6, 6);
                        canvas_draw_line(canvas, x_position + 3, note->y_position, x_position + 3, note->y_position - 10);
                        canvas_draw_line(canvas, x_position + 3, note->y_position - 10, x_position + 6, note->y_position - 13);
                        canvas_draw_line(canvas, x_position + 3, note->y_position - 7, x_position + 6, note->y_position - 10);
                        break;
                    case RestWhole:
                        // Ganze Pause: Rechteck unter der dritten Linie
                        canvas_draw_box(canvas, x_position - 3, start_y + 2 * line_spacing + 2, 6, 2);
                        break;
                    case RestHalf:
                        // Halbe Pause: Rechteck über der dritten Linie
                        canvas_draw_box(canvas, x_position - 3, start_y + 2 * line_spacing - 2, 6, 2);
                        break;
                    case RestQuarter:
                        // Viertelpause: Gefülltes Rechteck
                        canvas_draw_box(canvas, x_position - 2, start_y + 2 * line_spacing - 2, 4, 4);
                        break;
                    case RestEighth:
                        // Achtelpause: Gefülltes Rechteck mit Strich
                        canvas_draw_box(canvas, x_position - 2, start_y + 2 * line_spacing - 2, 4, 4);
                        canvas_draw_line(canvas, x_position, start_y + 2 * line_spacing - 2, x_position, start_y + 2 * line_spacing - 6);
                        break;
                    case RestSixteenth:
                        // Sechzehntelpause: Gefülltes Rechteck mit zwei Strichen
                        canvas_draw_box(canvas, x_position - 2, start_y + 2 * line_spacing - 2, 4, 4);
                        canvas_draw_line(canvas, x_position, start_y + 2 * line_spacing - 2, x_position, start_y + 2 * line_spacing - 6);
                        canvas_draw_line(canvas, x_position, start_y + 2 * line_spacing - 4, x_position - 2, start_y + 2 * line_spacing - 8);
                        break;
                    default:
                        break;
                }
            }
        }

        // Zeichnen der Notennummer unten
        char note_number[20]; // Puffergröße erhöht
        snprintf(note_number, sizeof(note_number), "Note: %d", sheet->current_note_index + 1);
        canvas_draw_str(canvas, 0, 64, note_number);
    }

    canvas_commit(canvas);
}

// Funktion zum Abspielen der Noten
void play_notes(NoteSheet* sheet) {
    for(int i = 0; i < sheet->total_notes; i++) {
        Note* note = &sheet->notes[i];
        int duration = note_durations[note->value % 5]; // Bestimme die Dauer der Note

        if(note->value < RestWhole) { // Wenn es keine Pause ist
            // Bestimme die Frequenz basierend auf der y-Position
            int frequency_index;
            switch(note->y_position) {
                case 43: frequency_index = 0; break; // B3 (H)
                case 40: frequency_index = 1; break; // C4
                case 37: frequency_index = 2; break; // D4
                case 34: frequency_index = 3; break; // E4
                case 31: frequency_index = 4; break; // F4
                case 28: frequency_index = 5; break; // G4
                case 25: frequency_index = 6; break; // A4
                case 22: frequency_index = 7; break; // B4
                case 19: frequency_index = 8; break; // C5
                case 16: frequency_index = 9; break; // D5
                case 13: frequency_index = 10; break; // E5
                case 10: frequency_index = 11; break; // F5
                default: frequency_index = -1; break; // Unbekannte Position
            }

            if(frequency_index >= 0 && frequency_index < 12) {
                float frequency = note_frequencies[frequency_index];
                play_sound(frequency, duration); // Spiele den Ton
            }
        }

        furi_delay_ms(duration); // Warte für die Dauer der Note
    }
}

// Eingabeverarbeitung für die Pfeiltasten und die OK-Taste
void input_callback(InputEvent* input_event, void* ctx) {
    NoteSheet* sheet = (NoteSheet*)ctx;

    if(input_event->type == InputTypePress || input_event->type == InputTypeRepeat) {
        int line_spacing = 3; // Kleinere Schritte für die Note (halbe Linie)
        Note* current_note = &sheet->notes[sheet->current_note_index];

        if(sheet->mode == ModeMenu) {
            // Menüeingaben verarbeiten
            switch(input_event->key) {
                case InputKeyUp:
                    sheet->menu_index = (sheet->menu_index - 1 + MENU_OPTIONS_COUNT) % MENU_OPTIONS_COUNT;
                    break;
                case InputKeyDown:
                    sheet->menu_index = (sheet->menu_index + 1) % MENU_OPTIONS_COUNT;
                    break;
                case InputKeyOk:
                    switch(sheet->menu_index) {
                        case 0: // Abspielen
                            sheet->mode = ModePlay;
                            play_notes(sheet);
                            sheet->mode = ModeNotes; // Zurück zum Notenmodus nach dem Abspielen
                            break;
                        case 1: // Laden
                            // Implementiere Laden-Funktionalität
                            break;
                        case 2: // Speichern
                            // Implementiere Speichern-Funktionalität
                            break;
                        case 3: // Beenden
                            sheet->mode = ModeExit; // Setze den Modus auf Beenden
                            break;
                    }
                    break;
                case InputKeyBack:
                    sheet->mode = ModeNotes; // Zurück zum Notenmodus
                    break;
                default:
                    break;
            }
        } else if(sheet->mode == ModeNotes) {
            // Noteneingaben verarbeiten
            switch(input_event->key) {
                case InputKeyUp:
                    if(current_note->y_position > 10) { // Oberes Limit (höchstes F5)
                        current_note->y_position -= line_spacing;
                    }
                    break;
                case InputKeyDown:
                    if(current_note->y_position < 43) { // Unteres Limit (B3)
                        current_note->y_position += line_spacing;
                    } else {
                        // Note auf unteres Limit erreicht, füge Pause hinzu oder lösche Note
                        current_note->value = (NoteValue)((int)current_note->value + RestWhole);
                        if(current_note->value > RestSixteenth) {
                            // Lösche Note, wenn sie über Sechzehntelpause hinausgeht
                            if(sheet->total_notes > 1) { // Sicherstellen, dass mindestens eine Note bleibt
                                for(int i = sheet->current_note_index; i < sheet->total_notes - 1; i++) {
                                    sheet->notes[i] = sheet->notes[i + 1];
                                }
                                sheet->total_notes--;
                                if(sheet->current_note_index >= sheet->total_notes) {
                                    sheet->current_note_index = sheet->total_notes - 1;
                                }
                            } else {
                                // Setze die Note zurück auf das untere B (H)
                                current_note->y_position = 43;
                                current_note->value = NoteWhole;
                            }
                        }
                    }
                    break;
                case InputKeyOk:
                    // Zyklisches Ändern des Notenwerts
                    current_note->value = (current_note->value + 1) % 5;
                    break;
                case InputKeyRight:
                    if(sheet->current_note_index < sheet->total_notes - 1) {
                        // Zur nächsten Note wechseln
                        sheet->current_note_index++;
                    } else if(sheet->total_notes < MAX_NOTES) {
                        // Neue Note hinzufügen
                        sheet->current_note_index++;
                        sheet->total_notes++;
                        // Kopiere die vorherige Note anstatt die Standard-Startposition zu setzen
                        sheet->notes[sheet->current_note_index] = sheet->notes[sheet->current_note_index - 1];
                        sheet->notes[sheet->current_note_index].x_position = sheet->current_note_index * 15 + 10; // Position der neuen Note, Abstand verkleinert
                    }
                    // Scrollen, wenn die Note außerhalb des sichtbaren Bereichs liegt
                    if(sheet->notes[sheet->current_note_index].x_position - sheet->scroll_offset > 128) {
                        sheet->scroll_offset = sheet->notes[sheet->current_note_index].x_position - 128 + 10;
                    }
                    break;
                case InputKeyLeft:
                    if(sheet->current_note_index > 0) {
                        // Zur vorherigen Note wechseln
                        sheet->current_note_index--;
                        // Scrollen, wenn die Note außerhalb des sichtbaren Bereichs liegt
                        if(sheet->notes[sheet->current_note_index].x_position - sheet->scroll_offset < 0) {
                            sheet->scroll_offset = sheet->notes[sheet->current_note_index].x_position - 10;
                        }
                    }
                    break;
                case InputKeyBack:
                    sheet->mode = ModeMenu; // Wechsel zum Menümodus
                    sheet->menu_index = 0; // Start bei der ersten Menüoption
                    break;
                default:
                    break;
            }
        }
    }
}

int32_t musicmaker_app(void) {
    // Initialisieren des ViewPorts
    ViewPort* view_port = view_port_alloc();
    NoteSheet sheet = { .current_note_index = 0, .total_notes = 1, .scroll_offset = 0, .mode = ModeNotes, .menu_index = 0 }; // Start mit einer Note

    // Initialisieren der Startnote
    sheet.notes[0].x_position = 10; // Startposition weiter links
    sheet.notes[0].y_position = 40;
    sheet.notes[0].value = NoteWhole;

    view_port_draw_callback_set(view_port, draw_music_lines, &sheet);
    view_port_input_callback_set(view_port, input_callback, &sheet);

    Gui* gui = furi_record_open("gui");
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Hauptschleife
    while(sheet.mode != ModeExit) {
        view_port_update(view_port);
        furi_delay_ms(100);
    }

    // Aufräumen
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close("gui");

    return 0;
}