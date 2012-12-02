#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <stdlib.h>	/* needed for os x*/
#include <string.h>	/* for strlen */
#include <netdb.h>      /* for gethostbyname() */
#include <sys/socket.h>
#include <netinet/in.h>
#include <fuse.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>

/* Authors: Joshua Beninson, Dean Douvikas, Omar Raja */


static struct Stock_INFO{ 
/* Struct used to hold data of any stock queried and different structs are maintained via a linked list. 
Each struct respresents a file in our directory.*/
	char Symbol [10];
	char CompanyName [20];
	char CurPrice[10];
	char Change[10];
	char Bid[10];
	char Ask[10];	
	char BidSize[10];
	char AskSize[10];
	int ID;
	int Favorite;
	struct Stock_INFO *next;
}*root;

struct Stock_INFO *stock_search(char *stock){
/* Function used to return pointer to particular's stock data if it is a file in our directory. 
Returns NULL if file not found */
	struct Stock_INFO *cur;
	
	for(cur=root;cur!=NULL;cur=cur->next){
		if(strncmp(cur->Symbol,stock,strlen(stock))==0){
			return cur;
		}	
	}	
	return cur;		
}

struct Stock_INFO *last_nextptr(void){
/* Used to traverse linked list to find location of last link in the list in order to be able 
to use it's next pointer to add another link to the end of the list */
	struct Stock_INFO *cur = root;
	if(root==NULL){}
	else
		while(cur->next!=NULL)
			cur=cur->next;
	return cur;
}

int is_emptylist(void){
/* Determines if our list is empty */
	if(root==NULL)
		return 1;		
	else
		return 0;	
}



static int StockFS_getattr(const char *path, struct stat *stbuf){
/* Returns attributes to the virtual file system depending on the nature of what is being called. 
Attributes passed into FUSE's special stat structure. */
	int res = 0;	
	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) { /*Is it a directory?*/
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} 
	else{	/*This is for everything else*/
		stbuf->st_mode = S_IFREG | 0666;
		stbuf->st_nlink = 1;
		stbuf->st_size = 4096;
	}
	return res;
}
	
static int StockFS_readdir(const char *path, void *buf, fuse_fill_dir_t filler,	off_t offset, struct fuse_file_info *fi){
/*Increments through the linked list and prints out all the ticker "symbols" in each file that exists in directory */ 	
    	(void) offset;
    	(void) fi;
	
    	if(strcmp(path, "/") != 0){
          	return -ENOENT;
  	}
    	filler(buf, ".", NULL, 0);
   	filler(buf, "..", NULL, 0);
    	struct Stock_INFO* cur;
	if(root!=NULL){ /*Is list empty?*/
		cur = root; 
		filler(buf,cur->Symbol,NULL,0);
		for(cur=root;cur->next!=NULL;cur=cur->next){ /*Iterate through touched files and print*/
			filler(buf,cur->next->Symbol,NULL,0);
		}
	}
	return 0;
}



