/***************************************************************************
 *   Copyright (C) 2008 by Marc Lajoie                                     *
 *   marc@gatherer                                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif



#include <stdio.h>
#include <Ewl.h>
#include "Keyhandler.h"
#include "Choicebox.h"


//extern void redraw_text();

const int noptions = 8;

//#define REL_THEME "themes/options.edj"
typedef struct _choice_info_struct {
	char **choices;
	char **values;
	int numchoices;
	int curindex;
	int navsel;
	int master;
	choice_handler handler;
    choicebox_close_handler close_handler;
	Ewl_Widget *parent;
    void *userdata;
} choice_info_struct;

/*
 * Returns edje theme file name.
 */
/*char *get_theme_file()
{
//	char *cwd = get_current_dir_name();
	char *rel_theme;
	asprintf(&rel_theme, "%s/%s", "/usr/share/FBReader", REL_THEME);
	//free(cwd);
	return rel_theme;
}*/

static void choicebox_reveal_cb(Ewl_Widget *w, void *ev, void *data) {
	ewl_window_move(EWL_WINDOW(w), (600 - CURRENT_W(w)) / 2, (800 - CURRENT_H(w)) / 2);
	ewl_window_keyboard_grab_set(EWL_WINDOW(w), 1);
}

static void choicebox_realize_cb(Ewl_Widget *w, void *ev, void *data) {
	Ewl_Widget *win;
	if(data)
		win = (Ewl_Widget *)data;
	else
		win = ewl_widget_name_find("mainwindow");
    if(win)
		ewl_window_keyboard_grab_set(EWL_WINDOW(win), 0);
}

static void choicebox_unrealize_cb(Ewl_Widget *w, void *ev, void *data) {
	Ewl_Widget *win;
	choice_info_struct *infostruct = (choice_info_struct *) ewl_widget_data_get(w, (void *)"choice_info");
    choicebox_close_handler close_handler=infostruct->close_handler;
    if(data)
		win = (Ewl_Widget *)data;
	else
		win = ewl_widget_name_find("mainwindow");
    if(win)
		ewl_window_keyboard_grab_set(EWL_WINDOW(win), 1);
    
	
    close_handler(w,infostruct->userdata);
}

void choicebox_change_selection(Ewl_Widget * widget, int new_navsel)
{
	choice_info_struct *infostruct =
		(choice_info_struct *) ewl_widget_data_get(widget, (void *)"choice_info");
	Ewl_Widget *vbox = ewl_container_child_get(EWL_CONTAINER(widget), 0);
	Ewl_Widget *oldselected =
		ewl_container_child_get(EWL_CONTAINER(vbox), infostruct->navsel + 1);
	Ewl_Widget *newselected =
		ewl_container_child_get(EWL_CONTAINER(vbox), new_navsel + 1);

	if (get_nav_mode() == 1) {
		ewl_widget_state_set(oldselected, "unselect",EWL_STATE_PERSISTENT);
		ewl_widget_state_set(ewl_container_child_get(EWL_CONTAINER(oldselected), 0), "unselect",EWL_STATE_PERSISTENT);
        if(infostruct->values)
            ewl_widget_state_set(ewl_container_child_get(EWL_CONTAINER(oldselected), 1), "unselect",EWL_STATE_PERSISTENT);
        
        ewl_widget_state_set(newselected, "select", EWL_STATE_PERSISTENT);
		ewl_widget_state_set(ewl_container_child_get(EWL_CONTAINER(newselected), 0), "select",	EWL_STATE_PERSISTENT);
        if(infostruct->values)
            ewl_widget_state_set(ewl_container_child_get(EWL_CONTAINER(newselected), 1), "select",	EWL_STATE_PERSISTENT);
        
	}
	infostruct->navsel = new_navsel;
}

