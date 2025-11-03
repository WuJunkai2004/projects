#ifndef SHAREMEM_H
#define SHAREMEM_H

#include <sys/types.h>


#if defined(__linux__) || defined(__GNU__) || defined (__APPLE__)
union __semaphore_union {
    int val;                  /* SETVAL 用的值 */
    struct semid_ds *buf;     /* IPC_STAT, IPC_SET 用的缓冲区 */
    unsigned short *array;    /* GETALL, SETALL 用的数组 */
    struct seminfo *__buf;    /* IPC_INFO (Linux 特有) */
};
typedef union __semaphore_union semun;
#endif


typedef struct {
    int shmid;  // 共享内存 ID
    int semid;  // 信号量 ID
} shm_header_t;


/**
 * @brief smalloc: 分配共享内存
 * @param key: 共享内存key
 * @param size: 共享内存大小
 */
void* smalloc(key_t key, size_t size);


/**
 * @brief sfree: 释放共享内存
 * @param ptr: 共享内存指针
 */
int smfree(void* ptr);


/**
 * @brief 锁定共享内存
 * @param ptr: 共享内存指针
 */
int smlock(void* ptr);

/**
 * @brief 解锁共享内存
 * @param ptr: 共享内存指针
 */
int smunlock(void* ptr);

#endif//SHAREMEM_H
