#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

/*Designed for 64 bits memory format*/

typedef struct dynamic_array_header {
   uint64_t item_size;
   uint64_t length;
} dynamic_array_header;

typedef void*(*allocator_allocate)  (uint64_t);
typedef void*(*allocator_reallocate)(void*, uint64_t);
typedef void (*allocator_deallocate)(void*, uint64_t);

#define get_dynamic_array_header(DynamicArray) (dynamic_array_header*)(((void*)DynamicArray) - sizeof(dynamic_array_header))

inline static void* _dynamic_array_create(uint64_t item_size, uint64_t length, allocator_allocate alloc) {
   assert(alloc && item_size > 0);

   uint64_t total_size = sizeof(dynamic_array_header) + (item_size * length);
   char* total_memory = alloc(total_size);
   assert(total_memory);
   for(uint64_t i = sizeof(dynamic_array_header); i < total_size; ++i) {
      total_memory[i] = 0;
   }
   dynamic_array_header* header = (dynamic_array_header*)total_memory;
   header->item_size = item_size;
   header->length = length;
   return total_memory + sizeof(dynamic_array_header);
}

inline static void _dynamic_array_destroy(void* darray, allocator_deallocate deallocate) {
   void* memory_start = get_dynamic_array_header(darray);
   dynamic_array_header* header = (dynamic_array_header*)memory_start;
   uint64_t memory_size = (header->item_size * header->length) + sizeof(dynamic_array_header);
   deallocate(memory_start, memory_size);
}

inline static uint64_t _dynamic_array_length(void* darray) {
   dynamic_array_header* header = get_dynamic_array_header(darray);
   return header->length;
}

inline static void* _dynamic_array_index(void* darray, uint64_t index) {
   dynamic_array_header* header = get_dynamic_array_header(darray);
   void* index_ptr = NULL;
   if(index < header->length) {
      index_ptr = darray + (header->item_size * index);
   }
   return index_ptr;
}

inline static void* _dynamic_array_resize(void* darray, uint64_t new_length, allocator_reallocate realloc) {
   dynamic_array_header* header = get_dynamic_array_header(darray);
   header = realloc(header, sizeof(dynamic_array_header) + (new_length * header->item_size));
   if(header->length < new_length) {
      char* darray_as_bytes = (char*)darray;
      for(uint64_t i = (new_length * header->item_size) - 1; i > (header->length * header->item_size) - 1; i--) {
         darray_as_bytes[i] = 0;
      }
   }
   header->length = new_length;
   return ((void*)header) + sizeof(dynamic_array_header);
}

inline static void* _dynamic_array_copy(void* darray, allocator_allocate alloc) {
   dynamic_array_header* header = get_dynamic_array_header(darray);
   uint64_t total_size = (header->length * header->item_size) + sizeof(dynamic_array_header);
   char* darray_bytes = alloc(total_size);
   char* bytes_to_copy = (char*)header;
   for(uint64_t i = 0; i < total_size; ++i) {
      darray_bytes[i] = bytes_to_copy[i];
   }
   return ((void*)darray_bytes) + sizeof(dynamic_array_header);
}

#define darray_create(Type, Count, AllocFunction) ((Type*)_dynamic_array_create(sizeof(Type), Count, AllocFunction))
#define darray_destroy(DynamicArray, DeallocFunction) _dynamic_array_destroy((void*)DynamicArray, DeallocFunction)
#define darray_length(DynamicArray) _dynamic_array_length((void*)DynamicArray)
#define darray_index(DynamicArray, Index) ((typeof(DynamicArray))_dynamic_array_index(DynamicArray, Index))
#define darray_iterate(DynamicArray, Index)  for(uint64_t Index = 0; Index < darray_length(DynamicArray); ++i)
#define darray_resize(DynamicArray, Length, ReallocFunction) (DynamicArray = (typeof(DynamicArray))_dynamic_array_resize((void*)DynamicArray, Length, ReallocFunction))
#define darray_copy(DynamicArray, AllocFunction) ((typeof(DynamicArray))_dynamic_array_copy((void*)DynamicArray, AllocFunction))

inline static void* _dynamic_array_insert(void* darray, void* data, uint64_t data_size, allocator_reallocate realloc) {
   dynamic_array_header* header = get_dynamic_array_header(darray);
   assert(header->item_size == data_size && realloc && data);

   darray = darray_resize(darray, header->length + 1, realloc);
   header = get_dynamic_array_header(darray);
   char* data_bytes = (char*)data;
   char* darray_bytes_to_fill = (darray + (header->length * header->item_size) - header->item_size);
   for(uint64_t i = 0; i < header->item_size; ++i) {
      darray_bytes_to_fill[i] = data_bytes[i];
   }
   return darray;
}

inline static void* _dynamic_array_remove(void* darray, uint64_t index, allocator_reallocate realloc) {
   dynamic_array_header* header = get_dynamic_array_header(darray);
   assert(header->length > index);
   char* copy_bytes = (char*)(darray + (header->item_size * index));
   char* read_bytes = copy_bytes + header->item_size;
   for(uint64_t i = 0; i < ((header->length - index -1) * header->item_size); ++i) {
      *copy_bytes = *read_bytes;
      copy_bytes++;
      read_bytes++;
   }
   darray = darray_resize(darray, header->length - 1, realloc);
   return darray;
}

#define darray_insert(DynamicArray, Data, ReallocFunction) \
{ \
   typeof(Data) temp = Data; \
   DynamicArray = _dynamic_array_insert(DynamicArray, &temp, sizeof(temp), ReallocFunction); \
}
#define darray_remove(DynamicArray, Index, ReallocFunction) (DynamicArray = (typeof(DynamicArray))_dynamic_array_remove((void*)DynamicArray, Index, ReallocFunction))

//Some deallocation methods require explicit memory size, munmap for example
static void deallocate(void* memory, uint64_t size) {
   free(memory);
}

static void validate_dynamic_array() {
   int32_t* darray32 = darray_create(int32_t, 8, malloc);
   assert(darray_length(darray32) == 8);
   assert(darray_index(darray32, 7) != NULL && darray_index(darray32, 8) == NULL);
   int iterated_count = 0;
   darray_iterate(darray32, i) {
      assert(darray32[i] == 0);
      darray32[i] = 89;
      iterated_count++;
   }
   assert(iterated_count == 8);
   darray_iterate(darray32, i) {
      assert(darray32[i] == 89);
   }
   darray_resize(darray32, 10, realloc);
   assert(darray_length(darray32) == 10);
   darray_iterate(darray32, i) {
      if(i < 8) {
         assert(darray32[i] == 89);
      } else {
         assert(darray32[i] == 0);
      }
   }
   darray_insert(darray32, 59, realloc);
   assert(darray32[10] == 59);
   darray32[5] = 55;
   darray_remove(darray32, 5, realloc);
   assert(darray32[5] != 55 && darray_length(darray32) == 10);
   int32_t* darray32_copy = darray_copy(darray32, malloc);
   assert(darray_length(darray32) == darray_length(darray32_copy));
   darray_iterate(darray32, i) {
      assert(darray32[i] == darray32_copy[i]);
   }
   darray_destroy(darray32, deallocate);
   darray_destroy(darray32_copy, deallocate);
   printf("Validated!\n");
}

int main() {
   validate_dynamic_array();
   return 0;
}
