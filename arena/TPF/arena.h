#ifndef TPF_ARENA_H
#define TPF_ARENA_H

#include <SDL3/SDL_stdinc.h>

struct TPF_ArenaChunk;

typedef struct TPF_ArenaConfig {
  size_t max_arena_size;
  size_t max_chunk_size;
  // size_t max_free_size; // TODO: Implement this thingy
  size_t def_chunk_size;
} TPF_ArenaConfig;

typedef struct TPF_Arena TPF_Arena;
typedef struct TPF_ArenaMark {
  struct TPF_ArenaChunk *chunk;
  size_t used;
} TPF_ArenaMark;

/**
 * @brief Create a new arena using a specified configuration.
 *
 * @param config to use while the arena exists.
 */
TPF_Arena *TPF_ArenaCreate(const TPF_ArenaConfig config);

/**
 * @brief Releases the memory used by the arena and the arena handler
 * itself.
 *
 * @param arena to be released.
 */
void TPF_ArenaDestroy(TPF_Arena *arena);

/**
 * @brief Tries to push size bytes on top of an arena, if there is no
 * more space, it will return NULL without setting an error.
 *
 * @param arena that will contain the block.
 * @param size of the block.
 * @return a pointer to the usable memory, NULL if EOM.
 */
void *TPF_ArenaTryPush(TPF_Arena *arena, size_t alignment, size_t size);

/**
 * @brief Pushes a block of size bytes in the arena, if no space
 * is available, then it will return NULL and set an error.
 *
 * @param arena that will contain the block.
 * @param alignment of the data.
 * @param size of the block.
 * @return a pointer to the usable memory, NULL if EOM.
 */
void *TPF_ArenaPush(TPF_Arena *arena, size_t alignment, size_t size);

/**
 * @brief Tries to push a block of size bytes in the arena, the block
 * is then initialized to zeroes. Returns NULL if no memory is
 * available.
 *
 * @param arena that will contain the block.
 * @param alignment of the data.
 * @param size of the block initialized to zeroes.
 * @return a pointer to the usable memory, NULL if EOM.
 */
void *TPF_ArenaTryPushZeroes(TPF_Arena *arena, size_t alignment, size_t size);

/**
 * @brief Pushes a block of size bytes in the arena, the block
 * is then initialized to zeroes. Returns NULL if no memory is
 * available and sets SDL_Error.
 *
 * @param arena that will contain the block.
 * @param alignment of the data.
 * @param size of the block initialized to zeroes.
 * @return a pointer to the usable memory, NULL if EOM.
 */
void *TPF_ArenaPushZeroes(TPF_Arena *arena, size_t alignment, size_t size);

/**
 * @brief Returns the current state of the arena.
 *
 * @param arena to be queried.
 * @return the current checkpoint of the given arena.
 */
const TPF_ArenaMark TPF_ArenaGetMark(const TPF_Arena *arena);

/**
 * @brief Recovers a checkpoint of an arena by setting
 * its head to the given position.
 *
 * @param arena arena to be reset.
 * @param mark to recover the arena.
 */
void TPF_ArenaResetTo(TPF_Arena *arena, const TPF_ArenaMark mark);

/**
 * @brief Clears everything from the arena, settings its position
 * to the start of the block.
 */
void TPF_ArenaClear(TPF_Arena *arena);

/**
 * @brief returns the current remaining storage in the arena.
 *
 * @param arena to be queried.
 * @return remaining memory on the arena.
 */
size_t TPF_ArenaRemaining(const TPF_Arena *arena);

/**
 * @brief returns the remaining storage in the current chunk.
 *
 * @param arena to be queried.
 * @return remaining memory on chunk.
 */
size_t TPF_ArenaTailRemaining(const TPF_Arena *arena);

/**
 * @brief returns the used memory in the arena.
 *
 * @param arena to be queried.
 * @return used memory on the arena.
 */
size_t TPF_ArenaUsed(const TPF_Arena *arena);
#endif /* TPF_ARENA_H */
