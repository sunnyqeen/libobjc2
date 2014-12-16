// On some platforms, we need _GNU_SOURCE to expose asprintf()
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include "objc/runtime.h"
#include "objc/blocks_runtime.h"
#include "blocks_runtime.h"
#include "lock.h"
#include "visibility.h"

#ifdef WIN32
#include <errno.h>
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif
#if __has_builtin(__builtin___clear_cache)
#	define clear_cache __builtin___clear_cache
#else
void __clear_cache(void* start, void* end);
#	define clear_cache __clear_cache
#endif


/* QNX needs a special header for asprintf() */
#ifdef __QNXNTO__
#include <nbutil.h>
#endif

#define PAGE_SIZE 4096

static void *executeBuffer;
static void *writeBuffer;
static ptrdiff_t offset;
static mutex_t trampoline_lock;


#ifdef WIN32

// mmap wrapper for Windows
// Based on https://code.google.com/p/mman-win32/

#ifndef FILE_MAP_EXECUTE
#define FILE_MAP_EXECUTE    0x0020
#endif

#define PROT_NONE       0
#define PROT_READ       1
#define PROT_WRITE      2
#define PROT_EXEC       4

#define MAP_FILE        0
#define MAP_SHARED      1
#define MAP_PRIVATE     2
#define MAP_TYPE        0xf
#define MAP_FIXED       0x10
#define MAP_ANONYMOUS   0x20
#define MAP_ANON        MAP_ANONYMOUS

#define MAP_FAILED      ((void *)-1)

/* Flags for msync. */
#define MS_ASYNC        1
#define MS_SYNC         2
#define MS_INVALIDATE   4

static int __map_mman_error(const DWORD err, const int deferr)
{
    if (err == 0)
        return 0;
    //TODO: implement
    return err;
}

static DWORD __map_mmap_prot_page(const int prot)
{
    DWORD protect = 0;
    
    if (prot == PROT_NONE)
        return protect;
        
    if ((prot & PROT_EXEC) != 0)
    {
        protect = ((prot & PROT_WRITE) != 0) ? 
                    PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
    }
    else
    {
        protect = ((prot & PROT_WRITE) != 0) ?
                    PAGE_READWRITE : PAGE_READONLY;
    }
    
    return protect;
}

static DWORD __map_mmap_prot_file(const int prot)
{
    DWORD desiredAccess = 0;
    
    if (prot == PROT_NONE)
        return desiredAccess;
        
    if ((prot & PROT_READ) != 0)
        desiredAccess |= FILE_MAP_READ;
    if ((prot & PROT_WRITE) != 0)
        desiredAccess |= FILE_MAP_WRITE;
    if ((prot & PROT_EXEC) != 0)
        desiredAccess |= FILE_MAP_EXECUTE;
    
    return desiredAccess;
}

void* mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off)
{
    HANDLE fm, h;
    
    void * map = MAP_FAILED;
    
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4293)
#endif

    const DWORD dwFileOffsetLow = (sizeof(off_t) <= sizeof(DWORD)) ? 
                    (DWORD)off : (DWORD)(off & 0xFFFFFFFFL);
    const DWORD dwFileOffsetHigh = (sizeof(off_t) <= sizeof(DWORD)) ?
                    (DWORD)0 : (DWORD)((off >> 32) & 0xFFFFFFFFL);
    const DWORD protect = __map_mmap_prot_page(prot);
    const DWORD desiredAccess = __map_mmap_prot_file(prot);

    const off_t maxSize = off + (off_t)len;

    const DWORD dwMaxSizeLow = (sizeof(off_t) <= sizeof(DWORD)) ? 
                    (DWORD)maxSize : (DWORD)(maxSize & 0xFFFFFFFFL);
    const DWORD dwMaxSizeHigh = (sizeof(off_t) <= sizeof(DWORD)) ?
                    (DWORD)0 : (DWORD)((maxSize >> 32) & 0xFFFFFFFFL);

