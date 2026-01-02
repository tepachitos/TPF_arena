#include "TPF/TPF_arena.h"

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_stdinc.h>
#include <stdint.h>

#ifdef TPF_ARENA_DEBUG
#include <SDL3/SDL_log.h>
#endif

struct TPF_ArenaChunk {
  Uint8 *base;
  size_t cap;
  size_t used;
  struct TPF_ArenaChunk *prev;
};

struct TPF_Arena {
  struct TPF_ArenaChunk *active_tail;
  struct TPF_ArenaChunk *free_tail;
  struct TPF_ArenaConfig config;
  size_t active_cap;  // sum(cap) of chunks in active list
  size_t active_used; // total bytes used across active chunks
  size_t free_cap;    // sum(cap) of chunk in free list
  size_t total_cap;   // active_cap + free_cap
};

static size_t align_is_pow2(const size_t a) {
  return a && ((a & (a - 1)) == 0);
}

static size_t pad_of(Uint8 *head, size_t alignment) {
  SDL_assert_paranoid(head != NULL && alignment > 0);
  if (head == NULL || alignment == 0) {
    return 0;
  }

  uintptr_t addr = (uintptr_t)head;
  size_t mask = alignment - 1;
  return (size_t)((alignment - (addr & mask)) & mask);
}

static void push_back_chunk(struct TPF_ArenaChunk **list,
                            struct TPF_ArenaChunk *chunk) {
  SDL_assert_paranoid(list != NULL && chunk != NULL);
  if (list == NULL || chunk == NULL) {
    return;
  }

  chunk->prev = *list;
  *list = chunk;
}

static struct TPF_ArenaChunk *create_chunk(struct TPF_ArenaChunk **list,
                                           size_t size, bool zeroed) {
  SDL_assert_paranoid(size > 0);
  if (size == 0) {
    return NULL;
  }

  struct TPF_ArenaChunk *c = SDL_malloc(sizeof(struct TPF_ArenaChunk));
  if (c == NULL) {
    return NULL;
  }

  c->base = SDL_malloc(size);
  if (c->base == NULL) {
    SDL_free(c);
    return NULL;
  }

  if (zeroed) {
    SDL_memset(c->base, 0, size);
  }

  c->cap = size;
  c->used = 0;
  c->prev = NULL;
  push_back_chunk(list, c);
  return c;
}

static void destroy_chunk(struct TPF_ArenaChunk *c) {
  if (c == NULL) {
    return;
  }

  if (c->base != NULL) {
    SDL_free(c->base);
  }

  SDL_free(c);
}

static void destroy_chunk_list(struct TPF_ArenaChunk *tail) {
  struct TPF_ArenaChunk *cur = tail;
  while (cur != NULL) {
    struct TPF_ArenaChunk *prev = cur->prev;
    destroy_chunk(cur);
    cur = prev;
  }
}

static void *alloc_in_chunk(struct TPF_ArenaChunk *c, size_t alignment,
                            size_t size, size_t *active_used) {
  SDL_assert_paranoid(c != NULL && size > 0);
  if (c == NULL || size == 0) {
    return NULL;
  }

  Uint8 *head = c->base + c->used;
  size_t pad = pad_of(head, alignment);
  size_t rem = c->cap - c->used;
  if (pad > rem || size > rem - pad) {
    return NULL;
  }

  size_t total_alloc = pad + size;
  c->used += total_alloc;
  *active_used += total_alloc;
  return head + pad;
}

static struct TPF_ArenaChunk *
extract_fittable_chunk(struct TPF_ArenaChunk **list, size_t alignment,
                       size_t size) {
  SDL_assert_paranoid(list != NULL && size > 0);
  if (list == NULL || size == 0) {
    return NULL;
  }

  struct TPF_ArenaChunk *cur = *list;
  struct TPF_ArenaChunk *prev = NULL;
  while (cur != NULL) {
    size_t need = size + alignment - 1;
    if (cur->cap >= need) {
      // unlink cur from list
      if (prev != NULL) {
        prev->prev = cur->prev;
      } else {
        *list = cur->prev;
      }

      cur->prev = NULL;
      return cur;
    }

    prev = cur;
    cur = cur->prev;
  }

  return NULL;
}

