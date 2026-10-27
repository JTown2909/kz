#include <stdlib.h>
#include <errno.h>
#include "menu.h"
#include "sys.h"
#include "menu_file.h"
#include "kz.h"
#include "kzresource.h"

#define FILE_ROWS 13

static menu_t               file_menu;
static menu_item_t         *file_menu_rows[FILE_ROWS];
static menu_item_t         *file_menu_location = NULL;
/* lines about the card shown in the file list when it has no filesystem */
#define FILE_DIAG_LEN 128
static char                *file_menu_diag = NULL;
static menu_item_t         *file_menu_text_entry = NULL;
static menu_item_t         *file_menu_accept_button = NULL;
static menu_item_t         *file_menu_clear_button = NULL;
static menu_item_t         *file_menu_down_button = NULL;
static menu_item_t         *file_menu_up_button = NULL;
static menu_item_t         *file_menu_new_folder = NULL;
static int                  file_menu_offset = 0;
static const char          *file_menu_default = NULL;
static const char          *file_menu_extension = NULL;
static file_mode_t          file_menu_mode;
static int                  file_menu_extension_len = 0;
static get_file_callback_t  file_menu_callback = NULL;
static void                *file_menu_callback_data = NULL;
static _Bool                file_menu_ready = 0;
static char                *file_menu_text_value;
static char                *new_folder_name;

/* where the browser was left, so reopening it returns to the same place */
static char                *last_dir = NULL;
static char                *last_name = NULL;
static int                  last_offset = 0;

/* directory listing. entries only hold a pointer to their name, so large
   folders fit in kz's small heap (vc and lite run in the same ~270kb). */
typedef struct {
    char   *name;
    _Bool   isdir;
} dir_entry_t;

static dir_entry_t         *dir_files = NULL;
static int                  dir_file_cnt = 0;
static int                  dir_file_cap = 0;

static _Bool stricmp(const char *a, const char *b){
    while(*a && *b){
        char ca = *a++;
        char cb = *b++;
        if(ca >= 'a' && ca <= 'z'){
            ca += 'A' - 'a';
        }
        if(cb >= 'a' && cb <= 'z'){
            cb += 'A' - 'a';
        }
        if(ca != cb){
            return 0;
        }
    }
    return *a == *b;
}

static void clear_file_list(void){
    for(int i = 0; i < dir_file_cnt; i++){
        free(dir_files[i].name);
    }
    dir_file_cnt = 0;
}

static _Bool dir_ent_comp(dir_entry_t *dir_a, dir_entry_t *dir_b);

/* insert in sorted order. returns 0 if out of memory */
static _Bool add_file(const char *name, _Bool isdir){
    if(dir_file_cnt == dir_file_cap){
        int cap = dir_file_cap ? dir_file_cap * 2 : 32;
        dir_entry_t *files = realloc(dir_files, sizeof(*files) * cap);
        if(!files){
            return 0;
        }
        dir_files = files;
        dir_file_cap = cap;
    }
    dir_entry_t ent;
    ent.name = malloc(strlen(name) + 1);
    if(!ent.name){
        return 0;
    }
    strcpy(ent.name, name);
    ent.isdir = isdir;

    int i = dir_file_cnt;
    while(i > 0 && dir_ent_comp(&ent, &dir_files[i - 1])){
        dir_files[i] = dir_files[i - 1];
        i--;
    }
    dir_files[i] = ent;
    dir_file_cnt++;
    return 1;
}

