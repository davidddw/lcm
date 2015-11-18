#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "lc_kernel.h"

lc_ker_sknob_t *lc_ker_get_sknob(void)
{
    lc_ker_sknob_t *knob = NULL;
    int shmid;
    key_t shmkey;

    shmkey = ftok(LC_KER_SHM_ID, 'a');

    if (shmkey == -1) {
        printf("%s:Get shm it error\n",__FUNCTION__);
        exit(-1);
    }
    shmid = shmget(shmkey, 0, 0);

    if (shmid != -1) {
        printf("%s:Get a shared memory segment %d",__FUNCTION__, shmid);
    } else {
        printf("%s:Get a shared memory error!",__FUNCTION__);
        return NULL;
    }

    if ((knob = (lc_ker_sknob_t *)shmat(shmid, (const void*)0, 0)) == (void *)-1) {
        printf("%s:shmat error:%s\n",__FUNCTION__,strerror(errno));
        return NULL;
    }

    return knob;

}

void lc_ker_dump_sknob(lc_ker_sknob_t *sknob)
{
    printf("===========================================\n");
    printf("LC Kernel Debug Counters \n");
    printf("===========================================\n\n");

    printf("LC Message received               %10d\n", 
            atomic_read(&sknob->lc_counters.lc_msg_in));
    printf("LC Driver Message received        %10d\n", 
            atomic_read(&sknob->lc_counters.lc_drv_msg_in));
    printf("LC Application Message received   %10d\n", 
            atomic_read(&sknob->lc_counters.lc_app_msg_in));
    printf("LC Valid Message received         %10d\n", 
            atomic_read(&sknob->lc_counters.lc_valid_msg_in));
    printf("LC Invalid Message received       %10d\n", 
            atomic_read(&sknob->lc_counters.lc_invalid_msg_in));
    printf("LC Receive Message Error          %10d\n", 
            atomic_read(&sknob->lc_counters.lc_msg_rcv_err));
    printf("LC Enqueue Received Message       %10d\n", 
            atomic_read(&sknob->lc_counters.lc_msg_enqueue));
    printf("LC Enqueue Drop Message           %10d\n", 
            atomic_read(&sknob->lc_counters.lc_thread_msg_drop));
}
int main()
{
    lc_ker_sknob_t *sknob = lc_ker_get_sknob();
    if ((sknob) < 0) {
        LCPD_LOG(LOG_ERR,"Get shared knob error! \n");
        return -1;
    }

    lc_ker_dump_sknob(sknob);

    return 0;
}
