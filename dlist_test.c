
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include "dlist.h"
#include "locker.h"
#include "typedef.h"


typedef struct _node_data_t
{
	int data;
}node_data_t;

static void* data_create(void* ctx, void* pData)
{
	node_data_t* pNode = (node_data_t*)pData;

	node_data_t* node_data = malloc(sizeof(node_data_t));
	node_data->data = pNode->data;
	return (void*)node_data;
}

static void data_destroy(void* ctx, void* data)
{
	if(data!=NULL)
		free(data);
	data=NULL;
}


static int cmp_int(void* ctx, void* pData)
{
	node_data_t* pNode = (node_data_t*)pData;

	return pNode->data - (int)ctx;
}

static Ret print_int(void* ctx, void* pData)
{
	node_data_t* pNode = (node_data_t*)pData;
	printf("%d ", pNode->data);

	return RET_OK;
}

static Ret check_and_dec_int(void* ctx, void* pData)
{
	node_data_t* pNode = (node_data_t*)pData;

	int* expected =(int*)ctx;
	assert(*expected == pNode->data);

	(*expected)--;

	return RET_OK;
}

static void test_int_dlist(void)
{
	int i = 0;
	int n = 100;
	node_data_t* pNode_data;
	DList* dlist = dlist_create(data_create, data_destroy, NULL, NULL);

	for(i = 0; i < n; i++)
	{
		node_data_t temp = {.data = i};
		assert(dlist_append(dlist, (void*)&temp) == RET_OK);

		assert(dlist_length(dlist) == (i + 1));
		assert(dlist_get_by_index(dlist, i, (void**)&pNode_data) == RET_OK);

		assert(pNode_data->data == i);

		temp.data = 2*i;
		assert(dlist_set_by_index(dlist, i, (void*)&temp) == RET_OK);
		assert(dlist_get_by_index(dlist, i, (void**)&pNode_data) == RET_OK);
		assert(pNode_data->data == 2*i);

		temp.data = i;
		assert(dlist_set_by_index(dlist, i, (void*)&temp) == RET_OK);
		assert(dlist_find(dlist, cmp_int, (void*)i) == i);

	}
	//assert(dlist_foreach(dlist, print_int, NULL) == RET_OK);

	//printf("=========>  dlist length = %ld\n", dlist_length(dlist));




	for(i = 0; i < n; i++)
	{
		assert(dlist_get_by_index(dlist, 0, (void**)&pNode_data) == RET_OK);
		assert(pNode_data->data == i);
		assert(dlist_length(dlist) == (n-i));
		assert(dlist_delete(dlist, 0) == RET_OK);
		assert(dlist_length(dlist) == (n-i-1));
		if((i + 1) < n)
		{
			assert(dlist_get_by_index(dlist, 0, (void**)&pNode_data) == RET_OK);
			assert(pNode_data->data == i+1);
		}

	}

	//assert(dlist_foreach(dlist, print_int, NULL) == RET_OK);

	//printf("=========>  dlist length = %ld\n", dlist_length(dlist));



	assert(dlist_length(dlist) == 0);

	for(i = 0; i < n; i++)
	{
		node_data_t temp = {.data = i};
		assert(dlist_prepend(dlist, (void*)&temp) == RET_OK);
		assert(dlist_length(dlist) == (i + 1));
		assert(dlist_get_by_index(dlist, 0, (void**)&pNode_data) == RET_OK);
		assert(pNode_data->data == i);
		temp.data = 2*i;
		assert(dlist_set_by_index(dlist, 0, (void*)&temp) == RET_OK);
		assert(dlist_get_by_index(dlist, 0, (void**)&pNode_data) == RET_OK);
		assert(pNode_data->data == 2*i);
		temp.data = i;
		assert(dlist_set_by_index(dlist, 0, (void*)&temp) == RET_OK);
	}

	i = n - 1;
	assert(dlist_foreach(dlist, check_and_dec_int, &i) == RET_OK);

	assert(dlist_foreach(dlist, print_int, NULL) == RET_OK);
	printf("=========>  dlist length = %ld\n", dlist_length(dlist));

	dlist_destroy(dlist);

	return;

}

static void test_invalid_params(void)
{
	node_data_t* pNode_data;
	node_data_t temp;

	printf("===========Warning is normal begin==============\n");
	assert(dlist_length(NULL) == 0);
	assert(dlist_prepend(NULL, (void*)&temp) == RET_INVALID_PARAMS);
	assert(dlist_append(NULL, (void*)&temp) == RET_INVALID_PARAMS);
	assert(dlist_delete(NULL, 0) == RET_INVALID_PARAMS);
	assert(dlist_insert(NULL, 0, 0) == RET_INVALID_PARAMS);
	assert(dlist_set_by_index(NULL, 0, (void*)&temp) == RET_INVALID_PARAMS);
	assert(dlist_get_by_index(NULL, 0, (void**)&pNode_data) == RET_INVALID_PARAMS);
	assert(dlist_find(NULL, NULL, NULL) < 0);
	assert(dlist_foreach(NULL, NULL, NULL) == RET_INVALID_PARAMS);
	printf("===========Warning is normal end==============\n");

	return;
}

static void single_thread_test(void)
{
	test_int_dlist();
	test_invalid_params();

	return;
}

static void* producer(void* param)
{
	int i = 0;
	DList* dlist = (DList*)param;


	for(i = 0; i < 100; i++)
	{
		node_data_t temp = {.data = i};
		printf("========> append {%d}\n", temp.data);
		assert(dlist_append(dlist, (void*)&temp) == RET_OK);
	}
	
	for(i = 0; i < 100; i++)
	{
		node_data_t temp = {.data = i};
		printf("========> preppend {%d}\n", temp.data);
		assert(dlist_prepend(dlist, (void*)&temp) == RET_OK);
	}

	return NULL;
}

static void* consumer(void* param)
{
	int i = 0;
	DList* dlist = (DList*)param;



	for(i = 0; i < 200; i++)
	{
		usleep(20);
		printf("********* delete {%d} *********\n", i);
		assert(dlist_delete(dlist, 0) == RET_OK);
	}

	return NULL;
}

static void multi_thread_test(void)
{
	pthread_t consumer_tid = 0;
	pthread_t producer_tid = 0;
	DList* dlist = dlist_create(data_create, data_destroy, NULL, locker_create());
	pthread_create(&producer_tid, NULL, producer, dlist);
	//pthread_create(&consumer_tid, NULL, consumer, dlist);

	//pthread_join(consumer_tid, NULL);
	pthread_join(producer_tid, NULL);

	printf("=========>  dlist length = %ld\n", dlist_length(dlist));

	return;
}

int main(int argc, char* argv[])
{
	single_thread_test();
	multi_thread_test();

	return 0;
}



