/***********************************************************************/
/**      Author: Steeve Johan Otoka Eyota                             **/
/**        Date: July 2021                                            **/
/***********************************************************************/

#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "queue.h"
#include "args.h"
#include "error.h"

int njobs,			/* number of jobs ever created */
  ttlserv,			/* total service offered by servers */
  ttlqlen;			/* the total qlength */
int nblocked;			/* The number of threads blocked */

/***********************************************************************
                           r a n d 0 _ 1
************************************************************************/
double rand0_1(unsigned int *seedp)
{
  double f;
  /* We use the re-entrant version of rand */
  f = (double)rand_r(seedp);
  return f/(double)RAND_MAX;
}


/***********************************************************************
                             C L I E N T
************************************************************************/

void *client(void *vptr)
{
  unsigned int seed;
  int pthrerr;
  struct thread_arg *ptr;

  ptr = (struct thread_arg*)vptr;

  while (1)
    {
      // check if waiting for a previous job
      if (ptr->nclient == size_q(ptr->q))
      {
        if (pthrerr = pthread_mutex_lock(ptr->statex) != 0)
        {
          perror("Failed to lock mutex");
        }
        nblocked++;
        if (pthrerr = pthread_mutex_unlock(ptr->statex) != 0)
        {
          perror("Failed to lock mutex");
        }
        if (pthrerr = pthread_cond_wait(ptr->thrblockcond, ptr->blocktex) != 0)
        {
          perror("Failed to wait");
        }
        if (pthrerr = pthread_cond_wait(ptr->clientblockcond, ptr->blocktex) != 0)
        {
          perror("Failed to wait");
        }
      }
      else
      {
        // job generation decision
        seed = rand(); // seeds need to be different and random
        double rand = rand0_1(&seed); // coin toss
        if(rand < ptr->lam){
          // create new job and place it on global queue
          if (pthrerr = pthread_mutex_lock(ptr->statex) != 0)
          {
            perror("Failed to lock mutex");
          }
          push_q(ptr->q);
          njobs++;
          if (pthrerr = pthread_mutex_unlock(ptr->statex) != 0)
          {
            perror("Failed to lock mutex");
          }
        }
      }
      
      // check if last one of not
      if (nblocked == ptr->nclient + ptr->nserver) 
      {        
        if (pthrerr = pthread_mutex_lock(ptr->statex) != 0)
        {
          perror("Failed to unlock mutex");
        }
        nblocked = 0;
        if (pthrerr = pthread_mutex_unlock(ptr->statex) != 0)
        {
          perror("Failed to unlock mutex");
        }
        if (pthrerr = pthread_cond_signal(ptr->clientblockcond) != 0)
        {
          perror("Failed to signal");
        }
        if (pthrerr = pthread_cond_signal(ptr->clkblockcond) != 0)
        {
          perror("Failed to signal");
        }
      }      

    }

  return NULL;
      
}

/***********************************************************************
                             S E R V E R
************************************************************************/

void *server(void *vptr)
{
  unsigned int seed;
  int busy;
  int pthrerr;
  struct thread_arg *ptr;

  ptr = (struct thread_arg*)vptr;

  busy = 0;

  while (1)
    {
      if (size_q(ptr->q) > 0)
      {
        busy = 1;
      }
      
      if (busy == 1)
      {
        pthread_cond_wait(ptr->clkblockcond, ptr->blocktex);
        if (pthrerr = pthread_mutex_lock(ptr->statex) != 0)
        {
          perror("Failed to lock mutex");
        }
        ttlserv++;
        nblocked--;
        if (pthrerr = pthread_mutex_unlock(ptr->statex) != 0)
        {
          perror("Failed to unlock mutex");
        }
        if (pthrerr = pthread_cond_signal(ptr->thrblockcond) != 0)
        {
          perror("Failed to signal");
        }

        // switching state decision
        seed = rand(); 
        double rand = rand0_1(&ptr->seed);
        if (rand < ptr->mu)
        {
          busy = 0;
          if (pthrerr = pthread_cond_signal(ptr->thrblockcond) != 0)
          {
            perror("Failed to signal");
          }
        }
      }
      else
      {
        pthread_cond_wait(ptr->clkblockcond, ptr->blocktex);
        if (pthrerr = pthread_mutex_lock(ptr->statex) != 0)
        {
          perror("Failed to lock mutex");
        }

        pthrerr = safepop_q(ptr->q);

        if (pthrerr = pthread_mutex_unlock(ptr->statex) != 0)
        {
          perror("Failed to unlock mutex");
        }
      }

      // check if last one of not
      if (nblocked == ptr->nclient + ptr->nserver) 
      {        
        if (pthrerr = pthread_mutex_lock(ptr->statex) != 0)
        {
          perror("Failed to lock mutex");
        }
        nblocked = 0;
        if (pthrerr = pthread_mutex_unlock(ptr->statex) != 0)
        {
          perror("Failed to unlock mutex");
        }
        if (pthrerr = pthread_cond_signal(ptr->clkblockcond) != 0)
        {
          perror("Failed to signal");
        }
      }      
      
    }
  return NULL;
  
}


/***********************************************************************
                                C L K
************************************************************************/

