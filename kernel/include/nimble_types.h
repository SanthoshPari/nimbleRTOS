#ifndef NIMBLE_TYPES_H
#define NIMBLE_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef int32_t  nimble_status_t;
typedef uint32_t nimble_tick_t;

#define NIMBLE_OK              ((nimble_status_t)0)
#define NIMBLE_ERR_INVALID_ARG ((nimble_status_t)-1)
#define NIMBLE_ERR_NO_MEMORY   ((nimble_status_t)-2)
#define NIMBLE_ERR_TIMEOUT     ((nimble_status_t)-3)
#define NIMBLE_ERR_NOT_OWNER   ((nimble_status_t)-4)
#define NIMBLE_ERR_FULL        ((nimble_status_t)-5)
#define NIMBLE_ERR_EMPTY       ((nimble_status_t)-6)

/* Special timeout value: block forever. */
#define NIMBLE_WAIT_FOREVER    ((nimble_tick_t)0xFFFFFFFFU)
/* Special timeout value: never block. */
#define NIMBLE_NO_WAIT         ((nimble_tick_t)0U)

typedef enum {
    NIMBLE_TASK_READY = 0,
    NIMBLE_TASK_RUNNING,
    NIMBLE_TASK_BLOCKED,
    NIMBLE_TASK_SUSPENDED,
    NIMBLE_TASK_DELETED
} nimble_task_state_t;

#endif /* NIMBLE_TYPES_H */
