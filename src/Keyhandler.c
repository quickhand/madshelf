#include <Ewl.h>
#include "Keyhandler.h"


int nav_mode=1;

int get_nav_mode()
{
    return nav_mode;    
}

void set_nav_mode(int mode)
{
    nav_mode=mode;    
}

/* FIXME: HACK */
static void _key_handler(Ewl_Widget* w, void *event, void *context)
{
    Ewl_Event_Key_Up* e = (Ewl_Event_Key_Up*)event;
    key_handler_info_t* handler_info = (key_handler_info_t*)context;

    const char* k = e->base.keyname;
    unsigned char lp = 0;
    if(e->base.modifiers & EWL_KEY_MODIFIER_ALT)
        lp=1;
    
#define HANDLE_ITEM(h, params) { if(handler_info->h) (*handler_info->h)(w,params,lp);}
#define HANDLE_KEY(h) {if(handler_info->h) (*handler_info->h)(w,lp);}
    if(!strcmp(k, "Return")) {
        if(nav_mode == 1)            HANDLE_KEY(nav_sel_handler)
        else                         HANDLE_KEY(ok_handler)
    }
    else if(!strcmp(k, "Escape"))    HANDLE_KEY(esc_handler)
    else if (isdigit(k[0]) && !k[1]) HANDLE_ITEM(item_handler, k[0]-'0')
    else if (!strcmp(k,"Up")) {
        if(nav_mode == 1)            HANDLE_KEY(nav_up_handler)
        else                         HANDLE_KEY(nav_right_handler)
    }
    else if (!strcmp(k, "Down")) {
        if(nav_mode == 1)            HANDLE_KEY(nav_down_handler)
        else                         HANDLE_KEY(nav_left_handler)
    }
    else if (!strcmp(k, "Left"))     HANDLE_KEY(nav_left_handler)
    else if (!strcmp(k, "Right"))    HANDLE_KEY(nav_right_handler)
    else if (!strcmp(k, "F2"))       HANDLE_KEY(nav_menubtn_handler)
    else if (!strcmp(k, "+"))        HANDLE_KEY(shift_handler)
}

void set_key_handler(Ewl_Widget* widget, key_handler_info_t* handler_info)
{
    ewl_callback_append(widget, EWL_CALLBACK_KEY_UP,
                        &_key_handler, handler_info);
}

