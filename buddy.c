#include "buddy.h"
#include<stdbool.h>
#include<assert.h>
#include<stdlib.h>
#include<stdio.h>
#define NULL ((void *)0)
#define MAXNRANK 16
struct node{
	struct node *next;
	void *p;//beginning
	bool use;
};

struct node *list_begin_node[MAXNRANK];
void *beginp;
int totpg;
int *pgrank;

static void DeleteFromList(int truerank,struct node *p){
	if(list_begin_node[truerank]==p){
		list_begin_node[truerank]=list_begin_node[truerank]->next;
		free(p);
	}
	else{
		for(struct node *now=list_begin_node[truerank];now!=NULL;now=now->next){
			if(now->next==p){
				now->next=p->next;
				free(p);
				break;
			}
		}
	}
}
static bool FindAndDeleteNode(int truerank,void *p){//查看是否存在&&use=0,如果是则删掉并返回1
	if(list_begin_node[truerank]->p==p){
		if(list_begin_node[truerank]->use==1)return 0;
		struct node *tmp=list_begin_node[truerank];
		list_begin_node[truerank]=list_begin_node[truerank]->next;
		free(tmp);
		return 1;
	}
	for(struct node *now=list_begin_node[truerank];now->next!=NULL;now=now->next){
		if(now->next->p==p){
			if(now->next->use==1)return 0;
			struct node *tmp=now->next;
			now->next=tmp->next;
			free(tmp);
			return 1;
		}
	}
	return 0;
}
static struct node* FindNode(int truerank,void *p){
	for(struct node *now=list_begin_node[truerank];now!=NULL;now=now->next){
		if(now->p==p)return now;
	}
	return NULL;
}
static int CountUnuseNode(int truerank){
	int num=0;
	for(struct node *now=list_begin_node[truerank];now!=NULL;now=now->next){
		if(!now->use)num++;
	}
	return num;
}

__attribute((destructor)) void free_All(){
	for(int i=0;i<MAXNRANK;i++){
		struct node *las=NULL;
		for(struct node *now=list_begin_node[i];now!=NULL;now=now->next){
			if(las!=NULL)free(las);
			las=now;
		}
		if(las!=NULL)free(las);
	}
	free(pgrank);
}

int init_page(void *p, int pgcount){
	//void *p,p++实际地址加1(void *可以看成char*)
	pgrank=malloc(sizeof(int)*pgcount);
	beginp=p,totpg=pgcount;
	void *currentp=p;
	for(int i=0;i<MAXNRANK;i++)list_begin_node[i]=NULL;
	for(int i=MAXNRANK-1;i>=0;i--){
		if((pgcount>>i)&1){
			list_begin_node[i]=malloc(sizeof(struct node));
			list_begin_node[i]->next=NULL;
			list_begin_node[i]->p=currentp;
			list_begin_node[i]->use=0;
			pgrank[(currentp-beginp)/4096]=i;
			currentp+=4096*(1<<i);
		}
	}
	// printf("pgcount=%d\n",pgcount);
	// for(int i=0;i<MAXNRANK;i++){
	// 	printf("rank=%d,num=%d\n",i,CountUnuseNode(i));
	// }
	return OK;
}

void *alloc_pages(int rank){
	if(rank<1||rank>16)return -EINVAL;
	rank-=1;
	struct node *ansnode=NULL;
	int ansrank=0;
	for(int i=rank;i<MAXNRANK;i++){
		for(struct node *now=list_begin_node[i];now!=NULL;now=now->next){
			if(!now->use){
				ansnode=now,ansrank=i;
				break;
			}
		}
		if(ansnode!=NULL)break;
	}
	if(ansnode==NULL)return -ENOSPC;
	// printf("!!!!\n");
	// printf("rank=%d,ansrank=%d\n",rank,ansrank);
	for(int i=ansrank;i>rank;i--){
		struct node *p1=malloc(sizeof(struct node)),*p2=malloc(sizeof(struct node));
		p1->p=ansnode->p,p1->use=0,p1->next=p2;
		p2->p=ansnode->p+4096*(1<<(i-1)),p2->use=0,p2->next=list_begin_node[i-1];
		list_begin_node[i-1]=p1;
		pgrank[(p1->p-beginp)/4096]=pgrank[(p2->p-beginp)/4096]=i-1;
		DeleteFromList(i,ansnode);
		ansnode=p1;
	}
	// for(int i=0;i<MAXNRANK;i++){
	// 	printf("rank=%d,num=%d\n",i,CountUnuseNode(i));
	// }
	// printf("ans=%lx\n",ansnode->p);
	ansnode->use=1;
	return ansnode->p;
}

int return_pages(void *p){
	if(p<beginp||(p-beginp)%4096!=0||(p-beginp)/4096>totpg)return -EINVAL;
	int rank=-1;
	for(int i=0;i<MAXNRANK;i++){
		struct node *nodep=FindNode(0,p);
		if(nodep!=NULL){
			nodep->use=0;
			rank=i;
			break;
		}
	}
	assert(rank!=-1);
	while(1){
		void *p2=(p-beginp)%(4096*(1<<(rank+1)))==0 ? p+(4096*(1<<rank)) : p-(4096*(1<<rank));
		// printf("rank: %d -> %d\n",rank,rank+1);
		// printf("p=%lx,p2=%lx\n",p,p2);
		bool isDelete=FindAndDeleteNode(rank,p2);
		if(!isDelete)break;
		FindAndDeleteNode(rank,p);
		struct node *newnode=malloc(sizeof(struct node));
		newnode->next=list_begin_node[rank+1],newnode->p=p<p2?p:p2,newnode->use=0;
		list_begin_node[rank+1]=newnode;
		pgrank[(newnode->p-beginp)/4096]=rank+1;
		p=newnode->p;
		// printf("rank: %d -> %d\n",rank,rank+1);
		rank++;
	}
	return OK;
}

int query_ranks(void *p){
	if(p<beginp||(p-beginp)%4096!=0||(p-beginp)/4096>=totpg)return -EINVAL;
	return pgrank[(p-beginp)/4096]+1;
	// for(int i=MAXNRANK-1;i>=0;i--){
	// 	if(FindNode(i,p)!=NULL)return i+1;
	// }
	// assert(0);
	// return -1;
}

int query_page_counts(int rank){
	if(rank<1||rank>16)return -EINVAL;
	rank-=1;
	return CountUnuseNode(rank);
}
