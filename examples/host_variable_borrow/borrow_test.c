#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "host_variable.h"

#define CHECK(condition) do { \
    if(!(condition)) { \
        fprintf(stderr, "check failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        abort(); \
    } \
} while(0)

struct sample {
    uint64_t sequence;
    uint8_t payload[256];
};

struct concurrency_context {
    host_variable variable;
    atomic_bool done;
    atomic_size_t reads;
};

struct writer_context {
    struct concurrency_context *shared;
    uint8_t id;
};

static void *concurrent_writer(void *arg)
{
    struct writer_context *context = arg;
    for(uint32_t i = 0; i < 2000; ++i) {
        host_variable_write_loan loan;
        while(borrow_host_variable_write(context->shared->variable,
                    sizeof(struct sample), &loan) != 0)
            sched_yield();

        struct sample *sample = loan.data;
        sample->sequence = ((uint64_t)context->id << 32) | i;
        memset(sample->payload, context->id, sizeof(sample->payload));
        int result = release_host_variable_write(&loan);
        CHECK(result == HOST_VARIABLE_WRITE_PUBLISHED
                || result == HOST_VARIABLE_WRITE_SUPERSEDED);
    }
    return NULL;
}

static void *concurrent_reader(void *arg)
{
    struct concurrency_context *context = arg;
    do {
        host_variable_read_loan loan;
        CHECK(borrow_host_variable_read(context->variable,
                    sizeof(struct sample), &loan) == 0);

        const struct sample *sample = loan.data;
        uint8_t expected = sample->sequence >> 32;
        for(size_t i = 0; i < sizeof(sample->payload); ++i)
            CHECK(sample->payload[i] == expected);

        CHECK(release_host_variable_read(&loan) == 0);
        atomic_fetch_add_explicit(&context->reads, 1, memory_order_relaxed);
    } while(!atomic_load_explicit(&context->done, memory_order_acquire));
    return NULL;
}

int main(void)
{
    const char *name = "/host_variable_borrow_test";
    const size_t size = sizeof(struct sample);
    host_variable_write_loan write_loan;
    host_variable_read_loan read_loan;
    struct sample copied;

    shm_unlink(name);
    host_variable variable = link_host_variable(name, size);
    CHECK(variable != NULL);

    CHECK(borrow_host_variable_write(variable, size, &write_loan) == 0);
    struct sample *writable = write_loan.data;
    writable->sequence = 1;
    memset(writable->payload, 0x11, sizeof(writable->payload));
    CHECK(release_host_variable_write(&write_loan)
            == HOST_VARIABLE_WRITE_PUBLISHED);

    errno = 0;
    CHECK(release_host_variable_write(&write_loan) == -1);
    CHECK(errno == EINVAL);

    CHECK(borrow_host_variable_read(variable, size, &read_loan) == 0);
    const struct sample *pinned = read_loan.data;
    CHECK(pinned->sequence == 1);
    CHECK(pinned->payload[0] == 0x11);

    CHECK(borrow_host_variable_write(variable, size, &write_loan) == 0);
    writable = write_loan.data;
    writable->sequence = 2;
    memset(writable->payload, 0x22, sizeof(writable->payload));
    usleep(1000);
    CHECK(release_host_variable_write(&write_loan)
            == HOST_VARIABLE_WRITE_PUBLISHED);

    /* The old read loan remains valid and pinned after a newer publication. */
    CHECK(pinned->sequence == 1);
    CHECK(pinned->payload[0] == 0x11);
    CHECK(release_host_variable_read(&read_loan) == 0);

    errno = 0;
    CHECK(release_host_variable_read(&read_loan) == -1);
    CHECK(errno == EINVAL);

    CHECK(borrow_host_variable_read(variable, size, &read_loan) == 0);
    pinned = read_loan.data;
    CHECK(pinned->sequence == 2);
    CHECK(pinned->payload[0] == 0x22);
    CHECK(release_host_variable_read(&read_loan) == 0);

    /* With four buffers, the current target leaves three writable loans. */
    host_variable_write_loan held[3];
    for(size_t i = 0; i < 3; ++i) {
        CHECK(borrow_host_variable_write(variable, size, &held[i]) == 0);
        ((struct sample *)held[i].data)->sequence = 10 + i;
    }

    errno = 0;
    CHECK(borrow_host_variable_write(variable, size, &write_loan) == -1);

    for(size_t i = 0; i < 3; ++i) {
        int result = release_host_variable_write(&held[i]);
        CHECK(result == HOST_VARIABLE_WRITE_PUBLISHED
                || result == HOST_VARIABLE_WRITE_SUPERSEDED);
    }

    memset(&copied, 0x33, sizeof(copied));
    copied.sequence = 100;
    CHECK(write_host_variable(variable, &copied, size, size) == 0);
    memset(&copied, 0, sizeof(copied));
    CHECK(read_host_variable(variable, &copied, size, size) == 0);
    CHECK(copied.sequence == 100);
    CHECK(copied.payload[0] == 0x33);

    memset(&copied, 0, sizeof(copied));
    CHECK(write_host_variable(variable, &copied, size, size) == 0);

    struct concurrency_context concurrency = {
        .variable = variable,
        .done = false,
        .reads = 0,
    };
    pthread_t readers[4];
    pthread_t writers[4];
    struct writer_context writer_contexts[4];
    for(size_t i = 0; i < 4; ++i)
        CHECK(pthread_create(&readers[i], NULL, concurrent_reader,
                    &concurrency) == 0);
    usleep(1000);
    for(size_t i = 0; i < 4; ++i) {
        writer_contexts[i].shared = &concurrency;
        writer_contexts[i].id = i + 1;
        CHECK(pthread_create(&writers[i], NULL, concurrent_writer,
                    &writer_contexts[i]) == 0);
    }
    for(size_t i = 0; i < 4; ++i)
        CHECK(pthread_join(writers[i], NULL) == 0);
    atomic_store_explicit(&concurrency.done, true, memory_order_release);
    for(size_t i = 0; i < 4; ++i)
        CHECK(pthread_join(readers[i], NULL) == 0);
    CHECK(atomic_load_explicit(&concurrency.reads, memory_order_relaxed) > 0);

    errno = 0;
    CHECK(write_host_variable(variable, &copied, size, size + 1) == -1);
    CHECK(errno == EINVAL);

    CHECK(unlink_host_variable(variable, name, size) == 0);

    /* Local unmapping must not remove the shared-memory name. */
    int fd = shm_open(name, O_RDWR, 0600);
    CHECK(fd >= 0);
    close(fd);
    CHECK(shm_unlink(name) == 0);
    return 0;
}
