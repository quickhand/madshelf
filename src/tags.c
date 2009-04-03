#include <stdio.h>
#include "tags.h"
const int num_predef_tags=1;
const char *predef_tags[] = { 		
		"_favorite",
	};
const char *predef_tag_names[] = {
        "Favorite",
    };
int get_num_predef_tags()
{
    return num_predef_tags;
}
int is_predef_tag(const char *instr)
{
    int i;
    for(i=0;i<num_predef_tags;i++)
    {
        if(!strcmp(instr,predef_tags[i]))
        {
            return 1;
            
        
        }
    
    }
    return 0;
}
const char *get_predef_tag(int i)
{
    if(i<0||i>=num_predef_tags)
        return NULL;
    return predef_tags[i];
}
const char *get_predef_tag_display_name(const char *instr)
{
    int i;
    for(i=0;i<num_predef_tags;i++)
    {
        if(!strcmp(instr,predef_tags[i]))
        {
            return gettext(predef_tag_names[i]);
            
        
        }
    
    }
    return NULL;

}
