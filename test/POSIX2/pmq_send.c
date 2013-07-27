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

static char canned_msg[] = "this is a test, and only a test, in the event of a real emergency, you would be instructed ...";

static char snd_msg[MAX_MSG_SIZE];

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

  sprintf(snd_msg, "MSG %d - ", sndCnt);
  strncat(snd_msg, canned_msg, sizeof(canned_msg));

  /* send message with priority=30 */
  if((nbytes = mq_send(mymq, snd_msg, strlen(snd_msg), 30)) == ERROR)
  {
    perror("mq_send");
  }
  else
  {
    sndCnt++;
    printf("%d sent: message successfully sent\n", sndCnt);
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
  int sendRequest=-1;
  int idx;

  /* setup common message q attributes */
  mq_attr.mq_maxmsg = MSG_ITERATIONS;
  mq_attr.mq_msgsize = MAX_MSG_SIZE;

  mq_attr.mq_flags = 0;


  // Create two communicating processes right here
  // 
  // test basic features
  //

  // Send messages as requested
  
  while(sendRequest != 0)
  {
      printf("\nEnter number of test messages to send: ");
      scanf("%d", &sendRequest);
      printf("Will send %d test messages\n", sendRequest);

      if(!(sendRequest > 0)) break;

      for(idx=0; idx < sendRequest; idx++)
          sender();
  };

  printf("%d sends requested, exiting\n", sendRequest);
 
}
