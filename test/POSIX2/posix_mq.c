/****************************************************************************/
/* Function: Basic POSIX message queue demo                                 */
/*                                                                          */
/* Sam Siewert, 02/05/2011                                                  */
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

#define SNDRCV_MQ "/send_receive_mq"
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
    printf("%d receive: msg %s received with priority = %d, length = %d\n",
           rcvCnt, buffer, prio, nbytes);
  }
    
  rc = mq_close(mymq);

  if(rc == ERROR)
  {
    perror("receiver mq_close");
    exit(-1);
  }

}

static char canned_msg[] = "this is a test, and only a test, in the event of a real emergency, you would be instructed ...";

static int sndCnt=0;

void sender(void)
{
  mqd_t mymq;
  int prio;
  int nbytes, rc;

  mymq = mq_open(SNDRCV_MQ, O_CREAT|O_RDWR, S_IRWXU, &mq_attr);

  if(mymq < 0)
  {
    perror("sender mq_open");
    exit(-1);
  }
  else
  {
    printf("sender opened mq\n");
  }

  /* send message with priority=30 */
  if((nbytes = mq_send(mymq, canned_msg, sizeof(canned_msg), 30)) == ERROR)
  {
    perror("mq_send");
  }
  else
  {
    sndCnt++;
    printf("%d send: message successfully sent\n", sndCnt);
  }
  
  rc = mq_close(mymq);

  if(rc < 0)
  {
    perror("sender mq_close");
    exit(-1);
  }
  else
  {
    printf("sender closed mq\n");
  }

}


void main(void)
{
  int idx;

  /* setup common message q attributes */
  mq_attr.mq_maxmsg = MSG_ITERATIONS;
  mq_attr.mq_msgsize = MAX_MSG_SIZE;

  mq_attr.mq_flags = 0;


  // Create two communicating processes right here
  // 
  // test basic features
  //

  // Send a message, receive and print
  for(idx=0; idx < MSG_ITERATIONS; idx++)
  {
      sender();
      receiver();
  }
 
 
}