#ifdef _MSC_VER
#pragma warning(pop)
#endif

    errno = 0;
    
    if (len == 0 
        /* Unsupported flag combinations */
        || (flags & MAP_FIXED) != 0
        /* Usupported protection combinations */
        || prot == PROT_EXEC)
    {
        errno = EINVAL;
        return MAP_FAILED;
    }
    
    h = ((flags & MAP_ANONYMOUS) == 0) ? 
                    (HANDLE)_get_osfhandle(fildes) : INVALID_HANDLE_VALUE;

    if ((flags & MAP_ANONYMOUS) == 0 && h == INVALID_HANDLE_VALUE)
    {
        errno = EBADF;
        return MAP_FAILED;
    }

    fm = CreateFileMapping(h, NULL, protect, dwMaxSizeHigh, dwMaxSizeLow, NULL);

    if (fm == NULL)
    {
        errno = __map_mman_error(GetLastError(), EPERM);
        return MAP_FAILED;
    }
  
    map = MapViewOfFile(fm, desiredAccess, dwFileOffsetHigh, dwFileOffsetLow, len);

    CloseHandle(fm);
  
    if (map == NULL)
    {
        errno = __map_mman_error(GetLastError(), EPERM);
        return MAP_FAILED;
    }

    return map;
}

int munmap(void *addr, size_t len)
{
    if (UnmapViewOfFile(addr))
        return 0;
        
    errno =  __map_mman_error(GetLastError(), EPERM);
    
    return -1;
}

int mprotect(void *addr, size_t len, int prot)
{
    DWORD newProtect = __map_mmap_prot_page(prot);
    DWORD oldProtect = 0;
    
    if (VirtualProtect(addr, len, newProtect, &oldProtect))
        return 0;
    
    errno =  __map_mman_error(GetLastError(), EPERM);
    
    return -1;
}

int msync(void *addr, size_t len, int flags)
{
    if (FlushViewOfFile(addr, len))
        return 0;
    
    errno =  __map_mman_error(GetLastError(), EPERM);
    
    return -1;
}

int mlock(const void *addr, size_t len)
{
    if (VirtualLock((LPVOID)addr, len))
        return 0;
        
    errno =  __map_mman_error(GetLastError(), EPERM);
    
    return -1;
}

int munlock(const void *addr, size_t len)
{
    if (VirtualUnlock((LPVOID)addr, len))
        return 0;
        
    errno =  __map_mman_error(GetLastError(), EPERM);
    
    return -1;
}


// mem fd wrapper for Windows

static void initTmpFile(void) {}

static int getAnonMemFd(void)
{
	TCHAR tempPathBuffer[MAX_PATH];
	TCHAR tempFileName[MAX_PATH];
	DWORD ret = GetTempPath(MAX_PATH, tempPathBuffer);
	if (ret > MAX_PATH || ret == 0)
		abort();
	ret = GetTempFileName(tempPathBuffer, TEXT("objc"), 0, tempFileName);
	if (ret == 0)
		abort();
	int fd = open(tempFileName, _O_RDWR | _O_BINARY);
	if (fd < 0)
		abort();
	return fd;
}

#else // !WIN32

#ifndef SHM_ANON
static char *tmpPattern;
static void initTmpFile(void)
{
	char *tmp = getenv("TMPDIR");
	if (NULL == tmp)
	{
		tmp = "/tmp/";
	}
	if (0 > asprintf(&tmpPattern, "%s/objc_trampolinesXXXXXXXXXXX", tmp))
	{
		abort();
	}
}
static int getAnonMemFd(void)
{
	const char *pattern = strdup(tmpPattern);
	int fd = mkstemp(pattern);
	unlink(pattern);
	free(pattern);
	return fd;
}
#else
static void initTmpFile(void) {}
static int getAnonMemFd(void)
{
	return shm_open(SHM_ANON, O_CREAT | O_RDWR, 0);
}
#endif

#endif // !WIN32


struct wx_buffer
{
	void *w;
	void *x;
};

