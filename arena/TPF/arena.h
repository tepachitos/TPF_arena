#ifndef TPF_ARENA_H
#define TPF_ARENA_H

#include <SDL3/SDL_stdinc.h>
#include <TPF/build_config.h>

typedef struct {
} TPF_Arena;

/**
 * @brief Allocates a new arena in heap memory with a preallocated
 * fixed size.
 *
 * @param total_size fixed amount of memory to use.
 */
TPF_Arena *TPF_ArenaAllocFixed(size_t total_size);

/**
 * @brief Return a new arena using a contiguous block of fixed size
 * within another arena.
 *
 * WARNING: Do not release this arena, instead pop it from top.
 * @param total_size fixed amount of memory to use from the parent arena.
 */
TPF_Arena *TPF_ArenaAllocNested(TPF_Arena *parent, size_t total_size);

/**
 * @brief Releases the memory used by the arena and the arena handler
 * itself.
 *
 * @param arena to be released.
 */
void TPF_ArenaRelease(TPF_Arena *arena);

/**
 * @brief Pushes a block of size bytes in the arena, returns
 * NULL if no memory is available.
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
 * left padding corresponding to the local machine standard aligment,
 * returns NULL if no memory is available.
 *
 * @param arena that will contain the block.
 * @param size of the block.
 * @return a pointer to the usable memory, NULL if EOM.
 */
void *TPF_ArenaAlignedPush(TPF_Arena *arena, size_t size);

/**
 * @brief Pushes a block of size bytes in the arena with a
 * left padding corresponding to the local machine standard aligment,
 * the block is then initialized to zeroes. Returns NULL if no memory is
 * available.
 *
 * @param arena that will contain the block.
 * @param size of the block initialized to zeroes.
 * @return a pointer to the usable memory, NULL if EOM.
 */
void *TPF_ArenaAlignedPushZeroes(TPF_Arena *arena, size_t size);

/**
 * @brief Releases size amount of bytes on top of the arena
 * memory.
 *
 * @param arena that will release the memory.
 * @param size of block to be released.
 */
void TPF_ArenaPop(TPF_Arena *arena, size_t size);

/**
 * @brief Returns the current size of the arena.
 *
 * @param arena to be queried.
 * @return the current checkpoint of the given arena.
 */
size_t TPF_ArenaCheckpoint(TPF_Arena *arena);

/**
 * @brief Recovers a checkpoint of an arena by setting
 * its head to the given position.
 *
 * @param checkpoint to recover the arena.
 */
void TPF_ArenaRecoverCheckpoint(TPF_Arena *arena, size_t checkpoint);

/**
 * @brief Clears everything from the arena, settings its position
 * to the start of the block.
 */
void TPF_ArenaClear(TPF_Arena *arena);
#endif /* TPF_ARENA_H */