void *clk(void *vptr)
{
  int tick;
  int pthrerr;
  struct thread_arg *ptr;

  ptr = (struct thread_arg*)vptr;

  // increments nticks and wake up everybody
  // (broadcast cond_var)
  for(tick = 0; tick < ptr->nticks; tick++){
    if (pthrerr = pthread_cond_broadcast(ptr->clkblockcond) != 0)
    {
      perror("Failed to broadcast");
    }
    ttlqlen += size_q(ptr->q);
  }

  printf("Average waiting time:    %f\n",(float)ttlqlen/(float)njobs);
  printf("Average turnaround time: %f\n",(float)ttlqlen/(float)njobs+
	 (float)ttlserv/(float)njobs);
  printf("Average execution time:  %f\n",(float)ttlserv/(float)njobs);
  printf("Average queue length: %f\n",(float)ttlqlen/(float)ptr->nticks);
  printf("Average interarrival time time: %f\n",(float)ptr->nticks/(float)njobs);
  /* Here we die with mutex locked and everyone else asleep */
  exit(0);
}

int main(int argc, char **argv)
{
  int pthrerr, i;
  int nserver, nclient, nticks;
  float lam, mu;

  pthread_t server_tid, client_tid;
  pthread_cond_t sthrblockcond, sclkblockcond, sclientblockcond;
  pthread_mutex_t sblocktex, sstatex;
  struct thread_arg *allargs;
  pthread_t *alltids;

  ttlserv  = 0;
  ttlqlen  = 0;
  nblocked = 0;
  njobs    = 0;

  nserver = 2;
  nclient = 2;
  lam = 0.005;
  mu = 0.01;
  nticks = 1000;
  i=1;
  while (i<argc-1)
    {
      if (strncmp("--lambda",argv[i],strlen(argv[i]))==0)
	  lam     = atof(argv[++i]);
      else if (strncmp("--mu",argv[i],strlen(argv[i]))==0)
	  mu      = atof(argv[++i]);
      else if (strncmp("--servers",argv[i],strlen(argv[i]))==0)
	  nserver = atoi(argv[++i]);
      else if (strncmp("--clients",argv[i],strlen(argv[i]))==0)
	  nclient = atoi(argv[++i]);
      else if (strncmp("--ticks",argv[i],strlen(argv[i]))==0)
	  nticks  = atoi(argv[++i]);
      else
	fatalerr(argv[i], 0, "Invalid argument\n");
      i++;
    }
  if (i!=argc)
    fatalerr(argv[0], 0, "Odd number of args\n");

  allargs = (struct thread_arg *)
    malloc((nserver+nclient+1)*sizeof(struct thread_arg));
  if (allargs==NULL)
    fatalerr(argv[0], 0,"Out of memory\n");
  alltids = (pthread_t*)
    malloc((nserver+nclient)*sizeof(pthread_t));
  if (alltids==NULL)
    fatalerr(argv[0], 0,"Out of memory\n");

  /* starts here */
  
  // initialize mutexes and cond_vars
  if (pthrerr = pthread_mutex_init(&sblocktex, NULL) != 0)
  {
    perror("Failed to initialize mutex\n");
  }
  
  if (pthrerr = pthread_mutex_init(&sstatex, NULL) != 0)
  {
    perror("Failed to initialize mutex\n");
  }

  if (pthrerr = pthread_cond_init(&sthrblockcond, NULL) != 0)
  {
    perror("Failed to initialize mutex\n");
  }

  if (pthrerr = pthread_cond_init(&sclkblockcond, NULL) != 0)
  {
    perror("Failed to initialize mutex\n");
  }

  // create nserver server threads
  for (i = 0; i < nserver; i++)
  {
    alltids[i] = server_tid;
    allargs[i].nserver = nserver;
    allargs[i].nclient = nclient;
    allargs[i].nticks = nticks;
    allargs[i].q = mk_queue();
    allargs[i].lam = lam;
    allargs[i].mu = mu;
    allargs[i].thrblockcond = &sthrblockcond;
    allargs[i].clkblockcond = &sclkblockcond;
    allargs[i].clientblockcond = &sclientblockcond;
    allargs[i].blocktex = &sblocktex;
    allargs[i].statex = &sstatex;
    if (pthrerr = pthread_create(&alltids[i], NULL, server, &allargs[i]) != 0)
    {
      perror("Failed to create server thread\n");
    }
  }
  
  // create nclient client threads
  for(i = 0; i < nclient; i++)
  {
    alltids[i] = client_tid;
    allargs[i].nserver = nserver;
    allargs[i].nclient = nclient;
    allargs[i].nticks = nticks;
    allargs[i].q = mk_queue();
    allargs[i].lam = lam;
    allargs[i].mu = mu;
    allargs[i].thrblockcond = &sthrblockcond;
    allargs[i].clkblockcond = &sclkblockcond;
    allargs[i].clientblockcond = &sclientblockcond;
    allargs[i].blocktex = &sblocktex;
    allargs[i].statex = &sstatex;
    if (pthrerr = pthread_create(&alltids[i], NULL, client, &allargs[i]) != 0)
    {
      perror("Failed to create client thread\n");
    }
  }

  // allargs transforms itself into the clock thread by calling clk
  clk(allargs);

  /* ends here */
  exit(-1);
}
