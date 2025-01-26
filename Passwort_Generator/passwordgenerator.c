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
#define MENU_OPTION_COUNT 3
#define MAX_FILENAME_LENGTH 64
#define AES_KEY_SIZE 16

typedef enum {
    StateMenu,
    StateEnterFilename,
    StateGeneratePassword,
    StateSelectFile,
    StateDisplayPassword,
    StateExit,
} AppState;

typedef struct {
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    FuriMutex* mutex;
    AppState state;
    char filename[MAX_FILENAME_LENGTH];
    char password[PASSGEN_MAX_LENGTH + 1];
    int menu_option;
    char** file_list;
    int file_count;
    int selected_file;
    int char_set_index[MAX_FILENAME_LENGTH];
} App;

static const char charsets[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!$%&-";
static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

// XOR encryption/decryption
void xor_encrypt_decrypt(const unsigned char* input, unsigned char* output, const unsigned char* key, size_t length) {
    for (size_t i = 0; i < length; i++) {
        output[i] = input[i] ^ key[i % AES_KEY_SIZE];
    }
}

// Generate a random encryption key
void generate_random_key(unsigned char* key, size_t key_size) {
    for (size_t i = 0; i < key_size; i++) {
        key[i] = rand() % 256;
    }
}

// Save encrypted password to file
void save_password_to_file(const char* filename, const char* password) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    const char* dir = "/ext/apps_assets/pwgen";
    char full_path[MAX_FILENAME_LENGTH + strlen(dir) + 2];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, filename);

    unsigned char key[AES_KEY_SIZE];
    generate_random_key(key, AES_KEY_SIZE);

    unsigned char encrypted_password[PASSGEN_MAX_LENGTH];
    xor_encrypt_decrypt((unsigned char*)password, encrypted_password, key, PASSGEN_MAX_LENGTH);

    if (storage_file_open(file, full_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, key, AES_KEY_SIZE);
        storage_file_write(file, encrypted_password, sizeof(encrypted_password));
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// Load decrypted password from file
bool load_password_from_file(const char* filename, char* password, size_t max_length) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool success = false;

    const char* dir = "/ext/apps_assets/pwgen";
    char full_path[MAX_FILENAME_LENGTH + strlen(dir) + 2];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, filename);

    if (storage_file_open(file, full_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        unsigned char key[AES_KEY_SIZE];
        ssize_t read_bytes = storage_file_read(file, key, AES_KEY_SIZE);
        if (read_bytes == AES_KEY_SIZE) {
            unsigned char encrypted_password[PASSGEN_MAX_LENGTH];
            read_bytes = storage_file_read(file, encrypted_password, sizeof(encrypted_password));
            if (read_bytes > 0) {
                unsigned char decrypted_password[PASSGEN_MAX_LENGTH];
                xor_encrypt_decrypt(encrypted_password, decrypted_password, key, PASSGEN_MAX_LENGTH);
                decrypted_password[PASSGEN_MAX_LENGTH - 1] = '\0';
                strncpy(password, (char*)decrypted_password, max_length - 1);
                password[max_length - 1] = '\0';
                success = true;
            }
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

// Generate a random password ensuring at least one special character and one number
// Generate a random password ensuring at least one special character and one number
void generate_password(char* password, int length) {
    size_t charsets_size = strlen(charsets);

    bool has_special = false;
    bool has_digit = false;

    for(int i = 0; i < length; i++) {
        password[i] = charsets[furi_hal_random_get() % charsets_size];
        if (strchr("!$%&-", password[i])) {
            has_special = true;
        }
        if (isdigit((unsigned char)password[i])) {
            has_digit = true;
        }
    }

    // Ensure at least one special character and one number
    if (!has_special) {
        char special_chars[] = "!$%&-";
        password[furi_hal_random_get() % length] = special_chars[furi_hal_random_get() % strlen(special_chars)];
    }
    if (!has_digit) {
        char digits[] = "0123456789";
        password[furi_hal_random_get() % length] = digits[furi_hal_random_get() % strlen(digits)];
    }

    password[length] = '\0';
}

// Display the password on the screen (placeholder function)
void display_password(const char* password) {
    printf("Password: %s\n", password);
}

// Sort the file list using bubble sort
void bubble_sort(char** list, int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (strcmp(list[j], list[j + 1]) > 0) {
                char* temp = list[j];
                list[j] = list[j + 1];
                list[j + 1] = temp;
            }
        }
    }
}

// Load and sort the file list
void load_file_list(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    app->file_count = 0;

    const char* dir = "/ext/apps_assets/pwgen";

    if(storage_dir_open(file, dir)) {
        FileInfo file_info;
        char file_name[MAX_FILENAME_LENGTH];

        while(storage_dir_read(file, &file_info, file_name, sizeof(file_name))) {
            if(file_info.size > 0) {
                app->file_list = realloc(app->file_list, (app->file_count + 1) * sizeof(char*));
                app->file_list[app->file_count] = strdup(file_name);
                app->file_count++;
            }
        }

        if (app->file_count > 1) {
            bubble_sort(app->file_list, app->file_count);
        }

        storage_dir_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// Add a character to the filename
void add_character_to_filename(App* app, char character) {
    int len = strlen(app->filename);
    if (len < MAX_FILENAME_LENGTH - 1) {
        app->filename[len] = character;
        app->filename[len + 1] = '\0';
    }
}

// Change the character in the filename (cycling through the charset)
void change_character(App* app, bool forward) {
    int len = strlen(app->filename);
    if (len == 0) {
        return;
    }

    int index = len - 1;
    int current_index = app->char_set_index[index];

    if (forward) {
        current_index = (current_index + 1) % strlen(charset);
    } else {
        current_index = (current_index - 1 + strlen(charset)) % strlen(charset);
    }

    app->char_set_index[index] = current_index;
    app->filename[index] = charset[current_index];
}

// Scroll through the characters in the filename
void scroll_character(App* app, bool up) {
    int len = strlen(app->filename);
    if (len == 0) {
        return;
    }

    int index = len - 1;
    int current_index = app->char_set_index[index];

    if (up) {
        current_index = (current_index + 1) % strlen(charset);
    } else {
        current_index = (current_index - 1 + strlen(charset)) % strlen(charset);
    }

    app->char_set_index[index] = current_index;
    app->filename[index] = charset[current_index];
}

// Render the screen based on the current state
void render_callback(Canvas* canvas, void* ctx) {
    App* app = ctx;
    canvas_clear(canvas);

    switch(app->state) {
    case StateMenu:
        canvas_draw_str(canvas, 2, 10, "Menu:");
        canvas_draw_str(canvas, 10, 25, app->menu_option == 0 ? "> New Password" : "  New Password");
        canvas_draw_str(canvas, 10, 40, app->menu_option == 1 ? "> Show Password" : "  Show Password");
        canvas_draw_str(canvas, 10, 55, app->menu_option == 2 ? "> Exit" : "  Exit");
        break;

    case StateEnterFilename:
        canvas_draw_str(canvas, 2, 10, "Enter Filename:");
        canvas_draw_str(canvas, 2, 25, app->filename);
        break;

    case StateGeneratePassword:
        canvas_draw_str(canvas, 2, 10, "Generated Password:");
        canvas_draw_str(canvas, 2, 25, app->password);
        break;

    case StateSelectFile:
        if (app->file_count > 0) {
            int y_position = 25;
            int max_visible_files = 3;

            for (int i = 0; i < max_visible_files && i + app->selected_file < app->file_count; i++) {
                int file_index = i + app->selected_file;
                bool is_selected = file_index == app->selected_file;
                canvas_draw_str(canvas, 10, y_position + (i * 15), is_selected ? "> " : "  ");
                canvas_draw_str(canvas, 20, y_position + (i * 15), app->file_list[file_index]);
            }

            if (app->selected_file > 0) {
                canvas_draw_str(canvas, 10, y_position + (max_visible_files * 15), "< Back");
            }
            if (app->selected_file < app->file_count - 1) {
                canvas_draw_str(canvas, 10, y_position + ((max_visible_files + 1) * 15), "Next >");
            }
        } else {
            canvas_draw_str(canvas, 2, 10, "No files found");
        }
        break;

    case StateDisplayPassword:
        if (app->file_count > 0) {
            canvas_draw_str(canvas, 2, 25, app->file_list[app->selected_file]);

            char page_info[16];
            snprintf(page_info, sizeof(page_info), "%d/%d", app->selected_file + 1, app->file_count);
            canvas_draw_str(canvas, 2, 10, page_info);

            canvas_draw_str(canvas, 2, 40, app->password);  
        } else {
            canvas_draw_str(canvas, 2, 10, "No files loaded");
        }
        break;

    case StateExit:
        break;

    default:
        break;
    }
}

// Handle input events
void input_callback(InputEvent* input_event, void* ctx) {
    App* app = ctx;
    if(input_event->type == InputTypeShort) {
        furi_message_queue_put(app->input_queue, input_event, 0);
    }
}

// Initialize the app
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
    memset(app->char_set_index, 0, sizeof(app->char_set_index));

    app->filename[0] = charset[0];
    app->char_set_index[0] = 0;

    view_port_input_callback_set(app->view_port, input_callback, app);
    view_port_draw_callback_set(app->view_port, render_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    return app;
}

// Free the app resources
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

// Main app loop
int32_t passwordgenerator_app(void) {
    App* app = app_init();

    while (app != NULL) {
        InputEvent input;
        
        if (app->state == StateExit) {
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
                        app->filename[0] = charset[0];
                        app->char_set_index[0] = 0;
                    } else if (app->menu_option == 1) {
                        app->state = StateSelectFile;
                        load_file_list(app);
                    } else if (app->menu_option == 2) {
                        app->state = StateExit;
                    }
                } else if (input.key == InputKeyBack) {
                    app->state = StateExit;
                }
                break;

            case StateEnterFilename:
                if (input.key == InputKeyBack) {
                    app->state = StateMenu;
                } else if (input.key == InputKeyOk) {
                    generate_password(app->password, PASSGEN_MAX_LENGTH - 1);
                    save_password_to_file(app->filename, app->password);
                    app->state = StateGeneratePassword;
                } else if (input.key == InputKeyRight) {
                    add_character_to_filename(app, charset[app->char_set_index[strlen(app->filename)]]);
                } else if (input.key == InputKeyLeft) {
                    change_character(app, false);
                } else if (input.key == InputKeyUp) {
                    scroll_character(app, false);
                } else if (input.key == InputKeyDown) {
                    scroll_character(app, true);
                }
                break;

            case StateGeneratePassword:
                if (input.key == InputKeyBack) {
                    app->state = StateMenu;
                }
                break;

            case StateSelectFile:
                if (input.key == InputKeyBack) {
                    app->state = StateMenu;
                } else if (input.key == InputKeyUp) {
                    app->selected_file = (app->selected_file - 1 + app->file_count) % app->file_count;
                } else if (input.key == InputKeyDown) {
                    app->selected_file = (app->selected_file + 1) % app->file_count;
                } else if (input.key == InputKeyOk) {
                    if (load_password_from_file(app->file_list[app->selected_file], app->password, sizeof(app->password))) {
                        app->state = StateDisplayPassword;
                    } else {
                        memset(app->password, 0, sizeof(app->password));
                    }
                }
                break;

            case StateDisplayPassword:
                if (input.key == InputKeyBack) {
                    app->state = StateSelectFile;
                } else if (input.key == InputKeyLeft) {
                    app->selected_file = (app->selected_file - 1 + app->file_count) % app->file_count;
                    if (load_password_from_file(app->file_list[app->selected_file], app->password, sizeof(app->password))) {
                        app->state = StateDisplayPassword;
                    } else {
                        memset(app->password, 0, sizeof(app->password));
                    }
                } else if (input.key == InputKeyRight) {
                    app->selected_file = (app->selected_file + 1) % app->file_count;
                    if (load_password_from_file(app->file_list[app->selected_file], app->password, sizeof(app->password))) {
                        app->state = StateDisplayPassword;
                    } else {
                        memset(app->password, 0, sizeof(app->password));
                    }
                }
                break;

            default:
                break;
            }

            view_port_update(app->view_port);
        }
    }

    app_free(app);
    return 0;
}