TPF_Arena *TPF_ArenaCreate(const TPF_ArenaConfig config) {
  SDL_assert_paranoid(config.def_chunk_size > 0);
  if (config.def_chunk_size == 0) {
    return NULL;
  }

  TPF_Arena *arena = SDL_malloc(sizeof(TPF_Arena));
  if (arena == NULL) {
    return NULL;
  }

  arena->config = config;
  arena->active_tail = NULL;
  arena->free_tail = NULL;
  arena->active_cap = 0;
  arena->active_used = 0;
  arena->free_cap = 0;
  arena->total_cap = 0;
  return arena;
}

void TPF_ArenaDestroy(TPF_Arena *arena) {
  if (arena == NULL) {
    return;
  }

  destroy_chunk_list(arena->free_tail);
  destroy_chunk_list(arena->active_tail);
  SDL_free(arena);
}

struct push_params {
  TPF_Arena *arena;
  size_t alignment;
  size_t size;
  bool zeroed;
  bool set_errors;
};

static void *arena_push_base(struct push_params params) {
  TPF_Arena *arena = params.arena;
  size_t alignment = params.alignment;
  size_t size = params.size;
  bool zeroed = params.zeroed;
  bool set_errors = params.set_errors;

  SDL_assert_paranoid(arena != NULL && size > 0);
  if (arena == NULL || size == 0) {
    return NULL;
  }

  if (!align_is_pow2(alignment)) {
    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    SDL_assert(false && "alignment is not a power of 2");
    return NULL;
  }

  Uint8 *ptr = NULL;

  // 1. Try to use space in trailing chunk
  struct TPF_ArenaChunk *tail = arena->active_tail;
  if (tail != NULL) {
    ptr = alloc_in_chunk(tail, alignment, size, &arena->active_used);
    if (ptr != NULL) {
      return ptr;
    }
  }

  // 2. Recycle a fittable chunk
#ifdef TPF_ARENA_DEBUG
  if (tail != NULL) {
    SDL_Log("WARNING(ARENA): wasting %zu bytes of memory",
            tail->cap - tail->used);
  }
#endif
  struct TPF_ArenaChunk *recycled =
      extract_fittable_chunk(&arena->free_tail, alignment, size);
  if (recycled != NULL) {
    arena->free_cap -= recycled->cap;
    arena->active_cap += recycled->cap;

    push_back_chunk(&arena->active_tail, recycled);
    ptr = alloc_in_chunk(recycled, alignment, size, &arena->active_used);
    if (ptr != NULL) {
      return ptr;
    }
  }

  // 3. Allocate a new chunk
  size_t need = size + alignment - 1;
  size_t cap = SDL_max(need, arena->config.def_chunk_size);
  if (arena->config.max_chunk_size != 0) {
    cap = SDL_min(cap, arena->config.max_chunk_size);
  }

  if (arena->config.max_arena_size != 0) {
    if (arena->total_cap + cap > arena->config.max_arena_size) {
      if (set_errors) {
        SDL_SetError("Arena cap limit reached");
      }
      return NULL;
    }
  }

  struct TPF_ArenaChunk *created =
      create_chunk(&arena->active_tail, cap, zeroed);
  if (created != NULL) {
    arena->active_cap += cap;
    arena->total_cap += cap;
    ptr = alloc_in_chunk(created, alignment, size, &arena->active_used);
    if (ptr != NULL) {
      return ptr;
    }
  }

  if (set_errors) {
    SDL_SetError("Failed to allocate %zu bytes of memory", size);
  }
  return NULL;
}

void *TPF_ArenaTryPush(TPF_Arena *arena, size_t alignment, size_t size) {
  struct push_params params = {
      .arena = arena,
      .alignment = alignment,
      .size = size,
      .zeroed = false,
      .set_errors = false,
  };
  return arena_push_base(params);
}