static int StockFS_open(const char *path, struct fuse_file_info *fi)
 {
/*Creates a socket and sends request to Yahoo server. Response received and parsed. Parsed input stored in file */
	char *path_copy;
	path_copy=strdup(path);
	struct Stock_INFO* cur;
	cur=malloc(sizeof(struct Stock_INFO));
	struct Stock_INFO* temp=cur;
	if(is_emptylist()==1){
	}
	else if((cur=stock_search(path_copy+1))!=NULL){		
		free(temp);
		cur = stock_search(path_copy+1);
	}	
	else {
		cur=temp;
	}

	(void) fi;
	int fd;	/* file descriptor for socket */
	struct hostent *hent;
	struct sockaddr_in servaddr;
	
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		return 0;
	}

	memset((char*)&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(80);
	
	hent = gethostbyname("download.finance.yahoo.com");
	memcpy(&servaddr.sin_addr, hent->h_addr, hent->h_length);
	
	/* connect to server */
	if (connect(fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) 		{
		close(fd); 
		return -1;
	}
	
	char out_buffer [128] = "GET /d/quotes.csv?s=";
	char last [] = "&f=snl1c1bab6a5 HTTP/1.0\n\n";
	char in_buffer [512];
	memset(in_buffer,0,sizeof(in_buffer));
	char delims_rn [] = "\r\n";
	char delims_quote [] = "\"";
	char delims_comma [] = ",";
	char *result = NULL;
	char *save_ptr = NULL;
	char *parsed = NULL;

	/*Form Get Request from string segments*/
	
	strcat(out_buffer + 20,path_copy+1);
	strcat(out_buffer + 20 + strlen(path_copy+1),last);

	send(fd,out_buffer,strlen(out_buffer),0);
	recv(fd,in_buffer, 512,0);
	save_ptr = in_buffer;
	result = strtok_r(in_buffer, delims_rn,&save_ptr);
	while(result != NULL){
		result = strtok_r(NULL, delims_rn,&save_ptr);
		if(result)
     			parsed=result;
     		
	}

	save_ptr = parsed+1;
	result = strtok_r(NULL,delims_quote,&save_ptr);

	strcpy(cur->Symbol,result);

	save_ptr=save_ptr+2;
	result = strtok_r(NULL,delims_quote,&save_ptr);
	strcpy(cur->CompanyName,result);

	save_ptr=save_ptr+1;
 	result = strtok_r(NULL,delims_comma,&save_ptr);
	strcpy(cur->CurPrice, result);

	result = strtok_r(NULL, delims_comma,&save_ptr);
	strcpy(cur->Change, result);

	result = strtok_r(NULL,delims_comma,&save_ptr);
	strcpy(cur->Bid,result);

	result = strtok_r(NULL,delims_comma,&save_ptr);
	strcpy(cur->Ask, result);
	
	result = strtok_r(NULL,delims_comma,&save_ptr);
	strcpy(cur->BidSize, result);
	
	result = strtok_r(NULL,delims_comma,&save_ptr);
	strcpy(cur->AskSize, result);

	/*close the socket*/
	close(fd);
	
	/* This if statement will check to see if it is a valid stock, if it isn't it will return an error
	and free the memory block */
	if(strcmp(cur->CurPrice,"0.00")==0){		
		free(cur);
		return -ENOENT;
	}	
	
	
	
	/* Check to see if list is empty, if it is, the root will point directly to this block
	to make this the first file in the list, otherwise have the last next pointer
	point to the cur block to put it at the end of the existing list */
	else{
		struct Stock_INFO *tempnew = NULL;
		if(is_emptylist()==0){
			if((tempnew=stock_search(path_copy+1))==NULL) {
				last_nextptr()->next=cur;
				cur->next=NULL;
			}
		}
		else{
			root=cur;
		}
	}
	 return 0;
 }

static int StockFS_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
/* Used to read directory contents. Searches for file designated by path name and passes the file's information 
into the buffer. */
	size_t len;
	(void) fi;
	(void) offset;
	char *path_copy=strdup(path);
	struct Stock_INFO *cur;

	if((cur=stock_search(path_copy+1))==NULL){
		return -ENOENT;
	}
	else{
		char buf_set[1024]="Symbol: ";
		memcpy(buf_set+strlen(buf_set),cur->Symbol, strlen(cur->Symbol));
		
		memcpy(buf_set+strlen(buf_set),"\nCompany Name: ", strlen("Company Name: "));
		memcpy(buf_set+strlen(buf_set),cur->CompanyName, strlen(cur->CompanyName));
		
		memcpy(buf_set+strlen(buf_set),"\nCurrent Price: ", strlen("Current Price: "));
		memcpy(buf_set+strlen(buf_set),cur->CurPrice, strlen(cur->CurPrice));
		
		memcpy(buf_set+strlen(buf_set),"\nChange: ", strlen("Change: "));
		memcpy(buf_set+strlen(buf_set),cur->Change, strlen(cur->Change));
		
		memcpy(buf_set+strlen(buf_set),"\nBid: ", strlen("Bid: "));
		memcpy(buf_set+strlen(buf_set),cur->Bid, strlen(cur->Bid));
		
		memcpy(buf_set+strlen(buf_set),"\nAsk: ", strlen(cur->Ask));
		memcpy(buf_set+strlen(buf_set),cur->Ask, strlen(cur->Ask));
		
		memcpy(buf_set+strlen(buf_set),"\nBid Size: ", strlen("Bid Size: "));
		memcpy(buf_set+strlen(buf_set),cur->BidSize, strlen(cur->BidSize));
		
		memcpy(buf_set+strlen(buf_set),"\nAsk Size: ", strlen("Ask Size: "));
		memcpy(buf_set+strlen(buf_set),cur->AskSize, strlen(cur->AskSize));
		memcpy(buf_set+strlen(buf_set),"\n\n", strlen("\n\n"));

		len=strlen(buf_set);
		memcpy(buf,buf_set,len);
	}
	size = len;
	return size;
	
}

static int StockFS_utime(const char *path, struct utimbuf *time){
/*If touch command, utime called after open and used to designate file that was just opened to be 
designated as a favorite. */
	char *path_copy=strdup(path);
	struct Stock_INFO *cur;
	if((cur=stock_search(path_copy+1))!=NULL){
		cur->Favorite=1;
	}
	return 0;
}
	
static int StockFS_release(const char *path, struct fuse_file_info *fi){
/*In the event that a file is opened that wasn't designated a favorite, it will be released after its 
information has been passed to the buffer to be read */
	
	(void) fi;
	
	char *path_copy=strdup(path);
	struct Stock_INFO *cur;
	struct Stock_INFO *erase;
	
	
	if((erase=stock_search(path_copy+1))==NULL){
	}
	else if(erase->Favorite==1){
		return 0;
	}
	else{
		if(erase==root){
			if(root->next!=NULL)
			{
				root=root->next;
			}
			free(erase);
			root=NULL;
		}
		else{
			for(cur=root;cur->next!=erase;cur=cur->next){}
			cur->next=erase->next;
			free(erase);
		}
	}
	return 0;
}

static struct fuse_operations StockFS_oper = {
/* Struct containing function pointers to functions which allows main method to link to fuse functions */
	.getattr	= StockFS_getattr,
	.readdir	= StockFS_readdir,
	.open		= StockFS_open,
	.release	= StockFS_release,
	.read		= StockFS_read,
	.utime		= StockFS_utime, 
};


int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &StockFS_oper, NULL);
}
