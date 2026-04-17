#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgpte(void)
{
  uint64 va;
  struct proc *p;  

  p = myproc();
  argaddr(0, &va);
  pte_t *pte = pgpte(p->pagetable, va);
  if(pte != 0) {
      return (uint64) *pte;
  }
  return 0;
}
#endif

#ifdef LAB_PGTBL
int
sys_kpgtbl(void)
{
  struct proc *p;  

  p = myproc();
  vmprint(p->pagetable);
  return 0;
}
#endif


uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_pgaccess(void)
{
  uint64 base;     // Tham số 1: Địa chỉ bắt đầu kiểm tra
  int len;         // Tham số 2: Số trang cần kiểm tra
  uint64 maskaddr; // Tham số 3: Địa chỉ User để trả kết quả về

  // 1. Nhận 3 tham số từ User
  if(argaddr(0, &base) < 0 || argint(1, &len) < 0 || argaddr(2, &maskaddr) < 0)
    return -1;

  // Giới hạn kiểm tra tối đa 32 trang (tương ứng với 32 bit của 1 biến số nguyên)
  if(len > 32 || len < 0)
    return -1;

  unsigned int bitmask = 0;  // Biến tạm để lưu kết quả các bit (1 = đã truy cập)
  struct proc *p = myproc(); // Lấy tiến trình hiện tại đang chạy

  // 2. Vòng lặp kiểm tra từng trang bộ nhớ
  for(int i = 0; i < len; i++){
    uint64 va = base + i * PGSIZE; // Địa chỉ ảo của trang thứ i

    // Tra cứu Bảng phân trang (Page Table) để tìm PTE
    pte_t *pte = walk(p->pagetable, va, 0);

    // Nếu PTE có tồn tại (khác 0) và Hợp lệ (PTE_V)
    if(pte != 0 && (*pte & PTE_V)){
      
      // Kiểm tra xem bit PTE_A có đang bật sáng không?
      if(*pte & PTE_A){
        // Bật bit thứ i trong biến kết quả lên 1
        bitmask |= (1 << i); 
        
        // CỰC KỲ QUAN TRỌNG: Tắt bit PTE_A đi để reset cho lần theo dõi sau
        *pte &= ~PTE_A;      
      }
    }
  }

  // 3. Trả kết quả (bitmask) từ Kernel về lại biến maskaddr của User
  if(copyout(p->pagetable, maskaddr, (char *)&bitmask, sizeof(bitmask)) < 0)
    return -1;

  return 0;
}
