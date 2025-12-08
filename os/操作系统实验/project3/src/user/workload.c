#include <conio.h>
#include <process.h>
#include <sched.h>
#include <sema.h>
#include <string.h>

#if !defined (NULL)
#define NULL 0
#endif

int main(int argc , char ** argv)
{
  int policy = -1;
  int start;
  int elapsed;
  int quantum;
  int scr_sem;			/* sid of screen semaphore */
  int sem_long;                 // 控制长进程启动的信号量（初始0）
  int sem_short[7]; // 控制7个短进程启动的信号量数组（初始0）
  int id_long;                  // 长进程ID
  int id_short[7];  // 7个短进程ID数组
  int i;

  if (argc == 3) {
      if (!strcmp(argv[1], "rr")) {
          policy = 0;
      } else if (!strcmp(argv[1], "mlf")) {
          policy = 1;
      } else {
	  Print("usage: %s [rr|mlf] <quantum>\n", argv[0]);
	  Exit(1);
      }
      quantum = atoi(argv[2]);
      Set_Scheduling_Policy(policy, quantum);
  } else {
      Print("usage: %s [rr|mlf] <quantum>\n", argv[0]);
      Exit(1);
  }

// 2. 手动创建7个短进程的启动信号量（初始值0，无循环、无拼接）
	sem_short[0] = Create_Semaphore("sem_short_1", 0);  // 第1个短进程信号量
	sem_short[1] = Create_Semaphore("sem_short_2", 0);  // 第2个短进程信号量
	sem_short[2] = Create_Semaphore("sem_short_3", 0);  // 第3个短进程信号量
	sem_short[3] = Create_Semaphore("sem_short_4", 0);  // 第4个短进程信号量
	sem_short[4] = Create_Semaphore("sem_short_5", 0);  // 第5个短进程信号量
	sem_short[5] = Create_Semaphore("sem_short_6", 0);  // 第6个短进程信号量
	sem_short[6] = Create_Semaphore("sem_short_7", 0);  // 第7个短进程信号量

  scr_sem = Create_Semaphore ("screen" , 1);
  sem_long = Create_Semaphore("sem_long",0);
  
  start = Get_Time_Of_Day(); // 记录总耗时起始时间
  P(scr_sem);
  Print("************* Start Workload Generator *********\n");
  V(scr_sem);

  id_long = Spawn_Program("/c/long.exe", "/c/long.exe");
  P(scr_sem);
  Print ("Long_id = %d\n",id_long);
  V(scr_sem);
 
	// 第1个短进程
	id_short[0] = Spawn_Program("/c/short.exe", "/c/short.exe 1");
	P(scr_sem);
	Print("Short1_id = : %d\n", id_short[0]);
	V(scr_sem);

	// 第2个短进程
	id_short[1] = Spawn_Program("/c/short.exe", "/c/short.exe 2");
	P(scr_sem);
	Print("Short2_id = : %d\n", id_short[1]);
	V(scr_sem);

	// 第3个短进程
	id_short[2] = Spawn_Program("/c/short.exe", "/c/short.exe 3");
	P(scr_sem);
	Print("Short3_id = : %d\n", id_short[2]);
	V(scr_sem);

	// 第4个短进程
	id_short[3] = Spawn_Program("/c/short.exe", "/c/short.exe 4");
	P(scr_sem);
	Print("Short4_id = : %d\n", id_short[3]);
	V(scr_sem);

	// 第5个短进程
	id_short[4] = Spawn_Program("/c/short.exe", "/c/short.exe 5");
	P(scr_sem);
	Print("Short5_id = : %d\n", id_short[4]);
	V(scr_sem);

	// 第6个短进程
	id_short[5] = Spawn_Program("/c/short.exe", "/c/short.exe 6");
	P(scr_sem);
	Print("Short6_id = : %d\n", id_short[5]);
	V(scr_sem);

	// 第7个短进程
	id_short[6] = Spawn_Program("/c/short.exe", "/c/short.exe 7");
	P(scr_sem);
	Print("Short7_id = : %d\n", id_short[6]);
	V(scr_sem);
  
  V(sem_long);                // 释放长进程：解除阻塞
  V(sem_short[0]);            // 释放第一个短进程：解除阻塞

  // 5. 循环等待短进程执行完成，执行完一个启动下一个
  for (i = 0; i < 7; i++) {
      Wait(id_short[i]); // 等待第i个短进程执行完成
      // 若不是最后一个短进程，释放下一个的信号量
      if (i < 6) {
          V(sem_short[i+1]); // 启动下一个短进程
      }
  }
 
  Wait(id_long);

  // 7. 统计并打印总耗时
  elapsed = Get_Time_Of_Day() - start;
  P(scr_sem);
  Print("Total elapsed time: %d ms\n", elapsed);
  V(scr_sem);

  // 8. 销毁信号量（可选，GeekOS会自动清理，规范起见显式销毁）
  Destroy_Semaphore(scr_sem);
  Destroy_Semaphore(sem_long);
  for (i = 0; i < 7; i++) {
      Destroy_Semaphore(sem_short[i]);
  }


  //elapsed = Get_Time_Of_Day() - start;
  //Print ("\nTests Completed at %d\n", elapsed) ;
  return 0;
}

