/* ------------------------------------------------------------------------
 Copyright (C) 2008 by Marc Lajoie

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2 of the License, or (at your option) any later
 version.

 This program is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 PARTICULAR PURPOSE.  See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 Place - Suite 330, Boston, MA 02111-1307, USA.
 ------------------------------------------------------------------------ */

#include <Ewl.h>
#include <ewl_macros.h>
#include <ewl_list.h>
#include <Edje.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>
#include <unistd.h>
#include <Eet.h>
#include <libintl.h>
#include <fcntl.h>

#include "IniFile.h"
#include "madshelf.h"
#define _(String) gettext (String)

char **rootstrlist;
char *statefilename=NULL;
Ecore_List *filelist;
char *scriptname=NULL;
char *argument[3];
char titletext[200];

//***********these variables need to be saved and restored in order to restore the state
int curindex=0;
int depth=0;
char *rootname;
char *curdir;
int initdirstrlen;
int sort_type=SORT_BY_NAME;
int sort_order=ECORE_SORT_MIN;
//***********

void update_list()
{
	int backflag=0;
	int count=0;
	Ewl_Widget* curwidget;
	char *file;
	char *finalstr;
	char tempname[20];
	char *tempfilename;
	char prefix[4];
	char *tempo;
	char *imagefile;
	char *pointptr;
	const char *tempstr2;
	struct stat stat_p;
	
	if(curindex>=ecore_list_count(filelist))
	{
		curindex-=8;
		return;
	}
	ecore_list_index_goto(filelist,curindex);
	
	
	
	for(count=0;count<8&&(file = (char*)ecore_list_next(filelist));count++)
	{
		//finalstr = (char *)calloc(strlen(file) + 3 + 1, sizeof(char));
		//sprintf(prefix,"%d. ",count+1);
		//strcat(finalstr,prefix);
		//strcat(finalstr, file);

		sprintf (tempname, "label%d",count);
		
		curwidget = ewl_widget_name_find(tempname);

		

		if(strlen(file)>45)
		{
			tempfilename=(char *)calloc(strlen(file) + 3+1, sizeof(char));
			strncpy(tempfilename,file,45);
			tempfilename[45]='\0';
			strcat(tempfilename,"...");
			ewl_label_text_set(EWL_LABEL(curwidget),tempfilename);
			free(tempfilename);
		}
		else
			ewl_label_text_set(EWL_LABEL(curwidget),file);//finalstr);
		ewl_widget_show(curwidget);		

		sprintf (tempname, "button%d",count);
		curwidget = ewl_widget_name_find(tempname);
		ewl_widget_show(curwidget);
		sprintf (tempname, "divider%d",count);
		curwidget = ewl_widget_name_find(tempname);
		ewl_widget_show(curwidget);
		sprintf (tempname, "type%d",count);
		curwidget = ewl_widget_name_find(tempname);
		
		tempo=(char *)calloc(strlen(file) + strlen(curdir)+1, sizeof(char));
		strcat(tempo,curdir);
		strcat(tempo,file);
           	if(!ecore_file_is_dir(tempo))
		{
			pointptr=strrchr(tempo,'.');
			if(pointptr==NULL)
				tempstr2=ReadString("icons",".","default.png");
			else 
				tempstr2=ReadString("icons",pointptr,"default.png");
			imagefile=(char *)calloc(strlen(tempstr2)+20+1, sizeof(char));
			strcat(imagefile,"/usr/share/madshelf/");
			strcat(imagefile,tempstr2);
			ewl_image_file_path_set(EWL_IMAGE(curwidget),imagefile);
			free(imagefile);
		
		}
		else
		{
			
			ewl_image_file_path_set(EWL_IMAGE(curwidget),"/usr/share/madshelf/folder.png");
		}
		
		ewl_widget_show(curwidget);

		



		free(tempo);
		


		//free(finalstr);
		//free(file);
	
	}
	for(;count<8;count++)
	{
		sprintf (tempname, "button%d",count);
		curwidget = ewl_widget_name_find(tempname);
		ewl_widget_hide(curwidget);
		sprintf (tempname, "divider%d",count);
		curwidget = ewl_widget_name_find(tempname);
		ewl_widget_hide(curwidget);
		sprintf (tempname, "type%d",count);
		curwidget = ewl_widget_name_find(tempname);
		ewl_widget_hide(curwidget);
		sprintf (tempname, "label%d",count);
		curwidget = ewl_widget_name_find(tempname);
		ewl_label_text_set(EWL_LABEL(curwidget),"");
		ewl_widget_hide(curwidget);
	}
	if(curindex>0)
	{
		curwidget = ewl_widget_name_find("backarr");
		ewl_widget_show(curwidget);
		backflag=1;
	}
	else
	{
		curwidget = ewl_widget_name_find("backarr");
		ewl_widget_hide(curwidget);
		backflag=0;
	}
	if((curindex+8)>=ecore_list_count(filelist))
	{
		curwidget = ewl_widget_name_find("forwardarr");
		ewl_widget_hide(curwidget);
	}
	else
	{
		curwidget = ewl_widget_name_find("forwardarr");
		if(backflag==1)
			ewl_object_padding_set(EWL_OBJECT(curwidget),0,0,0,0);
		else
			ewl_object_padding_set(EWL_OBJECT(curwidget),32,0,0,0);
		ewl_widget_show(curwidget);
	}
	

}
void update_title()
{
	char temptext[100];
	titletext[0]='\0';
	Ewl_Widget *curwidget;
	strcat(titletext,"Madshelf | ");
	strcat(titletext,rootname);
	if(!(strlen(curdir)==initdirstrlen))
	{
		strcat(titletext,"://");
		strcat(titletext,&(curdir[initdirstrlen]));

	}
//	strcat(titletext," | ");
//	
//	if(sort_type==SORT_BY_NAME)
//	{
//		if(sort_order==ECORE_SORT_MAX)
//			strcat(titletext,gettext("reverse-sorted by name"));
//		else
//			strcat(titletext,gettext("sorted by name"));
//	}
//	else if(sort_type==SORT_BY_TIME) 
//	{
//		if(sort_order==ECORE_SORT_MAX)
//			strcat(titletext,gettext("reverse-sorted by time"));
//		else
//			strcat(titletext,gettext("sorted by time"));
//	}
	curwidget = ewl_widget_name_find("mainborder");
	ewl_border_label_set(EWL_BORDER(curwidget),titletext);
}
void update_sort_label()
{

	Ewl_Widget *curwidget;

	curwidget = ewl_widget_name_find("sortlabel");
	if(sort_type==SORT_BY_NAME)
	{
		if(sort_order==ECORE_SORT_MAX)
			ewl_label_text_set(EWL_LABEL(curwidget),gettext("reverse-sorted by name"));
		else
			ewl_label_text_set(EWL_LABEL(curwidget),gettext("sorted by name"));
	}
	else if(sort_type==SORT_BY_TIME) 
	{
		if(sort_order==ECORE_SORT_MAX)
			ewl_label_text_set(EWL_LABEL(curwidget),gettext("reverse-sorted by time"));
		else
			ewl_label_text_set(EWL_LABEL(curwidget),gettext("sorted by time"));
	}
	
	





}
void update_menu()
{
	Ewl_Widget *curwidget;
	curwidget = ewl_widget_name_find("okmenu");
	ewl_button_label_set(EWL_BUTTON(curwidget),gettext("Menu (OK)"));
	curwidget = ewl_widget_name_find("menuitem1");
	ewl_button_label_set(EWL_BUTTON(curwidget),gettext("1. Sort by Name"));
	curwidget = ewl_widget_name_find("menuitem2");	
	ewl_button_label_set(EWL_BUTTON(curwidget),gettext("2. Sort by Time"));
	curwidget = ewl_widget_name_find("menuitem3");	
	ewl_button_label_set(EWL_BUTTON(curwidget),gettext("3. Reverse Sort Order"));
	curwidget = ewl_widget_name_find("menuitem4");
	ewl_button_label_set(EWL_BUTTON(curwidget),gettext("4. Language Settings"));
	curwidget = ewl_widget_name_find("menuitem5");
	ewl_button_label_set(EWL_BUTTON(curwidget),gettext("5. Go to"));
}
void cleanup()
{


}
int file_name_compare(const void *data1, const void *data2)
{
	int counter;
	char *fname1,*fname2;
	int retval=0;
	fname1=(char*)data1;
	fname2=(char*)data2;
	for(counter=0;counter<strlen(fname1)&&counter<strlen(fname2);counter++)
	{
		if(fname1[0]>fname2[0])
			return 1;
		else if(fname1[0]<fname2[0])
			return -1;
	}
	return 0;
}
int file_date_compare(const void *data1, const void *data2)
{
	char *fname1,*fname2;
	fname1=(char *)calloc(strlen((char*)data1) + strlen(curdir)+1, sizeof(char));
	strcat(fname1,curdir);
	strcat(fname1,(char*)data1);
	fname2=(char *)calloc(strlen((char*)data2) + strlen(curdir)+1, sizeof(char));
	strcat(fname2,curdir);
	strcat(fname2,(char*)data2);
	long long ftime1=ecore_file_mod_time(fname1);
	long long ftime2=ecore_file_mod_time(fname2);
	free(fname1);
	free(fname2);
	if(ftime1>ftime2)
		return 1;
	else if(ftime1<ftime2)
		return -1;
	else
		return 0;
}

