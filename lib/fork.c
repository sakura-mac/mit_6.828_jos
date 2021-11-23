// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	uint32_t write_err = err & FEC_WR;
	uint32_t COW = uvpt[PGNUM(addr)] & PTE_COW;
	if(!(write_err && COW))panic("pgfault: not write to the COW page fault!\n");
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	//alloc a page by PFTEMP

	addr = ROUNDDOWN(addr, PGSIZE);
	r = sys_page_alloc(0, PFTEMP, PTE_U | PTE_P | PTE_W);
	if(r < 0)panic("pgfault: sys_page_alloc failed!\n");
	//copy data
	memmove(PFTEMP, addr, PGSIZE);
	r = sys_page_map(0, PFTEMP, 0, addr, PTE_U | PTE_P | PTE_W);
	if(r < 0)panic("pgfault: sys_page_map failed!\n");
	
	//remove PTE:PFTEMP
	r = sys_page_unmap(0, PFTEMP);
	if(r < 0)panic("pgfault: sys_page_unmap failed!\n");
	//panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	//COW check, map page
	pte_t pte = uvpt[pn];
	void *addr = (void *) (pn * PGSIZE);
	
	uint32_t perm = pte&0xfff;
	if(perm & (PTE_W | PTE_COW)){
		perm &= ~PTE_W;
		perm |= PTE_COW;
	}
	
	r = sys_page_map(0, addr, envid, addr, perm & PTE_SYSCALL);
	if(r < 0)panic("duppage: sys_map_page child failed\n");
	//map self again : freeze parent and child
	r = sys_page_map(0, addr, 0, addr, perm & PTE_SYSCALL);
	if(r < 0)panic("duppage: sys_map_page self failed\n");
	//panic("duppage not implemented");
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	int r;
	envid_t envid;
	int *addr;
	//set up page fault handler
	set_pgfault_handler(pgfault);
	//create a child
	envid = sys_exofork();//copy the tf:all reg
	if(envid < 0)panic("fork: sys_exofork failed\n");
	if(envid == 0){
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;//after parent set,child env now run
	}
	//copy space : COW,so just the map only:duppage function
	for(addr = (int *)UTEXT; addr < (int *)USTACKTOP; addr += PGSIZE){
		if((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P))duppage(envid, PGNUM(addr));
	}
	//child's exception stack
	r = sys_page_alloc(envid, (void *)UXSTACKTOP-PGSIZE, PTE_W | PTE_U | PTE_P);
	if(r < 0)panic("fork: sys_page_alloc failed\n");
	//child's page fault handler
	extern void _pgfault_upcall();
	r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall);
	if(r) panic("fork: sys_env_set_pgfault_upcall failed\n");
	//set status = ENV_RUNNABLE
	r = sys_env_set_status(envid, ENV_RUNNABLE);
	if(r < 0)panic("fork: sys_env_set_status failed\n");

	return envid;
	//panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
