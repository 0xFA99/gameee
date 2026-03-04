/*
 * NOTE : Hallo,ini adalah memory allocator berbasis mmap() linux syscall
 * untuk utility dari gameee.
 *
 * mungkin memorynya masih banyak fragmentasi external dan akses yang
 * linear ,algoritma pencarian block yang overhead dan lainya.
 * jika ada saran,silahkan perbaiki dan buat request.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdalign.h>
#include <errno.h>

#ifdef _WIN32
  #define PLATFORM_WINDOWS
  #include <windows.h>
  #define MUTEX_T             CRITICAL_SECTION
  #define MUTEX_INIT(m)       InitializeCriticalSection(&(m))
  #define MUTEX_LOCK(m)       EnterCriticalSection(&(m))
  #define MUTEX_UNLOCK(m)     LeaveCriticalSection(&(m))
  #define MUTEX_DESTROY(m)    DeleteCriticalSection(&(m))
#else
  #define PLATFORM_POSIX
  #include <unistd.h>
  #include <sys/mman.h>
  #include <pthread.h>
  #define MUTEX_T             pthread_mutex_t
  #define MUTEX_INIT(m)       pthread_mutex_init(&(m), NULL)
  #define MUTEX_LOCK(m)       pthread_mutex_lock(&(m))
  #define MUTEX_UNLOCK(m)     pthread_mutex_unlock(&(m))
  #define MUTEX_DESTROY(m)    pthread_mutex_destroy(&(m))
#endif

#define MMAPALLOC "mmapalloc: "
#define ARENA_CONTEXT_DEFAULT_INIT_SIZE (1024 * 32)

#define ALIGNMENT __BIGGEST_ALIGNMENT__
#define ALIGN_UP(x) (((x) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

#define CHUNK_CONTEXT_FREE 1
#define CHUNK_CONTEXT_USED 0

#define MMAPALLOC_MAGIC 0xFFFF0000

union ChunkContext
{
    struct
    {
        size_t size_chunk;
        unsigned int is_free_chunk;
        unsigned int chunk_magic;
        union ChunkContext *next_chunk;
    } stub;
    char align[16];
};

struct ArenaContext
{
    size_t size_arena;
    size_t offset_arena;
    size_t block_active_arena;
    unsigned int magic_number_arena;

    union ChunkContext *head_list_chunk;
    union ChunkContext *tail_list_chunk;

    struct ArenaContext *next_arena;
};

/*
 * NOTE : metadata lain menggunakan union untuk meringkas dan aligned
 */

/* static block */
struct ArenaContext *global_head_arena = NULL,*global_tail_arena = NULL;

MUTEX_T mmapalloc_mutex_guard;
int mmapalloc_mutex_initialized = 0;

static void *os_alloc(size_t size)
{
#ifdef PLATFORM_WINDOWS
    void *ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!ptr) {
        fprintf(stderr, MMAPALLOC "VirtualAlloc failed, error: %lu\n", GetLastError());
        return NULL;
    }
    return ptr;
#else
    void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        fprintf(stderr, MMAPALLOC "mmap failed: %s\n", strerror(errno));
        return NULL;
    }
    return ptr;
#endif
}

static int os_free(void *ptr, size_t size)
{
#ifdef PLATFORM_WINDOWS
    if (!VirtualFree(ptr, 0, MEM_RELEASE)) {
        fprintf(stderr, MMAPALLOC "VirtualFree failed, error: %lu\n", GetLastError());
        return -1;
    }
    return 0;
#else
    return munmap(ptr, size);
#endif
}

static void *arena_init_context()
{
    size_t total = sizeof(struct ArenaContext) + ARENA_CONTEXT_DEFAULT_INIT_SIZE;
    struct ArenaContext *arena_ctx = (struct ArenaContext *)os_alloc(total);
    if (!arena_ctx) return NULL;

    arena_ctx->size_arena = ARENA_CONTEXT_DEFAULT_INIT_SIZE;
    arena_ctx->offset_arena = ALIGN_UP(sizeof(struct ArenaContext));
    arena_ctx->block_active_arena = 0;
    arena_ctx->next_arena = NULL;
    arena_ctx->magic_number_arena = MMAPALLOC_MAGIC;

    arena_ctx->head_list_chunk = NULL;
    arena_ctx->tail_list_chunk = NULL;

    return (void*)arena_ctx;
}

static void *expand_offset_arena(struct ArenaContext *arena_ctx,size_t size)
{
    if (!arena_ctx) return NULL;
    size = ALIGN_UP(size);
    if (arena_ctx->offset_arena + size > arena_ctx->size_arena + sizeof(struct ArenaContext))
    {
        fprintf(stderr,MMAPALLOC "failed to expand offset arena,out of memory\n");
        return NULL;
    }

    void *new_break = (void *)((uintptr_t)arena_ctx + arena_ctx->offset_arena);
    arena_ctx->offset_arena = arena_ctx->offset_arena + size;
    return new_break;
}

static union ChunkContext *src_chunk_free(struct ArenaContext *arena_ctx,size_t chunk_size)
{
    union ChunkContext *current_chunk = arena_ctx->head_list_chunk;
    while (current_chunk)
    {
        if (current_chunk->stub.is_free_chunk && current_chunk->stub.size_chunk >= chunk_size)
        {
            return current_chunk;
        }
        current_chunk = current_chunk->stub.next_chunk;
    }
    return NULL;
}


