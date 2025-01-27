#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <furi_hal.h>
#include <stdlib.h>
#include <string.h>
#include <storage/storage.h>

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
    ModePlay,
    ModeSave,
    ModeLoad  // Neuer Modus zum Laden
} DisplayMode;

// Struktur zur Verwaltung der Noten
typedef struct {
    int32_t x_position;
    int32_t y_position;
    NoteValue value;
} Note;

// Maximale Anzahl der Noten
#define MAX_NOTES 128
#define MAX_FILENAME_LENGTH 32
#define MAX_FILES 32

// Struktur zur Verwaltung des Notenblattes
typedef struct {
    Note notes[MAX_NOTES];
    int current_note_index;
    int total_notes;
    int scroll_offset;
    DisplayMode mode;
    int menu_index;
    char save_name[MAX_FILENAME_LENGTH];  // Dateiname (dynamische Länge)
    int save_name_length;
    int save_name_index;
    char* file_list[MAX_FILES]; // Array der Dateinamen
    int total_files;
} NoteSheet;

// Menu options
const char* menu_options[] = {
    "1. Play",
    "2. Load",
    "3. Save",
    "4. Exit"
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

// Funktion zum Auflisten der Dateien im Verzeichnis
void list_files(NoteSheet* sheet) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    sheet->total_files = 0;

    const char* dir = "/ext/apps_assets/musicmaker";

    if(storage_dir_open(file, dir)) {
        FileInfo file_info;
        char file_name[MAX_FILENAME_LENGTH];

        while(storage_dir_read(file, &file_info, file_name, sizeof(file_name))) {
            if(file_info.size > 0) {
                sheet->file_list[sheet->total_files] = strdup(file_name);
                sheet->total_files++;
            }
        }

        storage_dir_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// Drawing the five lines on the canvas
void draw_music_lines(Canvas* canvas, void* ctx) {
    NoteSheet* sheet = (NoteSheet*)ctx;

    canvas_clear(canvas);

    // Check if the menu, save mode, or load mode should be displayed
    if(sheet->mode == ModeMenu) {
        // Draw the menu
        for(int i = 0; i < (int)MENU_OPTIONS_COUNT; i++) {
            if(i == sheet->menu_index) {
                // Highlight the current selection
                canvas_draw_str(canvas, 10, 10 + i * 10, ">");
            }
            canvas_draw_str(canvas, 20, 10 + i * 10, menu_options[i]);
        }
    } else if(sheet->mode == ModeSave) {
        // Draw the save mode
        canvas_draw_str(canvas, 10, 10, "Save as:");
        canvas_draw_str(canvas, 10, 20, sheet->save_name);
        // Highlight the current character
        int mark_x = 10 + sheet->save_name_index * 6;
        canvas_draw_box(canvas, mark_x, 30, 6, 1);
    } else if(sheet->mode == ModeLoad) {
        // Draw the load menu
        canvas_draw_str(canvas, 10, 10, "Files:");
        for(int i = 0; i < sheet->total_files; i++) {
            if(i == sheet->menu_index) {
                // Highlight the current selection
                canvas_draw_str(canvas, 10, 20 + i * 10, ">");
            }
            canvas_draw_str(canvas, 20, 20 + i * 10, sheet->file_list[i]);
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
                        if (note->y_position <= 25) { // Noten B (H) und höher, Strich nach unten
                            canvas_draw_line(canvas, x_position - 3, note->y_position, x_position - 3, note->y_position + 10);
                        } else {
                            canvas_draw_line(canvas, x_position + 3, note->y_position, x_position + 3, note->y_position - 10);
                        }
                        break;
                    case NoteQuarter:
                        // Viertelnote: Ausgemalter Kreis mit Notenstrich
                        canvas_draw_box(canvas, x_position - 3, note->y_position - 3, 6, 6);
                        if (note->y_position <= 25) { // Noten B (H) und höher, Strich nach unten
                            canvas_draw_line(canvas, x_position - 3, note->y_position, x_position - 3, note->y_position + 10);
                        } else {
                            canvas_draw_line(canvas, x_position + 3, note->y_position, x_position + 3, note->y_position - 10);
                        }
                        break;
                    case NoteEighth:
                        // Achtelnote: Ausgemalter Kreis mit Notenstrich und Häckchen
                        canvas_draw_box(canvas, x_position - 3, note->y_position - 3, 6, 6);
                        if (note->y_position <= 25) { // Noten B (H) und höher, Strich und Häckchen nach unten
                            canvas_draw_line(canvas, x_position - 3, note->y_position, x_position - 3, note->y_position + 10);
                            canvas_draw_line(canvas, x_position - 3, note->y_position + 10, x_position - 6, note->y_position + 13);
                        } else {
                            canvas_draw_line(canvas, x_position + 3, note->y_position, x_position + 3, note->y_position - 10);
                            canvas_draw_line(canvas, x_position + 3, note->y_position - 10, x_position + 6, note->y_position - 13);
                        }
                        break;
                    case NoteSixteenth:
                        // Sechzehntelnote: Ausgemalter Kreis mit Notenstrich und zwei Häckchen
                        canvas_draw_box(canvas, x_position - 3, note->y_position - 3, 6, 6);
                        if (note->y_position <= 25) { // Noten B (H) und höher, Strich und Häckchen nach unten
                            canvas_draw_line(canvas, x_position - 3, note->y_position, x_position - 3, note->y_position + 10);
                            canvas_draw_line(canvas, x_position - 3, note->y_position + 10, x_position - 6, note->y_position + 13);
                            canvas_draw_line(canvas, x_position - 3, note->y_position + 7, x_position - 6, note->y_position + 10);
                        } else {
                            canvas_draw_line(canvas, x_position + 3, note->y_position, x_position + 3, note->y_position - 10);
                            canvas_draw_line(canvas, x_position + 3, note->y_position - 10, x_position + 6, note->y_position - 13);
                            canvas_draw_line(canvas, x_position + 3, note->y_position - 7, x_position + 6, note->y_position - 10);
                        }
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

        // Zeichnen des Punkts für die aktuelle Note
        int current_x_position = sheet->notes[sheet->current_note_index].x_position - sheet->scroll_offset;
        if(current_x_position >= 0 && current_x_position < 128) {
            canvas_draw_circle(canvas, current_x_position, 58, 2); // Punkt ganz unten auf dem Bildschirm (z.B. bei Y=58)
        }

        // Draw the note number at the bottom
        char note_number[20]; // Increased buffer size
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
// Funktion zum Speichern der Noten in eine Datei
void save_notes(NoteSheet* sheet) {
    const char* dir = "/ext/apps_assets/musicmaker";
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage) {
        char path[128];
        snprintf(path, sizeof(path), "%s/%s.txt", dir, sheet->save_name);

        File* file = storage_file_alloc(storage);
        if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            for(int i = 0; i < sheet->total_notes; i++) {
                Note* note = &sheet->notes[i];
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "%ld,%ld,%d;", note->x_position, note->y_position, note->value);
                storage_file_write(file, buffer, strlen(buffer));
            }
            storage_file_close(file);
        }
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
    }
}

// Funktion zum Laden der Noten aus einer Datei
void load_notes(NoteSheet* sheet) {
    const char* dir = "/ext/apps_assets/musicmaker";
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage) {
        char path[128];
        snprintf(path, sizeof(path), "%s/%s", dir, sheet->file_list[sheet->menu_index]);

        File* file = storage_file_alloc(storage);
        if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            sheet->total_notes = 0;
            char buffer[4096]; // Großer Puffer für die gesamte Datei
            int read = storage_file_read(file, buffer, sizeof(buffer) - 1);
            if(read > 0) {
                buffer[read] = '\0';
                char* token = strtok(buffer, ";");
                while(token != NULL && sheet->total_notes < MAX_NOTES) {
                    int x_position, y_position, value;
                    if(sscanf(token, "%d,%d,%d", &x_position, &y_position, &value) == 3) {
                        Note* note = &sheet->notes[sheet->total_notes];
                        note->x_position = x_position;
                        note->y_position = y_position;
                        note->value = (NoteValue)value;
                        sheet->total_notes++;
                    }
                    token = strtok(NULL, ";");
                }
            }
            sheet->current_note_index = 0; // Setze den aktuellen Notenindex auf die erste Note
            storage_file_close(file);
        }
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
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
                            sheet->mode = ModeLoad;
                            sheet->menu_index = 0;
                            list_files(sheet); // Dateien auflisten
                            break;
                        case 2: // Speichern
                            sheet->mode = ModeSave;
                            sheet->save_name_index = 0;
                            sheet->save_name_length = 1;
                            sheet->save_name[0] = 'A';
                            sheet->save_name[1] = '\0';
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
        } else if(sheet->mode == ModeSave) {
            // Eingaben für den Speichermodus verarbeiten
            switch(input_event->key) {
                case InputKeyUp:
                    if(sheet->save_name[sheet->save_name_index] == 'A') {
                        sheet->save_name[sheet->save_name_index] = 'Z';
                    } else {
                        sheet->save_name[sheet->save_name_index]--;
                    }
                    break;
                case InputKeyDown:
                    if(sheet->save_name[sheet->save_name_index] == 'Z') {
                        sheet->save_name[sheet->save_name_index] = 'A';
                    } else {
                        sheet->save_name[sheet->save_name_index]++;
                    }
                    break;
                case InputKeyOk:
                    save_notes(sheet); // Speichern der Noten
                    sheet->mode = ModeNotes; // Zurück zum Notenmodus
                    break;
                case InputKeyRight:
                    if(sheet->save_name_length < MAX_FILENAME_LENGTH - 1) {
                        sheet->save_name_index++;
                        sheet->save_name[sheet->save_name_index] = 'A';
                        sheet->save_name_length++;
                        sheet->save_name[sheet->save_name_length] = '\0';
                    }
                    break;
                case InputKeyBack:
                    sheet->mode = ModeMenu; // Zurück zum Menümodus
                    break;
                default:
                    break;
            }
        } else if(sheet->mode == ModeLoad) {
            // Eingaben für den Lademodus verarbeiten
            switch(input_event->key) {
                case InputKeyUp:
                    sheet->menu_index = (sheet->menu_index - 1 + sheet->total_files) % sheet->total_files;
                    break;
                case InputKeyDown:
                    sheet->menu_index = (sheet->menu_index + 1) % sheet->total_files;
                    break;
                case InputKeyOk:
                    load_notes(sheet); // Laden der Noten
                    sheet->mode = ModeNotes; // Zurück zum Notenmodus
                    break;
                case InputKeyBack:
                    sheet->mode = ModeMenu; // Zurück zum Menümodus
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
    NoteSheet sheet = { .current_note_index = 0, .total_notes = 1, .scroll_offset = 0, .mode = ModeNotes, .menu_index = 0, .total_files = 0 }; // Start mit einer Note

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

    // Speicher freigeben
    for(int i = 0; i < sheet.total_files; i++) {
        free(sheet.file_list[i]);
    }

    return 0;
}