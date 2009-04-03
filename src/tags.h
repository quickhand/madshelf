#ifndef TAGS_H
#define TAGS_H

int get_num_predef_tags();
int is_predef_tag(const char *instr);
const char *get_predef_tag(int i);
const char *get_predef_tag_display_name(const char *instr);

#endif
