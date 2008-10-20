#ifndef ZLEWLCHOICEBOX_H
#define ZLEWLCHOICEBOX_H

#include <Ewl.h>

typedef void (*choice_handler)(int choice, Ewl_Widget *parent);
typedef void (*choicebox_close_handler)(Ewl_Widget *widget);
Ewl_Widget *init_choicebox(const char *choices[], const char *values[], int numchoices,choice_handler handler, choicebox_close_handler close_handler,char *header, Ewl_Widget *parent, int master);
void fini_choicebox(Ewl_Widget *win);
void update_label(Ewl_Widget *w, int number, const char *value);
Ewl_Widget *choicebox_get_parent(Ewl_Widget *w);
//char *get_theme_file();

#endif
