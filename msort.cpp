#include <pthread.h>
#include <vector>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <algorithm>

using namespace std;

vector<int> l_array;
vector<int> r_array;
vector<int> rsizes;

int n;

pthread_mutex_t queue_mutex, thread_mutex, sync_mutex;
pthread_cond_t  cond, sync_cond; 

void extract_task(int &l, int &r, int &rsize)
{
    pthread_mutex_lock(&queue_mutex);
	l = l_array.back(); r = r_array.back(); rsize = rsizes.back();
	l_array.pop_back(); r_array.pop_back(); rsizes.pop_back();
    pthread_mutex_unlock(&queue_mutex);
}

void push_task(int l, int r, int rsize)
{
	pthread_mutex_lock(&queue_mutex);
	l_array.push_back(l);
	r_array.push_back(r);
	rsizes.push_back(rsize);
	pthread_mutex_unlock(&queue_mutex);
}

void queue_size(int &size)
{
    pthread_mutex_lock(&queue_mutex);
	size = rsizes.size();
	pthread_mutex_unlock(&queue_mutex);
}

int cmp(const void *first, const void *second)
{
    if (*(int *)first - *(int *)second < 0)
        return 1;
    else if (*(int *)first - *(int *)second > 0)
        return -1;
    return 0;
}

void swap(int* a, int* b)
{
	int c = *a; 
	*a = *b; 
	*b = c;
}

void merge(int *A, int l, int r, int rsize)
{
    if (l < 0 || r + rsize > n) return;

    int *pArray = &A[l];
    int lsize = r - l;
    int size_of_block = lsize + rsize;
    int *buff = (int*)malloc(size_of_block * sizeof(int));
    int i = 0;

    while (lsize && rsize)
    {
        if (A[l] >= A[r])
        {
            buff[i++] = A[l++];
            lsize--;
        }
        else
        {
            buff[i++] = A[r++];
            rsize--;
        }
    }

    for (;lsize-- > 0;) swap(buff[i++], A[l++]);
    for (;rsize-- > 0;) swap(buff[i++], A[r++]);
    
    memcpy(pArray, buff, size_of_block * sizeof(int));
    free(buff);
}

int working_threads = 0;
void* scheduler(void* _)
{
    int *A = (int*)_;
    while (true)
    {
        pthread_mutex_lock(&thread_mutex);
        pthread_cond_wait(&cond, &thread_mutex);
        pthread_mutex_unlock(&thread_mutex);

        int l, r, rsize;
		extract_task(l, r, rsize);
		
        merge(A, l, r, rsize);

        working_threads--;
        int size;
        queue_size(size);

		if (size >= 1)
        {
            pthread_mutex_lock(&thread_mutex);
            pthread_cond_signal(&cond);
            pthread_mutex_unlock(&thread_mutex);
        }
        else 
        {
            pthread_mutex_lock(&sync_mutex);
            pthread_cond_signal(&sync_cond);
            pthread_mutex_unlock(&sync_mutex);
        }
    }
}

int main()
{
    FILE* input = fopen("input.txt", "r");
	FILE* time_file = fopen("time.txt", "w"),
		* output = fopen("output.txt", "w");

	int threads_cnt;
    fscanf(input, "%d%d", &threads_cnt, &n);
	
	int *A = (int*)calloc(n, sizeof(int));
	for (int i = 0; i < n; i++)
		fscanf(input, "%d", A + i);

	pthread_t* threads = (pthread_t*)malloc(sizeof(pthread_t) * threads_cnt);

	pthread_mutex_init(&queue_mutex, NULL);
	pthread_mutex_init(&thread_mutex, NULL);
	pthread_mutex_init(&sync_mutex, NULL);

    pthread_cond_init(&cond, NULL);
    pthread_cond_init(&sync_cond, NULL);

	for (int i = 0; i < threads_cnt; i++)
		pthread_create(&threads[i], NULL, scheduler, (void*)A);
    usleep(100000);
    clock_t start = clock();

    int block_size = n / threads_cnt;
    vector<int> blocks_indexes;
    int i;
    
    for (i = 0; i + block_size < n; i += block_size)
        qsort(&A[i], block_size, sizeof(int), cmp),
        blocks_indexes.push_back(i);
    if (i < n)
        qsort(A + i, n - i, sizeof(int), cmp),
        blocks_indexes.push_back(i);

    
	while (blocks_indexes.size() != 1)
    {
        vector<int> tmp;
        while (blocks_indexes.size() > 1)
        {
            int r_start = blocks_indexes.back();
            blocks_indexes.pop_back();
            int l_start = blocks_indexes.back();
            blocks_indexes.pop_back();

            if (l_start > r_start) swap(l_start, r_start);

            int r_end = 2 * r_start - l_start;
            tmp.push_back(l_start);
            push_task(l_start, r_start, r_end - r_start);
        }

        blocks_indexes.insert(blocks_indexes.begin(), tmp.rbegin(), tmp.rend());

        pthread_mutex_lock(&sync_mutex);
        pthread_cond_signal(&cond);
        pthread_cond_wait(&sync_cond, &sync_mutex);
        pthread_mutex_unlock(&sync_mutex);
        usleep(20000);
    }

	clock_t end = clock();
	
	for (int i = 0; i < threads_cnt; i++)
        pthread_cancel(threads[i]);

    qsort(&A[0], n, sizeof(int), cmp);

    fprintf(output, "%d\n%d\n", threads_cnt, n);
	for (int i = n - 1; i >= 0; i--)
		fprintf(output, "%d ", A[i]);
    fprintf(output, "\n");

    fprintf(time_file, "%d\n", (end - start) * 1000 / CLOCKS_PER_SEC);

	fclose(output);
	fclose(time_file);

	free(A);
	free(threads);
}