void init_filelist()
{
	filelist = ecore_file_ls(curdir);
	if(sort_type==SORT_BY_NAME)
		ecore_list_sort(filelist,file_name_compare,sort_order);
	else if(sort_type==SORT_BY_TIME)
		ecore_list_sort(filelist,file_date_compare,sort_order);
}
void destroy_cb ( Ewl_Widget *w, void *event, void *data )
{
	ewl_widget_destroy ( w );
	ewl_main_quit();
}
void doActionForNum(unsigned int num)
{
	Ewl_Widget *winwidget;
	char *file;
	char *tempo;
	const char *tempstr;
	char *pointptr;
	char *theargv[1];
	char *homepoint;
	if(curindex+(num-1)>=ecore_list_count(filelist))
	{
		return;
	}
	ecore_list_index_goto(filelist,curindex+num-1);
	file = (char*)ecore_list_next(filelist);
	tempo=(char *)calloc(strlen(file) + strlen(curdir)+2, sizeof(char));
	strcat(tempo,curdir);
	strcat(tempo,file);
	if(!ecore_file_is_dir(tempo))
	{
		
		pointptr=strrchr(file,'.');
		if(pointptr==NULL)
			printf("none");
		else 
		{
			tempstr=ReadString("apps",pointptr,"not found");
			if(strcmp(tempstr,"not found")==0)
				return;
			homepoint=getenv("HOME");
			scriptname=(char *)calloc(strlen(tempstr)+strlen(homepoint)+19+1, sizeof(char));
			strcat(scriptname,homepoint);
			strcat(scriptname,"/.madshelf/scripts/");
			strcat(scriptname,tempstr);
			argument[0]=(char *)calloc(strlen(tempstr)+1, sizeof(char));
			strcat(argument[0],tempstr);
			argument[1]=tempo;
			argument[2]=NULL;
			winwidget=ewl_widget_name_find("mainwindow");

			ewl_main_quit();

		}
		
	}
	else
	{

		free(curdir);
		strcat(tempo,"/");
		curdir=tempo;
		ecore_list_destroy(filelist);
		filelist = ecore_file_ls(curdir);
		depth++;
		curindex=0;
		update_list();
		update_title();
		
	}


}
char *getUpLevelDir(char *thedir)
{
	char *strippeddir;
	char *ptr;
	int loc=-1;
	int count;
	
	strippeddir=(char *)calloc(strlen(thedir), sizeof(char));
	strncpy(strippeddir,thedir,strlen(thedir)-1);
	strippeddir[strlen(thedir)-1]='\0';
	ptr=strrchr(strippeddir,'/');
	if(ptr==NULL)
	{
		free(strippeddir);
		return NULL;
	}
	for(count=0;count<strlen(strippeddir);count++)
		if(&(strippeddir[count])==ptr)
			loc=count+1;
	if(loc==-1)
	{
		free(strippeddir);
		return NULL;
	}
	ptr=(char *)calloc(loc +1, sizeof(char));
	strncpy(ptr,strippeddir,loc);
  	ptr[loc]='\0';
	free(strippeddir);
	return ptr;
}
void cb_key_down(Ewl_Widget *w, void *ev, void *data)
{
	Ewl_Event_Key_Down *e;
	Ewl_Widget *curwidget;
	int buttonnum=0;
	int charloc;
	char *tmpchrptr;
	e = (Ewl_Event_Key_Down*)ev;

	if (!strcmp(e->base.keyname, "0"))
	{
		curindex+=8;
		update_list();	
		
	}
	else if (!strcmp(e->base.keyname, "9"))
	{
		if(curindex>0)
		{
			curindex-=8;
			update_list();	
		}
		
	}
	else if(!strcmp(e->base.keyname,"1"))
	{
		doActionForNum(1);
	}
	else if(!strcmp(e->base.keyname,"2"))
	{
		doActionForNum(2);
	}
	else if(!strcmp(e->base.keyname,"3"))
	{
		doActionForNum(3);
	}
	else if(!strcmp(e->base.keyname,"4"))
	{
		doActionForNum(4);
	}
	else if(!strcmp(e->base.keyname,"5"))
	{
		doActionForNum(5);
	}
	else if(!strcmp(e->base.keyname,"6"))
	{
		doActionForNum(6);
	}
	else if(!strcmp(e->base.keyname,"7"))
	{
		doActionForNum(7);
	}
	else if(!strcmp(e->base.keyname,"8"))
	{
		doActionForNum(8);
	}
	else if(!strcmp(e->base.keyname,"Return"))
	{
		curwidget = ewl_widget_name_find("okmenu");
		ewl_menu_cb_expand(curwidget,NULL,NULL);
	}
	else if(!strcmp(e->base.keyname,"Escape"))
	{
		if(depth==0)
			return;
		tmpchrptr=getUpLevelDir(curdir);
		if(tmpchrptr==NULL)
			return;

		free(curdir);
		curdir=tmpchrptr;
		ecore_list_destroy(filelist);
		filelist = ecore_file_ls(curdir);
		depth--;
		curindex=0;
		update_list();
		update_title();

	}
	
}
void cb_menu_key_down(Ewl_Widget *w, void *ev, void *data)
{
	Ewl_Event_Key_Down *e;
	Ewl_Widget *curwidget;

	e = (Ewl_Event_Key_Down*)ev;
	
	if(!strcmp(e->base.keyname,"Escape"))
	{
		curwidget = ewl_widget_name_find("okmenu");
		ewl_menu_collapse(EWL_MENU(curwidget));
		
	}
	else if(!strcmp(e->base.keyname,"1"))
	{
		ecore_list_sort(filelist,file_name_compare,ECORE_SORT_MIN);
		sort_order=ECORE_SORT_MIN;
		sort_type=SORT_BY_NAME;
		curindex=0;
		update_list();
		update_sort_label();
		curwidget = ewl_widget_name_find("okmenu");
		ewl_menu_collapse(EWL_MENU(curwidget));
		
	}
	else if(!strcmp(e->base.keyname,"2"))
	{
		ecore_list_sort(filelist,file_date_compare,ECORE_SORT_MIN);
		sort_order=ECORE_SORT_MIN;
		sort_type=SORT_BY_TIME;
		curindex=0;
		update_list();
		update_sort_label();
		curwidget = ewl_widget_name_find("okmenu");
		ewl_menu_collapse(EWL_MENU(curwidget));
	}
	else if(!strcmp(e->base.keyname,"3"))
	{
		if(sort_order==ECORE_SORT_MIN)
			sort_order=ECORE_SORT_MAX;
		else
			sort_order=ECORE_SORT_MIN;
		if(sort_type==SORT_BY_NAME)
			ecore_list_sort(filelist,file_name_compare,sort_order);
		else if(sort_type==SORT_BY_TIME)
			ecore_list_sort(filelist,file_date_compare,sort_order);
		curindex=0;
		update_list();
		update_sort_label();
		curwidget = ewl_widget_name_find("okmenu");
		ewl_menu_collapse(EWL_MENU(curwidget));
	}
	else if(!strcmp(e->base.keyname,"4"))
	{
		curwidget = ewl_widget_name_find("menuitem4");
		ewl_menu_cb_expand(curwidget,NULL,NULL);
		ewl_widget_focus_send(EWL_WIDGET(EWL_MENU(curwidget)->popup));
		//ewl_object_alignment_set(EWL_OBJECT(EWL_CONTEXT_MENU(EWL_MENU_ITEM(curwidget)->inmenu)->container),EWL_FLAG_ALIGN_TOP);
		
		//ewl_popup_offset_set(EWL_POPUP(EWL_MENU(curwidget)->popup),50, 0);
	}
	else if(!strcmp(e->base.keyname,"5"))
	{
		curwidget = ewl_widget_name_find("menuitem5");
		ewl_menu_cb_expand(curwidget,NULL,NULL);
		ewl_widget_focus_send(EWL_WIDGET(EWL_MENU(curwidget)->popup));
		//ewl_object_alignment_set(EWL_OBJECT(EWL_CONTEXT_MENU(EWL_MENU_ITEM(curwidget)->inmenu)->container),EWL_FLAG_ALIGN_TOP);
		
		//ewl_popup_offset_set(EWL_POPUP(EWL_MENU(curwidget)->popup),50, 0);
	}
	
}
void cb_lang_menu_key_down(Ewl_Widget *w, void *ev, void *data)
{
	Ewl_Event_Key_Down *e;
	Ewl_Widget *curwidget;

	e = (Ewl_Event_Key_Down*)ev;
	if(!strcmp(e->base.keyname,"Escape"))
	{
		curwidget = ewl_widget_name_find("menuitem4");
		ewl_menu_collapse(EWL_MENU(curwidget));
		curwidget = ewl_widget_name_find("okmenu");
		ewl_menu_collapse(EWL_MENU(curwidget));
		
	}
	else if(!strcmp(e->base.keyname,"1"))
	{/* Change language.  */
            	setenv ("LANGUAGE", "en", 1);
          
		/* Make change known.  */
		{
		extern int  _nl_msg_cat_cntr;
		++_nl_msg_cat_cntr;
		}
		
		curwidget = ewl_widget_name_find("menuitem4");
		ewl_menu_collapse(EWL_MENU(curwidget));
		curwidget = ewl_widget_name_find("okmenu");
		ewl_menu_collapse(EWL_MENU(curwidget));
		update_title();
		update_sort_label();
		update_menu();
	}
	else if(!strcmp(e->base.keyname,"2"))
	{
		/* Change language.  */
            	setenv ("LANGUAGE", "fr", 1);
          
		/* Make change known.  */
		{
		extern int  _nl_msg_cat_cntr;
		++_nl_msg_cat_cntr;
		}
		
		curwidget = ewl_widget_name_find("menuitem4");
		ewl_menu_collapse(EWL_MENU(curwidget));
		curwidget = ewl_widget_name_find("okmenu");
		ewl_menu_collapse(EWL_MENU(curwidget));
		update_title();
		update_sort_label();
		update_menu();

	}
	else if(!strcmp(e->base.keyname,"3"))
	{
		/* Change language.  */
            	setenv ("LANGUAGE", "ru", 1);
          
		/* Make change known.  */
		{
		extern int  _nl_msg_cat_cntr;
		++_nl_msg_cat_cntr;
		}
		
		curwidget = ewl_widget_name_find("menuitem4");
		ewl_menu_collapse(EWL_MENU(curwidget));
		curwidget = ewl_widget_name_find("okmenu");
		ewl_menu_collapse(EWL_MENU(curwidget));
		update_title();
		update_sort_label();
		update_menu();

	}
	else if(!strcmp(e->base.keyname,"4"))
	{
		/* Change language.  */
            	setenv ("LANGUAGE", "zh_CN", 1);
          
		/* Make change known.  */
		{
		extern int  _nl_msg_cat_cntr;
		++_nl_msg_cat_cntr;
		}
		
		curwidget = ewl_widget_name_find("menuitem4");
		ewl_menu_collapse(EWL_MENU(curwidget));
		curwidget = ewl_widget_name_find("okmenu");
		ewl_menu_collapse(EWL_MENU(curwidget));
		update_title();
		update_sort_label();
		update_menu();

	}

	
}
void cb_goto_menu_key_down(Ewl_Widget *w, void *ev, void *data)
{
	Ewl_Event_Key_Down *e;
	Ewl_Widget *curwidget;
	int index=-1;
	int count=0;
	const char *tempstr;
	e = (Ewl_Event_Key_Down*)ev;
	if(!strcmp(e->base.keyname,"Escape"))
	{
		curwidget = ewl_widget_name_find("menuitem5");
		ewl_menu_collapse(EWL_MENU(curwidget));
		curwidget = ewl_widget_name_find("okmenu");
		ewl_menu_collapse(EWL_MENU(curwidget));
		
	}
	else if(!strcmp(e->base.keyname,"1"))
		index=0;
	else if(!strcmp(e->base.keyname,"2"))
		index=1;
	else if(!strcmp(e->base.keyname,"3"))
		index=2;
	else if(!strcmp(e->base.keyname,"4"))
		index=3;
	else if(!strcmp(e->base.keyname,"5"))
		index=4;
	else if(!strcmp(e->base.keyname,"6"))
		index=5;
	else if(!strcmp(e->base.keyname,"7"))
		index=6;
	else if(!strcmp(e->base.keyname,"8"))
		index=7;	
	if(index==-1)
		return;
	curwidget = ewl_widget_name_find("menuitem5");
	ewl_menu_collapse(EWL_MENU(curwidget));
	curwidget = ewl_widget_name_find("okmenu");
	ewl_menu_collapse(EWL_MENU(curwidget));
	for(count=0;count<=index;count++)
	{
		if(rootstrlist[count]==NULL)
			return;
	}
	tempstr=ReadString("roots",rootstrlist[index],getenv("HOME"));
	free(curdir);
	curdir=(char *)calloc(strlen(tempstr)+1,sizeof(char));
	strcpy(curdir,tempstr);	
	free(rootname);
	rootname=(char *)calloc(strlen(rootstrlist[index]) +1, sizeof(char));
	strcpy(rootname,rootstrlist[index]);
	initdirstrlen=strlen(curdir);
	depth=0;
	curindex=0;
	init_filelist();
	update_title();
	update_list();
}
void save_state()
{
	Eet_File *state;
	state=eet_open(statefilename,EET_FILE_MODE_WRITE);
	const int a=1;
	eet_write(state,"statesaved",(void *)&a, sizeof(int),0);
	eet_write(state,"curindex",(void *)&curindex,sizeof(int),0);
	eet_write(state,"depth",(void *)&depth,sizeof(int),0);
	eet_write(state,"rootname",(void *)rootname,sizeof(char)*(strlen(rootname)+1),0);
	eet_write(state,"curdir",curdir,sizeof(char)*(strlen(curdir)+1),0);
	eet_write(state,"initdirstrlen",(void *)&initdirstrlen,sizeof(int),0);
	eet_write(state,"sort_type",(void *)&sort_type,sizeof(int),0);
	eet_write(state,"sort_order",(void *)&sort_order,sizeof(int),0);
	eet_close(state);
}
void refresh_state()
{
	char *temp;
	Eet_File *state;
	int size;
	state=eet_open(statefilename,EET_FILE_MODE_READ);
	int a=0;
	if(eet_read(state,"statesaved",&size)==NULL)
	{
		eet_close(state);
		return;
	}
	curindex=*((int*)eet_read(state,"curindex",&size));
	depth=*((int*)eet_read(state,"depth",&size));
	temp=(char *)eet_read(state,"rootname",&size);
	free(rootname);
	rootname=(char *)calloc(strlen(temp) + 1, sizeof(char));
	strcpy(rootname,temp);
	temp=(char *)eet_read(state,"curdir",&size);
	free(curdir);
	curdir=(char *)calloc(strlen(temp) + 1, sizeof(char));
	strcpy(curdir,temp);
	initdirstrlen=*((int*)eet_read(state,"initdirstrlen",&size));
	sort_type=*((int*)eet_read(state,"sort_type",&size));
	sort_order=*((int*)eet_read(state,"sort_order",&size));
	eet_close(state);
}
int main ( int argc, char ** argv )
{	
	
	int file_desc;
	//char *eetfilename;
	char tempname1[20];
	char tempname2[20];
	char tempname3[20];
	char tempname4[20];
	Ewl_Widget *win = NULL;
	Ewl_Widget *border = NULL;

	Ewl_Widget *box = NULL;
	Ewl_Widget *box2=NULL;
	Ewl_Widget *box3=NULL;
	Ewl_Widget *box4=NULL;
	Ewl_Widget *box5=NULL;
	Ewl_Widget *box6=NULL;
	Ewl_Widget *box7=NULL;
	Ewl_Widget *list = NULL;
	Ewl_Widget *label[8];
	Ewl_Widget *iconimage[8];
	Ewl_Widget *buttonimage[8];
	Ewl_Widget *borderimage;
	Ewl_Widget *menubar=NULL;
	Ewl_Widget *numlabel=NULL;
	Ewl_Widget *forwardarr=NULL;
	Ewl_Widget *backarr=NULL;
	Ewl_Widget *sorttypetext;
	int w,h;
	char imgfile[100];
	char flun[200];
	char *homedir;
	char *configfile;
	int count=0;
	const char *tempstr;
	char *tempstr2;
	char *tempstr3;
	char *tempstr4;
	char *tempstr5;
	struct ENTRY *rootlist;




	if ( !ewl_init ( &argc, argv ) )
	{
		return 1;
	}
	eet_init();



	setlocale( LC_ALL, "" );
	//bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain("madshelf");
	
	homedir=getenv("HOME");
	configfile=(char *)calloc(strlen(homedir) + 1+18 + 1, sizeof(char));
	strcat(configfile,homedir);
	strcat(configfile,"/.madshelf/");
	if(!ecore_file_path_dir_exists(configfile))
	{
		ecore_file_mkpath(configfile);
	}
	strcat(configfile,"config");
	if(!ecore_file_exists(configfile))
	{
		file_desc=open(configfile, O_CREAT |O_RDWR | O_CREAT,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		write(file_desc,"[roots]\nHome=", 13*sizeof(char));
		write(file_desc,getenv("HOME"),strlen(getenv("HOME"))*sizeof(char));
		write(file_desc,"\n[apps]\n[icons]",15*sizeof(char));
		close(file_desc);

	}
	OpenIniFile (configfile);
	free(configfile);
	rootlist=FindSection("roots");
	if(rootlist->pNext!=NULL)
		rootlist=rootlist->pNext;
	while(rootlist!=NULL &&rootlist->Type!=tpSECTION &&count<8)
	{	
		if(rootlist->Type!=tpKEYVALUE)
			continue;
		count++;
		rootlist=rootlist->pNext;
	}
	rootstrlist=(char **)calloc(count +  1, sizeof(char *));	
	count=0;
	rootlist=FindSection("roots");
	if(rootlist->pNext!=NULL)
		rootlist=rootlist->pNext;
	while(rootlist!=NULL &&rootlist->Type!=tpSECTION)
	{	
		if(rootlist->Type!=tpKEYVALUE)
			continue;
		tempstr5=(char *)calloc(strlen(rootlist->Text)+1,sizeof(char));
		strcat(tempstr5,rootlist->Text);
		//tempstr5=(char *)calloc(strlen(tempstr4)+1,sizeof(char));
		//strcat(tempstr5,tempstr4);
		rootstrlist[count]=strtok(tempstr5,"=");//rootlist->Text;//strtok(rootlist->Text,"=");
		
		count++;
		rootlist=rootlist->pNext;
	}
	rootstrlist[count]=NULL;

	tempstr=ReadString("roots",rootstrlist[0],getenv("HOME"));
	
	curdir=(char *)calloc(strlen(tempstr)+1,sizeof(char));
	strcat(curdir,tempstr);	
	
	rootname=(char *)calloc(strlen(rootstrlist[0]) +1, sizeof(char));
	strcat(rootname,rootstrlist[0]);
	
	initdirstrlen=strlen(curdir);
	
	
	statefilename=(char *)calloc(strlen(homedir) + 1+21 + 1, sizeof(char));
	strcat(statefilename,homedir);
	strcat(statefilename,"/.madshelf/state.eet");
	
	//state=eet_open(eetfilename,EET_FILE_MODE_READ_WRITE);
	refresh_state();

	//curdir=(char *)calloc(strlen(crudehack) +1, sizeof(char));
	//strcat(curdir,crudehack);
	
	win = ewl_window_new();
	ewl_window_title_set ( EWL_WINDOW ( win ), "EWL_WINDOW" );
	ewl_window_name_set ( EWL_WINDOW ( win ), "EWL_WINDOW" );
	ewl_window_class_set ( EWL_WINDOW ( win ), "EWLWindow" );
	ewl_object_size_request ( EWL_OBJECT ( win ), 600, 800 );
	ewl_callback_append ( win, EWL_CALLBACK_DELETE_WINDOW, destroy_cb, NULL );
	ewl_callback_append(win, EWL_CALLBACK_KEY_DOWN, cb_key_down, NULL);
	ewl_widget_name_set(win,"mainwindow");
	ewl_widget_show ( win );
	
	init_filelist();

	box2 = ewl_vbox_new();
	ewl_container_child_append(EWL_CONTAINER(win),box2);
	ewl_object_fill_policy_set(EWL_OBJECT(box2), EWL_FLAG_FILL_ALL);
	ewl_widget_show(box2);
	
	border=ewl_border_new();
	//ewl_border_label_set(EWL_BORDER(border),"MadShelf - listed by filename");
	ewl_object_fill_policy_set(EWL_OBJECT(border), EWL_FLAG_FILL_ALL);
	ewl_container_child_append(EWL_CONTAINER(box2),border);
	ewl_widget_name_set(border,"mainborder");
	//ewl_theme_data_str_set(EWL_WIDGET(border),"/border/file","/usr/share/madshelf/subtheme.edj");
	//ewl_theme_data_str_set(EWL_WIDGET(border),"/border/file","/usr/share/madshelf/bordertheme.edj");
	//ewl_theme_data_str_set(EWL_WIDGET(EWL_WIDGET(border)),"/border/label/file","/usr/share/madshelf/subtheme.edj");
	//ewl_theme_data_str_set(EWL_WIDGET(border),"/border/label/group","ewl/border/label");
	//"/border/label/group" "ewl/border/label"
	ewl_widget_show(border);
	
	update_title();

	box3 = ewl_vbox_new();
	ewl_container_child_append(EWL_CONTAINER(border),box3);
	ewl_object_fill_policy_set(EWL_OBJECT(box3), EWL_FLAG_FILL_VSHRINK|EWL_FLAG_FILL_HFILL);
	ewl_widget_show(box3);	
	
	box4 = ewl_hbox_new();
	ewl_container_child_append(EWL_CONTAINER(border),box4);
	ewl_object_fill_policy_set(EWL_OBJECT(box4), EWL_FLAG_FILL_HFILL|EWL_FLAG_FILL_VSHRINK);
	
	ewl_widget_show(box4);	

	backarr = ewl_image_new();
	ewl_image_file_path_set(EWL_IMAGE(backarr),"/usr/share/madshelf/backarr.png");
	ewl_container_child_append(EWL_CONTAINER(box4), backarr);
	//ewl_object_size_request ( EWL_OBJECT ( iconimage[count] ), 32,200 );
	ewl_widget_name_set(backarr,"backarr");
	ewl_object_alignment_set(EWL_OBJECT(backarr),EWL_FLAG_ALIGN_LEFT|EWL_FLAG_ALIGN_TOP);
	
	forwardarr = ewl_image_new();
	ewl_image_file_path_set(EWL_IMAGE(forwardarr),"/usr/share/madshelf/forwardarr.png");
	ewl_container_child_append(EWL_CONTAINER(box4), forwardarr);
	//ewl_object_size_request ( EWL_OBJECT ( iconimage[count] ), 32,200 );
	ewl_widget_name_set(forwardarr,"forwardarr");
	



	menubar=ewl_hmenubar_new();
	{
		Ewl_Widget *temp=NULL;
		Ewl_Widget *temp2=NULL;
		Ewl_Widget *temp3=NULL;
		temp=ewl_menu_new();	

		ewl_container_child_append(EWL_CONTAINER(menubar),temp);
		ewl_widget_name_set(temp,"okmenu");
		ewl_callback_append(EWL_MENU(temp)->popup, EWL_CALLBACK_KEY_DOWN, cb_menu_key_down, NULL);
		
		
		
		ewl_widget_show(temp);
		

		temp2=ewl_menu_item_new();
		ewl_widget_name_set(temp2,"menuitem1");

		ewl_container_child_append(EWL_CONTAINER(temp),temp2);
		
		ewl_widget_show(temp2);

		temp2=ewl_menu_item_new();

		ewl_widget_name_set(temp2,"menuitem2");
		ewl_container_child_append(EWL_CONTAINER(temp),temp2);
		ewl_widget_show(temp2);

		temp2=ewl_menu_item_new();

		ewl_widget_name_set(temp2,"menuitem3");
		ewl_container_child_append(EWL_CONTAINER(temp),temp2);
		ewl_widget_show(temp2);

		temp2=ewl_menu_new();

		ewl_container_child_append(EWL_CONTAINER(temp),temp2);
		
		ewl_widget_name_set(temp2,"menuitem4");
		ewl_callback_append(EWL_MENU(temp2)->popup, EWL_CALLBACK_KEY_DOWN, cb_lang_menu_key_down, NULL);
		
		//ewl_object_alignment_set(EWL_OBJECT(temp2),EWL_FLAG_ALIGN_BOTTOM);
		ewl_widget_show(temp2);

		temp3=ewl_menu_item_new();
		ewl_button_label_set(EWL_BUTTON(temp3),"1. English");
		ewl_container_child_append(EWL_CONTAINER(temp2),temp3);
		ewl_widget_show(temp3);
	
		temp3=ewl_menu_item_new();
		ewl_button_label_set(EWL_BUTTON(temp3),"2. Français");
		ewl_container_child_append(EWL_CONTAINER(temp2),temp3);
		ewl_widget_show(temp3);

		temp3=ewl_menu_item_new();
		ewl_button_label_set(EWL_BUTTON(temp3),"3. Русский");
		ewl_container_child_append(EWL_CONTAINER(temp2),temp3);
		ewl_widget_show(temp3);

		temp3=ewl_menu_item_new();
		ewl_button_label_set(EWL_BUTTON(temp3),"4. 简体中文");
		ewl_container_child_append(EWL_CONTAINER(temp2),temp3);
		ewl_widget_show(temp3);
		

		temp2=ewl_menu_new();
		ewl_container_child_append(EWL_CONTAINER(temp),temp2);
		ewl_widget_name_set(temp2,"menuitem5");
		
		ewl_callback_append(EWL_MENU(temp2)->popup, EWL_CALLBACK_KEY_DOWN, cb_goto_menu_key_down, NULL);
		ewl_widget_show(temp2);

		count=0;
		while(rootstrlist[count]!=NULL)
		{
			temp3=ewl_menu_item_new();
			tempstr4=(char *)calloc(strlen(rootstrlist[count])+3+1,sizeof(char));
			sprintf(tempstr4,"%d. %s",count+1,rootstrlist[count]);
			ewl_button_label_set(EWL_BUTTON(temp3),tempstr4);
			free(tempstr4);
			ewl_container_child_append(EWL_CONTAINER(temp2),temp3);
			ewl_widget_show(temp3);
			count++;
		}
	}
	ewl_container_child_append(EWL_CONTAINER(box2),menubar);
	update_menu();
	ewl_widget_show(menubar);

	sorttypetext=ewl_label_new();
	ewl_container_child_append(EWL_CONTAINER(box3), sorttypetext);
	ewl_widget_name_set(sorttypetext,"sortlabel");
	update_sort_label();
	ewl_widget_show(sorttypetext);

	for(count=0;count<8;count++)
	{
		box = ewl_hbox_new();
		ewl_container_child_append(EWL_CONTAINER(box3), box);
		
		ewl_widget_show(box);
		
		sprintf(imgfile,"/usr/share/madshelf/img%d.png",count+1);
		sprintf(tempname1,"button%d",count);
		buttonimage[count] = ewl_image_new();
		ewl_image_file_path_set(EWL_IMAGE(buttonimage[count]),imgfile);
		ewl_container_child_append(EWL_CONTAINER(box), buttonimage[count]);
		ewl_object_size_request ( EWL_OBJECT ( buttonimage[count] ), 32,64 );
		ewl_widget_name_set(buttonimage[count],tempname1 );
		ewl_object_alignment_set(EWL_OBJECT(buttonimage[count]),EWL_FLAG_ALIGN_LEFT|EWL_FLAG_ALIGN_BOTTOM);
		ewl_widget_show(buttonimage[count]);
		
		sprintf (tempname2, "type%d",count);
		iconimage[count] = ewl_image_new();
		
		ewl_container_child_append(EWL_CONTAINER(box), iconimage[count]);
		ewl_object_size_request ( EWL_OBJECT ( iconimage[count] ), 64,64 );
		ewl_widget_name_set(iconimage[count],tempname2 );
		ewl_object_alignment_set(EWL_OBJECT(iconimage[count]),EWL_FLAG_ALIGN_LEFT|EWL_FLAG_ALIGN_BOTTOM);
		ewl_widget_show(iconimage[count]);
		
		
		box5 = ewl_vbox_new();
		ewl_container_child_append(EWL_CONTAINER(box),box5);
		ewl_object_fill_policy_set(EWL_OBJECT(box5), EWL_FLAG_FILL_ALL);
		ewl_widget_show(box5);

		box7 = ewl_vbox_new();
		ewl_container_child_append(EWL_CONTAINER(box5),box7);
		ewl_object_fill_policy_set(EWL_OBJECT(box7), EWL_FLAG_FILL_ALL);
		ewl_widget_show(box7);
		
		sprintf (tempname3, "label%d",count);

		label[count] = ewl_label_new();
		ewl_container_child_append(EWL_CONTAINER(box7), label[count]);
		
		ewl_widget_name_set(label[count],tempname3 );
		ewl_label_text_set(EWL_LABEL(label[count]), "");
		ewl_object_padding_set(EWL_OBJECT(label[count]),3,0,0,0);

		
		ewl_widget_show(label[count]);
		sprintf (tempname4, "divider%d",count);
		borderimage = ewl_image_new();
		ewl_image_file_path_set(EWL_IMAGE(borderimage),"/usr/share/madshelf/border.png");
		ewl_container_child_append(EWL_CONTAINER(box5), borderimage);
		ewl_object_size_request ( EWL_OBJECT ( iconimage[count] ), 32,200 );
		ewl_widget_name_set(borderimage,tempname4 );
		
		ewl_object_alignment_set(EWL_OBJECT(borderimage),EWL_FLAG_ALIGN_LEFT|EWL_FLAG_ALIGN_BOTTOM);
		
		ewl_widget_show(borderimage);
	}
	update_list();
	ewl_widget_focus_send(EWL_WIDGET(border));

	
	ewl_main();
	//CloseIniFile ();
	//ecore_list_destroy(filelist);
	//free(rootname);
	//free(curdir);
	
	
	
	
	if(scriptname!=NULL)
	{	
		
		save_state();
		free(statefilename);
		eet_shutdown();
		CloseIniFile ();
		ecore_list_destroy(filelist);
		free(rootname);
		
		free(rootstrlist);
		free(curdir);
		
		//printf("%s %s",scriptname,argument[0]);
		execvp(scriptname,argument);//,specialparm);
	}
	CloseIniFile ();
	ecore_list_destroy(filelist);
	eet_shutdown();

	free(rootname);
	
	free(rootstrlist);
	free(curdir);
	
	if(ecore_file_exists(statefilename))
	{
		remove(statefilename);
	}

	free(statefilename);
	
	
	return 0;
}