static _Bool update_file_list(void){
    clear_file_list();
    if(file_menu_diag){
        file_menu_diag[0] = 0;
    }
    DIR *dir = opendir(".");
    if(!dir){
        const char *err = strerror(errno);
        if(errno == ENODEV){
            err = "no disk";
        }else if(errno == ENOENT){
            err = "no filesystem";
        }
        /* say what the card did too, to help when a card doesn't work */
        const char *status = sd_status();
        char *text = file_menu_location->text;
        strncpy(text, err, 31);
        text[31] = 0;
        if(status){
            /* the first line goes after the error, the rest in the file list */
            const char *nl = strchr(status, '\n');
            int len = nl ? nl - status : (int)strlen(status);
            strncat(text, " (", 31 - strlen(text));
            int room = 31 - strlen(text);
            strncat(text, status, len < room ? len : room);
            strncat(text, ")", 31 - strlen(text));
            if(nl && file_menu_diag){
                strncpy(file_menu_diag, nl + 1, FILE_DIAG_LEN - 1);
                file_menu_diag[FILE_DIAG_LEN - 1] = 0;
            }
        }
        return 0;
    }
    getcwd(file_menu_location->text, 32);
    file_menu_location->text[31] = 0;
    dirent_t *dirent;
    while((dirent = readdir(dir)) != NULL) {
        if((dirent->dir_name[0] == '.' && strcmp(dirent->dir_name, "..")) != 0 ||
            !(dirent->mode & S_IRUSR)){
            continue;
        }
        _Bool isdir = ((dirent->mode & S_IFMT) == S_IFDIR);
        int name_len = strlen(dirent->dir_name);
        if(!isdir && (name_len < file_menu_extension_len || !stricmp(&dirent->dir_name[name_len - file_menu_extension_len], file_menu_extension))) {
            continue;
        }

        if(!add_file(dirent->dir_name, isdir)){
            kz_log("not enough memory to list all files");
            break;
        }
    }
    closedir(dir);

    return 1;
}

static void update_view(void){
    int y = 0;
    if(update_file_list()){
        if(file_menu_mode == FILE_MODE_LOAD){
            y = 3;
            file_menu_accept_button->enabled = 0;
            file_menu_clear_button->enabled = 0;
            file_menu_text_entry->enabled = 0;
            file_menu_new_folder->enabled = 0;
        }else{
            file_menu_accept_button->enabled = 1;
            file_menu_clear_button->enabled = 1;
            file_menu_text_entry->enabled = 1;
            file_menu_new_folder->enabled = 1;
            y = 6;
        }
        file_menu_up_button->y_cell = y;
        file_menu_down_button->y_cell = y + FILE_ROWS - 1;
        if(dir_file_cnt > FILE_ROWS){
            file_menu_up_button->enabled = 1;
            file_menu_down_button->enabled = 1;
        }else{
            file_menu_up_button->enabled = 0;
            file_menu_down_button->enabled = 0;
        }
        for(int i = 0; i < FILE_ROWS;i++){
            menu_item_t *item = file_menu_rows[i];
            item->y_cell = y++;
            item->interactive = 1;
            if(i < dir_file_cnt){
                item->enabled = 1;
            }else{
                item->enabled = 0;
            }
        }
    }else{
        file_menu_up_button->enabled = 0;
        file_menu_down_button->enabled = 0;
        file_menu_accept_button->enabled = 0;
        file_menu_clear_button->enabled = 0;
        file_menu_text_entry->enabled = 0;
        file_menu_new_folder->enabled = 0;
        for(int i = 0; i < FILE_ROWS;i++){
            file_menu_rows[i]->enabled = 0;
        }
        /* the card's details, if there are any, in the first rows */
        if(file_menu_diag && file_menu_diag[0]){
            for(int i = 0; i < 3 && i < FILE_ROWS; i++){
                menu_item_t *item = file_menu_rows[i];
                item->y_cell = 3 + i;
                item->enabled = 1;
                item->interactive = 0;
            }
        }
    }
}

