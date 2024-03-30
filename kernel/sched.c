/*
 * implementing the scheduler
 */

#include "sched.h"
#include "spike_interface/spike_utils.h"

#include "sync_utils.h"
#include "spike_interface/atomic.h"

process* ready_queue_head[NCPU] = {NULL};

//
// insert a process, proc, into the END of ready queue.
//
spinlock_t insert_lock;

void insert_to_ready_queue( process* proc ) {
  spinlock_lock(&insert_lock);
  uint64 hartid = read_tp();
  sprint("hartid = %lld: going to insert process %d to ready queue.\n", hartid, proc->pid);
  // if the queue is empty in the beginning
  if( ready_queue_head[hartid] == NULL ){
    proc->status = READY;
    proc->queue_next = NULL;
    ready_queue_head[hartid] = proc;
    spinlock_unlock(&insert_lock);
    return;
  }

  // ready queue is not empty
  process *p;
  // browse the ready queue to see if proc is already in-queue
  for( p=ready_queue_head[hartid]; p->queue_next!=NULL; p=p->queue_next )
    if( p == proc ) return;  //already in queue

  // p points to the last element of the ready queue
  if( p==proc ) return;
  p->queue_next = proc;
  proc->status = READY;
  proc->queue_next = NULL;

  spinlock_unlock(&insert_lock);

  return;
}

//
// choose a proc from the ready queue, and put it to run.
// note: schedule() does not take care of previous current process. If the current
// process is still runnable, you should place it into the ready queue (by calling
// ready_queue_insert), and then call schedule().
//

static int shutdown_mutex = 0;
extern process procs[NPROC];
static int cpu_tag[NCPU] = {0};
void schedule() {

  uint64 hartid = read_tp();
  if ( !ready_queue_head[hartid] ){
    if(!cpu_tag[hartid]){
      cpu_tag[hartid] = 1;
      // by default, if there are no ready process, and all processes are in the status of
      // FREE and ZOMBIE, we should shutdown the emulated RISC-V machine.
      int should_shutdown = 1;

      for( int i=0; i<NPROC; i++ )
        if( (procs[i].status != FREE) && (procs[i].status != ZOMBIE) ){
          // should_shutdown = 0;
          sprint( "hartid = %lld: ready queue empty, but process %d is not in free/zombie state:%d, maybe other cpu still have process should be done\n", 
            hartid, i, procs[i].status );
        }
      sync_barrier(&shutdown_mutex, NCPU);
      if( should_shutdown ){
          // ! add for lab1_challenge3
          sync_barrier(&shutdown_mutex, NCPU);
          if(!read_tp()){
            sprint("hartid = %d: shutdown with code:%d.\n", read_tp(), 0);
            shutdown( 0 );
          }
        // sprint( "no more ready processes, system shutdown now.\n" );
        // shutdown( 0 );
      }else{
        panic( "Not handled: we should let system wait for unfinished processes.\n" );
      }
    }
    else{
      sprint("sorry");
      schedule();
    }
  }

  current[hartid] = ready_queue_head[hartid];
  assert( current[hartid]->status == READY );
  ready_queue_head[hartid] = ready_queue_head[hartid]->queue_next;

  current[hartid]->status = RUNNING;
  sprint( "hartid = %lld: going to schedule process %d to run.\n", hartid, current[hartid]->pid );
  switch_to( current[hartid] );
}
