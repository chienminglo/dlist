/*
 * File:    dlist.c
 * Author:  Li XianJing <xianjimli@hotmail.com>
 * Brief:   double list implementation.
 *
 * Copyright (c) Li XianJing
 *
 * Licensed under the Academic Free License version 2.1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * History:
 * ================================================================
 * 2008-11-24 Li XianJing <xianjimli@hotmail.com> created
 * 2008-12-08 Li XianJing <xianjimli@hotmail.com> add autotest.
 * 2008-12-10 Li XianJing <xianjimli@hotmail.com> add lock support.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "dlist.h"

typedef struct _DListNode
{
	struct _DListNode* prev;
    struct _DListNode* next;

	void* data;
}DListNode;

struct _DList
{
	Locker* locker;
	DListNode* first;
	void* data_create_ctx;
	void* data_destroy_ctx;
	DListDataCreateFunc data_create;
	DListDataDestroyFunc data_destroy;
};

static void* dlist_create_data(DList* thiz, void* data)
{
	if(thiz->data_create != NULL)
	{
		return thiz->data_create(thiz->data_create_ctx, data);
	}
	else
	{
		return NULL;
	}
}

static void dlist_destroy_data(DList* thiz, void* data)
{
	if(thiz->data_destroy != NULL)
	{
		thiz->data_destroy(thiz->data_destroy_ctx, data);
	}

	return;
}

static DListNode* dlist_create_node(DList* thiz, void* data)
{
	DListNode* node = malloc(sizeof(DListNode));

	if(node != NULL)
	{
		node->prev = NULL;
		node->next = NULL;
		node->data = dlist_create_data(thiz, data);
	}

	return node;
}

static void dlist_destroy_node(DList* thiz, DListNode* node)
{
	if(node != NULL)
	{
		node->next = NULL;
		node->prev = NULL;
    	dlist_destroy_data(thiz, node->data);
		SAFE_FREE(node);
	}

	return;
}

static void dlist_lock(DList* thiz)
{
	if(thiz->locker != NULL)
	{
		locker_lock(thiz->locker);
	}

	return;
}

static void dlist_unlock(DList* thiz)
{
	if(thiz->locker != NULL)
	{
		locker_unlock(thiz->locker);
	}

	return;
}

static void dlist_destroy_locker(DList* thiz)
{
	if(thiz->locker != NULL)
	{
		locker_unlock(thiz->locker);
		locker_destroy(thiz->locker);
	}

	return;
}

DList* dlist_create(DListDataCreateFunc data_create, DListDataDestroyFunc data_destroy, void* ctx, Locker* locker)
{
	DList* thiz = malloc(sizeof(DList));

	if(thiz != NULL)
	{
		thiz->first  = NULL;
		thiz->locker = locker;
		thiz->data_create = data_create;
		thiz->data_create_ctx = ctx;
		thiz->data_destroy = data_destroy;
		thiz->data_destroy_ctx = ctx;
	}

	return thiz;
}

static DListNode* dlist_get_node(DList* thiz, size_t index, int fail_return_last)
{
	DListNode* iter = NULL;
	
	return_val_if_fail(thiz != NULL, NULL); 

	iter = thiz->first;

	while(iter != NULL && iter->next != NULL && index > 0)
	{
		iter = iter->next;
		index--;
	}

	if(!fail_return_last)
	{
		iter = index > 0 ? NULL : iter;
	}

	return iter;
}

Ret dlist_insert(DList* thiz, size_t index, void* data)
{
	Ret ret = RET_OK;
	DListNode* node = NULL;
	DListNode* cursor = NULL;

	return_val_if_fail(thiz != NULL, RET_INVALID_PARAMS); 

	dlist_lock(thiz);

	do
	{
		if((node = dlist_create_node(thiz, data)) == NULL)
		{
			ret = RET_OOM;
			break;
		}

		if(thiz->first == NULL)
		{
			thiz->first = node;
			break;
		}

		cursor = dlist_get_node(thiz, index, 1);
		
		if(index < dlist_length(thiz))
		{
			node->next = cursor;
			if(cursor->prev != NULL)
			{
				cursor->prev->next = node;
			}
			cursor->prev = node;
			if(thiz->first == cursor)
			{
				thiz->first = node;
			}
		}
		else
		{
			cursor->next = node;
			node->prev = cursor;
		}
	}while(0);

	dlist_unlock(thiz);

	return ret;
}

Ret dlist_prepend(DList* thiz, void* data)
{
	return dlist_insert(thiz, 0, data);
}

Ret dlist_append(DList* thiz, void* data)
{
	return dlist_insert(thiz, -1, data);
}

Ret dlist_delete(DList* thiz, size_t index)
{
	Ret ret = RET_OK;
	DListNode* cursor = NULL;

	return_val_if_fail(thiz != NULL, RET_INVALID_PARAMS); 
	
	dlist_lock(thiz);
	cursor = dlist_get_node(thiz, index, 0);

	do
	{
		if(cursor == NULL)
		{
			ret = RET_INVALID_PARAMS;
			break;
		}

		if(cursor != NULL)
		{
			if(cursor == thiz->first)
			{
				thiz->first = cursor->next;
			}

			if(cursor->next != NULL)
			{
				cursor->next->prev = cursor->prev;
			}

			if(cursor->prev != NULL)
			{
				cursor->prev->next = cursor->next;
			}

			dlist_destroy_node(thiz, cursor);
		}

	}while(0);

	dlist_unlock(thiz);

	return RET_OK;
}

Ret dlist_get_by_index(DList* thiz, size_t index, void** data)
{
	DListNode* cursor = NULL;

	return_val_if_fail(thiz != NULL && data != NULL, RET_INVALID_PARAMS); 

	dlist_lock(thiz);

	cursor = dlist_get_node(thiz, index, 0);

	if(cursor != NULL)
	{
		*data = cursor->data;
	}
	dlist_unlock(thiz);

	return cursor != NULL ? RET_OK : RET_INVALID_PARAMS;
}

Ret dlist_set_by_index(DList* thiz, size_t index, void* data)
{
	DListNode* cursor = NULL;

	return_val_if_fail(thiz != NULL, RET_INVALID_PARAMS);

	dlist_lock(thiz);
	
	cursor = dlist_get_node(thiz, index, 0);

	if(cursor != NULL)
	{
		dlist_destroy_data(thiz, cursor->data);
		cursor->data = dlist_create_data(thiz, data);
	}

	dlist_unlock(thiz);

	return cursor != NULL ? RET_OK : RET_INVALID_PARAMS;
}

size_t   dlist_length(DList* thiz)
{
	size_t length = 0;
	DListNode* iter = NULL;
	
	return_val_if_fail(thiz != NULL, 0);

	dlist_lock(thiz);

	iter = thiz->first;

	while(iter != NULL)
	{
		length++;
		iter = iter->next;
	}

	dlist_unlock(thiz);

	return length;
}

Ret dlist_foreach(DList* thiz, DListDataVisitFunc visit, void* ctx)
{
	Ret ret = RET_OK;
	DListNode* iter = NULL;
	
	return_val_if_fail(thiz != NULL && visit != NULL, RET_INVALID_PARAMS);

	dlist_lock(thiz);

	iter = thiz->first;

	while(iter != NULL && ret != RET_STOP)
	{
		ret = visit(ctx, iter->data);

		iter = iter->next;
	}
	dlist_unlock(thiz);

	return ret;
}

int dlist_find(DList* thiz, DListDataCompareFunc cmp, void* ctx)
{
	int i = 0;
	DListNode* iter = NULL;

	return_val_if_fail(thiz != NULL && cmp != NULL, -1);

	dlist_lock(thiz);

	iter = thiz->first;
	while(iter != NULL)
	{
		if(cmp(ctx, iter->data) == 0)
		{
			break;
		}
		i++;
		iter = iter->next;
	}

	dlist_unlock(thiz);

	return i;
}

void dlist_destroy(DList* thiz)
{
	DListNode* iter = NULL;
	DListNode* next = NULL;
	
	return_if_fail(thiz != NULL);

	dlist_lock(thiz);

	iter = thiz->first;
	while(iter != NULL)
	{
		next = iter->next;
		dlist_destroy_node(thiz, iter);
		iter = next;
	}

	thiz->first = NULL;
	dlist_destroy_locker(thiz);
	
	SAFE_FREE(thiz);

	return;
}

