/***************************************************************************
 *   Copyright (C) 2008 by Marc Lajoie   *
 *   marc@gatherer   *
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
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <expat.h>
#include "filtertree.h"
#include "filefilter.h"

typedef struct filter_struct {
    filter_node *headnode;
    int active;
} filter;

static int nfilters=0;
static filter *filterlist=NULL;


void addFilter(char *filtername)
{
    filter *newlist;
    newlist=malloc((nfilters+1)*sizeof(filter));
    int i;
    for(i=0;i<nfilters;i++)
    {
        newlist[i]=filterlist[i];
    }
    
    newlist[i].headnode=createNode(NULL,0);
    newlist[i].active=0;
    setTextContent(newlist[i].headnode,filtername);
    
    if(nfilters>0)
        free(filterlist);
    filterlist=newlist;
    
    nfilters++;
}


// Filter Callbacks
char *strToLower(char *str)
{
    char *retstr;
    asprintf(&retstr,"%s",str);
    int i;
    for(i=0;retstr[i]!='\0';i++)
        retstr[i]=tolower(str[i]);
    return retstr;
}



int filename_substring_filter(char *filename,char *textcontent)
{
    if(strstr(filename,textcontent)==NULL)
        return 0;
    return 1;
}
int filename_substring_filter_nocase(char *filename,char *textcontent)
{
    char *filenamei,testcontenti;
    filenamei=strToLower(filename);
    char *textcontenti=strToLower(textcontent);
    int retval;
	if(strstr(filenamei,textcontenti)==NULL)
		retval=0;
	else
        retval=1;
    free(filenamei);
    free(textcontenti);
   
    return retval;
}
int filename_match_filter(char *filename,char *textcontent)
{
    if(strcmp(filename,textcontent)==0)
        return 1;
    return 0;
}
int filename_match_filter_nocase(char *filename,char *textcontent)
{
    char *filenamei,testcontenti;
    filenamei=strToLower(filename);
    char *textcontenti=strToLower(textcontent);
    int retval;
    if(strcmp(filenamei,textcontenti)==0)
        retval=1;
    else
        retval=0;
    free(filenamei);
    free(textcontenti);
   
    return retval;
}



// Parse filter file with expat

typedef struct parse_data_struct
{
    int in_filter;
    int in_condition;
    filter_node *curnode;
} parsing_data;




void handlestart(void *userData,const XML_Char *name,const XML_Char **atts)
{
    
    
    parsing_data *parsinginfo=XML_GetUserData((XML_Parser*)userData);
    
    int i;
    if((strcmp(name,"filterlist")==0 && !(parsinginfo->in_filter)) || parsinginfo->in_condition)
    {
        //do nothing
    }
    else if(strcmp(name,"filter")==0 && !(parsinginfo->in_filter))
    {
	    char *filtname=NULL;
        
        parsinginfo->in_filter=1;
        
	    for (i = 0; atts[i]; i += 2) {
            if(strcmp(atts[i],"name")==0)
            	asprintf(&filtname,"%s",atts[i+1]);
        }
	    if(name==NULL)
	        asprintf(&filtname,"Filter #%d",nfilters);
	    addFilter(filtname);
	    free(filtname);
	    parsinginfo->curnode=filterlist[nfilters-1].headnode;
        parsinginfo->in_filter=1;
        
    }
    else if(strcmp(name,"not")==0 && parsinginfo->in_filter)
    {
        filter_node *newnode=createNode(parsinginfo->curnode,2);
        attachChild(parsinginfo->curnode,newnode);
        parsinginfo->curnode=newnode;
    }
    else if(strcmp(name,"or")==0 && parsinginfo->in_filter)
    {
        filter_node *newnode=createNode(parsinginfo->curnode,3);
        attachChild(parsinginfo->curnode,newnode);
        parsinginfo->curnode=newnode;
    }
    else if(strcmp(name,"and")==0 && parsinginfo->in_filter)
    {
        filter_node *newnode=createNode(parsinginfo->curnode,4);
        attachChild(parsinginfo->curnode,newnode);
        parsinginfo->curnode=newnode;
    }
    else if(strcmp(name,"condition")==0 && parsinginfo->in_filter)
    {
        parsinginfo->in_condition=1;
        int type=0; //0=match, 1=substring
        int matchcase=0; //0 is case insensitive, 1 is case sensitive
        int domain=0; //0=filename,1=basename,2=extension,3=tag,4=mimetype,5=title,6=author...
        for (i = 0; atts[i]; i += 2) {
            if(strcmp(atts[i],"type")==0)
            {
                if(strcmp(atts[i+1],"match")==0)
                    type=0;
                else if(strcmp(atts[i+1],"substring")==0)
                    type=1;
            }
            else if(strcmp(atts[i],"domain")==0)
            {
                if(strcmp(atts[i+1],"filename")==0)
                    domain=0;
                else if(strcmp(atts[i+1],"basename")==0)
                    domain=1;
                else if(strcmp(atts[i+1],"extension")==0)
                    domain=2;
                else if(strcmp(atts[i+1],"tag")==0)
                    domain=3;
                else if(strcmp(atts[i+1],"mimetype")==0)
                    domain=4;
                else if(strcmp(atts[i+1],"title")==0)
                    domain=5;
                else if(strcmp(atts[i+1],"author")==0)
                    domain=6;
            }
            else if(strcmp(atts[i],"matchcase")==0)
            {
                if(strcmp(atts[i+1],"no")==0)
                    matchcase=0;
                else if(strcmp(atts[i+1],"yes")==0)
                    matchcase=1;
            }
            
            
            




 
        }
        filter_node *newnode=createNode(parsinginfo->curnode,1);
        attachChild(parsinginfo->curnode,newnode);
        parsinginfo->curnode=newnode;
        //attach appropriate leaf function        
        if(domain==0)
        {
            if(type==0)
            {   
                if(matchcase==0)
                    setLeafFunction(newnode,filename_match_filter_nocase);       
                if(matchcase==1)
                    setLeafFunction(newnode,filename_match_filter);
            }
            else if(type==1)
            {
                if(matchcase==0)
                    setLeafFunction(newnode,filename_substring_filter_nocase);       
                if(matchcase==1)
                    setLeafFunction(newnode,filename_substring_filter);
            }
        }
            


        


    }
    
}

void handleend(void *userData,const XML_Char *name)
{
    
    parsing_data *parsinginfo=XML_GetUserData((XML_Parser*)userData);
    if(!parsinginfo->in_filter)
        return;
    if(strcmp(name,"filter")==0 && parsinginfo->in_condition==0)
    {
        parsinginfo->in_filter=0;
        parsinginfo->curnode=NULL;
    }
    else if((strcmp(name,"and")==0 || strcmp(name,"or")==0 || strcmp(name,"not")==0)&& parsinginfo->curnode!=NULL &&parsinginfo->in_condition==0)
        parsinginfo->curnode=parsinginfo->curnode->parent;
    else if(strcmp(name,"condition")==0)
    {
        parsinginfo->in_condition=0;
        parsinginfo->curnode=parsinginfo->curnode->parent;
    }
}


void handlechar(void *userData,const XML_Char *s,int len)
{
    parsing_data *parsinginfo=XML_GetUserData(userData);
    if(len>0 && parsinginfo->in_condition)
    {
        char *temp2 = strndup(s, len);
        if(parsinginfo->curnode!=NULL)
            appendTextContent(parsinginfo->curnode,temp2);
        free(temp2);
    }
}


void load_filters(const char *filename)
{
    struct stat stat_p;
    int filehandle;
    char *buffer;
    long nread;
    XML_Parser myparse;
    
    myparse=XML_ParserCreate("UTF-8");
    

    parsing_data *parseinfo;
    parseinfo=(parsing_data*)malloc(sizeof(parsing_data));
    parseinfo->in_filter=0;
    parseinfo->in_condition=0;
    parseinfo->curnode=NULL;
 




    XML_SetUserData(myparse,(void *)parseinfo);
    XML_UseParserAsHandlerArg(myparse);
    XML_SetElementHandler(myparse,handlestart,handleend);
    XML_SetCharacterDataHandler(myparse,handlechar);
    
    
    
    
    filehandle=open(filename,O_RDONLY);
    if(filehandle == -1) {
        XML_ParserFree(myparse);
        return;
    }

    stat(filename,&stat_p);
    buffer=(char *)malloc(stat_p.st_size);
    nread=read(filehandle,(void *)buffer,stat_p.st_size);
    
    XML_Parse(myparse,buffer,nread,1);
    
    
    free(buffer);
    free(parseinfo);
    close(filehandle);
    
    XML_ParserFree(myparse);

}


void free_filters()
{
    if(filterlist==NULL)
        return;
    int i;
    for(i=0;i<nfilters;i++)
    {
        destroyNode(filterlist[i].headnode);
        //free(filterlist[i]);
    }
    free(filterlist);
    filterlist=NULL;
    nfilters=0;

}

// External interface functions
int getNumFilters()
{

    return nfilters;
}
int evaluateFilter(int filtnum,char *filename)
{
    if(filtnum>=nfilters || filtnum<0)
        return -1;
    return evaluateNode(filterlist[filtnum].headnode,filename);

}
int isFilterActive(int filtnum)
{
    
    if(filtnum>=nfilters || filtnum<0)
        return -1;
    return filterlist[filtnum].active;

}
void setFilterActive(int filtnum,int active)
{
    if(filtnum>=nfilters || filtnum<0)
        return;
    filterlist[filtnum].active=active;
}
const char *getFilterName(int filtnum)
{
    if(filtnum>=nfilters || filtnum<0)
    {
        return NULL;
    }
    return filterlist[filtnum].headnode->textcontent;


}