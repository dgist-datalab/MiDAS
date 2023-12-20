#ifndef __H_LIST_H
#define __H_LIST_H
typedef struct list_node{
	void *data;
	struct list_node *prv;
	struct list_node *nxt;
}li_node;

typedef struct list{
	int size;
	struct list_node *head;
	struct list_node *tail;
}list;


list* list_init();
void list_insert(list *, void *data);
void list_delete_node(list *, li_node*);
void list_free(list *li);

#define for_each_list_node(li, ln)\
	for(ln=li->head; ln!=NULL; ln=ln->nxt)


#define for_each_list_node_safe(li, ln, lp)\
	for(ln=li->head, lp=li->head->nxt; ln!=NULL; ln=lp, lp=ln?ln->nxt:NULL)

#endif
