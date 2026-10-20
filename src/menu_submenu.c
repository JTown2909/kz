#include "menu.h"
#include "kz.h"

int menu_submenu_onactivate(event_handler_t *handler, menu_event_t event, void **event_data){
    menu_item_t *item = handler->subscriber;
    *event_data = handler->callback_data;
    menu_trigger_event(item->owner, MENU_EVENT_ENTER, event_data);
    return 1;
}

menu_item_t *menu_submenu_add(menu_t *menu, uint16_t cell_x, uint16_t cell_y, char *text, menu_t *submenu){
    menu_item_t *item = menu_add(menu, cell_x, cell_y);
    if(item){
        item->text = text;
        item->interactive = 1;
        menu_item_register_event(item, MENU_EVENT_ACTIVATE, menu_submenu_onactivate, submenu);
    }
    return item;
}

/*
 * lazy submenus are built when they are entered and freed once they are no
 * longer open, so menus that are rarely open (warps has ~600 entries) don't
 * hold on to heap memory.  they are freed in menu_lazy_collect, at the start
 * of a frame, because a menu that is being returned from is still used by the
 * event that returns from it.
 */
struct lazy_submenu {
    menu_ctor_t             ctor;
    void                   *ctor_data;
    menu_t                 *menu;
    int                     selected;
    struct lazy_submenu    *next;
};

static struct lazy_submenu *lazy_submenus = NULL;

void menu_destroy(menu_t *menu){
    menu_item_t *item;
    while((item = menu->items.first) != NULL){
        if(item->remove_proc){
            item->remove_proc(item);
        }
        list_erase(&menu->items, item);
    }
    list_destroy(&menu->items);
    list_destroy(&menu->events);
    list_destroy(&menu->item_events);
}

static void lazy_free(struct lazy_submenu *lazy){
    menu_t *menu = lazy->menu;
    /* remember the selected row for the next time the menu is built */
    lazy->selected = 0;
    for(menu_item_t *item = menu->items.first; item != NULL; item = list_next(item)){
        if(item == menu->selected_item){
            break;
        }
        lazy->selected++;
    }
    lazy->menu = NULL;
    menu_destroy(menu);
    free(menu);
}

static int lazy_onactivate(event_handler_t *handler, menu_event_t event, void **event_data){
    menu_item_t *item = handler->subscriber;
    struct lazy_submenu *lazy = item->data;
    if(lazy->menu == NULL){
        lazy->menu = lazy->ctor(lazy->ctor_data);
        if(lazy->menu == NULL){
            kz_log("not enough memory");
            return 1;
        }
        menu_item_t *selected = list_at(&lazy->menu->items, lazy->selected);
        if(selected != NULL && selected->interactive && selected->enabled){
            lazy->menu->selected_item = selected;
        }
    }
    *event_data = lazy->menu;
    menu_trigger_event(item->owner, MENU_EVENT_ENTER, event_data);
    return 1;
}

static void lazy_remove(menu_item_t *item){
    struct lazy_submenu *lazy = item->data;
    if(lazy->menu != NULL){
        lazy_free(lazy);
    }
    for(struct lazy_submenu **p = &lazy_submenus; *p != NULL; p = &(*p)->next){
        if(*p == lazy){
            *p = lazy->next;
            break;
        }
    }
    free(lazy);
}

menu_item_t *menu_submenu_lazy_add(menu_t *menu, uint16_t cell_x, uint16_t cell_y, char *text, menu_ctor_t ctor, void *ctor_data){
    struct lazy_submenu *lazy = malloc(sizeof(*lazy));
    if(lazy == NULL){
        return NULL;
    }
    menu_item_t *item = menu_add(menu, cell_x, cell_y);
    if(item == NULL){
        free(lazy);
        return NULL;
    }
    lazy->ctor = ctor;
    lazy->ctor_data = ctor_data;
    lazy->menu = NULL;
    lazy->selected = 0;
    lazy->next = lazy_submenus;
    lazy_submenus = lazy;
    item->text = text;
    item->interactive = 1;
    item->data = lazy;
    item->remove_proc = lazy_remove;
    menu_item_register_event(item, MENU_EVENT_ACTIVATE, lazy_onactivate, NULL);
    return item;
}

static _Bool menu_is_open(menu_t *menu){
    for(menu_t *m = &kz.main_menu; m != NULL; m = m->child){
        if(m == menu){
            return 1;
        }
    }
    return 0;
}

void menu_lazy_collect(void){
    _Bool freed;
    do{
        /* freeing a menu removes the lazy submenus inside it from the list, so start over each time */
        freed = 0;
        for(struct lazy_submenu *lazy = lazy_submenus; lazy != NULL; lazy = lazy->next){
            if(lazy->menu != NULL && !menu_is_open(lazy->menu)){
                lazy_free(lazy);
                freed = 1;
                break;
            }
        }
    }while(freed);
}
