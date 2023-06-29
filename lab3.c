#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/resource.h>
#include <unistd.h>


#define BLOCK_SIZE 24
#define NUM_OF_THREADS 12
#define NUM_OF_CYCLES 2


void *first_block = NULL;
pthread_mutex_t mutex;

typedef struct s_block *t_block;

struct s_block {
    size_t size;
    t_block prev;
    t_block next;
    int free;
    int padding;
    void *ptr;
    char data[1];
};

t_block find_block(t_block *last, size_t size) {
    t_block b = first_block;

    while (b && !(b->free && b->size >= size)) {
	*last = b;
	b = b->next;
    }
    return b;
}

t_block extend_heap(t_block last, size_t s) {
    t_block b;
    b = sbrk(0);
    if (sbrk(BLOCK_SIZE + s) == (void *)-1) {
	return NULL;
    }
    b->size = s;
    b->next = NULL;
    if (last) {
	last->next = b;
    }
    b->free = 0;
    return b;
}

void split_block(t_block b, size_t s) {
    t_block new_;

    new_ = (t_block)(b->data + s);
    // new_ = b->data + s;
    new_->size = b->size - s - BLOCK_SIZE;
    new_->next = b->next;
    new_->free = 1;

    b->size = s;
    b->next = new_;
}

size_t align8(size_t s) {
    if ((s & 0x7) == 0) {
	return s;
    }
    return ((s >> 3) + 1) << 3;
}

t_block get_block(void *p) {
    char *tmp;
    tmp = p;
    return (p = tmp -= BLOCK_SIZE);
}

int valid_addr(void *p) {
    if (first_block) {
	if (first_block < p && p < sbrk(0)) {
	    return p == (get_block(p))->ptr;
	}
    }
    return 0;
}

t_block fusion(t_block b) {
    if (b->next && b->next->free) {
	b->size += BLOCK_SIZE + b->next->size;
	b->next = b->next->next;
	if (b->next) {
	    b->next->prev = b;
	}
    }
    return b;
}

void copy_block(t_block src, t_block dst) {
    size_t *sdata;
    size_t *ddata;
    sdata = src->ptr;
    ddata = dst->ptr;
    for (size_t i = 0; (i * 8) < src->size && (i * 8) < dst->size; ++i) {
	ddata[i] = sdata[i];
    }
}


void *my_malloc(size_t size) {
    pthread_mutex_lock(&mutex);

    t_block b, last;
    size_t s;
    s = align8(size);

    if (first_block) {
	last = (t_block)first_block;
	b = find_block(&last, s);
	if (b) {
	    if ((b->size - s) >= (BLOCK_SIZE + 8)) {
		split_block(b, s);
	    }
	    b->free = 0;
	}
	else {
	    b = extend_heap(last, s);
	    if (!b) {
		return NULL;
	    }
	}
    }
    else {
	b = extend_heap(NULL, s);
	if (!b) {
	    return NULL;
	}
	first_block = b;
    }
    pthread_mutex_unlock(&mutex);
    return b->data;
}


void *my_calloc(size_t number, size_t size) {
    size_t *new_ = NULL;
    size_t s8;
    new_ = (size_t *)my_malloc(number * size);

    if (new_) {
	s8 = align8(number * size) >> 3;

	for (size_t i = 0; i < s8; ++i) {
	    new_[i] = 0;
	}
    }

    return new_;
}


void *my_realloc(void *p, size_t size) {
    size_t s;
    t_block b, new_;
    void *newp;
    if (!p) {
	return my_malloc(size);
    }
    if (valid_addr(p)) {
	s = align8(size);
	b = get_block(p);
	if (b->size >= s) {
	    if (b->size - s >= (BLOCK_SIZE + 8)) {
		split_block(b, s);
	    }
	}
	else {
	    if (b->next && b->next->free && (b->size + BLOCK_SIZE + b->next->size) >= s) {
		fusion(b);
		if (b->size - s >= (BLOCK_SIZE + 8)) {
		    split_block(b, s);
		}
	    }
	    else {
		newp = my_malloc(s);
		if (!newp) {
		    return NULL;
		}
		new_ = get_block(newp);
		copy_block(b, new_);
		free(p);
		return newp;
	    }
	}
	return p;
    }
    return NULL;
}

void my_free(void *p){
    pthread_mutex_lock(&mutex);
    t_block b;
    if (valid_addr(p)) {
	b = get_block(p);
	b->free = 1;
	if (b->prev && b->prev->free) {
	    b = fusion(b->prev);
	}
	if (b->next) {
	    fusion(b);
	}
	else {
	    if (b->prev) {
		b->prev->prev = NULL;
	    } else {
		first_block = NULL;
	    }
	    brk(b);
	}
    }
    pthread_mutex_unlock(&mutex);
}

struct test {
    int *big;
    int *small;
    int i;
};

void *alloc_f(struct test *arg) {
    arg->big = my_malloc(sizeof(int) * 256);
    arg->small = my_malloc(sizeof(int) * 4);
    printf("%d stream: \n Big adress: \t%p\n Small adress: \t%p\n", arg->i, arg->big, arg->small);
}

void *add(struct test *arg) {
    for (int i = 0; i < 256; ++i) {
	arg->big[i] = i;
    }
    for (int i = 0; i < 4; ++i) {
	arg->small[i] = i;
    }
}

void *result(struct test *arg) {
    FILE* file = fopen("test.txt", "a");
    fprintf(file, "\n\nBig (count: 16):\n");
    for (int i = 0; i < 256; i += 16) {
	fprintf(file, "%d ", arg->big[i]);
    }
    fprintf(file, "\n\nSmall (count: 1)^\n");
    for (int i = 0; i < 4; i++) {
	fprintf(file, "%d ", arg->small[i]);
    }
    fprintf(file, "\n");

    my_free(arg->big);
    my_free(arg->small);

    fclose(file);
}

int checker(result) {
    if (result) {
	exit(EXIT_FAILURE);
    }
    return 0;
}

int main() {

    pthread_mutex_init(&mutex, NULL);

    for (int z = 0; z < NUM_OF_CYCLES; z++){
	printf("\n%d iteration\n", z);
	pthread_t threads[NUM_OF_THREADS];
	struct test testing[NUM_OF_THREADS/3];
	size_t i;

	for (i = 0; i < NUM_OF_THREADS/3; i++) {
	    struct test* stream_testing = &(testing[i%(NUM_OF_THREADS/3)]);
	    stream_testing->i = i;
	    checker(pthread_create(&threads[i], NULL, alloc_f, stream_testing));
	}

	for (i = 0; i < NUM_OF_THREADS/3; i++) {
	    checker(pthread_join(threads[i], NULL));
	}

	for (i = NUM_OF_THREADS/3; i < 2 * NUM_OF_THREADS/3; i++) {
	    checker(pthread_create(&threads[i], NULL, add, &(testing[i%(NUM_OF_THREADS/3)])));
	}

	for (i = NUM_OF_THREADS/3; i < 2 * NUM_OF_THREADS/3; i++) {
	    checker(pthread_join(threads[i], NULL));
	}

	for (i = 2 * NUM_OF_THREADS/3; i < NUM_OF_THREADS; i++) {
	    checker(pthread_create(&threads[i], NULL, result, &(testing[i%(NUM_OF_THREADS/3)])));
	}

	for (i = 2 * NUM_OF_THREADS/3; i < NUM_OF_THREADS; i++) {
	    checker(pthread_join(threads[i], NULL));
	}
    }

    pthread_mutex_destroy(&mutex);
    return 0;
}
