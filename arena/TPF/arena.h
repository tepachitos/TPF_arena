#ifndef TPF_ARENA_H
#define TPF_ARENA_H

#include <SDL3/SDL_stdinc.h>

typedef struct TPF_Arena {
  size_t data_offset;
  size_t data_size;
  Uint8 *data_base;
} TPF_Arena;

/**
 * @brief Create a new arena using a specified configuration (for now only
 * static, heap arenas are supported).
 */
TPF_Arena *TPF_CreateArena(size_t size);

/**
 * @brief Releases the memory used by the arena and the arena handler
 * itself.
 *
 * @param arena to be released.
 */
void TPF_DestroyArena(TPF_Arena *arena);

/**
 * @brief Tries to push size bytes on top of arena, if there is no
 * more space the it will return NULL without setting an error.
 *
 * @param arena that will contain the block.
 * @param size of the block.
 * @return a pointer to the usable memory, NULL if EOM.
 */
void *TPF_ArenaTryPush(TPF_Arena *arena, size_t size);

/**
 * @brief Pushes a block of size bytes in the arena, if no space
 * is available then it will return NULL and set an error.
 *
 * @param arena that will contain the block.
 * @param size of the block.
 * @return a pointer to the usable memory, NULL if EOM.
 */
void *TPF_ArenaPush(TPF_Arena *arena, size_t size);

/**
 * @brief Pushes a block of size bytes in the arena, the block
 * is then initialized to zeroes. Returns NULL if no memory is
 * available.
 *
 * @param arena that will contain the block.
 * @param size of the block initialized to zeroes.
 * @return a pointer to the usable memory, NULL if EOM.
 */
void *TPF_ArenaPushZeroes(TPF_Arena *arena, size_t size);

/**
 * @brief Pushes a block of size bytes in the arena with a
 * left padding corresponding to the local machine standard alignment,
 * returns NULL if no memory is available.
 *
 * @param arena that will contain the block.
 * @param alignment to use for the allocation.
 * @param size of the block.
 * @return a pointer to the usable memory, NULL if EOM.
 */
void *TPF_ArenaTryAlignedPush(TPF_Arena *arena, size_t alignment, size_t size);

/**
 * @brief Pushes a block of size bytes in the arena with a
 * left padding corresponding to the local machine standard alignment,
 * returns NULL and sets an error if no memory is available.
 *
 * @param arena that will contain the block.
 * @param alignment to use for the allocation.
 * @param size of the block.
 * @return a pointer to the usable memory, NULL if EOM.
 */
void *TPF_ArenaAlignedPush(TPF_Arena *arena, size_t alignment, size_t size);

/**
 * @brief Pushes a block of size bytes in the arena with a
 * left padding corresponding to the local machine standard alignment,
 * the block is then initialized to zeroes. Returns NULL if no memory is
 * available.
 *
 * @param arena that will contain the block.
 * @param alignment to use for the allocation.
 * @param size of the block initialized to zeroes.
 * @return a pointer to the usable memory, NULL if EOM.
 */
void *TPF_ArenaAlignedPushZeroes(TPF_Arena *arena, size_t algiment,
                                 size_t size);

/**
 * @brief Returns the current size of the arena.
 *
 * @param arena to be queried.
 * @return the current checkpoint of the given arena.
 */
size_t TPF_ArenaMark(const TPF_Arena *arena);

/**
 * @brief Recovers a checkpoint of an arena by setting
 * its head to the given position.
 *
 * @param checkpoint to recover the arena.
 */
void TPF_ArenaResetTo(TPF_Arena *arena, size_t checkpoint);

/**
 * @brief Clears everything from the arena, settings its position
 * to the start of the block.
 */
void TPF_ArenaClear(TPF_Arena *arena);

/**
 * @brief returns the current remaining storage in the arena.
 *
 * @param arena to be queried.
 * @return remaining memory on arena.
 */
size_t TPF_ArenaRemaining(const TPF_Arena *arena);

/**
 * @brief returns the used memory in the arena.
 *
 * @param arena to be queried.
 * @return used memory on arena.
 */
size_t TPF_ArenaUsed(const TPF_Arena *arena);
#endif /* TPF_ARENA_H */
