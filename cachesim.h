#ifndef _CACHESIM_H_
#define _CACHESIM_H_

/* Feel free to add any constants, enums, structs etc. that
you need to this file! But you should probably put them at the
bottom so that you can use the types I've given you.*/

/*******************************************************************************
*
*
*
* DO NOT MODIFY ANYTHING BELOW THIS LINE!
*
* But you should read the comments below.
*
*
*
*******************************************************************************/

typedef enum
{
	Access_I_FETCH, /* An instruction fetch. Read from the I-cache. */
	Access_D_READ,  /* A data read. Read from the D-cache. */
	Access_D_WRITE, /* A data write. Write to the D-cache. */
} AccessType;

typedef enum
{
	Write_WRITE_BACK,
	Write_WRITE_THROUGH,
} WriteScheme;

typedef enum
{
	Allocate_ALLOCATE,
	Allocate_NO_ALLOCATE,
} AllocateType;

typedef enum
{
	Replacement_LRU,
	Replacement_RANDOM,
} ReplacementType;

typedef unsigned long memaddr_t;

/*
num_blocks is how many cache blocks there are. This is not how many words! And
in an associative cache, this is not how many sets!

If num_blocks is 0, it means that cache (or cache level) is disabled, so you
don't have to simulate it.

words_per_block is.. how many words there are in each block. So the data size of
the cache will be num_blocks * words_per_block * 4.

If the associativity == 1, it's a direct-mapped cache.
If the associativity == num_blocks, it's a fully-associative cache.
If it's anywhere in-between, it's a set-associative cache. The number of sets
will be num_blocks / associativity, always.

The replacement type is only used when associativity > 1. It can be LRU (least
recently used) or random. This decides how blocks are "kicked out" of the set/
cache when a new block needs to be brought in.

write_scheme and allocate_scheme are only used for the data cache.

write_scheme can be either Write_WRITE_BACK or Write_WRITE_THROUGH. This decides
whether we immediately write the data through to the next level or delay the
write until we kick out the block.

allocate_scheme can be Allocate_ALLOCATE or Allocate_NO_ALLOCATE. This is what
happens when you write to the cache, and it's a miss.
*/

typedef struct
{
	int num_blocks;
	int words_per_block;
	int associativity;
	ReplacementType replacement;
	WriteScheme write_scheme;     /* D-cache only! */
	AllocateType allocate_scheme; /* D-cache only! */
} CacheInfo;

void dump_cache_info();

/*******************************************************************************
*
*
*
* DO NOT MODIFY ANYTHING ABOVE THIS LINE!
*
* But you should read the comments above.
*
*
*
*******************************************************************************/

void bit_extractor_calculator(int*, int*, int*, int, int);

unsigned int bit_extractor(char, memaddr_t);

void hex_binary_converter(memaddr_t, char[]);

struct Block
{
	unsigned int byte_select;
	unsigned int word_bits;
	unsigned int row_bits;
	unsigned int tag_bits;
	int data;
	unsigned int valid;
	unsigned int dirty;
};

struct CacheObject
{
	struct Block **cache2;
	struct Block *cache;
} ;	/* cache itself will be a 2D array of block structs */

struct Stats
{

};

#endif
