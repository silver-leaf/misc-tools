#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <gmodule.h>

int g_pagesize=4096;
unsigned long total_cachesize=0;
unsigned long total_cachfile=0;
GSList *g_list = NULL;

struct fcore_object{
	long fsize;
	long cachsize;
	char fname[0];
};

int fcore_object_cmp(gconstpointer obj1, gconstpointer obj2){
	 struct fcore_object *fobj1=obj1;
	 struct fcore_object *fobj2=obj2;
	 return (fobj1->cachsize) < (fobj2->cachsize) ;    // > 为真，表示从小到大排序。
	
};

int insort_sortlist(struct fcore_object*obj){
	if(g_list==NULL)
		g_list = g_slist_append(g_list, obj);
	else {
		g_list = g_slist_insert_sorted(g_list,obj,fcore_object_cmp);
	}
	return 1;
};

int fincore(char* path){
	struct stat fst;
	 int fd=0,cached=0;
	 void *file_mmap=NULL;
	 char *mincore_vec=NULL;
	 int vec_size=0;
	 int page_index=0;
	 struct fcore_object *fobject=NULL;
	fd = open(path,O_RDONLY); 
	
	if(fd<=0 || fstat(fd,&fst)<0){
		//printf("open [%s] failed,errno=%d,%s\n",path,errno,strerror(errno));
	 	return 0;
	 }
	 
	 if(fst.st_size==0){
	 	 close(fd);
		 return 0;
	 }
		
	 file_mmap=mmap(0,fst.st_size, PROT_NONE,MAP_SHARED,fd,0);
	 if(file_mmap<=0){
		 	close(fd); 
	 		return 0;
	 }
			
	 vec_size = (fst.st_size+ g_pagesize -1) / g_pagesize ;
	 mincore_vec = calloc(1,vec_size);
	 if(mincore_vec == NULL) {
	 	munmap(file_mmap,fst.st_size);
		 close(fd);
	     return 0;
	 }
	
	 if(mincore(file_mmap,fst.st_size, mincore_vec)!=0){
	     free(mincore_vec);
		 munmap(file_mmap,fst.st_size);
		 close(fd);
	 	return 0;
	 }
	 for(page_index=0; page_index < vec_size ; page_index++) {
	     if(mincore_vec[page_index]&1){
		 	cached++;
		 }
	 }
	if(cached > 0) {
		total_cachesize+=cached;
		//printf("file [%s size:%d,cached:%d]\n",path,fst.st_size,cached*g_pagesize);
		fobject=malloc(sizeof(struct fcore_object)+strlen(path)+1);
		if(fobject){	
			fobject->fsize=fst.st_size;
			fobject->cachsize=cached * g_pagesize;
			strcpy(fobject->fname,path);
			insort_sortlist(fobject);
			total_cachfile++;
		}	
	}
	free(mincore_vec);
	munmap(file_mmap,fst.st_size);
	close(fd);
	return 1;

};

int read_filelist(char *Parentdir){
	DIR*dir;
	struct dirent *pdir;
	char path[1024]={0};
	
	dir=opendir(Parentdir);
	if(dir==NULL){
		return 0;
	}
	while((pdir=readdir(dir)) !=NULL){
		if(strncmp(pdir->d_name,".",strlen(pdir->d_name))==0 || 
			strncmp(pdir->d_name,"..",strlen(pdir->d_name))==0)
					continue;
					
		memset(path,0,sizeof(path));
		strcat(path,Parentdir);
		strcat(path,"/");
		strcat(path,pdir->d_name);
		if (pdir->d_type == DT_REG)	
			fincore(path);
		else if (pdir->d_type == DT_DIR)
			read_filelist(path);			
	}
	closedir(dir);
	return 1;
};

int main(int argc,char*argv[]){
	GSList  * iterator  =  NULL;
	char *parent=NULL;
	struct fcore_object *obj;

	int top_number=20; // default show top 20
	int i=0,c=0;
	static struct option options[] = {   
        {"top", 1, 0, 't'},
        {"help", 0, 0, 'h'}, 
        {0, 0, 0, 0}
    };
	if(argc==1){
		printf("%s [-t top] dir\n",argv[0]);
		return 0;
	}
	while (1) {
        c = getopt_long(argc, argv, "ht:", options, 0);
        if (c == -1)
            break;
        switch (c) {
            case 't':
                top_number = atoi(optarg); 
                break;
			case 'h':	
				 printf("%s [-t top] dir\n",argv[0]);
				 exit(0);
        }
    }
	if(optind >= argc)
		 return 0;
	g_pagesize=getpagesize();
	parent = argv[optind];
	read_filelist(parent);
	printf("Total files pagecache size=%dM\t total cachfiles=%d\n",total_cachesize*g_pagesize/1024/1024,total_cachfile);
    if(top_number){
		printf("Show top %d pagecaches file:\n",top_number);
	}
	for(iterator = g_list; iterator; iterator=iterator-> next){
		 obj=iterator->data;
		 i++;
		 if(obj->cachsize<524288)
			printf( "  cachsize=%8.2fKB  \t[%s]\n",obj->cachsize/1024.0,obj->fname); 
		 else
         	printf( "  cachsize=%8.2fMB  \t[%s]\n",obj->cachsize/1024/1024.0,obj->fname);
		 if((top_number>0 )&&(i>top_number))
			 break;
    }
	g_slist_free(g_list);
	return 0;
}
