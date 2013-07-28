#ifndef _SCULL_H_
#define _SCULL_H_

/* Use 's' as magic number */
#define SSD_IOCTL_MAGIC  's'

#define SSD_IOCTL_ERASE    _IO(SSD_IOCTL_MAGIC, 0)

#define SSD_IOCTL_RECOVER _IO(SSD_IOCTL_MAGIC, 1)

void decode_raid_5(void);
void compute_raid_5(void);
void raid5_recover(int failure);

struct ssd_dev
{
        unsigned long size;       /* amount of data stored here */
        struct semaphore sem;     /* mutual exclusion semaphore */
};



#endif