void choicebox_next_page(Ewl_Widget * widget)
{
	choice_info_struct *infostruct;
	infostruct =
		(choice_info_struct *) ewl_widget_data_get(widget, (void *)"choice_info");
	Ewl_Widget *vbox = ewl_container_child_get(EWL_CONTAINER(widget), 0);
	Ewl_Widget *tempw1, *v1, *v2;
	if (infostruct->numchoices < noptions
			|| (infostruct->curindex + noptions + 1) >= infostruct->numchoices)
		return;
	infostruct->curindex += noptions;

	int shownum =
		((infostruct->numchoices - infostruct->curindex) >
		 noptions) ? noptions : (infostruct->numchoices -
			 infostruct->curindex);
    int i=0;
	for (i = 0; i < noptions; i++) {
		tempw1 =
			EWL_WIDGET(ewl_container_child_get(EWL_CONTAINER(vbox), i + 1));

		v1 = EWL_WIDGET(ewl_container_child_get(EWL_CONTAINER(tempw1), 0));
        if(infostruct->values)
            v2 = EWL_WIDGET(ewl_container_child_get(EWL_CONTAINER(tempw1), 1));
		if (i < shownum) {
			ewl_label_text_set(EWL_LABEL
					(ewl_container_child_get
					 (EWL_CONTAINER(v1), 0)),
					infostruct->choices[infostruct->curindex +
					i]);
            if(infostruct->values)
                ewl_label_text_set(EWL_LABEL
                        (ewl_container_child_get
                         (EWL_CONTAINER(v2), 0)),
                        infostruct->values[infostruct->curindex +
                        i]);

		} else {
			ewl_label_text_set(EWL_LABEL
					(ewl_container_child_get
					 (EWL_CONTAINER(v1), 0)), "");
            if(infostruct->values)
                ewl_label_text_set(EWL_LABEL
                        (ewl_container_child_get
                         (EWL_CONTAINER(v2), 0)), "");
		}
	}

	char *p;
	asprintf(&p, "Page %d of %d", 1 + infostruct->curindex / 8, (7 + infostruct->numchoices) / 8);

	tempw1 =
		EWL_WIDGET(ewl_container_child_get(EWL_CONTAINER(vbox), noptions + 1));

	v1 = EWL_WIDGET(ewl_container_child_get(EWL_CONTAINER(tempw1), 0));
	ewl_label_text_set(EWL_LABEL
			(ewl_container_child_get
			 (EWL_CONTAINER(v1), 0)),
			p);
	free(p);

	choicebox_change_selection(widget, 0);
}

void choicebox_previous_page(Ewl_Widget * widget)
{
	choice_info_struct *infostruct;
	infostruct =
		(choice_info_struct *) ewl_widget_data_get(widget, (void *)"choice_info");
	Ewl_Widget *vbox = ewl_container_child_get(EWL_CONTAINER(widget), 0);
	Ewl_Widget *tempw1, *v1, *v2;
	if (infostruct->numchoices < noptions || infostruct->curindex == 0)
		return;
	infostruct->curindex -= noptions;
    int i=0;
	for (i = 0; i < noptions; i++) {
		tempw1 =
			EWL_WIDGET(ewl_container_child_get(EWL_CONTAINER(vbox), i + 1));
		v1 = EWL_WIDGET(ewl_container_child_get(EWL_CONTAINER(tempw1), 0));
		ewl_label_text_set(EWL_LABEL
				(ewl_container_child_get
				 (EWL_CONTAINER(v1), 0)),
				infostruct->choices[infostruct->curindex + i]);
        if(infostruct->values)
        {
            v2 = EWL_WIDGET(ewl_container_child_get(EWL_CONTAINER(tempw1), 1));
            ewl_label_text_set(EWL_LABEL
                    (ewl_container_child_get
                     (EWL_CONTAINER(v2), 0)),
                    infostruct->values[infostruct->curindex + i]);
        }
	}

	char *p;
	asprintf(&p, "Page %d of %d", 1 + infostruct->curindex / 8, (7 + infostruct->numchoices) / 8);

	tempw1 =
		EWL_WIDGET(ewl_container_child_get(EWL_CONTAINER(vbox), noptions + 1));

	v1 = EWL_WIDGET(ewl_container_child_get(EWL_CONTAINER(tempw1), 0));
	ewl_label_text_set(EWL_LABEL
			(ewl_container_child_get
			 (EWL_CONTAINER(v1), 0)),
			p);
	free(p);

	choicebox_change_selection(widget, 0);
}

void choicebox_back(Ewl_Widget * widget, unsigned char lp)
{
	fini_choicebox(widget);
}

void choicebox_item(Ewl_Widget * widget, int item, unsigned char lp)
{
	choice_info_struct *infostruct;
	if (item >= 1 && item <= 8) {
		int curchoice;
		infostruct =
			(choice_info_struct *) ewl_widget_data_get(widget,
					(void *)"choice_info");
		curchoice = infostruct->curindex + (item - 1);
		if (curchoice < infostruct->numchoices) {
			choicebox_change_selection(widget, item);
			(infostruct->handler) (curchoice, widget,infostruct->userdata);
		}			
	} else if (item == 9)
		choicebox_previous_page(widget);
	else if (item == 0)
		choicebox_next_page(widget);
}


