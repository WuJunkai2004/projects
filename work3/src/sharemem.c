#include "sharemem.h"

#include <errno.h>
#include <stdio.h>
#include <sys/sem.h>

#include <sys/shm.h>


static int semaphore_p(int semid) {
    struct sembuf op;
    op.sem_num = 0;
    op.sem_op = -1;       // P 操作 (减 1，等待)
    op.sem_flg = SEM_UNDO;//进程崩溃时自动撤销操作，防止死锁
    if(semop(semid, &op, 1) == -1){
        perror("Error: semaphore p");
        return -1;
    }
    return 0;
}

static int semaphore_v(int semid) {
    struct sembuf op;
    op.sem_num = 0;
    op.sem_op = 1;        // V 操作 (加 1，释放)
    op.sem_flg = SEM_UNDO;
    if(semop(semid, &op, 1) == -1){
        perror("Error: semaphore v");
        return -1;
    }
    return 0;
}


void* smalloc(key_t key, size_t size) {
    key_t sem_key = key + 1; // 约定信号量的 key
    size_t total_size = sizeof(shm_header_t) + size;

    // 获取或创建共享内存 (shmget)
    int shmid = shmget(key, total_size, IPC_CREAT | IPC_EXCL | 0666);
    int is_creator = (shmid != -1);

    if(!is_creator){
        if(errno != EEXIST){
            perror("Error: the shared memory is not created by other but get it failed");
            return NULL;
        }
        shmid = shmget(key, total_size, 0666);
        if(shmid == -1){
            perror("Error: the shared memory is created by other but get it failed");
            return NULL;
        }
    }

    // 获取或创建信号量 (semget)
    int semid = semget(sem_key, 1, IPC_CREAT | IPC_EXCL | 0666);
    if(is_creator){
        if(semid == -1){
            perror("Error: cannot create the semaphore");
            shmctl(shmid, IPC_RMID, NULL);
            return NULL;
        }
        semun arg;
        arg.val = 1;
        if(semctl(semid, 0, SETVAL, arg) == -1){
            perror("Error: cannot set the semaphore value");
            shmctl(shmid, IPC_RMID, NULL);
            semctl(semid, 0, IPC_RMID, NULL);
            return NULL;
        }
    } else {
        semid = semget(sem_key, 1, 0666);
        if(semid == -1){
            perror("Error: cannot get the semaphore created by other");
            return NULL;
        }
    }

    // 映射共享内存 (shmat)
    void* base_ptr = shmat(shmid, NULL, 0);
    if(base_ptr == (void*)-1){
        perror("Error: shared memory attach failed");
        if(is_creator){
            shmctl(shmid, IPC_RMID, NULL);
            semctl(semid, 0, IPC_RMID, NULL);
        }
        return NULL;
    }

    // 填充头部信息
    shm_header_t* header = (shm_header_t*)base_ptr;
    if(is_creator){
        header->shmid = shmid;
        header->semid = semid;
    }

    return (void*)(header + 1);
}

int smfree(void* ptr) {
    if(ptr == NULL){
        return -1;
    }

    shm_header_t* header = ((shm_header_t*)ptr) - 1;

    // 获取元数据
    int shmid = header->shmid;
    int semid = header->semid;

    // 找回 shmat 返回的基地址
    void* base_ptr = (void*)header;

    // 分离 (shmdt)
    if(shmdt(base_ptr) == -1){
        perror("Error: shared memory detach failed");
        return -1;
    }

    // 获取shm的状态 (shmctl IPC_STAT)
    struct shmid_ds buf;
    shmctl(shmid, IPC_STAT, &buf);
    if(buf.shm_nattch == 0){
        shmctl(shmid, IPC_RMID, NULL);
        semctl(semid, 0, IPC_RMID, NULL);
    }

    return 0;
}

int smlock(void* ptr) {
    if(ptr == NULL){
        return -1;
    }
    shm_header_t* header = ((shm_header_t*)ptr) - 1;
    return semaphore_p(header->semid);
}

int smunlock(void* ptr) {
    if(ptr == NULL){
        return -1;
    }
    shm_header_t* header = ((shm_header_t*)ptr) - 1;
    return semaphore_v(header->semid);
}