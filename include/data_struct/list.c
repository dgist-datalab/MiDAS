#include"list.h"
#include <stdlib.h>

list* list_init(){
	list *res=(list*)malloc(sizeof(list));

	res->size=0;
	res->head=res->tail=NULL;
	return res;
}

inline li_node *new_li_node(void *data){
	li_node* res=(li_node*)malloc(sizeof(li_node));
	res->data=data;
	res->prv=res->nxt=NULL;
	return res;
}


void list_insert(list *li, void *data){
	li_node *t=new_li_node(data);
	li->size++;
	if(!li->head){
		li->head=li->tail=t;
		return;
	}
	
	t->prv=li->tail;
	li->tail->nxt=t;
	li->tail=t;
	return;
}

void list_delete_node(list *li, li_node* t){
	if(t==li->head){
		li->head=li->head->nxt;
		if(li->head)
			li->head->prv=NULL;
	}
	else if(t==li->tail){
		li->tail=li->tail->prv;
		if(li->tail)
			li->tail->nxt=NULL;
	}
	else{
		li_node *prv=t->prv, *nxt=t->nxt;
		prv->nxt=nxt;
		nxt->prv=prv;	
	}	
	li->size--;
	free(t);
}
void list_free(list *li){
	li_node *now, *nxt;
	if(li->size){
		for_each_list_node_safe(li,now,nxt){
			list_delete_node(li,now);
		}
	}
	free(li);
}
