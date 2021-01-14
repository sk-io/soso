#include "descriptortables.h"
#include "timer.h"
#include "multiboot.h"
#include "fs.h"
#include "syscalls.h"
#include "serial.h"
#include "isr.h"
#include "vmm.h"
#include "alloc.h"
#include "process.h"
#include "keyboard.h"
#include "ttydriver.h"
#include "devfs.h"
#include "systemfs.h"
#include "pipe.h"
#include "sharedmemory.h"
#include "random.h"
#include "null.h"
#include "elf.h"
#include "debugprint.h"
#include "ramdisk.h"
#include "fatfilesystem.h"
#include "vbe.h"
#include "fifobuffer.h"
#include "gfx.h"
#include "mouse.h"
#include "sleep.h"
#include "console.h"

extern uint32 _start;
extern uint32 _end;
uint32 gPhysicalKernelStartAddress = (uint32)&_start;
uint32 gPhysicalKernelEndAddress = (uint32)&_end;

static void* locateInitrd(struct Multiboot *mbi, uint32* size)
{
    if (mbi->mods_count > 0)
    {
        uint32 startLocation = *((uint32*)mbi->mods_addr);
        uint32 endLocation = *(uint32*)(mbi->mods_addr + 4);

        *size = endLocation - startLocation;

        return (void*)startLocation;
    }

    return NULL;
}

int executeFile(const char *path, char *const argv[], char *const envp[], FileSystemNode* tty)
{
    int result = -1;

    Process* process = getCurrentThread()->owner;
    if (process)
    {
        FileSystemNode* node = getFileSystemNodeAbsoluteOrRelative(path, process);
        if (node)
        {
            File* f = open_fs(node, 0);
            if (f)
            {
                void* image = kmalloc(node->length);

                int32 bytesRead = read_fs(f, node->length, image);

                if (bytesRead > 0)
                {
                    char* name = "userProcess";
                    if (NULL != argv && NULL != argv[0])
                    {
                        name = argv[0];
                    }
                    Process* newProcess = createUserProcessFromElfData(name, image, argv, envp, process, tty);

                    if (newProcess)
                    {
                        result = newProcess->pid;
                    }
                }
                close_fs(f);

                kfree(image);
            }

        }
    }

    return result;
}

void printAsciiArt()
{
    printkf("     ________       ________      ________       ________     \n");
    printkf("    |\\   ____\\     |\\   __  \\    |\\   ____\\     |\\   __  \\    \n");
    printkf("    \\ \\  \\___|_    \\ \\  \\|\\  \\   \\ \\  \\___|_    \\ \\  \\|\\  \\   \n");
    printkf("     \\ \\_____  \\    \\ \\  \\\\\\  \\   \\ \\_____  \\    \\ \\  \\\\\\  \\  \n");
    printkf("      \\|____|\\  \\    \\ \\  \\\\\\  \\   \\|____|\\  \\    \\ \\  \\\\\\  \\ \n");
    printkf("        ____\\_\\  \\    \\ \\_______\\    ____\\_\\  \\    \\ \\_______\\\n");
    printkf("       |\\_________\\    \\|_______|   |\\_________\\    \\|_______|\n");
    printkf("       \\|_________|                 \\|_________|              \n");
    printkf("\n");
}

int kmain(struct Multiboot *mboot_ptr)
{
    int stack = 5;

    initializeDescriptorTables();

    uint32 memoryKb = mboot_ptr->mem_upper;//96*1024;
    initializeMemory(memoryKb);

    initializeVFS();
    initializeDevFS();

    if (MULTIBOOT_FRAMEBUFFER_TYPE_RGB == mboot_ptr->framebuffer_type)
    {
        Gfx_Initialize((uint32*)(uint32)mboot_ptr->framebuffer_addr, mboot_ptr->framebuffer_width, mboot_ptr->framebuffer_height, mboot_ptr->framebuffer_bpp / 8, mboot_ptr->framebuffer_pitch);

        //initializeTTYs(TRUE);
        initializeConsole(TRUE);
    }
    else
    {
        initializeTTYs(FALSE);
    }
    //printkf works after TTY initialization

    printAsciiArt();

    printkf("Kernel built on %s %s\n", __DATE__, __TIME__);
    //printkf("Lower Memory: %d KB\n", mboot_ptr->mem_lower);
    //printkf("Upper Memory: %d KB\n", mboot_ptr->mem_upper);
    printkf("Memory initialized for %d MB\n", memoryKb / 1024);
    printkf("Kernel resides in %x - %x\n", gPhysicalKernelStartAddress, gPhysicalKernelEndAddress);
    //printkf("Initial stack: %x\n", &stack);
    printkf("Video: %x\n", (uint32)mboot_ptr->framebuffer_addr);
    printkf("Video: %dx%dx%d Pitch:%d\n", mboot_ptr->framebuffer_width, mboot_ptr->framebuffer_height, mboot_ptr->framebuffer_bpp, mboot_ptr->framebuffer_pitch);

    initializeSystemFS();
    initializePipes();
    initializeSharedMemory();

    initializeTasking();

    initialiseSyscalls();

    initializeTimer();

    initializeKeyboard();
    initializeMouse();

    if (0 != mboot_ptr->cmdline)
    {
        printkf("Kernel cmdline:%s\n", (char*)mboot_ptr->cmdline);
    }

    initializeSerial();

    //Debug_initialize("/dev/com1");
    Debug_initialize("/dev/ptty9");

    initializeRandom();
    initializeNull();

    createRamdisk("ramdisk1", 20*1024*1024);

    initializeFatFileSystem();

    printkf("System started!\n");

    char* argv[] = {"shell", NULL};
    char* envp[] = {"HOME=/", "PATH=/initrd", NULL};

    uint32 initrdSize = 0;
    uint8* initrdLocation = locateInitrd(mboot_ptr, &initrdSize);
    uint8* initrdEndLocation = initrdLocation + initrdSize;
    if (initrdLocation == NULL)
    {
        PANIC("Initrd not found!\n");
    }
    else
    {
        printkf("Initrd found at %x - %x (%d bytes)\n", initrdLocation, initrdEndLocation, initrdSize);
        if ((uint32)KERN_PD_AREA_BEGIN < (uint32)initrdEndLocation)
        {
            printkf("Initrd must reside below %x !!!\n", KERN_PD_AREA_BEGIN);
            PANIC("Initrd image is too big!");
        }
        memcpy((uint8*)*(uint32*)getFileSystemNode("/dev/ramdisk1")->privateNodeData, initrdLocation, initrdSize);
        BOOL mountSuccess = mountFileSystem("/dev/ramdisk1", "/initrd", "fat", 0, 0);

        if (mountSuccess)
        {
            printkf("Starting shell on TTYs\n");

            executeFile("/initrd/shell", argv, envp, getFileSystemNode("/dev/ptty1"));
            executeFile("/initrd/shell", argv, envp, getFileSystemNode("/dev/ptty2"));
            executeFile("/initrd/shell", argv, envp, getFileSystemNode("/dev/ptty3"));
            executeFile("/initrd/shell", argv, envp, getFileSystemNode("/dev/ptty4"));
        }
        else
        {
            printkf("Mounting initrd failed!\n");
        }
    }

    createPipe("pipe0", 8);

    enableScheduler();

    enableInterrupts();

    while(TRUE)
    {
        //Idle thread

        halt();
    }

    return 0;
}
