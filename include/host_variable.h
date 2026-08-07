/* host_variable.h
 */

#ifndef _H_SUPER_VARIABLE
#define _H_SUPER_VARIABLE

#include <time.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _s_host_variable* host_variable;

typedef struct {
    host_variable variable;
    const void *data;
    int target;
} host_variable_read_loan;

typedef struct {
    host_variable variable;
    void *data;
    int target4;
} host_variable_write_loan;

enum {
    HOST_VARIABLE_WRITE_PUBLISHED = 0,
    HOST_VARIABLE_WRITE_SUPERSEDED = 1,
};

// create or link to an existing host variable
host_variable link_host_variable(const char *name, const size_t size);   
int borrow_host_variable_read(host_variable p, const size_t size,
        host_variable_read_loan *loan);
int release_host_variable_read(host_variable_read_loan *loan);
int borrow_host_variable_write(host_variable p, const size_t size,
        host_variable_write_loan *loan);
int release_host_variable_write(host_variable_write_loan *loan);
int read_host_variable(host_variable p, void *buf, \
        const size_t size, const size_t op_size);
int write_host_variable(host_variable p, const void *data, \
        const size_t size, const size_t op_size);
void get_host_variable_timestamp(host_variable p, struct timespec *ret);
// Unmap this process's handle. The shared-memory name remains available.
int unlink_host_variable(host_variable p, const char *name, const size_t size);

#ifdef __cplusplus
}
#endif

#endif