void *TPF_ArenaPush(TPF_Arena *arena, size_t alignment, size_t size) {
  struct push_params params = {
      .arena = arena,
      .alignment = alignment,
      .size = size,
      .zeroed = false,
      .set_errors = true,
  };
  return arena_push_base(params);
}

void *TPF_ArenaTryPushZeroes(TPF_Arena *arena, size_t alignment, size_t size) {
  struct push_params params = {
      .arena = arena,
      .alignment = alignment,
      .size = size,
      .zeroed = true,
      .set_errors = false,
  };
  return arena_push_base(params);
}

void *TPF_ArenaPushZeroes(TPF_Arena *arena, size_t alignment, size_t size) {
  struct push_params params = {
      .arena = arena,
      .alignment = alignment,
      .size = size,
      .zeroed = true,
      .set_errors = true,
  };
  return arena_push_base(params);
}

const TPF_ArenaMark TPF_ArenaGetMark(const TPF_Arena *arena) {
  SDL_assert_paranoid(arena != NULL);
  if (arena == NULL) {
    return (TPF_ArenaMark){0};
  }

  struct TPF_ArenaChunk *tail = arena->active_tail;
  return (TPF_ArenaMark){
      .chunk = tail,
      .used = tail != NULL ? tail->used : 0,
  };
}

void TPF_ArenaResetTo(TPF_Arena *arena, const TPF_ArenaMark mark) {
  SDL_assert_paranoid(arena != NULL);
  if (arena == NULL) {
    return;
  }

  // Move chunks until we reach mark.chunk (or until empty if chunk == NULL)
  while (arena->active_tail != NULL && arena->active_tail != mark.chunk) {
    struct TPF_ArenaChunk *c = arena->active_tail;
    arena->active_tail = c->prev;

    // accounting
    arena->active_used -= c->used;

    arena->active_cap -= c->cap;
    arena->free_cap += c->cap;

    c->prev = NULL;
    c->used = 0;
    push_back_chunk(&arena->free_tail, c);
  }

  if (mark.chunk == NULL) {
    // cleared everything
    SDL_assert_paranoid(arena->active_tail == NULL);
    return;
  }

  // check if mark chunk has been found
  struct TPF_ArenaChunk *tail = arena->active_tail;
  SDL_assert_paranoid(tail == mark.chunk);
  if (tail != NULL) {
    size_t old_used = tail->used;
    SDL_assert_paranoid(mark.used <= old_used);
    tail->used = mark.used;
    arena->active_used -= (old_used - mark.used);
  }
}

void TPF_ArenaClear(TPF_Arena *arena) {
  SDL_assert_paranoid(arena != NULL);
  if (arena == NULL) {
    return;
  }

  TPF_ArenaMark clear_mark = (const TPF_ArenaMark){
      .chunk = NULL,
      .used = 0,
  };
  TPF_ArenaResetTo(arena, clear_mark);
}

size_t TPF_ArenaRemaining(const TPF_Arena *arena) {
  SDL_assert_paranoid(arena != NULL);
  if (arena == NULL) {
    return 0;
  }

  TPF_ArenaConfig config = arena->config;
  if (config.max_arena_size == 0) {
    return (size_t)-1;
  }

  size_t cap = arena->total_cap;
  return cap >= config.max_arena_size ? 0 : (config.max_arena_size - cap);
}

size_t TPF_ArenaTailRemaining(const TPF_Arena *arena) {
  SDL_assert_paranoid(arena != NULL);
  if (arena == NULL || arena->active_tail == NULL) {
    return 0;
  }

  return arena->active_tail->cap - arena->active_tail->used;
}

size_t TPF_ArenaUsed(const TPF_Arena *arena) {
  SDL_assert_paranoid(arena != NULL);
  if (arena == NULL) {
    return 0;
  }

  return arena->active_used;
}
