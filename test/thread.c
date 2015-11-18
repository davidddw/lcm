#include <pthread.h>  
#include <unistd.h>  
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
      
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;  
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;  

static int main_cnt = 0;
static int thread_cnt = 0;

struct node {  
    int n_number;  
    struct node *n_next;  
} *head = NULL;  
    
FILE *fp;
void lc_ker_log(char *str) {
    fwrite(str, strlen(str), 1, fp);
    fflush(fp);

}
/*[thread_func]*/  
static void cleanup_handler(void *arg)  
{  
    printf("Cleanup handler of second thread.\n");  
    free(arg);  
    (void)pthread_mutex_unlock(&mtx);  
}  

static void *thread_func(void *arg)  
{  
    struct node *p = NULL;  

    pthread_cleanup_push(cleanup_handler, p);  
    while (1) {  
        pthread_mutex_lock(&mtx);
        while (head == NULL)   {
            pthread_cond_wait(&cond, &mtx);
        }  
        thread_cnt++;
        p = head;  
        head = head->n_next;  
        lc_ker_log("thread in process\n");
        free(p);  
        pthread_mutex_unlock(&mtx);
   }  
   pthread_cleanup_pop(0);  
   return 0;  
}  



int main(void)  
{  
    pthread_t tid;  
    int i;  
    struct node *p;  
    fp = fopen("/tmp/log.txt", "a");
    pthread_create(&tid, NULL, thread_func, NULL); 
    for (i = 0; i < 20000; i++) {  
        p = malloc(sizeof(struct node));  
        p->n_number = i;  
        pthread_mutex_lock(&mtx); 
        main_cnt++;
        p->n_next = head;  
        head = p;  
        pthread_cond_signal(&cond);  
        pthread_mutex_unlock(&mtx);
 //       sleep(1);  
    }  
    sleep(5);
    printf("thread 1 wanna end the line.So cancel thread 2.\n");  
    pthread_cancel(tid);
    pthread_join(tid, NULL);  
    printf("All done -- exiting\n");  

    printf("main cnt=%d thread cnt=%d\n", main_cnt, thread_cnt);
    return 0;  
}  
