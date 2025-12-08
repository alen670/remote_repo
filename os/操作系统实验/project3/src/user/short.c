#include "libuser.h"
#include "process.h"
#include <string.h>

int main(int argc, char **argv)
{
  if (argc < 2) {
        Print("[Short Process] Error: No sequence number!\n");
        Exit(1);
  }
  // 获取短进程序号（1~7）
  char seq[2];
  seq[0] = argv[1][0];
  seq[1] = '\0';
  // 查找对应序号的启动信号量


  int i, j ;     	/* loop index */
  int scr_sem;		/* id of screen semaphore */
  int now, start, elapsed;\
  char sem_name[32];
  char sem_name1[32];
  sem_name[0] = 's'; sem_name[1] = 'e'; sem_name[2] = 'm';
  sem_name[3] = '_'; sem_name[4] = 's'; sem_name[5] = 'h';
  sem_name[6] = 'o'; sem_name[7] = 'r'; sem_name[8] = 't';
  sem_name[9] = '_';
  // 步骤2：拼接序号
  sem_name[10] = seq[0];
  // 步骤3：补结束符
  sem_name[11] = '\0';
  int sem_short = Create_Semaphore(sem_name,0);		

  start = Get_Time_Of_Day();
  scr_sem = Create_Semaphore ("screen" , 1) ;   /* register for screen use */
  
  P(sem_short);
  for (i=0; i < 20; i++) {
      for (j=0 ; j < 50 ; j++) ;
      now = Get_Time_Of_Day();
  }
  
  elapsed = Get_Time_Of_Day() - start;
  sem_name[10] = seq[0] + 1;
  int sem_short1 = Create_Semaphore(sem_name,0);
  V(sem_short1);
  //P (scr_sem);
  //Print("\nProcess Short is done at time: %d\n", elapsed) ;
  //V(scr_sem);


  return 0;
}
