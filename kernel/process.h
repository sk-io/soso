#ifndef PROCESS_H
#define PROCESS_H

#define KERNELMODE	0
#define USERMODE	1

#define MAX_OPENED_FILES 20

#define PROCESS_NAME_MAX 32

#include "common.h"
#include "fs.h"
#include "fifobuffer.h"
#include "spinlock.h"

typedef enum ThreadState
{
    TS_RUN,
    TS_WAITIO,
    TS_WAITCHILD,
    TS_SLEEP,
    TS_SUSPEND
} ThreadState;

struct Process
{
    char name[PROCESS_NAME_MAX];

    uint32 pid;


    uint32 *pd;

    uint32 b_exec;
    uint32 e_exec;
    uint32 b_bss;
    uint32 e_bss;

    char *brkBegin;
    char *brkEnd;
    char *brkNextUnallocatedPageBegin;

    uint8 mmappedVirtualMemory[RAM_AS_4K_PAGES / 8];

    uint32 signal;
    void* sigfn[32];

    FileSystemNode* tty;

    FileSystemNode* workingDirectory;

    //Thread* mainThread;

    Process* parent;

    File* fd[MAX_OPENED_FILES];

} __attribute__ ((packed));

typedef struct Process Process;

struct Thread
{
    uint32 threadId;

    struct
    {
        uint32 eax, ecx, edx, ebx;
        uint32 esp, ebp, esi, edi;
        uint32 eip, eflags;
        uint32 cs:16, ss:16, ds:16, es:16, fs:16, gs:16;
        uint32 cr3;
    } regs __attribute__ ((packed));

    struct
    {
        uint32 esp0;
        uint16 ss0;
        uint32 stackStart;
    } kstack __attribute__ ((packed));


    uint32 userMode;

    ThreadState state;
    void* state_privateData;

    Process* owner;

    uint32 contextSwitchCount;
    uint32 birthTime;
    uint32 contextStartTime;
    uint32 contextEndTime;
    uint32 consumedCPUTimeMs;
    uint32 consumedCPUTimeMsAtPrevMark;
    uint32 usageCPU; //FromPrevMark


    FifoBuffer* messageQueue;
    Spinlock messageQueueLock;

    struct Thread* next;

};

typedef struct Thread Thread;

typedef struct TimerInt_Registers
{
    uint32 gs, fs, es, ds;
    uint32 edi, esi, ebp, esp, ebx, edx, ecx, eax; //pushed by pushad
    uint32 eip, cs, eflags, esp_if_privilege_change, ss_if_privilege_change; //pushed by the CPU
} TimerInt_Registers;

typedef void (*Function0)();

void initializeTasking();
void createKernelThread(Function0 func);
Process* createUserProcessFromElfData(const char* name, uint8* elfData, char *const argv[], char *const envp[], Process* parent, FileSystemNode* tty);
Process* createUserProcessFromFunction(const char* name, Function0 func, char *const argv[], char *const envp[], Process* parent, FileSystemNode* tty);
Process* createUserProcessEx(const char* name, uint32 processId, uint32 threadId, Function0 func, uint8* elfData, char *const argv[], char *const envp[], Process* parent, FileSystemNode* tty);
void destroyThread(Thread* thread);
void destroyProcess(Process* process);
void changeThreadState(Thread* thread, ThreadState state, void* privateData);
void resumeThread(Thread* thread);
void threadStateToString(ThreadState state, uint8* buffer, uint32 bufferSize);
void waitForSchedule();
int32 getEmptyFd(Process* process);
int32 addFileToProcess(Process* process, File* file);
int32 removeFileFromProcess(Process* process, File* file);
Thread* getThreadById(uint32 threadId);
Thread* getPreviousThread(Thread* thread);
Thread* getMainKernelThread();
Thread* getCurrentThread();
void schedule(TimerInt_Registers* registers);
BOOL isThreadValid(Thread* thread);
BOOL isProcessValid(Process* process);
uint32 getSystemContextSwitchCount();

#endif // PROCESS_H
