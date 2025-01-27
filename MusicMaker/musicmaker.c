#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <furi_hal.h>
#include <stdlib.h>
#include <string.h>
#include <storage/storage.h>

// Enumeration für die Notenwerte und Pausen
typedef enum {
    NoteWhole, NoteHalf, NoteQuarter, NoteEighth, NoteSixteenth,
    RestWhole, RestHalf, RestQuarter, RestEighth, RestSixteenth
} NoteValue;

// Enumeration für den Anzeigemodus
typedef enum {
    ModeNotes, ModeMenu, ModeExit, ModePlay, ModeSave, ModeLoad
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
    char save_name[MAX_FILENAME_LENGTH];
    int save_name_length;
    int save_name_index;
    char* file_list[MAX_FILES];
    int total_files;
} NoteSheet;

// Menu options
const char* menu_options[] = {
    "1. Play", "2. Save", "3. Load", "4. New", "5. Exit"
};
#define MENU_OPTIONS_COUNT (sizeof(menu_options) / sizeof(menu_options[0]))

// Frequenzen für die Noten (B3 bis F5)
const float note_frequencies[] = {
    246.94, 261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 493.88, 523.25, 587.33, 659.25, 698.46
};

// Dauer für die Notenwerte (in Millisekunden)
const int note_durations[] = {
    1000, 500, 250, 125, 62
};

// Funktion zum Abspielen eines Tons
void play_sound(float frequency, uint32_t duration_ms) {
    if(furi_hal_speaker_acquire(1000)) {
        furi_hal_speaker_start(frequency, 1.0);
        furi_delay_ms(duration_ms);
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

// Funktion zum Abspielen eines kurzen Tons
void play_short_sound(Note* note) {
    int frequency_index = (43 - note->y_position) / 3;
    if(frequency_index >= 0 && frequency_index < 12) {
        play_sound(note_frequencies[frequency_index], 100);
    }
}

// Funktion zum Freigeben des Speichers
void free_memory(NoteSheet* sheet) {
    for(int i = 0; i < sheet->total_files; i++) {
        free(sheet->file_list[i]);
        sheet->file_list[i] = NULL;
    }
    sheet->total_files = 0;
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
            char buffer[4096];
            int read = storage_file_read(file, buffer, sizeof(buffer) - 1);
            if(read > 0) {
                buffer[read] = '\0';
                char* token = strtok(buffer, ";");
                while(token != NULL && sheet->total_notes < MAX_NOTES) {
                    int x_position, y_position, value;
                    if(sscanf(token, "%d,%d,%d", &x_position, &y_position, &value) == 3) {
                        Note* note = &sheet->notes[sheet->total_notes++];
                        note->x_position = x_position;
                        note->y_position = y_position;
                        note->value = (NoteValue)value;
                    }
                    token = strtok(NULL, ";");
                }
            }
            sheet->current_note_index = 0;
            storage_file_close(file);
        }
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
    }
}

