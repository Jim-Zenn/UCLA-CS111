/*
 * lab2_list.c
 * Copyright (C) 2019 Qingwei Zeng <zenn@ucla.edu>
 *
 * Distributed under terms of the MIT license.
 */

#include "SortedList.h"

#include <errno.h>
#include <getopt.h>
#include <memory.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PROGRAM_NAME "lab2a-list"
#define AUTHORS proper_name("Qingwei Zeng")

#define THREADS_SHORT_OPTION 256
#define ITERATIONS_SHORT_OPTION 257
#define YIELD_SHORT_OPTION 258
#define SYNC_SHORT_OPTION 259

#define EXIT_WRONG_RESULT 2

#define KEY_LENGTH 512

int thread_n = 1;  // number of parallel threads, defaults to 1
long iter_n = 1;   // number of iterations, defaults to 1
long list_len = 0;

char opt_sync;
int opt_yield = 0;

int spin_lock = 0;
pthread_mutex_t mutex;
char test_name[256];
char yield_partial_name[5] = "none";
char sync_partial_name[5] = "none";

SortedList_t list;
SortedList_t *head;
SortedListElement_t *elems;

struct timespec start_time, end_time;

char const program_name[] = PROGRAM_NAME;

void _c(int ret, char *errmsg);

void usage();

void add(long long *pointer, long long value);
void add_mutex(long long *pointer, long long value);
void add_test_and_set(long long *pointer, long long value);
void add_compare_and_swap(long long *pointer, long long value);

void randomize_key(char *key);
void thread_op(void *tid_);

/* options descriptor */
static struct option const long_opts[] = {
    {"threads", required_argument, NULL, THREADS_SHORT_OPTION},
    {"iterations", required_argument, NULL, ITERATIONS_SHORT_OPTION},
    {"yield", required_argument, NULL, YIELD_SHORT_OPTION},
    {"sync", required_argument, NULL, SYNC_SHORT_OPTION},
    {0, 0, 0, 0},
};

void check_opt_sync(char *optarg);

int main(int argc, char *argv[]) {
  /* initialized list head */
  head = &list;
  head->next = head;
  head->prev = head;

  /* seed random */
  srand(time(0));
  /* option parsing */
  int optc;
  while ((optc = getopt_long(argc, argv, ":", long_opts, NULL)) != -1) {
    switch (optc) {
      case THREADS_SHORT_OPTION:
        thread_n = atoi(optarg);
        break;
      case ITERATIONS_SHORT_OPTION:
        iter_n = atoi(optarg);
        break;
      case YIELD_SHORT_OPTION:
        for (size_t i = 0; i < strlen(optarg); i++) {
          switch (optarg[i]) {
            case 'i':
              opt_yield |= INSERT_YIELD;
              strcpy(yield_partial_name, optarg);
              break;
            case 'd':
              opt_yield |= DELETE_YIELD;
              strcpy(yield_partial_name, optarg);
              break;
            case 'l':
              opt_yield |= LOOKUP_YIELD;
              strcpy(yield_partial_name, optarg);
              break;
            default:
              fprintf(stderr, "Invalid yield option argument\n");
          }
        }
        break;
      case SYNC_SHORT_OPTION:
        check_opt_sync(optarg);
        opt_sync = *optarg;
        switch (opt_sync) {
          case 'm':
            strcpy(sync_partial_name, "m");
            _c(pthread_mutex_init(&mutex, NULL), "Failed to initialize mutex.");
            break;
          case 's':
            strcpy(sync_partial_name, "s");
            break;
          case 'c':
            strcpy(sync_partial_name, "c");
            break;
        }
        break;
      default:
        fputs("Invalid arguments.\n", stderr);
        usage();
    }
  }

  long elem_n = thread_n * iter_n;

  /* allocate memory */
  if ((elems = malloc(sizeof(SortedListElement_t) * elem_n)) == NULL) {
    fprintf(stderr, "Failed to allocate memory for elems\n");
    exit(EXIT_FAILURE);
  }

  char **keys;
  if ((keys = malloc(sizeof(char *) * elem_n)) == NULL) {
    fprintf(stderr, "Failed to allocate memory for key pointers\n");
    exit(EXIT_FAILURE);
  }

  for (long i = 0; i < elem_n; i++) {
    if ((keys[i] = malloc(sizeof(char) * KEY_LENGTH)) == NULL) {
      fprintf(stderr, "Could not allocate memory for keys.\n");
      exit(EXIT_FAILURE);
    }
    randomize_key(keys[i]);
    elems[i].key = keys[i];
  }

  pthread_t *threads;
  if ((threads = malloc(sizeof(pthread_t) * thread_n)) == NULL) {
    fprintf(stderr, "Failed to allocate memory for thread ids\n");
    exit(EXIT_FAILURE);
  }

  int *tids;
  if ((tids = malloc(sizeof(int) * thread_n)) == NULL) {
    fprintf(stderr, "Failed to allocate memory for thread ids\n");
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i < thread_n; i++) tids[i] = i;

  _c(clock_gettime(CLOCK_MONOTONIC, &start_time),
     "Failed to record the start time");

  for (int t = 0; t < thread_n; t++) {
    _c(pthread_create(&threads[t], NULL, (void *)&thread_op, (void *)&tids[t]),
       "Failed to create thread");
  }

  for (int t = 0; t < thread_n; t++) {
    _c(pthread_join(threads[t], NULL), "Failed to join threads");
  }

  _c(clock_gettime(CLOCK_MONOTONIC, &end_time),
     "Failed to record the end time");

  if (SortedList_length(head) != 0)
    fprintf(stderr, "The length of the list is %ld instead of zero\n",
            list_len);

  /* free memory */
  free(threads);
  free(tids);
  free(elems);
  for (long i = 0; i < elem_n; i++) free(keys[i]);
  free(keys);

  long long runtime = end_time.tv_nsec - start_time.tv_nsec;
  long list_n = 1, op_n = thread_n * iter_n * 3;

  // create test name
  sprintf(test_name, "list-%s-%s", yield_partial_name, sync_partial_name);
  printf("%s,%d,%ld,%ld,%ld,%lld,%lld\n", test_name, thread_n, iter_n, list_n,
         op_n, runtime, runtime / op_n);
}

