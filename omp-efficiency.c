/*
 * Copyright (C) 2026 - Cambridge Greys Limited
 * Copyright (C) 2026 - Red Hat, LLC
 * Licensed under the BSD 3-clause.
 */


#include <float.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <omp.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#define TEST_SIZE 256
#define VECTOR_SIZE 2048000
#define REPEATS 100

static void* (*m_a)[TEST_SIZE];
static void* (*m_b)[TEST_SIZE];
static void* (*m_c)[TEST_SIZE];

static void *mmap_address;

float *init_vector(float *vector)
{
        for (int j=0; j < VECTOR_SIZE; j++) {
            vector[j] = rand();
        }
        return vector;
}

void setup_test(char *filename)
{
    void *buffer;
    float scrap;
    int i;
    int fd = open(filename, O_RDWR);
    
    if ((fd == -1) && (errno == ENOENT)) {
        fd = open(filename, O_RDWR + O_CREAT, S_IRWXU);
        if (fd == -1) {
            exit(errno);
        }
        buffer = malloc(sizeof(float) * VECTOR_SIZE);
        for (int i=0; i < TEST_SIZE * 3; i++) {
            init_vector(buffer);
            if (write(fd, buffer, sizeof(float) * VECTOR_SIZE) < sizeof(float) * VECTOR_SIZE) {
                exit(errno);
            }
        }
        close(fd);
        fd = open(filename, O_RDWR);
        if (fd == -1) {
            exit(errno);
        }
    }
    mmap_address = mmap(NULL, TEST_SIZE * VECTOR_SIZE * sizeof(float) * 3, PROT_READ, MAP_SHARED, fd, 0);

    m_a = malloc(sizeof(void*) * TEST_SIZE);
    m_b = malloc(sizeof(void*) * TEST_SIZE);
    m_c = malloc(sizeof(void*) * TEST_SIZE);
    for (i = 0; i < TEST_SIZE; i++) {
        (* m_a)[i] = malloc(sizeof(float) * VECTOR_SIZE);
        (* m_b)[i] = malloc(sizeof(float) * VECTOR_SIZE);
        (* m_c)[i] = malloc(sizeof(float) * VECTOR_SIZE);
    }

}

void compute()
{
    void *addr;

    for (int j = 0; j < REPEATS; j++ ) {
        addr = mmap_address;
        for (int i = 0; i < TEST_SIZE; i++){
            memcpy((*m_a)[i], addr, sizeof(float) * VECTOR_SIZE);
            addr += sizeof(float) * VECTOR_SIZE;
            memcpy((*m_b)[i], addr, sizeof(float) * VECTOR_SIZE);
            addr += sizeof(float) * VECTOR_SIZE;
            memcpy((*m_c)[i], addr, sizeof(float) * VECTOR_SIZE);
            addr += sizeof(float) * VECTOR_SIZE;
        }
#pragma omp parallel for
        for (int i = 0; i < TEST_SIZE; i++){
            float *a = (*m_a)[i];
            float *b = (*m_b)[i];
            float *c = (*m_c)[i];
                    for (int j=0; j<VECTOR_SIZE; j++) {
                a[j] = a[j]*b[j] + c[j];
            }
        }
    }
}

double get_time()
{
    struct timespec ts;
    double s, ns;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        ns = ts.tv_nsec;
        ns = ns / 1000000000;
        s = ts.tv_sec;
        return s + ns;
    }
    return 0.0;
}

double total_cpu(struct rusage *rstart, struct rusage *rfinish)
{
    double s, f;
    s = rstart->ru_utime.tv_usec;
    s = s / 1000000;
    s += rstart->ru_utime.tv_sec;
    f = rfinish->ru_utime.tv_usec;
    f = f / 1000000;
    f += rfinish->ru_utime.tv_sec;
    return f - s;
}


void compute_master(int sock)
{
    int count;
    unsigned long stride = 1;
    double start;
    char *buffer;
    struct rusage rstart, rfinish;

    buffer = malloc(256);

    if (sock > 0)  {
        if (recv(sock, buffer, 256, 0) < 0) {
            exit(1);
        }
        stride = strtoul(buffer, NULL, 10);
    }
    start = get_time();
    getrusage(RUSAGE_SELF, &rstart);
    for (count = 0; count < stride ; count++) {
        compute();
    }
    getrusage(RUSAGE_SELF, &rfinish);



    snprintf(buffer, 256, "omp efficiency is %f\n", 1/(total_cpu(&rstart, &rfinish)/((get_time() - start) * omp_get_num_threads())));
    if (sock > 0) {
        send(sock, &stride, sizeof(int), 0);
    } else {
        printf("%s", buffer);
    }
}

int main(int argc, char *argv[])
{
    double start;
    int stride = 0, discard;
    int sock = -1;
    struct sockaddr_un sock_data;

    if (argc < 2) {
        exit(1);
    }

    if (argc == 3) {
        sock = socket(AF_UNIX, SOCK_DGRAM, 0);
        sock_data.sun_family = AF_UNIX;
        strncpy((char *)&sock_data.sun_path, argv[1], 107);
        connect(sock, (struct sockaddr *)&sock_data, sizeof(struct sockaddr_un));
    }
    setup_test(argv[1]);
    compute_master(sock);
}

