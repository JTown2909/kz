#include <stdlib.h>
#include "scenes.h"
#include "kz.h"

static int warp_onactivate(event_handler_t *handler, menu_event_t event, void **event_data){
    uint16_t entrance = (uint16_t)((uint32_t)handler->callback_data);
    z2_game.entrance_index = entrance;
    z2_file.exit = z2_game.entrance_index;
    z2_game.common.execute_state = 0;
    z2_game.common.gamestate_ctor = z2_gamestate_table[3].vram_ctor;
    z2_game.common.ctxt_size = z2_gamestate_table[3].alloc_size;
    int scene = ((uint32_t)entrance >> 8) & 0xFF;
    if(scenes[scene].entrance_cnt > 0){
        menu_trigger_event(&kz.main_menu, MENU_EVENT_RETURN, NULL);
    }
    menu_trigger_event(&kz.main_menu, MENU_EVENT_RETURN, NULL);
    return 1;
}

static menu_t *new_list_menu(void){
    menu_t *menu = malloc(sizeof(*menu));
    if(menu == NULL){
        return NULL;
    }
    menu_init(menu, 0, 0);
    menu_padding_set(menu, 0, 1);
    menu->selected_item = menu_button_add(menu, 0, 0, "return", menu_return, NULL);
    return menu;
}

/* the entrances of one scene, built when the scene is opened */
static menu_t *create_entrance_menu(void *data){
    struct kz_scene *scene = &scenes[(int)data];
    menu_t *entrance_menu = new_list_menu();
    if(entrance_menu == NULL){
        return NULL;
    }
    for(int k = 0;k < scene->entrance_cnt;k++){
        menu_button_add(entrance_menu, 0, k + 1, scene->entrances[k], warp_onactivate, (void*)(((scene->scene & 0xFF) << 8) | ((k & 0xFF) << 4)));
    }
    return entrance_menu;
}

/* the scenes of one category, built when the category is opened */
static menu_t *create_category_menu(void *data){
    struct kz_scene_category *cat = &scene_categories[(int)data];
    menu_t *cat_menu = new_list_menu();
    if(cat_menu == NULL){
        return NULL;
    }
    for(int j = 0; j < cat->scene_cnt;j++){
        struct kz_scene *scene = &scenes[cat->scenes[j]];
        if(scene->entrance_cnt > 1){
            menu_submenu_lazy_add(cat_menu, 0, j + 1, scene->scene_name, create_entrance_menu, (void*)(int)cat->scenes[j]);
        }else{
            menu_button_add(cat_menu, 0, j + 1, scene->scene_name, warp_onactivate, (void*)(((scene->scene & 0xFF) << 8) | ((0 & 0xFF) << 4)));
        }
    }
    return cat_menu;
}

menu_t *create_warps_menu(void){
    static menu_t warps;
    menu_init(&warps, 0, 0);
    menu_padding_set(&warps, 0, 1);
    warps.selected_item = menu_button_add(&warps, 0, 0, "return", menu_return, NULL);
    for(int i = 0;i < sizeof(scene_categories) / sizeof(struct kz_scene_category);i++){
        menu_submenu_lazy_add(&warps, 0, i + 1, scene_categories[i].name, create_category_menu, (void*)i);
    }
    return &warps;
}
