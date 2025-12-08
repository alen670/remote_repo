#include "libuser.h"
#include "process.h"
#include <string.h>
int main(int argc, char **argv)
{
  int i, j ;     	/* loop index */
  int scr_sem;		/* id of screen semaphore */
  int now, start, elapsed;
  int sem_long = Create_Semaphore ("sem_long" , 0);

  P(sem_long);  		

  start = Get_Time_Of_Day();
  scr_sem = Create_Semaphore ("screen" , 1) ;   /* register for screen use */

  for (i=0; i < 200000; i++) {
      for (j=0 ; j < 50000 ; j++) ;
      now = Get_Time_Of_Day();
  }
  elapsed = Get_Time_Of_Day() - start;
  //P (scr_sem);
  //Print("\nProcess Long is done at time: %d\n", elapsed) ;
  //V(scr_sem);


  return 0;
}

