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
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <sys/mman.h>
#include <pthread.h>
#include <errno.h>
#include <stdalign.h>

#define MMAPALLOC "mmapalloc: "
#define ARENA_CONTEXT_DEFAULT_INIT_SIZE (1024 * 32)

#define ALIGNMENT __BIGGEST_ALIGNMENT__
#define ALIGN_UP(x) (((x) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

#define CHUNK_CONTEXT_FREE 1
#define CHUNK_CONTEXT_USED 0

union ChunkContext
{
    struct
    {
        size_t size_chunk;
        unsigned int is_free_chunk;
        union ChunkContext *next_chunk;
    } stub;
    char align[16];
};

struct ArenaContext
{
    size_t size_arena;
    size_t offset_arena;
    size_t block_active_arena;

    union ChunkContext *head_list_chunk;
    union ChunkContext *tail_list_chunk;

    struct ArenaContext *next_arena;
};

/*
 * NOTE : metadata lain menggunakan union untuk meringkas dan aligned
 */

/* static block */
struct ArenaContext *global_head_arena = NULL,*global_tail_arena = NULL;

pthread_mutex_t mmapalloc_mutex_guard = PTHREAD_MUTEX_INITIALIZER;

static void *arena_init_context()
{
    void *new_mmap_arena_memory = (void *)mmap(NULL,sizeof(struct ArenaContext) + ARENA_CONTEXT_DEFAULT_INIT_SIZE,
        PROT_READ | PROT_WRITE,MAP_PRIVATE | MAP_ANONYMOUS,-1,0
        );
    if (new_mmap_arena_memory == MAP_FAILED)
    {
        fprintf(stderr,MMAPALLOC "mmap failed %s\n",strerror(errno));
        return NULL;
    }
    struct ArenaContext *arena_ctx = (struct ArenaContext *)new_mmap_arena_memory;

    arena_ctx->size_arena = ARENA_CONTEXT_DEFAULT_INIT_SIZE;
    arena_ctx->offset_arena = ALIGN_UP(sizeof(struct ArenaContext));
    arena_ctx->block_active_arena = 0;
    arena_ctx->next_arena = NULL;

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
 * allocated memory size N
 */
void *mmapalloc(size_t size)
{
    size = ALIGN_UP(size);
    if (!global_head_arena)
    {
      pthread_mutex_lock(&mmapalloc_mutex_guard);
        global_head_arena = arena_init_context();
        if (!global_head_arena)
        {
            fprintf(stderr,MMAPALLOC "failed to initialized arena context\n");
            pthread_mutex_unlock(&mmapalloc_mutex_guard);
            return NULL;
        }
    }
   
    struct ArenaContext *arena_ctx_current = global_head_arena;

    union ChunkContext *chunk_ctx_current = src_chunk_free(arena_ctx_current,size);
    if (chunk_ctx_current)
    {
        chunk_ctx_current->stub.is_free_chunk = CHUNK_CONTEXT_USED;
        global_head_arena->block_active_arena++;
        pthread_mutex_unlock(&mmapalloc_mutex_guard);
        return (void *)((uintptr_t)chunk_ctx_current + sizeof(union ChunkContext));
    }

    void *new_memory_reserve = expand_offset_arena(arena_ctx_current,sizeof(union ChunkContext) + size);
    /* pthread_mutex_unlock(&mmapalloc_mutex_guard); */
    if (!new_memory_reserve)
    {
        fprintf(stderr,MMAPALLOC "failed to allocated new memory for chunk\n");
        pthread_mutex_unlock(&mmapalloc_mutex_guard);
        return NULL;
    }

    union ChunkContext *new_chunk_context = (union ChunkContext *)new_memory_reserve;

    new_chunk_context->stub.is_free_chunk = CHUNK_CONTEXT_USED;
    new_chunk_context->stub.size_chunk = size;
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
    pthread_mutex_unlock(&mmapalloc_mutex_guard);
    return (void *)((uintptr_t)new_chunk_context + sizeof(union ChunkContext));
}

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

  current_chunk_ctx_2_free->stub.is_free_chunk = CHUNK_CONTEXT_FREE;
  global_head_arena->block_active_arena--;
  coalesing_chunk();
}

// Hoam ngantuk
