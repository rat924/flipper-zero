#include <furi.h>
#include <furi_hal_random.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <stdbool.h>

#define PASSGEN_MAX_LENGTH 17
#define MENU_OPTION_COUNT 3  // Die Anzahl der Optionen im Menü
#define MAX_FILENAME_LENGTH 64 // Maximale Länge für Dateinamen
#define AES_KEY_SIZE 16  // AES-128 benötigt 16 Byte

typedef enum {
    StateMenu,
    StateEnterFilename,
    StateGeneratePassword,
    StateSelectFile,
    StateDisplayPassword,
    StateExit,  // Zustand für das Beenden der App
} AppState;

typedef struct {
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    FuriMutex* mutex;
    AppState state;
    char filename[MAX_FILENAME_LENGTH];  // Dateiname
    char password[PASSGEN_MAX_LENGTH + 1];
    int menu_option;
    char** file_list;  // Dynamische Liste der Dateinamen
    int file_count;                 // Anzahl der Dateien
    int selected_file;              // Index der ausgewählten Datei
    int char_set_index[MAX_FILENAME_LENGTH]; // Indizes für den Zeichensatz
} App;

static const char charsets[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!$%&-";
static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

#define AES_KEY_SIZE 16  // AES-128 erfordert 16 Bytes

// XOR Verschlüsselung / Entschlüsselung
void xor_encrypt_decrypt(const unsigned char* input, unsigned char* output, const unsigned char* key, size_t length) {
    for (size_t i = 0; i < length; i++) {
        output[i] = input[i] ^ key[i % AES_KEY_SIZE]; // XOR mit dem Schlüssel
    }
}

// Generiere einen zufälligen Schlüssel für die Verschlüsselung
void generate_random_key(unsigned char* key, size_t key_size) {
    for (size_t i = 0; i < key_size; i++) {
        key[i] = rand() % 256; // Zufallszahlen von 0 bis 255
    }
}

// Passwort in eine Datei speichern (verschlüsselt mit XOR)
void save_password_to_file(const char* filename, const char* password) {
    // Speicherort für Datei
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    const char* dir = "/ext/apps_assets/pwgen";  // Der Pfad zum Verzeichnis
    char full_path[MAX_FILENAME_LENGTH + strlen(dir) + 2];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, filename);

    // XOR Schlüssel generieren
    unsigned char key[AES_KEY_SIZE];
    generate_random_key(key, AES_KEY_SIZE);  // Schlüssel zufällig generieren

    // Verschlüsseltes Passwort speichern
    unsigned char encrypted_password[PASSGEN_MAX_LENGTH];
    xor_encrypt_decrypt((unsigned char*)password, encrypted_password, key, PASSGEN_MAX_LENGTH);  // Verschlüsseln des Passworts

    if (storage_file_open(file, full_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        // Hier kannst du den Schlüssel und das verschlüsselte Passwort speichern
        storage_file_write(file, key, AES_KEY_SIZE);  // Schlüssel speichern
        storage_file_write(file, encrypted_password, sizeof(encrypted_password));  // Verschlüsseltes Passwort speichern
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// Passwort aus einer Datei laden (entschlüsselt mit XOR)
bool load_password_from_file(const char* filename, char* password, size_t max_length) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool success = false;

    const char* dir = "/ext/apps_assets/pwgen";  // Der Pfad zum Verzeichnis
    char full_path[MAX_FILENAME_LENGTH + strlen(dir) + 2];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, filename);

    if (storage_file_open(file, full_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        // Lese den Schlüssel und das verschlüsselte Passwort
        unsigned char key[AES_KEY_SIZE];
        ssize_t read_bytes = storage_file_read(file, key, AES_KEY_SIZE);  // Schlüssel lesen
        if (read_bytes == AES_KEY_SIZE) {
            unsigned char encrypted_password[PASSGEN_MAX_LENGTH];
            read_bytes = storage_file_read(file, encrypted_password, sizeof(encrypted_password));  // Verschlüsseltes Passwort lesen
            if (read_bytes > 0) {
                unsigned char decrypted_password[PASSGEN_MAX_LENGTH];
                xor_encrypt_decrypt(encrypted_password, decrypted_password, key, PASSGEN_MAX_LENGTH);  // Entschlüsseln des Passworts

                // Null-terminierung hinzufügen, damit es als string verwendet werden kann
                decrypted_password[PASSGEN_MAX_LENGTH - 1] = '\0';  // Sicherstellen, dass es nullterminiert ist
                strncpy(password, (char*)decrypted_password, max_length - 1);  // Passwort in Puffer kopieren
                password[max_length - 1] = '\0';  // Null-terminieren
                success = true;
            }
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

// Funktion zum Erzeugen eines zufälligen Passworts
void generate_password(char* password, int length) {
    size_t charsets_size = strlen(charsets);

    for(int i = 0; i < length; i++) {
        password[i] = charsets[furi_hal_random_get() % charsets_size];
    }
    password[length] = '\0';
}

void display_password(const char* password) {
    // Hier wird das Passwort angezeigt
    printf("Password: %s\n", password);  // Zum Testen auf der Konsole
    // In einer echten Anwendung könntest du es auch auf dem Bildschirm anzeigen
}

#include <string.h>

// Einfacher Bubble Sort zum Sortieren der Dateinamen
void bubble_sort(char** list, int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (strcmp(list[j], list[j + 1]) > 0) {
                // Swap the pointers
                char* temp = list[j];
                list[j] = list[j + 1];
                list[j + 1] = temp;
            }
        }
    }
}

// Liste der Dateien im Verzeichnis laden und sortieren
void load_file_list(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    app->file_count = 0;

    // Verzeichnis /ext/apps_assets/pwgen durchsuchen
    const char* dir = "/ext/apps_assets/pwgen";  // Der Pfad zum Verzeichnis

    if(storage_dir_open(file, dir)) {  // Verzeichnis öffnen
        FileInfo file_info;  // Verwende die FileInfo-Struktur der SDK
        char file_name[MAX_FILENAME_LENGTH];  // Dateiname-Puffer

        // Verzeichnisinhalt lesen
        while(storage_dir_read(file, &file_info, file_name, sizeof(file_name))) {
            // Überprüfen, ob es sich um eine reguläre Datei handelt
            if(file_info.size > 0) {  // Falls die Datei eine Größe hat, ist es eine reguläre Datei
                // Datei gefunden, zur Liste hinzufügen
                app->file_list = realloc(app->file_list, (app->file_count + 1) * sizeof(char*)); // Speicher für die neue Datei reservieren
                app->file_list[app->file_count] = strdup(file_name);  // Dateinamen kopieren
                app->file_count++;
            }
        }

        // Jetzt sortieren wir die Liste der Dateinamen mit Bubble Sort
        if (app->file_count > 1) {
            bubble_sort(app->file_list, app->file_count);
        }

        storage_dir_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}



// Zeichen hinzufügen
void add_character_to_filename(App* app, char character) {
    int len = strlen(app->filename);
    if (len < MAX_FILENAME_LENGTH - 1) { // Sicherstellen, dass der Dateiname nicht die Maximalgröße überschreitet
        app->filename[len] = character;
        app->filename[len + 1] = '\0';
    }
}

// Index für das nächste Zeichen (Zyklen durch den Zeichensatz)
void change_character(App* app, bool forward) {
    int len = strlen(app->filename);
    if (len == 0) {
        return;  // Keine Änderungen, wenn der Dateiname leer ist
    }

    int index = len - 1;
    int current_index = app->char_set_index[index];

    if (forward) {
        // Vorwärts durch den Zeichensatz (nach dem letzten Zeichen geht es zurück zu 'A')
        current_index = (current_index + 1) % strlen(charset);
    } else {
        // Rückwärts durch den Zeichensatz (vor dem ersten Zeichen geht es zurück zu '9')
        current_index = (current_index - 1 + strlen(charset)) % strlen(charset);
    }

    app->char_set_index[index] = current_index;  // Aktualisiere den Index des Zeichens
    app->filename[index] = charset[current_index]; // Setze das Zeichen im Dateinamen
}

// Funktion für Hoch-/Runterscrollen
void scroll_character(App* app, bool up) {
    int len = strlen(app->filename);
    if (len == 0) {
        return;  // Keine Änderungen, wenn der Dateiname leer ist
    }

    int index = len - 1;
    int current_index = app->char_set_index[index];

    if (up) {
        // Runter-Taste (von A -> B -> C -> ... -> 9, dann wieder A)
        current_index = (current_index + 1) % strlen(charset);
    } else {
        // Hoch-Taste (von 9 -> 0 -> Z -> Y -> ... zurück zu A)
        current_index = (current_index - 1 + strlen(charset)) % strlen(charset);
    }

    app->char_set_index[index] = current_index;  // Aktualisiere den Index des Zeichens
    app->filename[index] = charset[current_index]; // Setze das Zeichen im Dateinamen
}

void render_callback(Canvas* canvas, void* ctx) {
    App* app = ctx;
    canvas_clear(canvas);

    switch(app->state) {
    case StateMenu:
        canvas_draw_str(canvas, 2, 10, "Menu:");
        canvas_draw_str(canvas, 10, 25, app->menu_option == 0 ? "> Neues Passwort" : "  Neues Passwort");
        canvas_draw_str(canvas, 10, 40, app->menu_option == 1 ? "> Passwort anzeigen" : "  Passwort anzeigen");
        canvas_draw_str(canvas, 10, 55, app->menu_option == 2 ? "> Beenden" : "  Beenden");
        break;

    case StateEnterFilename:
        canvas_draw_str(canvas, 2, 10, "Enter Filename:");
        canvas_draw_str(canvas, 2, 25, app->filename);  // Zeige den aktuellen Dateinamen
        break;

    case StateGeneratePassword:
        canvas_draw_str(canvas, 2, 10, "Passwort generiert:");
        canvas_draw_str(canvas, 2, 25, app->password);
        break;

case StateSelectFile:
    if (app->file_count > 0) {
        int y_position = 25;  // Startposition für die Dateiliste
        int max_visible_files = 3; // Maximale Anzahl an sichtbaren Dateien

        // Zeige die aktuellen Dateien basierend auf dem Index und der maximalen Anzahl an sichtbaren Dateien
        for (int i = 0; i < max_visible_files && i + app->selected_file < app->file_count; i++) {
            int file_index = i + app->selected_file;
            bool is_selected = file_index == app->selected_file;
            // Helle die aktuell ausgewählte Datei hervor
            canvas_draw_str(canvas, 10, y_position + (i * 15), is_selected ? "> " : "  ");
            canvas_draw_str(canvas, 20, y_position + (i * 15), app->file_list[file_index]);
        }

        // Scroll-Indikator anzeigen, wenn es mehr Dateien gibt
        if (app->selected_file > 0) {
            canvas_draw_str(canvas, 10, y_position + (max_visible_files * 15), "< Zurück");
        }
        if (app->selected_file < app->file_count - 1) {
            canvas_draw_str(canvas, 10, y_position + ((max_visible_files + 1) * 15), "Weiter >");
        }
    } else {
        canvas_draw_str(canvas, 2, 10, "Keine Dateien gefunden");
    }
    break;

case StateDisplayPassword:
    // Anzeigen der Dateiinformationen (Dateiname und Passwort)
    if (app->file_count > 0) {
        // Zeige den Dateinamen an
        canvas_draw_str(canvas, 2, 25, app->file_list[app->selected_file]);

        // Zeige die aktuelle Seitenzahl (z.B. 1/2) an
        char page_info[16];  // Puffer für die Seiteninfo
        snprintf(page_info, sizeof(page_info), "%d/%d", app->selected_file + 1, app->file_count);
        canvas_draw_str(canvas, 2, 10, page_info);  // Zeige die Seitenzahl unter dem Dateinamen

        // Zeige das entschlüsselte Passwort an
        canvas_draw_str(canvas, 2, 40, app->password);  
    } else {
        canvas_draw_str(canvas, 2, 10, "Keine Dateien geladen");
    }
    break;


    case StateExit:
        //canvas_draw_str(canvas, 2, 10, "Beenden...");
        break;

    default:
        break;
    }
}

void input_callback(InputEvent* input_event, void* ctx) {
    App* app = ctx;
    if(input_event->type == InputTypeShort) {
        furi_message_queue_put(app->input_queue, input_event, 0);
    }

    // Zustand für die Auswahl und Anzeige der Datei und des Passworts
    switch(app->state) {
        case StateDisplayPassword:
            if(input_event->key == InputKeyRight) {
                // Vorherige Datei anzeigen
                app->selected_file = (app->selected_file - 1 + app->file_count) % app->file_count;
                // Entschlüsseltes Passwort für die vorherige Datei anzeigen
                if(load_password_from_file(app->file_list[app->selected_file], app->password, sizeof(app->password))) {
                    // Passwort erfolgreich geladen
                    app->state = StateDisplayPassword;
                } else {
                    // Fehler: Passwort konnte nicht geladen werden
                    memset(app->password, 0, sizeof(app->password));  // Leeres Passwort
                }
            } else if(input_event->key == InputKeyLeft) {
                // Nächste Datei anzeigen
                app->selected_file = (app->selected_file + 1) % app->file_count;
                // Entschlüsseltes Passwort für die nächste Datei anzeigen
                if(load_password_from_file(app->file_list[app->selected_file], app->password, sizeof(app->password))) {
                    // Passwort erfolgreich geladen
                    app->state = StateDisplayPassword;
                } else {
                    // Fehler: Passwort konnte nicht geladen werden
                    memset(app->password, 0, sizeof(app->password));  // Leeres Passwort
                }
            } else if(input_event->key == InputKeyBack) {
                // Zurück zur Dateiauswahl
                app->state = StateSelectFile;
            }
            break;

        default:
            break;
    }
}


App* app_init() {
    App* app = malloc(sizeof(App));
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    app->gui = furi_record_open(RECORD_GUI);
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->state = StateMenu;
    app->menu_option = 0;
    memset(app->filename, 0, sizeof(app->filename));
    memset(app->password, 0, sizeof(app->password));
    app->file_list = NULL;
    app->file_count = 0;
    app->selected_file = 0;
    memset(app->char_set_index, 0, sizeof(app->char_set_index));  // Initialisiere Indizes für den Zeichensatz

    // Setze das erste Zeichen direkt auf 'A'
    app->filename[0] = charset[0];
    app->char_set_index[0] = 0;

    view_port_input_callback_set(app->view_port, input_callback, app);
    view_port_draw_callback_set(app->view_port, render_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    return app;
}

void app_free(App* app) {
    if (app->file_list) {
        for (int i = 0; i < app->file_count; i++) {
            free(app->file_list[i]);
        }
        free(app->file_list);
    }
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);
    free(app);
}

int32_t passwordgenerator_app(void) {
    App* app = app_init();

    while (app != NULL) {  // Sicherstellen, dass app nicht NULL ist
        InputEvent input;
        
        if (app->state == StateExit) {
            // Wenn wir im Zustand StateExit sind, beenden wir sofort das Programm.
            break;
        }

        if (furi_message_queue_get(app->input_queue, &input, FuriWaitForever) == FuriStatusOk) {
            switch (app->state) {
            case StateMenu:
                if (input.key == InputKeyUp) {
                    app->menu_option = (app->menu_option - 1 + MENU_OPTION_COUNT) % MENU_OPTION_COUNT;
                } else if (input.key == InputKeyDown) {
                    app->menu_option = (app->menu_option + 1) % MENU_OPTION_COUNT;
                } else if (input.key == InputKeyOk) {
                    if (app->menu_option == 0) {
                        app->state = StateEnterFilename;
                        memset(app->filename, 0, sizeof(app->filename));
                        app->filename[0] = charset[0];  // Das erste Zeichen ist bereits "A"
                        app->char_set_index[0] = 0;     // Der Index für 'A'
                    } else if (app->menu_option == 1) {
                        // Hier auch direkt zu StateSelectFile wechseln, ohne neue Datei zu erstellen
                        app->state = StateSelectFile;
                        load_file_list(app);  // Lade die Dateiliste
                    } else if (app->menu_option == 2) {
                        app->state = StateExit; // Beenden des Programms sofort
                    }
                } else if (input.key == InputKeyBack) {
                    app->state = StateExit; // Programm beenden, wenn im Hauptmenü "Back" gedrückt wird
                }
                break;

            case StateEnterFilename:
                if (input.key == InputKeyBack) {
                    app->state = StateMenu;  // Zurück ins Hauptmenü
                } else if (input.key == InputKeyOk) {
                    generate_password(app->password, PASSGEN_MAX_LENGTH - 1);
                    save_password_to_file(app->filename, app->password);  // Speichern des Passworts in der Datei
                    app->state = StateGeneratePassword;
                } else if (input.key == InputKeyRight) {
                    // Rechts-Taste: Neues Zeichen auswählen
                    add_character_to_filename(app, charset[app->char_set_index[strlen(app->filename)]]);
                } else if (input.key == InputKeyLeft) {
                    // Links-Taste: Vorheriges Zeichen ändern
                    change_character(app, false);
                } else if (input.key == InputKeyUp) {
                    // Runter-Taste: Durch das Zeichen scrollen
                    scroll_character(app, false);
                } else if (input.key == InputKeyDown) {
                    // Hoch-Taste: Durch das Zeichen scrollen
                    scroll_character(app, true);
                }
                break;

            case StateGeneratePassword:
                if (input.key == InputKeyBack) {
                    app->state = StateMenu;  // Zurück ins Hauptmenü
                }
                break;

            case StateSelectFile:
                if (input.key == InputKeyBack) {
                    app->state = StateMenu;  // Zurück ins Hauptmenü
                } else if (input.key == InputKeyUp) {
                    app->selected_file = (app->selected_file - 1 + app->file_count) % app->file_count;
                } else if (input.key == InputKeyDown) {
                    app->selected_file = (app->selected_file + 1) % app->file_count;
                } else if (input.key == InputKeyOk) {
                    if (load_password_from_file(app->file_list[app->selected_file], app->password, sizeof(app->password))) {
                        app->state = StateDisplayPassword;  // Passwort anzeigen
                    } else {
                        memset(app->password, 0, sizeof(app->password));  // Leeres Passwort setzen, falls das Laden fehlgeschlagen ist
                    }
                }
                break;

            case StateDisplayPassword:
                if (input.key == InputKeyBack) {
                    app->state = StateSelectFile;  // Zurück zur Dateiauswahl
                }
                break;

            default:
                break;
            }

            view_port_update(app->view_port);  // Aktualisiere die Ansicht
        }
    }

    app_free(app);  // Ressourcen freigeben
    return 0;
}
