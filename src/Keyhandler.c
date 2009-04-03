#include <stdio.h>
#include <Ewl.h>
#include "Keyhandler.h"
#include "IniFile.h"

/*char *trim_whitespace(const char *instr)
{
    if(!instr)
        return NULL;
    int i;
    int count=0;
    for(i=0;instr[i];i++)
    {
        if(instr[i]!=' ' && instr[i]!='\t' && instr[i]!='\r' && instr[i]!='\n')
            count++;     
    }
    char *outstr=malloc((count+1)*sizeof(char));
    int count2=0;
    for(i=0;instr[i];i++)
    {
        if(instr[i]!=' ' && instr[i]!='\t' && instr[i]!='\r' && instr[i]!='\n')
        {
            outstr[count2]=instr[i];
            count2++;
        }
        
    }
    outstr[count]='\0';
    return outstr;
    
}*/



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
    
    const char* action=ReadString("keys",k,NULL);
        
#define HANDLE_ITEM(h, params) { if(handler_info->h) (*handler_info->h)(w,params,lp);}
#define HANDLE_KEY(h) {if(handler_info->h) (*handler_info->h)(w,lp);}
    
    /*if(!strcmp(k, "Return")) {
        if(nav_mode == 1)            HANDLE_KEY(nav_sel_handler)
        else                         HANDLE_KEY(menu_handler)
    }
    else if(!strcmp(k, "Escape"))    HANDLE_KEY(back_handler)
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
    else if (!strcmp(k, "F2"))       HANDLE_KEY(menu_handler)
    else if (!strcmp(k, "+"))        HANDLE_KEY(mod_handler)*/

    if(!strcmp(action,"MENU"))                            HANDLE_KEY(menu_handler)
    else if(!strcmp(action,"BACK"))                       HANDLE_KEY(back_handler)
    else if(!strcmp(action,"MOD"))                        HANDLE_KEY(mod_handler)
    else if(!strcmp(action,"NAV_UP") && nav_mode==1)      HANDLE_KEY(nav_up_handler)
    else if(!strcmp(action,"NAV_DOWN") && nav_mode==1)    HANDLE_KEY(nav_down_handler)
    else if(!strcmp(action,"NAV_RIGHT") && nav_mode==1)   HANDLE_KEY(nav_right_handler)
    else if(!strcmp(action,"NAV_LEFT") && nav_mode==1)    HANDLE_KEY(nav_left_handler)
    else if(!strcmp(action,"NAV_SELECT") && nav_mode==1)  HANDLE_KEY(nav_sel_handler)
    else if(!strcmp(action,"NEXT") && nav_mode==0)        HANDLE_KEY(next_handler)
    else if(!strcmp(action,"PREVIOUS") && nav_mode==0)    HANDLE_KEY(previous_handler)
    else if(nav_mode==0) // try to parse as int
    {
        int parsed=(int)strtol(action,NULL,10);
        if(parsed>0)                                      HANDLE_ITEM(item_handler,parsed)
        
    }
}

void set_key_handler(Ewl_Widget* widget, key_handler_info_t* handler_info)
{
    ewl_callback_append(widget, EWL_CALLBACK_KEY_UP,
                        &_key_handler, handler_info);
}