void choicebox_nav_up(Ewl_Widget * widget, unsigned char lp)
{
	choice_info_struct *infostruct =
		(choice_info_struct *) ewl_widget_data_get(widget, (void *)"choice_info");
	if (infostruct->navsel == 0)
		return;

	choicebox_change_selection(widget, infostruct->navsel - 1);
}

void choicebox_nav_down(Ewl_Widget * widget, unsigned char lp)
{
	choice_info_struct *infostruct =
		(choice_info_struct *) ewl_widget_data_get(widget, (void *)"choice_info");
	if (infostruct->navsel == (noptions - 1)
			|| ((infostruct->curindex + infostruct->navsel + 1) >=
				infostruct->numchoices))
		return;
	choicebox_change_selection(widget, infostruct->navsel + 1);
}

void choicebox_nav_left(Ewl_Widget * widget, unsigned char lp)
{
	choicebox_previous_page(widget);
}

void choicebox_nav_right(Ewl_Widget * widget, unsigned char lp)
{
	choicebox_next_page(widget);
}

void choicebox_nav_sel(Ewl_Widget * widget, unsigned char lp)
{
	choice_info_struct *infostruct =
		(choice_info_struct *) ewl_widget_data_get(widget, (void *)"choice_info");
	(infostruct->handler) (infostruct->curindex * noptions +
			infostruct->navsel, widget,infostruct->userdata);

}

/*static key_handler_info_t choicebox_handlers = {
	.nav_up_handler = &choicebox_nav_up,
	.nav_down_handler = &choicebox_nav_down,
	.nav_left_handler = &choicebox_nav_left,
	.nav_right_handler = &choicebox_nav_right,
	.nav_sel_handler = &choicebox_nav_sel,
	.esc_handler = &choicebox_esc,
	.item_handler = &choicebox_item,
};
*/

static key_handler_info_t choicebox_handlers = {
	NULL,
	choicebox_back,//choicebox_esc
    NULL,
	choicebox_nav_up,
	choicebox_nav_down,
	choicebox_nav_left,
	choicebox_nav_right,
	choicebox_nav_sel,
	choicebox_nav_right,
	choicebox_nav_left,
	choicebox_item
};

void choicebox_destroy_cb(Ewl_Widget * w, void *event, void *data)
{
    //choice_info_struct *infostruct = (choice_info_struct *) ewl_widget_data_get(w, (void *)"choice_info");
    //choicebox_close_handler close_handler=infostruct->close_handler;
	fini_choicebox(w);
	Ewl_Widget *win;
	win = ewl_widget_name_find("mainwindow");
    //close_handler(w);
	//if(win == data)
	//	redraw_text();
}