PRIVATE void init_trampolines(void)
{
	INIT_LOCK(trampoline_lock);
	initTmpFile();
}

static struct wx_buffer alloc_buffer(size_t size)
{
	LOCK_FOR_SCOPE(&trampoline_lock);
	if ((0 == offset) || (offset + size >= PAGE_SIZE))
	{
		int fd = getAnonMemFd();
		ftruncate(fd, PAGE_SIZE);
		void *w = mmap(NULL, PAGE_SIZE, PROT_WRITE, MAP_SHARED, fd, 0);
		executeBuffer = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_EXEC, MAP_SHARED, fd, 0);
		*((void**)w) = writeBuffer;
		writeBuffer = w;
		offset = sizeof(void*);
	}
	struct wx_buffer b = { writeBuffer + offset, executeBuffer + offset };
	offset += size;
	return b;
}

extern void __objc_block_trampoline;
extern void __objc_block_trampoline_end;
extern void __objc_block_trampoline_sret;
extern void __objc_block_trampoline_end_sret;

IMP imp_implementationWithBlock(void *block)
{
	struct Block_layout *b = block;
	void *start;
	void *end;

	if ((b->flags & BLOCK_USE_SRET) == BLOCK_USE_SRET)
	{
		start = &__objc_block_trampoline_sret;
		end = &__objc_block_trampoline_end_sret;
	}
	else
	{
		start = &__objc_block_trampoline;
		end = &__objc_block_trampoline_end;
	}

	size_t trampolineSize = end - start;
	// If we don't have a trampoline intrinsic for this architecture, return a
	// null IMP.
	if (0 >= trampolineSize) { return 0; }

	struct wx_buffer buf = alloc_buffer(trampolineSize + 2*sizeof(void*));
	void **out = buf.w;
	out[0] = (void*)b->invoke;
	out[1] = Block_copy(b);
	memcpy(&out[2], start, trampolineSize);
	out = buf.x;
	char *newIMP = (char*)&out[2];
	clear_cache(newIMP, newIMP+trampolineSize);
	return (IMP)newIMP;
}

static void* isBlockIMP(void *anIMP)
{
	LOCK(&trampoline_lock);
	void *e = executeBuffer;
	void *w = writeBuffer;
	UNLOCK(&trampoline_lock);
	while (e)
	{
		if ((anIMP > e) && (anIMP < e + PAGE_SIZE))
		{
			return ((char*)w) + ((char*)anIMP - (char*)e);
		}
		e = *(void**)e;
		w = *(void**)w;
	}
	return 0;
}

void *imp_getBlock(IMP anImp)
{
	if (0 == isBlockIMP((void*)anImp)) { return 0; }
	return *(((void**)anImp) - 1);
}
BOOL imp_removeBlock(IMP anImp)
{
	void *w = isBlockIMP((void*)anImp);
	if (0 == w) { return NO; }
	Block_release(((void**)anImp) - 1);
	return YES;
}

PRIVATE size_t lengthOfTypeEncoding(const char *types);

char *block_copyIMPTypeEncoding_np(void*block)
{
	char *buffer = strdup(block_getType_np(block));
	if (NULL == buffer) { return NULL; }
	char *replace = buffer;
	// Skip the return type
	replace += lengthOfTypeEncoding(replace);
	while (isdigit(*replace)) { replace++; }
	// The first argument type should be @? (block), and we need to transform
	// it to @, so we have to delete the ?.  Assert here because this isn't a
	// block encoding at all if the first argument is not a block, and since we
	// got it from block_getType_np(), this means something is badly wrong.
	assert('@' == *replace);
	replace++;
	assert('?' == *replace);
	// Use strlen(replace) not replace+1, because we want to copy the NULL
	// terminator as well.
	memmove(replace, replace+1, strlen(replace));
	// The next argument should be an object, and we want to replace it with a
	// selector
	while (isdigit(*replace)) { replace++; }
	if ('@' != *replace)
	{
		free(buffer);
		return NULL;
	}
	*replace = ':';
	return buffer;
}