/* returns 1 if dir_a sorts before dir_b */
static _Bool dir_ent_comp(dir_entry_t *dir_a, dir_entry_t *dir_b){
    if(strcmp(dir_a->name, "..") == 0){
        return 1;
    }
    if(strcmp(dir_b->name, "..") == 0){
        return 0;
    }
    if(dir_a->isdir && !dir_b->isdir){
        return 1;
    }
    if(!dir_a->isdir && dir_b->isdir){
        return 0;
    }
    char *a_name = dir_a->name;
    char *b_name = dir_b->name;
    int d;
    while(*a_name && *b_name){
        char ac = *a_name++;
        char bc = *b_name++;
        if(ac >= '0' && ac <= '9' && bc >= '0' && bc <= '9'){
            char *na = a_name - 1;
            char *nb = b_name - 1;
            while(*na =='0'){
                na++;
            }
            while(*nb == '0'){
                nb++;
            }
            while(*a_name>='0' && *a_name<='9'){
                a_name++;
            }
            while(*b_name>='0' && *b_name<='9'){
                b_name++;
            }
            d = (a_name - na) - (b_name - nb);
            if(d < 0){
                return 1;
            }
            if(d > 0){
                return 0;
            }
            while(na != a_name && nb != b_name){
                d = *na++ - *nb++;
                if(d < 0){
                    return 1;
                }
                if(d > 0){
                    return 0;
                }
            }
            continue;
        }
        if(ac >= 'a' && ac <= 'z'){
            ac += 'A' - 'a';
        }
        if(bc >= 'a' && bc <= 'z'){
            bc += 'A' - 'a';
        }
        d = ac - bc;
        if(d < 0){
            return 1;
        }
        if(d > 0){
            return 0;
        }
    }
    while(*a_name) {
        a_name++;
    }

    while(*b_name) {
        b_name++;
    }
    d = (a_name - dir_a->name) - (b_name - dir_b->name);
    if(d == 0){
        /* names that only differ in case or leading zeros, e.g. "a01b" and "a1bc" */
        d = strcmp(dir_a->name, dir_b->name);
    }
    return d < 0;
}

#define PATH_MAX_LEN 512

static char *cwd_buf;

static char *str_dup(const char *str){
    char *ret = malloc(strlen(str) + 1);
    if(ret){
        strcpy(ret, str);
    }
    return ret;
}

/* on wii vc, getcwd returns buf even when it fails and leaves it untouched */
static _Bool get_cwd(char *buf, size_t size){
    buf[0] = 0;
    if(getcwd(buf, size) == NULL){
        return 0;
    }
    buf[size - 1] = 0;
    return buf[0] != 0;
}

/* remember the folder, scroll position and chosen file for next time */
static void remember_position(const char *name){
    char *cwd = cwd_buf;
    if(!get_cwd(cwd, PATH_MAX_LEN)){
        return;
    }
    free(last_dir);
    free(last_name);
    last_dir = str_dup(cwd);
    last_name = name ? str_dup(name) : NULL;
    last_offset = file_menu_offset;
}

static void restore_position(void){
    char *cwd = cwd_buf;
    if(!last_dir || !get_cwd(cwd, PATH_MAX_LEN) || strcmp(cwd, last_dir) != 0){
        return;
    }
    int max_offset = dir_file_cnt - FILE_ROWS;
    if(max_offset < 0){
        max_offset = 0;
    }
    file_menu_offset = last_offset > max_offset ? max_offset : last_offset;
    if(!last_name){
        return;
    }
    for(int i = 0; i < dir_file_cnt; i++){
        if(strcmp(dir_files[i].name, last_name) == 0){
            if(i < file_menu_offset){
                file_menu_offset = i;
            }else if(i >= file_menu_offset + FILE_ROWS){
                file_menu_offset = i - FILE_ROWS + 1;
            }
            file_menu.selected_item = file_menu_rows[i - file_menu_offset];
            break;
        }
    }
}

static void return_path(char *path){
    char *ret = malloc(PATH_MAX_LEN);
    if(!ret){
        kz_log("not enough memory");
        return;
    }
    if(!get_cwd(ret, PATH_MAX_LEN)){
        kz_log("could not get folder");
        free(ret);
        return;
    }
    int dl = strlen(ret);
    int nl = strlen(path);
    if(dl + 1 + nl + file_menu_extension_len >= PATH_MAX_LEN){
        kz_log("path too long");
        free(ret);
        return;
    }
    ret[dl] = '/';
    strcpy(&ret[dl + 1], path);
    strcpy(&ret[dl + 1 + nl], file_menu_extension);
    remember_position(&ret[dl + 1]);
    file_menu_callback(ret, file_menu_callback_data);
    menu_trigger_event(&file_menu, MENU_EVENT_RETURN, NULL);
    free(ret);
}