Ewl_Widget *init_choicebox(const char *choicelist[], const char *values[], int numchoices,
		choice_handler handler,choicebox_close_handler close_handler, char *header, Ewl_Widget *parent, void *userdata,int master)
{
	Ewl_Widget *win, *vbox, *tempw1, *tempw2, *w, *v1, *v2;

	w = ewl_widget_name_find("mainwindow");

	
	choice_info_struct *info =
		(choice_info_struct *) malloc(sizeof(choice_info_struct));

	info->numchoices = numchoices;
	info->curindex = 0;
	info->navsel = 0;
	info->handler = handler;
	info->master = master;
	info->parent = parent;
    info->close_handler=close_handler;
	info->choices = (char **) malloc(sizeof(char *) * numchoices);
    if(values)
        info->values = (char **) malloc(sizeof(char *) * numchoices);
    else
        info->values=NULL;
    info->userdata=userdata;
    int i=0;
	for (i = 0; i < numchoices; i++) {
		asprintf(&(info->choices[i]), "%s", choicelist[i]);
        if(values)
            asprintf(&(info->values[i]), "%s", values[i]);
	}

	win = ewl_window_new();
	ewl_window_title_set(EWL_WINDOW(win), "EWL_WINDOW");
	ewl_window_name_set(EWL_WINDOW(win), "EWL_WINDOW");
	ewl_window_class_set(EWL_WINDOW(win), "ChoiceBox");
	set_key_handler(EWL_WIDGET(win), &choicebox_handlers);
	ewl_callback_append(win, EWL_CALLBACK_DELETE_WINDOW, choicebox_destroy_cb, (void *)parent);
	ewl_callback_append(win, EWL_CALLBACK_REVEAL, choicebox_reveal_cb, (void *)parent);
	ewl_callback_append(win, EWL_CALLBACK_REALIZE, choicebox_realize_cb, (void *)parent);
	ewl_callback_append(win, EWL_CALLBACK_UNREALIZE, choicebox_unrealize_cb, (void *)parent);
    ewl_theme_data_str_set(win, "/window/group","ewl/window/dlg_choicebox");
	EWL_EMBED(win)->x = 600;
	EWL_EMBED(win)->y = 0;
	ewl_widget_data_set(EWL_WIDGET(win), (void *)"choice_info",
			(void *) info);
	ewl_widget_show(win);

	ewl_widget_focus_send(win);

	vbox = ewl_vbox_new();
	ewl_container_child_append(EWL_CONTAINER(win), vbox);
	ewl_object_fill_policy_set(EWL_OBJECT(vbox), EWL_FLAG_FILL_FILL);
	ewl_widget_show(vbox);

	int shownum = (numchoices <= noptions) ? numchoices : noptions;

	tempw1 = ewl_hbox_new();
	ewl_container_child_append(EWL_CONTAINER(vbox), tempw1);
	ewl_theme_data_str_set(EWL_WIDGET(tempw1), "/hbox/group",
			"ewl/box/dlg_optionbox");
	ewl_object_fill_policy_set(EWL_OBJECT(tempw1), EWL_FLAG_FILL_FILL);
	ewl_widget_show(tempw1);

	v1 = ewl_vbox_new();
	ewl_container_child_append(EWL_CONTAINER(tempw1), v1);
	ewl_theme_data_str_set(EWL_WIDGET(v1), "/hbox/group",
			"ewl/box/dlg_optionbox");
	ewl_object_fill_policy_set(EWL_OBJECT(v1), EWL_FLAG_FILL_HFILL);
	ewl_widget_show(v1);

	v2 = ewl_vbox_new();
	ewl_container_child_append(EWL_CONTAINER(tempw1), v2);
	ewl_theme_data_str_set(EWL_WIDGET(v2), "/hbox/group",
			"ewl/box/dlg_optionbox");
	ewl_object_fill_policy_set(EWL_OBJECT(v2), EWL_FLAG_FILL_HFILL);
	ewl_widget_show(v2);

	tempw2 = ewl_label_new();
	ewl_container_child_append(EWL_CONTAINER(v1), tempw2);
	ewl_theme_data_str_set(EWL_WIDGET(tempw2), "/label/group",
			"ewl/label/dlg_header");
	ewl_theme_data_str_set(EWL_WIDGET(tempw2), "/label/textpart",
		"ewl/label/dlg_header/text");
	ewl_object_fill_policy_set(EWL_OBJECT(tempw2), EWL_FLAG_FILL_HFILL);
	ewl_label_text_set(EWL_LABEL(tempw2), header);
	ewl_widget_show(tempw2);

	/*tempw2 = ewl_label_new();
	ewl_container_child_append(EWL_CONTAINER(v2), tempw2);
	ewl_theme_data_str_set(EWL_WIDGET(tempw2), "/label/group",
			"ewl/label/dlg_label");
	ewl_theme_data_str_set(EWL_WIDGET(tempw2), "/label/textpart",
			"ewl/label/dlg_label/text");
	ewl_object_fill_policy_set(EWL_OBJECT(tempw2), EWL_FLAG_FILL_HFILL);
	ewl_label_text_set(EWL_LABEL(tempw2), "");
	ewl_widget_show(tempw2);*/
    
	for(i = 0; i < shownum + 1; i++) {

		tempw1 = ewl_hbox_new();
		ewl_container_child_append(EWL_CONTAINER(vbox), tempw1);
		ewl_theme_data_str_set(EWL_WIDGET(tempw1), "/hbox/group",
				"ewl/box/dlg_optionbox");
		ewl_object_fill_policy_set(EWL_OBJECT(tempw1), EWL_FLAG_FILL_FILL);
		if (get_nav_mode() == 1 && i == 0)
			ewl_widget_state_set(tempw1, "select", EWL_STATE_PERSISTENT);

		ewl_widget_show(tempw1);

		v1 = ewl_vbox_new();
		ewl_container_child_append(EWL_CONTAINER(tempw1), v1);
		ewl_theme_data_str_set(EWL_WIDGET(v1), "/hbox/group",
				"ewl/box/dlg_optionbox");
		ewl_object_fill_policy_set(EWL_OBJECT(v1), EWL_FLAG_FILL_HFILL);
		ewl_widget_show(v1);

		v2 = ewl_vbox_new();
		ewl_container_child_append(EWL_CONTAINER(tempw1), v2);
		ewl_theme_data_str_set(EWL_WIDGET(v2), "/hbox/group",
				"ewl/box/dlg_optionbox");
		ewl_object_fill_policy_set(EWL_OBJECT(v2), EWL_FLAG_FILL_HFILL);
		ewl_widget_show(v2);

		tempw2 = ewl_label_new();
		ewl_container_child_append(EWL_CONTAINER(v1), tempw2);
		ewl_object_fill_policy_set(EWL_OBJECT(tempw2), EWL_FLAG_FILL_HFILL);
		if(i < shownum) {
            if(values)
			{
                ewl_theme_data_str_set(EWL_WIDGET(tempw2), "/label/group","ewl/label/dlg_optionlabel");
                ewl_theme_data_str_set(EWL_WIDGET(tempw2), "/label/textpart","ewl/label/dlg_optionlabel/text");
            }
            else
            {
                ewl_theme_data_str_set(EWL_WIDGET(tempw2), "/label/group","ewl/label/dlg_longoptionlabel");
                ewl_theme_data_str_set(EWL_WIDGET(tempw2), "/label/textpart","ewl/label/dlg_longoptionlabel/text");
                
            }
			if (get_nav_mode() == 1 && i == 0)
				ewl_widget_state_set(tempw2, "select", EWL_STATE_PERSISTENT);
			ewl_label_text_set(EWL_LABEL(tempw2), info->choices[i]);
		}
        else 
        {
			ewl_theme_data_str_set(EWL_WIDGET(tempw2), "/label/group",
					"ewl/label/dlg_footer");
			ewl_theme_data_str_set(EWL_WIDGET(tempw2), "/label/textpart",
					"ewl/label/dlg_footer/text");
			char *p;
			asprintf(&p, "Page %d of %d", 1 + info->curindex / 8, (7 + info->numchoices) / 8);
			ewl_label_text_set(EWL_LABEL(tempw2), p);
			free(p);
		}
		ewl_widget_show(tempw2);
        if(i < shownum  && values) { 
            tempw2 = ewl_label_new();
            ewl_container_child_append(EWL_CONTAINER(v2), tempw2);
            ewl_object_fill_policy_set(EWL_OBJECT(tempw2), EWL_FLAG_FILL_HFILL);
		
			ewl_theme_data_str_set(EWL_WIDGET(tempw2), "/label/group",
					"ewl/label/dlg_valuelabel");
			ewl_theme_data_str_set(EWL_WIDGET(tempw2), "/label/textpart",
					"ewl/label/dlg_valuelabel/text");
			ewl_label_text_set(EWL_LABEL(tempw2), info->values[i]);
            ewl_widget_show(tempw2);
		} 
		
	}
	return win;
}