// Funktion zum Auflisten der Dateien im Verzeichnis
void list_files(NoteSheet* sheet) {
    free_memory(sheet);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    sheet->total_files = 0;

    const char* dir = "/ext/apps_assets/musicmaker";

    if(storage_dir_open(file, dir)) {
        FileInfo file_info;
        char file_name[MAX_FILENAME_LENGTH];

        while(storage_dir_read(file, &file_info, file_name, sizeof(file_name))) {
            if(file_info.size > 0) {
                sheet->file_list[sheet->total_files++] = strdup(file_name);
            }
        }

        storage_dir_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// Funktion zum Erstellen eines neuen Notenblattes
void new_note_sheet(NoteSheet* sheet) {
    sheet->total_notes = 1;
    sheet->current_note_index = 0;
    sheet->scroll_offset = 0;
    sheet->notes[0].x_position = 10;
    sheet->notes[0].y_position = 40;
    sheet->notes[0].value = NoteWhole;
}

// Funktion zum Ändern des Tons
void change_note_value(Note* note, NoteValue new_value) {
    note->value = new_value;
    play_short_sound(note);
}

// Drawing the five lines on the canvas
void draw_music_lines(Canvas* canvas, void* ctx) {
    NoteSheet* sheet = (NoteSheet*)ctx;

    canvas_clear(canvas);

    if(sheet->mode == ModeMenu) {
        for(int i = 0; i < (int)MENU_OPTIONS_COUNT; i++) {
            if(i == sheet->menu_index) {
                canvas_draw_str(canvas, 10, 10 + i * 10, ">");
            }
            canvas_draw_str(canvas, 20, 10 + i * 10, menu_options[i]);
        }
    } else if(sheet->mode == ModeSave) {
        canvas_draw_str(canvas, 10, 10, "Save as:");
        canvas_draw_str(canvas, 10, 20, sheet->save_name);
        int mark_x = 10 + sheet->save_name_index * 6;
        canvas_draw_box(canvas, mark_x, 30, 6, 1);
    } else if(sheet->mode == ModeLoad) {
        canvas_draw_str(canvas, 10, 10, "Files:");
        for(int i = 0; i < sheet->total_files; i++) {
            if(i == sheet->menu_index) {
                canvas_draw_str(canvas, 10, 20 + i * 10, ">");
            }
            canvas_draw_str(canvas, 20, 20 + i * 10, sheet->file_list[i]);
        }
    } else {
        int start_y = 10;
        int line_spacing = 6;

        for(int i = 0; i < 5; i++) {
            canvas_draw_line(canvas, 0, start_y + i * line_spacing, 128, start_y + i * line_spacing);
        }

        for(int i = 0; i < sheet->total_notes; i++) {
            Note* note = &sheet->notes[i];
            int x_position = note->x_position - sheet->scroll_offset;

            if(x_position >= 0 && x_position < 128) {
                switch(note->value) {
                    case NoteWhole:
                        canvas_draw_circle(canvas, x_position, note->y_position, 3);
                        break;
                    case NoteHalf:
                        canvas_draw_circle(canvas, x_position, note->y_position, 3);
                        if(note->y_position <= 25) {
                            canvas_draw_line(canvas, x_position - 3, note->y_position, x_position - 3, note->y_position + 10);
                        } else {
                            canvas_draw_line(canvas, x_position + 3, note->y_position, x_position + 3, note->y_position - 10);
                        }
                        break;
                    case NoteQuarter:
                        canvas_draw_box(canvas, x_position - 3, note->y_position - 3, 6, 6);
                        if(note->y_position <= 25) {
                            canvas_draw_line(canvas, x_position - 3, note->y_position, x_position - 3, note->y_position + 10);
                        } else {
                            canvas_draw_line(canvas, x_position + 3, note->y_position, x_position + 3, note->y_position - 10);
                        }
                        break;
                    case NoteEighth:
                        canvas_draw_box(canvas, x_position - 3, note->y_position - 3, 6, 6);
                        if(note->y_position <= 25) {
                            canvas_draw_line(canvas, x_position - 3, note->y_position, x_position - 3, note->y_position + 10);
                            canvas_draw_line(canvas, x_position - 3, note->y_position + 10, x_position - 6, note->y_position + 13);
                        } else {
                            canvas_draw_line(canvas, x_position + 3, note->y_position, x_position + 3, note->y_position - 10);
                            canvas_draw_line(canvas, x_position + 3, note->y_position - 10, x_position + 6, note->y_position - 13);
                        }
                        break;
                    case NoteSixteenth:
                        canvas_draw_box(canvas, x_position - 3, note->y_position - 3, 6, 6);
                        if(note->y_position <= 25) {
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
                        canvas_draw_box(canvas, x_position - 3, start_y + 2 * line_spacing + 2, 6, 2);
                        break;
                    case RestHalf:
                        canvas_draw_box(canvas, x_position - 3, start_y + 2 * line_spacing - 2, 6, 2);
                        break;
                    case RestQuarter:
                        canvas_draw_box(canvas, x_position - 2, start_y + 2 * line_spacing - 2, 4, 4);
                        break;
                    case RestEighth:
                        canvas_draw_box(canvas, x_position - 2, start_y + 2 * line_spacing - 2, 4, 4);
                        canvas_draw_line(canvas, x_position, start_y + 2 * line_spacing - 2, x_position, start_y + 2 * line_spacing - 6);
                        break;
                    case RestSixteenth:
                        canvas_draw_box(canvas, x_position - 2, start_y + 2 * line_spacing - 2, 4, 4);
                        canvas_draw_line(canvas, x_position, start_y + 2 * line_spacing - 2, x_position, start_y + 2 * line_spacing - 6);
                        canvas_draw_line(canvas, x_position, start_y + 2 * line_spacing - 4, x_position - 2, start_y + 2 * line_spacing - 8);
                        break;
                    default:
                        break;
                }
            }
        }

        int current_x_position = sheet->notes[sheet->current_note_index].x_position - sheet->scroll_offset;
        if(current_x_position >= 0 && current_x_position < 128) {
            canvas_draw_circle(canvas, current_x_position, 58, 2);
        }

        char note_number[20];
        snprintf(note_number, sizeof(note_number), "Note: %d", sheet->current_note_index + 1);
        canvas_draw_str(canvas, 0, 64, note_number);
    }

    canvas_commit(canvas);
}

// Funktion zum Abspielen der Noten
void play_notes(NoteSheet* sheet) {
    for(int i = 0; i < sheet->total_notes; i++) {
        Note* note = &sheet->notes[i];
        int duration = note_durations[note->value % 5];

        if(note->value < RestWhole) {
            int frequency_index = (43 - note->y_position) / 3;
            if(frequency_index >= 0 && frequency_index < 12) {
                play_sound(note_frequencies[frequency_index], duration);
            }
        }

        furi_delay_ms(duration);
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

// Eingabeverarbeitung für die Pfeiltasten und die OK-Taste
void input_callback(InputEvent* input_event, void* ctx) {
    NoteSheet* sheet = (NoteSheet*)ctx;

    if(input_event->type == InputTypePress || input_event->type == InputTypeRepeat) {
        int line_spacing = 3;
        Note* current_note = &sheet->notes[sheet->current_note_index];

        if(sheet->mode == ModeMenu) {
            switch(input_event->key) {
                case InputKeyUp:
                    sheet->menu_index = (sheet->menu_index - 1 + MENU_OPTIONS_COUNT) % MENU_OPTIONS_COUNT;
                    break;
                case InputKeyDown:
                    sheet->menu_index = (sheet->menu_index + 1) % MENU_OPTIONS_COUNT;
                    break;
                case InputKeyOk:
                    switch(sheet->menu_index) {
                        case 0:
                            sheet->mode = ModePlay;
                            play_notes(sheet);
                            sheet->mode = ModeNotes;
                            break;
                        case 1:
                            sheet->mode = ModeSave;
                            sheet->save_name_index = 0;
                            sheet->save_name_length = 1;
                            sheet->save_name[0] = 'A';
                            sheet->save_name[1] = '\0';
                            break;
                        case 2:
                            sheet->mode = ModeLoad;
                            sheet->menu_index = 0;
                            list_files(sheet);
                            break;
                        case 3:
                            new_note_sheet(sheet);
                            sheet->mode = ModeNotes;
                            break;
                        case 4:
                            sheet->mode = ModeExit;
                            break;
                    }
                    break;
                case InputKeyBack:
                    sheet->mode = ModeNotes;
                    break;
                default:
                    break;
            }
        } else if(sheet->mode == ModeSave) {
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
                    save_notes(sheet);
                    sheet->mode = ModeNotes;
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
                    sheet->mode = ModeMenu;
                    break;
                default:
                    break;
            }
        } else if(sheet->mode == ModeLoad) {
            switch(input_event->key) {
                case InputKeyUp:
                    sheet->menu_index = (sheet->menu_index - 1 + sheet->total_files) % sheet->total_files;
                    break;
                case InputKeyDown:
                    sheet->menu_index = (sheet->menu_index + 1) % sheet->total_files;
                    break;
                case InputKeyOk:
                    load_notes(sheet);
                    sheet->mode = ModeNotes;
                    break;
                case InputKeyBack:
                    sheet->mode = ModeMenu;
                    break;
                default:
                    break;
            }
        } else if(sheet->mode == ModeNotes) {
            switch(input_event->key) {
                case InputKeyUp:
                    if(current_note->y_position > 10) {
                        current_note->y_position -= line_spacing;
                        play_short_sound(current_note);
                    }
                    break;
                case InputKeyDown:
                    if(current_note->y_position < 43) {
                        current_note->y_position += line_spacing;
                        play_short_sound(current_note);
                    } else {
                        current_note->value = (NoteValue)((int)current_note->value + RestWhole);
                        if(current_note->value > RestSixteenth) {
                            if(sheet->total_notes > 1) {
                                for(int i = sheet->current_note_index; i < sheet->total_notes - 1; i++) {
                                    sheet->notes[i] = sheet->notes[i + 1];
                                }
                                sheet->total_notes--;
                                if(sheet->current_note_index >= sheet->total_notes) {
                                    sheet->current_note_index = sheet->total_notes - 1;
                                }
                            } else {
                                current_note->y_position = 43;
                                current_note->value = NoteWhole;
                            }
                        }
                    }
                    break;
                case InputKeyOk:
                    change_note_value(current_note, (current_note->value + 1) % 5);
                    break;
                case InputKeyRight:
                    if(sheet->current_note_index < sheet->total_notes - 1) {
                        sheet->current_note_index++;
                    } else if(sheet->total_notes < MAX_NOTES) {
                        sheet->current_note_index++;
                        sheet->total_notes++;
                        sheet->notes[sheet->current_note_index] = sheet->notes[sheet->current_note_index - 1];
                        sheet->notes[sheet->current_note_index].x_position = sheet->current_note_index * 15 + 10;
                    }
                    if(sheet->notes[sheet->current_note_index].x_position - sheet->scroll_offset > 128) {
                        sheet->scroll_offset = sheet->notes[sheet->current_note_index].x_position - 128 + 10;
                    }
                    break;
                case InputKeyLeft:
                    if(sheet->current_note_index > 0) {
                        sheet->current_note_index--;
                        if(sheet->notes[sheet->current_note_index].x_position - sheet->scroll_offset < 0) {
                            sheet->scroll_offset = sheet->notes[sheet->current_note_index].x_position - 10;
                        }
                    }
                    break;
                case InputKeyBack:
                    sheet->mode = ModeMenu;
                    sheet->menu_index = 0;
                    break;
                default:
                    break;
            }
        }
    }
}

int32_t musicmaker_app(void) {
    ViewPort* view_port = view_port_alloc();
    NoteSheet sheet = { .current_note_index = 0, .total_notes = 1, .scroll_offset = 0, .mode = ModeNotes, .menu_index = 0, .total_files = 0 };

    sheet.notes[0].x_position = 10;
    sheet.notes[0].y_position = 40;
    sheet.notes[0].value = NoteWhole;

    view_port_draw_callback_set(view_port, draw_music_lines, &sheet);
    view_port_input_callback_set(view_port, input_callback, &sheet);

    Gui* gui = furi_record_open("gui");
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    while(sheet.mode != ModeExit) {
        view_port_update(view_port);
        furi_delay_ms(100);
    }

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close("gui");

    free_memory(&sheet);

    return 0;
}