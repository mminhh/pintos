#include <stdio.h>
#include <stdlib.h>
#include "lib/kernel/list.h"
#include "tests/threads/listpop.h"


struct item{
	struct list_elem elem;
	int priority;
};

#define get_item(ELEM) list_entry(ELEM,struct item, elem)

static void populate(struct list *l, int *a, int n){
	struct item *v;
	int i;

	for (i=0; i<n; i++){
		v=malloc(sizeof(struct item));
		v->priority = a[i];

		list_push_back(l,&(v->elem));
	}
}


int compare(const struct list_elem *a, const struct list_elem *b, void * aux){
	
	return get_item(a)->priority<get_item(b)->priority;
}

static void print_sorted(struct list *l){
	struct list_elem * it;
	
	list_sort(l, compare, NULL);

	for (it=list_begin(l); it!=list_end(l); it=list_next(it))
		printf("%d ",get_item(it)->priority); 
	
}


static int test_sorted(struct list *l){
	struct list_elem * it = list_begin(l);
	int last_p=get_item(it)->priority;
	it = list_next(it);

	while (it!=list_end(l)){
		if (get_item(it)->priority<last_p)
			return 0;

		last_p=get_item(it)->priority;
		it=list_next(it);
	}

	return 1;
}


void test_list(){
	struct list l;
	list_init(&l);

	populate(&l, ITEMARRAY, ITEMCOUNT);

	printf("Sorted array:\n");
	print_sorted(&l);
	printf("\n");

	
	ASSERT(test_sorted(&l));
}