static inline void coalesing_chunk()
{
  union ChunkContext *current_chunk_ctx = global_head_arena->head_list_chunk;
  while(current_chunk_ctx && current_chunk_ctx->stub.next_chunk){
    if(current_chunk_ctx->stub.is_free_chunk && current_chunk_ctx->stub.next_chunk->stub.is_free_chunk){
      current_chunk_ctx->stub.size_chunk += (sizeof(union ChunkContext) + current_chunk_ctx->stub.next_chunk->stub.size_chunk);
      current_chunk_ctx->stub.next_chunk = current_chunk_ctx->stub.next_chunk->stub.next_chunk;

      if(current_chunk_ctx->stub.next_chunk == NULL){
        global_head_arena->tail_list_chunk = current_chunk_ctx;
      }
    } else {
      current_chunk_ctx = current_chunk_ctx->stub.next_chunk;
    }
  }
}

/*
 * @brief allocated memory size N
 *
 * @param size_t size
 * @return (void *)ptr memory
 */
void *mmapalloc(size_t size)
{
    size = ALIGN_UP(size);
    if (!mmapalloc_mutex_initialized) {
        MUTEX_INIT(mmapalloc_mutex_guard);
        mmapalloc_mutex_initialized = 1;
    }
    MUTEX_LOCK(mmapalloc_mutex_guard);

    if (!global_head_arena)
    {
        global_head_arena = arena_init_context();
        if (!global_head_arena)
        {
            fprintf(stderr,MMAPALLOC "failed to initialized arena context\n");
            MUTEX_UNLOCK(mmapalloc_mutex_guard);
            return NULL;
        }
    }
   
    struct ArenaContext *arena_ctx_current = global_head_arena;

    union ChunkContext *chunk_ctx_current = src_chunk_free(arena_ctx_current,size);
    if (chunk_ctx_current)
    {
        chunk_ctx_current->stub.is_free_chunk = CHUNK_CONTEXT_USED;
        global_head_arena->block_active_arena++;
        MUTEX_UNLOCK(mmapalloc_mutex_guard);
        return (void *)((uintptr_t)chunk_ctx_current + sizeof(union ChunkContext));
    }

    void *new_memory_reserve = expand_offset_arena(arena_ctx_current,sizeof(union ChunkContext) + size);
    /* pthread_mutex_unlock(&mmapalloc_mutex_guard); */
    if (!new_memory_reserve)
    {
        fprintf(stderr,MMAPALLOC "failed to allocated new memory for chunk\n");
        MUTEX_UNLOCK(mmapalloc_mutex_guard);
        return NULL;
    }

    union ChunkContext *new_chunk_context = (union ChunkContext *)new_memory_reserve;

    new_chunk_context->stub.is_free_chunk = CHUNK_CONTEXT_USED;
    new_chunk_context->stub.size_chunk = size;
    new_chunk_context->stub.chunk_magic = MMAPALLOC_MAGIC;
    new_chunk_context->stub.next_chunk = NULL;

    if (!arena_ctx_current->head_list_chunk)
    {
        arena_ctx_current->head_list_chunk = new_chunk_context;
        arena_ctx_current->tail_list_chunk = new_chunk_context;
    } else
    {
        arena_ctx_current->tail_list_chunk->stub.next_chunk = new_chunk_context;
        arena_ctx_current->tail_list_chunk = new_chunk_context;
    }
    global_head_arena->block_active_arena++;
    MUTEX_UNLOCK(mmapalloc_mutex_guard);
    return (void *)((uintptr_t)new_chunk_context + sizeof(union ChunkContext));
}

/*
 * @brief mmapfree() free chunk memory
 *
 * @param void *chunk_ptr
 */
void mmapfree(void *chunk_ptr){
  if(!chunk_ptr) {
    fprintf(stderr,MMAPALLOC "pointer invalid\n");
    return;
  }

  union ChunkContext *current_chunk_ctx_2_free = (union ChunkContext *)((uintptr_t)chunk_ptr - sizeof(union ChunkContext));
  if(current_chunk_ctx_2_free->stub.is_free_chunk == CHUNK_CONTEXT_FREE){
    fprintf(stderr,MMAPALLOC "double free detected at ptr : %p\n",current_chunk_ctx_2_free);
    __builtin_trap();
  }

  if (current_chunk_ctx_2_free->stub.chunk_magic != MMAPALLOC_MAGIC)
  {
      fprintf(stderr,MMAPALLOC "magic number invalid/corrupt\n");
      __builtin_trap();
  }
  current_chunk_ctx_2_free->stub.is_free_chunk = CHUNK_CONTEXT_FREE;
  global_head_arena->block_active_arena--;
  coalesing_chunk();
}

/*
 * @brief mmapalloc_destroy() Destroy entire arena
 *
 * @return history of block active
 */
int mmapalloc_destroy()
{
    if (!global_head_arena) {
        fprintf(stderr, MMAPALLOC "invalid destroy arena\n");
        return EXIT_FAILURE;
    }

    if (global_head_arena->magic_number_arena != MMAPALLOC_MAGIC) {
        fprintf(stderr, MMAPALLOC "resource corrupt\n");
        return EXIT_FAILURE;
    }

    int block_remaining = global_head_arena->block_active_arena;
    size_t total_size   = global_head_arena->size_arena + sizeof(struct ArenaContext);

    int ret = os_free(global_head_arena, total_size);
    global_head_arena = NULL;
    global_tail_arena = NULL;

    if (mmapalloc_mutex_initialized) {
        MUTEX_DESTROY(mmapalloc_mutex_guard);
        mmapalloc_mutex_initialized = 0;
    }

    if (ret != 0) {
        fprintf(stderr, MMAPALLOC "failed to destroy arena\n");
        return EXIT_FAILURE;
    }

    return block_remaining;
}

// Hoam ngantuk
