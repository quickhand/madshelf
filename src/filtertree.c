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
#include "filtertree.h"
#include <stdio.h>
#include <stdlib.h>




filter_node *createNode(filter_node *parent,int type)
{
    
	filter_node *newnode=malloc(sizeof(filter_node));
	newnode->parent=parent;
	newnode->type=type;
	newnode->children=NULL;
	newnode->nchild=0;
	newnode->textcontent=NULL;
	return newnode;
}
void setTextContent(filter_node *node,char *textcontent)
{
    if(node->textcontent!=NULL)
        free(node->textcontent);
    
    if(textcontent!=NULL)
    {
        
        asprintf(&node->textcontent,"%s",textcontent);
    }
    else
        node->textcontent=NULL;

}
void appendTextContent(filter_node *node,char *textcontent)
{
    
    if(textcontent==NULL)
        return;
    if(node->textcontent==NULL)
    {
        setTextContent(node,textcontent);
        return;
    }
    char *newstr;
    asprintf(&newstr,"%s%s",node->textcontent,textcontent);
    setTextContent(node,newstr);
    free(newstr);
}
void attachChild(filter_node *parent,filter_node *child)
{
	filter_node **new_list=malloc((parent->nchild+1)*sizeof(filter_node*));
	int i;
	for(i=0;i<parent->nchild;i++)
	{
		new_list[i]=parent->children[i];
	}
	new_list[i]=child;
	free(parent->children);
	parent->nchild++;
	parent->children=new_list;
	child->parent=parent;
}
void detachChild(filter_node *parent,filter_node *child)
{
	int i;
	for(i=0;i<parent->nchild;i++)
		if(parent->children[i]==child)
			break;
	if(i==parent->nchild)
		return;
	filter_node **new_list=malloc((parent->nchild-1)*sizeof(filter_node*));
	int count=0;
	for(i=0;i<parent->nchild;i++)
	{
		if(parent->children[i]!=child)
		{
			new_list[count]=parent->children[i];
			count++;
		}
	}
	free(parent->children);

	child->parent=NULL;
	parent->nchild--;
	parent->children=new_list;

}
void setLeafFunction(filter_node *node,leaf_handler handler)
{
	node->handler=handler;
}
void destroyNode(filter_node *node)
{
	
	int i;
	for(i=0;i<node->nchild;i++)
		destroyNode(node->children[i]);
	if(node->parent!=NULL)
		detachChild(node->parent,node);
	if(node->textcontent!=NULL)
		free(node->textcontent);
	free(node);
}
int evaluateNode(filter_node *node,char *filename)
{
	int i=0;
	int type=node->type;
	if(type==1)
	{

		return (node->handler)(filename,node->textcontent);
	}
	else if(type==0 || type==2 || type==3 || type==4)
	{
		int truth;
		int curtruth;
		for(i=0;i<node->nchild;i++)
		{
			curtruth=evaluateNode(node->children[i],filename);
            if(i==0)
                truth=curtruth;
			if(type==0 || type==2 || type==3)
				truth= (truth||curtruth);
			else
				truth= (truth&&curtruth);


		}
		if(type==2)
			truth=(!truth);
		return truth;
	}
	return 1;


}