/****************************************************************************/
/* Function: Basic POSIX message queue demo                                 */
/*                                                                          */
/* Sam Siewert, 02/09/2012                                                  */
/****************************************************************************/

//   Mounting the message queue file system
//       On  Linux,  message queues are created in a virtual file system.
//       (Other implementations may also provide such a feature, but  the
//       details  are likely to differ.)  This file system can be mounted
//       (by the superuser) using the following commands:
//
//    By mounting, while not required, you can get message queue status
//
//           # mkdir /dev/mqueue
//           # mount -t mqueue none /dev/mqueue
//
//     Do man mq_overview for more information
//


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <mqueue.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SNDRCV_MQ "/a335_mq"
#define MAX_MSG_SIZE (128)
#define MSG_ITERATIONS (10)
#define ERROR (-1)

static struct mq_attr mq_attr;
static int rcvCnt=0;

void receiver(void)
{
  mqd_t mymq;
  char buffer[MAX_MSG_SIZE];
  int prio;
  int nbytes, rc;

  mymq = mq_open(SNDRCV_MQ, O_CREAT|O_RDWR, S_IRWXU, &mq_attr);

  if(mymq == (mqd_t)ERROR)
  {
    perror("receiver mq_open");
    exit(-1);
  }

  /* read oldest, highest priority msg from the message queue */
  if((nbytes = mq_receive(mymq, buffer, MAX_MSG_SIZE, &prio)) == ERROR)
  {
    perror("mq_receive");
  }
  else
  {
    rcvCnt++;
    buffer[nbytes] = '\0';
    printf("%d received: msg %s received with priority = %d, length = %d\n",
           rcvCnt, buffer, prio, nbytes);
  }

  rc = mq_close(mymq);

  if(rc == ERROR)
  {
    perror("receiver mq_close");
    exit(-1);
  }

}


void main(void)
{
  int recvRequest=-1;
  int idx;

  /* setup common message q attributes */
  mq_attr.mq_maxmsg = MSG_ITERATIONS;
  mq_attr.mq_msgsize = MAX_MSG_SIZE;

  mq_attr.mq_flags = 0;


  // Create two communicating processes right here
  // 
  // test basic features
  //

  // Receive messages as requested
  
  while(recvRequest != 0)
  {
      printf("\nEnter number of test messages to receive from a335_mq queue: ");
      scanf("%d", &recvRequest);
      printf("Will dequeue %d test messages\n", recvRequest);

      if(!(recvRequest > 0)) break;

      for(idx=0; idx < recvRequest; idx++)
          receiver();

  };

  printf("%d receive messages requested, exiting\n", recvRequest);
 
}
