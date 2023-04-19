#include "buddy.h"
#include<stdbool.h>
#include<assert.h>
#include<stdlib.h>
#include<stdio.h>
#define NULL ((void *)0)
#define MAXNRANK 16
struct node{
	struct node *pre,*next;
	void *p;//beginning
	bool use;
	int rank;
};

struct node *list_begin_node[MAXNRANK];
void *beginp;
int totpg;
struct node **p_to_node;
static void InsertNode(int truerank,void *p){
	struct node *nodep=malloc(sizeof(struct node));
	nodep->p=p,nodep->use=0,nodep->next=list_begin_node[truerank],nodep->pre=NULL,nodep->rank=truerank;
	if(list_begin_node[truerank]!=NULL)list_begin_node[truerank]->pre=nodep;
	list_begin_node[truerank]=nodep;
	p_to_node[(nodep->p-beginp)/4096]=nodep;
}
static void DeleteNode(struct node *p){
	if(p->pre!=NULL)p->pre->next=p->next;
	if(p->next!=NULL)p->next->pre=p->pre;
	if(p==list_begin_node[p->rank])list_begin_node[p->rank]=list_begin_node[p->rank]->next;
	free(p);
}
static bool FindAndDeleteNode(int truerank,void *p){//查看是否存在&&rank=truerank&&use=0,如果是则删掉并返回1
	int pos=(p-beginp)/4096;
	if(pos==totpg)return 0;
	if(p_to_node[pos]==NULL)return 0;
	if(p_to_node[pos]->rank!=truerank)return 0;
	if(p_to_node[pos]->use)return 0;
	DeleteNode(p_to_node[pos]);
	return 1;
}
static struct node* FindNode(int truerank,void *p){
	int pos=(p-beginp)/4096;
	if(p_to_node[pos]==NULL||p_to_node[pos]->rank!=truerank)return NULL;
	return p_to_node[pos];
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
	free(p_to_node);
}

int init_page(void *p, int pgcount){
	//void *p,p++实际地址加1(void *可以看成char*)
	p_to_node=malloc(8*pgcount);
	for(int i=0;i<pgcount;i++)p_to_node[i]=NULL;
	beginp=p,totpg=pgcount;
	void *currentp=p;
	for(int i=0;i<MAXNRANK;i++)list_begin_node[i]=NULL;
	for(int i=MAXNRANK-1;i>=0;i--){
		if((pgcount>>i)&1){
			list_begin_node[i]=malloc(sizeof(struct node));
			list_begin_node[i]->next=NULL;
			list_begin_node[i]->pre=NULL;
			list_begin_node[i]->p=currentp;
			list_begin_node[i]->use=0;
			list_begin_node[i]->rank=i;
			p_to_node[(currentp-beginp)/4096]=list_begin_node[i];
			currentp+=4096*(1<<i);
		}
	}
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
	if(ansrank>rank){
		void *currentp=ansnode->p;
		DeleteNode(ansnode);
		for(int i=ansrank-1;i>=rank;i--){
			if(i==rank){
				InsertNode(i,currentp+4096*(1<<i));
				InsertNode(i,currentp);
				p_to_node[(currentp-beginp)/4096]->use=1;
				return currentp;
			}
			else {
				InsertNode(i,currentp+4096*(1<<i));
			}
		}
	}
	else {
		ansnode->use=1;
		return ansnode->p;
	}
}

int return_pages(void *p){
	if(p<beginp||(p-beginp)%4096!=0||(p-beginp)/4096>totpg)return -EINVAL;
	int pos=(p-beginp)/4096;
	if(p_to_node[pos]==NULL)return -EINVAL;
	
	int rank=p_to_node[pos]->rank;
	// p_to_node[pos]->use=0;
	for(int i=0;i<MAXNRANK;i++){
		struct node *nodep=FindNode(i,p);
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
		// printf("p=%lx,p2=%lx,xxx=%lx\n",p,p2,p2-beginp);
		bool isDelete=FindAndDeleteNode(rank,p2);
		if(!isDelete)break;
		FindAndDeleteNode(rank,p);
		p_to_node[(p-beginp)/4096]=p_to_node[(p2-beginp)/4096]=NULL;
		InsertNode(rank+1,p<p2?p:p2);
		p=p<p2?p:p2;
		// printf("rank: %d -> %d\n",rank,rank+1);
		rank++;
	}
	return OK;
}

int query_ranks(void *p){
	if(p<beginp||(p-beginp)%4096!=0||(p-beginp)/4096>=totpg)return -EINVAL;
	return p_to_node[(p-beginp)/4096]->rank+1;
}

int query_page_counts(int rank){
	if(rank<1||rank>16)return -EINVAL;
	rank-=1;
	return CountUnuseNode(rank);
}
