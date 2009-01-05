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
#ifndef FILTER_TREE_H
#define FILTER_TREE_H
typedef int (*leaf_handler)(char *filename,char *matchtext);
typedef struct fn_struct {
	int nchild;
	struct fn_struct *parent;
	struct fn_struct **children;
	int type;
	leaf_handler handler;
	char *textcontent;
} filter_node;


filter_node *createNode(filter_node *parent,int type);
void setTextContent(filter_node *node,char *textcontent);
void attachChild(filter_node *parent,filter_node *child);
void detachChild(filter_node *parent,filter_node *child);
void setLeafFunction(filter_node *node,leaf_handler handler);
void destroyNode(filter_node *node);
int evaluateNode(filter_node *node,char *filename);
#endif