/* select the first row, so the selection never sits on a row without a file */
static void select_first_row(void){
    if(dir_file_cnt > 0){
        file_menu.selected_item = file_menu_rows[0];
    }else{
        file_menu.selected_item = file_menu.items.first;
    }
}

static int reset_disk_onactivate(event_handler_t *handler, menu_event_t event, void **event_data){
    reset_disk();
    file_menu_offset = 0;
    update_view();
    return 1;
}

static int accept_onactivate(event_handler_t *handler, menu_event_t event, void **event_data){
    return_path(file_menu_text_value);
    return 1;
}

static int clear_onactivate(event_handler_t *handler, menu_event_t event, void **event_data){
    memset(file_menu_text_value, 0, 32);
    return 1;
}

static int menu_file_onactivate(event_handler_t *handler, menu_event_t event, void **event_data){
    menu_item_t *item = handler->subscriber;
    int idx = (int)item->data + file_menu_offset;
    if(idx >= dir_file_cnt){
        return 1;
    }
    dir_entry_t *dirent = &dir_files[idx];
    if(dirent->isdir){
        /* chdir before update_view frees the name */
        chdir(dirent->name);
        file_menu_offset = 0;
        update_view();
        select_first_row();
        remember_position(NULL);
    }else{
        int len = strlen(dirent->name) - file_menu_extension_len;
        char *path = malloc(len + 1);
        if(!path){
            kz_log("not enough memory");
            return 1;
        }
        memcpy(path, dirent->name, len);
        path[len] = 0;
        if(file_menu_mode == FILE_MODE_SAVE){
            strncpy(file_menu_text_value, path, 31);
            file_menu_text_value[31] = 0;
            file_menu.selected_item = file_menu_accept_button;
        }else{
            return_path(path);
        }
        free(path);
    }
    return 1;
}

static void menu_file_row_draw(menu_item_t *item){
    int idx = (int)item->data + file_menu_offset;
    if(dir_file_cnt == 0 && file_menu_diag && file_menu_diag[0]){
        /* the card's details, one line per row */
        const char *line = file_menu_diag;
        for(int i = 0; i < (int)item->data && line; i++){
            line = strchr(line, '\n');
            if(line){
                line++;
            }
        }
        if(line){
            const char *end = strchr(line, '\n');
            int len = end ? end - line : (int)strlen(line);
            char text[32];
            if(len > 31){
                len = 31;
            }
            memcpy(text, line, len);
            text[len] = 0;
            gfx_printf(menu_item_x(item) + 10, menu_item_y(item), "%s", text);
        }
        return;
    }
    if(idx >= dir_file_cnt){
        return;
    }
    dir_entry_t *ent = &dir_files[idx];
    int x = menu_item_x(item);
    int y = menu_item_y(item);
    int tile = 0;
    char text[32];
    if(strcmp(ent->name, "..") == 0){
        tile = 2;
        strcpy(text, "back");
    }else{
        if(ent->isdir){
            tile = 1;
        }
        strncpy(text, ent->name, 31);
        text[31] = 0;
        if(strlen(ent->name) > 31){
            strcpy(&text[28], "...");
        }
    }
    uint32_t color = DEFAULT_COLOR;
    if(item->owner->selected_item == item){
        color = SELECTED_COLOR;
    }
    gfx_draw_sprite_color(resource_get(R_KZ_FILES), x, y, tile, 8, 8, color);
    gfx_printf_color(x + 10, y,color,"%s",text);
}

static void menu_new_folder_draw(menu_item_t *item) {
    int x = menu_item_x(item);
    int y = menu_item_y(item);
    uint32_t color = DEFAULT_COLOR;
    if(item->owner->selected_item == item) {
        color = SELECTED_COLOR;
    }
    gfx_draw_sprite_color(resource_get(R_KZ_FILES), x, y, 3, 8, 8, color);
    gfx_printf_color(x + 10, y, color, "create new folder");
}

