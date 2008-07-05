/* ------------------------------------------------------------------------
 Copyright (C) 2008 by Marc Lajoie <quickhand@openinkpot.org>

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
#include <ewl_list.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>
#include <unistd.h>
#include <Eet.h>
#include <libintl.h>

#include "IniFile.h"
#include "madshelf.h"
#define _(String) gettext (String)


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
	int count=0;
	Ewl_Widget* curwidget;
	char *file;
	char *finalstr;
	char tempname[20];
	char *tempfilename;
	char prefix[4];
	char *tempo;

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

		

		if(strlen(file)>65)
		{
			tempfilename=(char *)calloc(strlen(file) + 3+1, sizeof(char));
			strncpy(tempfilename,file,65);
			tempfilename[66]='\0';
			strcat(tempfilename,"...");
			ewl_text_text_set(EWL_TEXT(curwidget),tempfilename);
			free(tempfilename);
		}
		else
			ewl_text_text_set(EWL_TEXT(curwidget),file);//finalstr);
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
			ewl_image_file_path_set(EWL_IMAGE(curwidget),"book.png");
		}
		else
		{
			ewl_image_file_path_set(EWL_IMAGE(curwidget),"folder.png");
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
		ewl_text_text_set(EWL_TEXT(curwidget),"");
		ewl_widget_hide(curwidget);
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
	strcat(titletext," | ");
	
	if(sort_type==SORT_BY_NAME)
	{
		if(sort_order==ECORE_SORT_MAX)
			strcat(titletext,gettext("reverse-sorted by name"));
		else
			strcat(titletext,gettext("sorted by name"));
	}
	else if(sort_type==SORT_BY_TIME) 
	{
		if(sort_order==ECORE_SORT_MAX)
			strcat(titletext,gettext("reverse-sorted by time"));
		else
			strcat(titletext,gettext("sorted by time"));
	}
	curwidget = ewl_widget_name_find("mainborder");
	ewl_border_label_set(EWL_BORDER(curwidget),titletext);
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
			scriptname=(char *)calloc(strlen(tempstr)+1, sizeof(char));
			strcat(scriptname,tempstr);
			argument[0]=(char *)calloc(strlen(tempstr)+1, sizeof(char));
			strcat(argument[0],scriptname);
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
	else if(!strcmp(e->base.keyname,"="))
	{
		curwidget = ewl_widget_name_find("okmenu");
		ewl_menu_cb_expand(curwidget,NULL,NULL);
	}
	else if(!strcmp(e->base.keyname,"-"))
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
	if(!strcmp(e->base.keyname,"-"))
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
		update_title();
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
		update_title();
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
		update_title();
		curwidget = ewl_widget_name_find("okmenu");
		ewl_menu_collapse(EWL_MENU(curwidget));
	}

	
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
	Ewl_Widget *list = NULL;
	Ewl_Widget *label[8];
	Ewl_Widget *iconimage[8];
	Ewl_Widget *buttonimage[8];
	Ewl_Widget *borderimage;
	Ewl_Widget *menubar=NULL;
	Ewl_Widget *numlabel=NULL;
	int w,h;
	char imgfile[10];
	char flun[200];
	char *homedir;
	char *configfile;
	int count=0;
	const char *tempstr;
	char *tempstr2;
	char *tempstr3;
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
	strcat(configfile,"/.madshelf/config");
	OpenIniFile (configfile);
	free(configfile);
	tempstr=ReadString("roots","first","notfound");
	
	tempstr2=(char *)calloc(strlen(tempstr) +1, sizeof(char));
	strcat(tempstr2,tempstr);
	
	tempstr3=strtok(tempstr2,",");
	rootname=(char *)calloc(strlen(tempstr3) +1, sizeof(char));
	strcat(rootname,tempstr3);
	
	tempstr3=strtok(NULL,",");
	curdir=(char *)calloc(strlen(tempstr3) +1, sizeof(char));
	strcat(curdir,tempstr3);
	initdirstrlen=strlen(curdir);
	free(tempstr2);
	
	statefilename=(char *)calloc(strlen(homedir) + 1+21 + 1, sizeof(char));
	strcat(statefilename,homedir);
	strcat(statefilename,"/.madshelf/state.eet");
	
	//state=eet_open(eetfilename,EET_FILE_MODE_READ_WRITE);
	refresh_state();

	//curdir=(char *)calloc(strlen(crudehack) +1, sizeof(char));
	//strcat(curdir,crudehack);
	
	win = ewl_window_new();
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
	ewl_theme_data_reset(EWL_WIDGET(border));
	
	ewl_widget_show(border);
	
	update_title();

	box3 = ewl_vbox_new();
	ewl_container_child_append(EWL_CONTAINER(border),box3);
	ewl_object_fill_policy_set(EWL_OBJECT(box3), EWL_FLAG_FILL_VSHRINK|EWL_FLAG_FILL_HFILL);
	ewl_widget_show(box3);	
	
	box4 = ewl_vbox_new();
	ewl_container_child_append(EWL_CONTAINER(border),box4);
	ewl_object_fill_policy_set(EWL_OBJECT(box4), EWL_FLAG_FILL_HFILL|EWL_FLAG_FILL_VSHRINK);
	ewl_widget_show(box4);	

	menubar=ewl_hmenubar_new();
	{
		Ewl_Widget *temp=NULL;
		Ewl_Widget *temp2=NULL;
		temp=ewl_menu_new();	
		ewl_button_label_set(EWL_BUTTON(temp),gettext("Menu (OK)"));
		ewl_container_child_append(EWL_CONTAINER(menubar),temp);
		ewl_widget_name_set(temp,"okmenu");
		ewl_callback_append(EWL_MENU(temp)->popup, EWL_CALLBACK_KEY_DOWN, cb_menu_key_down, NULL);
		ewl_widget_show(temp);
		
		temp2=ewl_menu_item_new();
		ewl_button_label_set(EWL_BUTTON(temp2),gettext("1. Sort by Name"));
		ewl_container_child_append(EWL_CONTAINER(temp),temp2);
		ewl_widget_show(temp2);

		temp2=ewl_menu_item_new();
		ewl_button_label_set(EWL_BUTTON(temp2),gettext("2. Sort by Time"));
		ewl_container_child_append(EWL_CONTAINER(temp),temp2);
		ewl_widget_show(temp2);

		temp2=ewl_menu_item_new();
		ewl_button_label_set(EWL_BUTTON(temp2),gettext("3. Reverse Sort Order"));
		ewl_container_child_append(EWL_CONTAINER(temp),temp2);
		ewl_widget_show(temp2);
	}
	ewl_container_child_append(EWL_CONTAINER(box2),menubar);
	ewl_widget_show(menubar);
	for(count=0;count<8;count++)
	{
		box = ewl_hbox_new();
		ewl_container_child_append(EWL_CONTAINER(box3), box);
		
		ewl_widget_show(box);
		
		sprintf(imgfile,"img%d.png",count+1);
		sprintf(tempname1,"button%d",count);
		buttonimage[count] = ewl_image_new();
		ewl_image_file_path_set(EWL_IMAGE(buttonimage[count]),imgfile);
		ewl_container_child_append(EWL_CONTAINER(box), buttonimage[count]);
		ewl_object_size_request ( EWL_OBJECT ( buttonimage[count] ), 32,64 );
		ewl_widget_name_set(buttonimage[count],tempname1 );
		ewl_widget_show(buttonimage[count]);
		
		sprintf (tempname2, "type%d",count);
		iconimage[count] = ewl_image_new();
		
		ewl_container_child_append(EWL_CONTAINER(box), iconimage[count]);
		ewl_object_size_request ( EWL_OBJECT ( iconimage[count] ), 64,64 );
		ewl_widget_name_set(iconimage[count],tempname2 );
		ewl_widget_show(iconimage[count]);
		
		
		box5 = ewl_vbox_new();
		ewl_container_child_append(EWL_CONTAINER(box),box5);
		ewl_object_fill_policy_set(EWL_OBJECT(box5), EWL_FLAG_FILL_ALL);
		ewl_widget_show(box5);



		sprintf (tempname3, "label%d",count);

		label[count] = ewl_text_new();
		ewl_container_child_append(EWL_CONTAINER(box5), label[count]);
		
		ewl_widget_name_set(label[count],tempname3 );
		ewl_text_text_set(EWL_TEXT(label[count]), "");
		
		//ewl_widget_color_set(EWL_WIDGET(label),255,255,255,0);
		ewl_object_padding_set(EWL_OBJECT(label[count]), 12, 0, 5, 0);
		//ewl_text_styles_set(EWL_TEXT(label[count]), EWL_TEXT_STYLE_SOFT_SHADOW);
		//ewl_text_font_size_set(EWL_TEXT(label[count]),80);
		
		ewl_widget_show(label[count]);
		//free(tempname);
		sprintf (tempname4, "divider%d",count);
		borderimage = ewl_image_new();
		ewl_image_file_path_set(EWL_IMAGE(borderimage),"border.png");
		ewl_container_child_append(EWL_CONTAINER(box5), borderimage);
		ewl_object_size_request ( EWL_OBJECT ( iconimage[count] ), 32,200 );
		ewl_widget_name_set(borderimage,tempname4 );
		
		ewl_object_alignment_set(EWL_OBJECT(borderimage),1u);
		
		
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
		free(curdir);
		
		//printf("%s %s",scriptname,argument[0]);
		execvp(scriptname,argument);//,specialparm);
	}
	CloseIniFile ();
	ecore_list_destroy(filelist);
	eet_shutdown();

	free(rootname);
	free(curdir);
	
	if(ecore_file_exists(statefilename))
	{
		remove(statefilename);
	}

	free(statefilename);
	
	
	return 0;
}