void fini_choicebox(Ewl_Widget * win)
{
	int master = FALSE;
	ewl_widget_hide(win);
	choice_info_struct *infostruct =
		(choice_info_struct *) ewl_widget_data_get(win, (void *)"choice_info");
	if(infostruct->master)
		master = TRUE;
    int i=0;
	for (i = 0; i < infostruct->numchoices; i++) {
		free(infostruct->choices[i]);
        if(infostruct->values)
            free(infostruct->values[i]);
	}
	free(infostruct->choices);
    if(infostruct->values)
        free(infostruct->values);
	free(infostruct);
	ewl_widget_destroy(win);
	//if(master)
	//	redraw_text();
}

Ewl_Widget *choicebox_get_parent(Ewl_Widget *w)
{
	choice_info_struct *infostruct;
	infostruct =
		(choice_info_struct *) ewl_widget_data_get(w, (void *)"choice_info");

	return infostruct->parent;
}


void update_label(Ewl_Widget *w, int number, const char *value)
{
	Ewl_Widget *vbox = ewl_container_child_get(EWL_CONTAINER(w), 0);
	Ewl_Widget *tempw1, *v1, *v2;

	tempw1 =
		EWL_WIDGET(ewl_container_child_get(EWL_CONTAINER(vbox), number + 1));
	v2 = EWL_WIDGET(ewl_container_child_get(EWL_CONTAINER(tempw1), 1));
    if(!v2)
        return;
	ewl_label_text_set(EWL_LABEL
			(ewl_container_child_get
			 (EWL_CONTAINER(v2), 0)),
			value);
}