static int new_folder_event(event_handler_t *handler, menu_event_t event, void **event_data) {
    if(event == MENU_EVENT_ACTIVATE) {
        strcpy(new_folder_name, "new folder");
        menu_keyboard_get(handler->subscriber, &new_folder_name);
    } else if (event == MENU_EVENT_KEYBOARD) {
        mkdir(new_folder_name, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        update_view();
    }
    return 1;
}

static int file_menu_up_onactivate(event_handler_t *handler, menu_event_t event, void **event_data){
    if(file_menu_offset == 0){
        file_menu_offset = dir_file_cnt - FILE_ROWS;
    }else{
        file_menu_offset--;
    }
    remember_position(NULL);
    return 1;
}

static int file_menu_down_onactivate(event_handler_t *handler, menu_event_t event, void **event_data){
    if(file_menu_offset >= dir_file_cnt - FILE_ROWS){
        file_menu_offset = 0;
    }else{
        file_menu_offset++;
    }
    remember_position(NULL);
    return 1;
}

static void menu_file_init(void){
    if(file_menu_ready){
        return;
    }
    menu_init(&file_menu, 0, 0);
    file_menu.selected_item = menu_button_add(&file_menu, 0, 0, "return", menu_return, NULL);
    menu_button_add(&file_menu, 0, 1, "reset disk", reset_disk_onactivate, NULL);
    file_menu_location = menu_label_add(&file_menu, 0, 2, malloc(32));
    file_menu_location->text[0] = '\0';
    file_menu_text_value = malloc(32);
    file_menu_text_entry = menu_text_input_add(&file_menu, 0, 3, "untitled", &file_menu_text_value, 32);
    file_menu_accept_button = menu_button_add(&file_menu, 0, 4, "accept", accept_onactivate, NULL);
    file_menu_clear_button = menu_button_add(&file_menu, 8, 4, "clear", clear_onactivate, NULL);
    file_menu_new_folder = menu_add(&file_menu, 0, 5);
    file_menu_new_folder->draw_proc = menu_new_folder_draw;
    menu_item_register_event(file_menu_new_folder, MENU_EVENT_ACTIVATE | MENU_EVENT_KEYBOARD, new_folder_event, NULL);
    file_menu_new_folder->interactive = 1;
    int y = 3;

    file_menu_up_button = menu_gfx_button_add(&file_menu, 0, y, scroll_up_sprite, file_menu_up_onactivate, NULL);
    file_menu_down_button = menu_gfx_button_add(&file_menu, 0, y + FILE_ROWS - 1, scroll_down_sprite, file_menu_down_onactivate, NULL);

    for(int i = 0;i < FILE_ROWS;i++){
        menu_item_t *item = menu_add(&file_menu, 1, y++);
        item->data = (void*)i;
        item->draw_proc = menu_file_row_draw;
        item->interactive = 1;
        menu_item_register_event(item, MENU_EVENT_ACTIVATE, menu_file_onactivate, NULL);
        file_menu_rows[i] = item;
    }

    new_folder_name = malloc(256);
    cwd_buf = malloc(PATH_MAX_LEN);
    file_menu_diag = malloc(FILE_DIAG_LEN);
    if(file_menu_diag){
        file_menu_diag[0] = 0;
    }

    file_menu_ready = 1;
}

void menu_file_get(file_mode_t mode, const char *default_fn, const char *extension, get_file_callback_t callback, void *callback_data){
    if(!file_menu_ready){
        menu_file_init();
    }
    file_menu.selected_item = file_menu.items.first;
    file_menu_default = default_fn;
    file_menu_extension = extension;
    file_menu_extension_len = strlen(extension);
    file_menu_callback = callback;
    file_menu_callback_data = callback_data;
    file_menu_mode = mode;
    file_menu_offset = 0;
    if(default_fn){
        strncpy(file_menu_text_value, default_fn, 31);
        file_menu_text_value[31] = 0;
    }
    /* the folder is forgotten if the card was unmounted (kz-lite frees the sd code when the menu closes) */
    if(last_dir != NULL){
        chdir(last_dir);
    }
    void *menu = &file_menu;
    menu_trigger_event(&kz.main_menu, MENU_EVENT_ENTER, &menu);
    update_view();
    restore_position();
}
