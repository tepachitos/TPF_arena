# TPF Arena

Memory arena implementation on top of SDL3.


## Interface

```
TPF_Arena *TPF_ArenaAlloc(size_t total_size);
void TPF_ArenaRelease(TPF_Arena *arena);
void *TPF_ArenaPush(TPF_Arena *arena, size_t size);
void *TPF_ArenaPushZero(TPF_Arena *arena, size_t size);
void TPF_ArenaPop(TPF_Arena *arena, size_t size);
size_t TPF_ArenaGetPos(TPF_Arena *arena);
void TPF_ArenaSetPosBack(TPF_Arena *arena, size_t pos);
void TPF_ArenaClear(TPF_Arena *arena);
```
