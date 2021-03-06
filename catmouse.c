#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>      //used for aasertion
#include <unistd.h>
#include <sys/time.h>
#include <time.h>		//used for time functions 
#include <errno.h>		//defines macros for reporting and retrieving error conditions using the symbol//optional extra features 
#include <pthread.h>

#define n_dishes        2      //no of dishes 
#define n_cats          4 	   //no of cats 
#define cat_wait        15 		//time for cat to wait for taking the dish     
#define cat_eat         1 		//no 0f cats that can eat at a time     
#define cat_n_eat       4       //no of instances for cat to eat 
     
#define n_mice          2       //no 0f rats 
#define mouse_wait      1      //time for mouse or rat for taking the dish 
#define mouse_eat       1       //no of rats that can eat at a time 
#define mouse_n_eat     4       //no of instances of mouse to eat 

typedef struct dish {
    int free_dishes;            // how many dishes are free 
    int cats_eating;            // how many cats are eating at the moment 
    int mice_eating;            // how many mice are eating at the moment 
    int cats_waiting;           // how many cats are waiting for dish 
    enum {
        none_eating,			//no cat is eating 
        cat_eating,				//cat is eating 	
        mouse_eating			//mouse is eating 
    } 
	status[n_dishes];         // status of each dish 
    pthread_mutex_t mutex;      // mutex for accessing dish 
    pthread_cond_t free_cv;     // used to wait for a free dish 
    pthread_cond_t cat_cv;      // used to wait for coming cats 
} dish_t;

static const char *progname = "pets";
static void dump_dish(const char *name, pthread_t pet, const char *what, dish_t *dish, int my_dish)
{
    int i;
    struct tm t;
    time_t tt;
    
    tt = time(NULL);
    assert(tt != (time_t) -1);    //we have made an assertion here 
    localtime_r(&tt, &t);		  //time variable shwing when the actions are taking place 

    printf("%02d:%02d:%02d [", t.tm_hour, t.tm_min, t.tm_sec);
    for (i = 0; i < n_dishes; i++) {
        if (i) printf(":");
        switch (dish->status[i]) {
        case none_eating:
            printf("-");
            break;
        case cat_eating:
            printf("c");
            break;
        case mouse_eating:
            printf("m");
            break;
        }
    }
    printf("] %s (id %x) %s eating from dish %d\n", name, pet, what, my_dish);
}


void* mouse(void *arg)
{
    dish_t *dish = (dish_t *) arg;
    int n = mouse_n_eat;
    struct timespec ts;
    struct timeval tp;
    int my_dish;
    int i;

    for (n = mouse_n_eat; n > 0; n--) {
		 pthread_mutex_lock(&dish->mutex);
        while (dish->free_dishes <= 0 || dish->cats_eating > 0
               || dish->cats_waiting > 0) {
            pthread_cond_wait(&dish->free_cv, &dish->mutex);
        }
        assert(dish->free_dishes > 0);
        dish->free_dishes--;
        assert(dish->cats_eating == 0);
        assert(dish->mice_eating < n_mice);
        dish->mice_eating++;
        for (i = 0; i < n_dishes && dish->status[i] != none_eating; i++) ;
        my_dish = i;
        assert(dish->status[my_dish] == none_eating);
        dish->status[my_dish] = mouse_eating;
        dump_dish("mouse", pthread_self(), "started", dish, my_dish);
        pthread_mutex_unlock(&dish->mutex);
        gettimeofday(&tp,NULL);
        ts.tv_sec  = tp.tv_sec;
        ts.tv_nsec = tp.tv_usec * 1000;
        ts.tv_sec += mouse_eat;
        pthread_mutex_lock(&dish->mutex);
        pthread_cond_timedwait(&dish->cat_cv, &dish->mutex, &ts);
        pthread_mutex_lock(&dish->mutex);
        assert(dish->free_dishes < n_dishes);
        dish->free_dishes++;
        assert(dish->cats_eating == 0);
        assert(dish->mice_eating > 0);
        dish->mice_eating--;
        dish->status[my_dish]=none_eating;
        pthread_cond_broadcast(&dish->free_cv);
        dump_dish("mouse", pthread_self(), "finished", dish, my_dish);
        pthread_mutex_unlock(&dish->mutex);
        sleep(rand() % mouse_wait);
    }
return NULL;
}
void* cat(void *arg)
{
    dish_t *dish = (dish_t *) arg;
    int n = cat_n_eat;
    int my_dish = -1;
    int i;

    for (n = cat_n_eat; n > 0; n--) {

        pthread_mutex_lock(&dish->mutex);
       
        pthread_cond_broadcast(&dish->cat_cv);  			//initializes the specified condition variable
        dish->cats_waiting++;
        while (dish->free_dishes <= 0 || dish->mice_eating > 0) {
            pthread_cond_wait(&dish->free_cv, &dish->mutex);
        }
        dish->cats_waiting--;
        assert(dish->free_dishes > 0); 					//assertion 
        dish->free_dishes--;
        assert(dish->cats_eating < n_cats);				//assertion 
        dish->cats_eating++;
        
        for (i = 0; i < n_dishes && dish->status[i] != none_eating; i++) ;
        my_dish = i;
        assert(dish->status[my_dish] == none_eating);
        dish->status[my_dish] = cat_eating;
        dump_dish("cat", pthread_self(), "started", dish, my_dish);
        pthread_mutex_unlock(&dish->mutex);

        sleep(cat_eat);
        pthread_mutex_lock(&dish->mutex);
        assert(dish->free_dishes < n_dishes);
        dish->free_dishes++;
        assert(dish->cats_eating > 0);
        dish->cats_eating--;
        dish->status[my_dish] = none_eating;
        pthread_cond_broadcast(&dish->free_cv);
        dump_dish("cat", pthread_self(), "finished", dish, my_dish);
        pthread_mutex_unlock(&dish->mutex);
        sleep(rand() % cat_wait);		//raand() and srand() used in C to generate random numbers
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    int i, err;
    dish_t _dish, *dish;
    pthread_t cats[n_cats];
    pthread_t mice[n_mice];

    srand(time(NULL));  
    dish = &_dish;
    memset(dish, 0, sizeof(dish_t));
    dish->free_dishes = n_dishes;
    pthread_mutex_init(&dish->mutex, NULL);
    pthread_cond_init(&dish->free_cv, NULL);
    pthread_cond_init(&dish->cat_cv, NULL);
    for (i = 0; i < n_cats; i++) {
        err = pthread_create(&cats[i], NULL, cat, dish);
        if (err != 0) {
            fprintf(stderr, "%s: %s: unable to create cat thread %d: %d\n",
                    progname, __func__, i, err);
        }
    }
    for (i = 0; i < n_mice; i++) {
        err = pthread_create(&mice[i], NULL, mouse, dish);
        if (err != 0) {
            fprintf(stderr, "%s: %s: unable to create mouse thread %d: %d\n",
                    progname, __func__, i, err);
        }
    }
    for (i = 0; i < n_cats; i++) {
        (void) pthread_join(cats[i], NULL);
    }
    for (i = 0; i < n_mice; i++) {
        (void) pthread_join(mice[i], NULL);
    }
    pthread_mutex_destroy(&dish->mutex);
    pthread_cond_destroy(&dish->free_cv);
    pthread_cond_destroy(&dish->cat_cv);
    
    return EXIT_SUCCESS;
}