void check_opt_sync(char *optarg) {
  if (strcmp(optarg, "s") == 0 && strcmp(optarg, "m") == 0) {
    fprintf(stderr, "Invalid sync option argument\n");
    usage();
  }
}

void thread_op(void *tid_) {
  int tid = *((int *)tid_);

  switch (opt_sync) {
    case 'm':
      pthread_mutex_lock(&mutex);
      break;
    case 's':
      while (__sync_lock_test_and_set(&spin_lock, 1))
        ;
      break;
  }

  for (long i = 0; i < iter_n; i++) {
    /* printf("tid, index: %d %ld\n", tid, tid * iter_n + i); */
    SortedList_insert(head, &(elems[tid * iter_n + i]));
  }

  if ((list_len = SortedList_length(head)) < iter_n) {
    fprintf(stderr, "Failed to insert all elems. Should be %ld, but get %ld.\n",
            iter_n, list_len);
    exit(EXIT_WRONG_RESULT);
  }

  SortedListElement_t *curr;

  for (long i = 0; i < iter_n; i++) {
    if ((curr = SortedList_lookup(head, elems[tid * iter_n + i].key)) == NULL) {
      fprintf(stderr, "The expected list element is not found in the list\n");
      exit(EXIT_WRONG_RESULT);
    }
    if (SortedList_delete(curr) != 0) {
      fprintf(stderr, "Fail to delete element\n");
      exit(EXIT_WRONG_RESULT);
    }
  }

  switch (opt_sync) {
    case 'm':
      pthread_mutex_unlock(&mutex);
      break;
    case 's':
      __sync_lock_release(&spin_lock);
      break;
  }
}

void usage() {
  fprintf(stderr,
          "\n\
Usage: %s\n\
       %s --threads=THREADNUM\n\
       %s --iterations=ITERNUM\n\
       %s --yield=[i|d|l|id|il|dl|idl]\n\
       %s --sync=[m|s]\n\
\n\
",
          program_name, program_name, program_name, program_name, program_name);
  fputs(
      "\
--threads=THREADNUM            number of threads to use, defaults to 1\n\
--iterations=ITERNUM           number of iterations to run, defaults to 1\n\
--yield=[i|d|l|id|il|dl|idl]   yield mode, i: insert, d: delete, l: lookup\n\
--sync=[m|s]                   sync mode, m: mutex, s: spin\n\
",
      stderr);
  exit(EXIT_FAILURE);
}

void randomize_key(char *key) {
  for (unsigned long i = 0; i < KEY_LENGTH - 1; i++)
    key[i] = rand() % 94 + 33;  // printable character range
  key[KEY_LENGTH - 1] = '\0';
}

/* syscall check */
void _c(int ret, char *errmsg) {
  if (ret != -1) return;  // syscall suceeded
  fprintf(stderr, "%s: %s. errno %d\n", errmsg, strerror(errno), errno);
  exit(EXIT_FAILURE);
}

