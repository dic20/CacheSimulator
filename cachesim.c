#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "cachesim.h"

/*
Usage:
./cachesim -I 4096:1:2:R -D 1:4096:2:4:R:B:A -D 2:16384:4:8:L:T:N trace.txt

The -I flag sets instruction cache parameters. The parameter after looks like:
4096:1:2:R
This means the I-cache will have 4096 blocks, 1 word per block, with 2-way
associativity.

The R means Random block replacement; L for that item would mean LRU. This
replacement scheme is ignored if the associativity == 1.

The -D flag sets data cache parameters. The parameter after looks like:
1:4096:2:4:R:B:A

The first item is the level and must be 1, 2, or 3.

The second through fourth items are the number of blocks, words per block, and
associativity like for the I-cache. The fifth item is the replacement scheme,
just like for the I-cache.

The sixth item is the write scheme and can be:
B for write-Back
T for write-Through

The seventh item is the allocation scheme and can be:
A for write-Allocate
N for write-No-allocate

The last argument is the filename of the memory trace to read. This is a text
file where every line is of the form:
0x00000000 R
A hexadecimal address, followed by a space and then R, W, or I for data read,
data write, or instruction fetch, respectively.
*/

/* These global variables will hold the info needed to set up your caches in
your setup_caches function. Look in cachesim.h for the description of the
CacheInfo struct for docs on what's inside it. Have a look at dump_cache_info
for an example of how to check the members. */
static CacheInfo icache_info;
static CacheInfo dcache_info[3];
struct Block block;
struct Wrapper wrapper;
struct Wrapper d_wrapper;
int byte_bits;
int data_size;
int tag_bits;
int word_bits;
int row_bits;
int d_tag_bits;
int d_word_bits;
int d_row_bits;
int compulsory_miss = 0;
int d_compulsory_miss = 0;
char binary[8][5];
char binary_address[32];
int cache_size;
int d_cache_size;
int mem_reads = 0;
int d_mem_reads = 0;
int d_mem_writes = 0;
int conflict_miss = 0;
int d_conflict_miss = 0;
int wpb;
int read_cache = 0;
int d_read_cache = 0;
float miss_rate = 0.0;
float d_miss_rate = 0.0;
int associativity;
int d_associativity;
int d_wpb;
int writes_to_cache = 0;
int words_written_to_mem = 0;
AllocateType a_type;
WriteScheme w_scheme;

void setup_caches()
{
	/* Set up your caches here! */

	/* Setting up I-Cache *************************************************************/
	int num_blocks = icache_info.num_blocks;
	int words_per_block = icache_info.words_per_block;
	associativity = icache_info.associativity;
	wpb = words_per_block;

	srand(num_blocks);

	/* direct-mapped cache setup */
	if( associativity == 1 ) {

		if( num_blocks != 0 ) {

			bit_extractor_calculator(&word_bits, &tag_bits, &row_bits, words_per_block, num_blocks);

			wrapper.cache = (struct Block *)malloc(sizeof(struct Block)*num_blocks);
			for( int i = 0; i < num_blocks; i++ ) {
				wrapper.cache[i].tag_bits = '\0';
			}
			cache_size = num_blocks;
		}
	} else {	//not direct-mapped setup

		if( num_blocks != 0 ) {

			bit_extractor_calculator(&word_bits, &tag_bits, &row_bits, words_per_block, num_blocks);

			wrapper.cache2D = (struct Block **)malloc(sizeof(struct Block)*num_blocks*associativity);
			for( int i = 0; i < num_blocks; i++ ) {
				wrapper.cache2D[i] = (struct Block*)malloc(sizeof(struct Block)*associativity);
				for( int j = 0; j < associativity; j++ ) {
					wrapper.cache2D[i][j].tag_bits = '\0';
					wrapper.cache2D[i][j].used_last = 0;
				}
			}
			cache_size = num_blocks;
		}
	}
	/*******************************************************************************/
	// d-cache-Level 1
	int d_num_blocks = dcache_info[0].num_blocks;
	int d_words_per_block = dcache_info[0].words_per_block;
	d_associativity = dcache_info[0].associativity;
	a_type = dcache_info[0].allocate_scheme;
	w_scheme = dcache_info[0].write_scheme;
	d_wpb = d_words_per_block;

	if( d_associativity == 1 ) {

		if( d_num_blocks != 0 ) {

			bit_extractor_calculator(&d_word_bits, &d_tag_bits, &d_row_bits, d_words_per_block, d_num_blocks);

			d_wrapper.cache = (struct Block *)malloc(sizeof(struct Block)*d_num_blocks);
			for( int i = 0; i < d_num_blocks; i++ ) {
				d_wrapper.cache[i].tag_bits = '\0';
			}
			d_cache_size = d_num_blocks;
		}
	} else {	//not direct-mapped setup

		if( d_num_blocks != 0 ) {

			bit_extractor_calculator(&d_word_bits, &d_tag_bits, &d_row_bits, d_words_per_block, d_num_blocks);

			d_wrapper.cache2D = (struct Block **)malloc(sizeof(struct Block)*d_num_blocks*d_associativity);
			for( int i = 0; i < d_num_blocks; i++ ) {
				d_wrapper.cache2D[i] = (struct Block*)malloc(sizeof(struct Block)*d_associativity);
				for( int j = 0; j < d_associativity; j++ ) {
					d_wrapper.cache2D[i][j].tag_bits = '\0';
					d_wrapper.cache2D[i][j].used_last = 0;
				}
			}
			d_cache_size = d_num_blocks;
		}
	}

	/* This call to dump_cache_info is just to show some debugging information
	and you may remove it. */
	dump_cache_info();
}

/* calculates size of the of all the bits for row, word, and tag */
void bit_extractor_calculator(int* word_bits, int* tag_bits, int* row_bits, int words_per_block, int num_blocks) {
	int address_size = 32;

	*row_bits = (int)ceil(log(num_blocks)/log(2));	/* calculates how many bits are needed to find each row */

	*word_bits = (int)ceil(log(words_per_block)/log(2));	/* number of bits needed for word indexing */

	*tag_bits = address_size - *row_bits - *word_bits - 2;
}

void hex_binary_converter(memaddr_t address, char** binary, char * binary_address) {
	/* turn every hex character into 4 binary bits */
	char* temp;
	for( int i = 0; i < 8; i++ ) {
		switch( address % 16 ) {	/* switch lsb */
			case 0:
			temp = "0000";
			binary[i] = temp;
			break;
			case 1:
			temp = "0001";
			binary[i] = temp;
			break;
			case 2:
			temp = "0010";
			binary[i] = temp;
			break;
			case 3:
			temp = "0011";
			binary[i] = temp;
			break;
			case 4:
			temp = "0100";
			binary[i] = temp;
			break;
			case 5:
			temp = "0101";
			binary[i] = temp;
			break;
			case 6:
			temp = "0110";
			binary[i] = temp;
			break;
			case 7:
			temp = "0111";
			binary[i] = temp;
			break;
			case 8:
			temp = "1000";
			binary[i] = temp;
			break;
			case 9:
			temp = "1001";
			binary[i] = temp;
			break;
			case 10:
			temp = "1010";
			binary[i] = temp;
			break;
			case 11:
			temp = "1011";
			binary[i] = temp;
			break;
			case 12:
			temp = "1100";
			binary[i] = temp;
			break;
			case 13:
			temp = "1101";
			binary[i] = temp;
			break;
			case 14:
			temp = "1110";
			binary[i] = temp;
			break;
			case 15:
			temp = "1111";
			binary[i] = temp;
			break;
		}
		address = address/16;
	}

	//take the binary[8][5] array and convert to a binary_address[32]
	int z = 0;
	for( int i = 0; i < 8; i++ ) {
		for( int j = 3; j >= 0; j-- ) {
			binary_address[z] = binary[i][j];
			//printf("binary_address[%d]: %c\n", z, binary_address[z]);
			z++;
		}
	}
}

/*
fills an array with the appropriate bits
fills array based on precalculated index and char index
*/
void address_decompress(char index, char* array_to_fill, char* binary_address) {
	int j = 0;
	int stop;
	switch(index)
	{
		case 'r':
		stop = 32-tag_bits-row_bits-1;
		for( int i = 32-tag_bits; i > stop; i-- ) {
			array_to_fill[j] = binary_address[i];
			//printf("row bit %d: %c\n", j, array_to_fill[j]);
			j++;
		}
		array_to_fill[stop] = '\0';
		//printf("row length %d\n", row_bits);
		break;
		case 't':
		stop = 32-tag_bits-1;
		for( int i = 31; i > stop; i-- ) {
			array_to_fill[j] = binary_address[i];
			//printf("tag bit %d: %c\n", j, array_to_fill[j]);
			j++;
		}
		array_to_fill[stop] = '\0';
		//printf("tag length %d\n", tag_bits);
		break;
		case 'w':
		stop = 1;
		for( int i = 1+word_bits; i > stop; i-- ) {
			array_to_fill[j] = binary_address[i];
			//printf("word bit %d: %c\n", j, array_to_fill[j]);
			j++;
		}
		array_to_fill[stop] = '\0';
		//printf("word length %d\n", word_bits);
		break;
	}
}

//converts row array into an int index
int row_index_converter(char* row) {
	int row_index = 0;
	int base = 2;
	int position = 0;
	//max row index size = 2^row_bits
	for( int i = row_bits; i > 0; i-- ) {
		//printf("row[i]: %c\n", row[i]);
		if( row[i] == '1') {
			row_index += ( 1 * pow(base, position) );
		}
		position++;
	}

	return row_index;
}

//converst word array to an index
int word_index_converter(char* word) {
	int word_index = 0;
	int base = 2;
	int position = 0;

	for( int i = word_bits; i > 0; i-- ) {
		if( word[i] == '1' ) {
			word_index += ( 1 * pow(base, position) );
		}
		position++;
	}
	return word_index;
}

/*adds new block to cache.cache[] at row_index, sets valid = 1, dirty = 0
simulates pulling from memory*/
void add_block(char* temp_word, char* temp_row, char* temp_tag, int row_index, char cache_type) {
	switch(cache_type)
	{
		case 'I':
		if( row_index < cache_size ) {
			wrapper.cache[row_index].word_bits = temp_word;
			wrapper.cache[row_index].row_bits = temp_row;
			wrapper.cache[row_index].tag_bits = temp_tag;
			wrapper.cache[row_index].valid = 0;
			wrapper.cache[row_index].dirty = 0;
		} else {	// cache is too small, add block to first index
			add_block(temp_row, temp_row, temp_tag, 0, 'I');
			compulsory_miss = compulsory_miss + wpb;
		}
		break;
		case 'D':
		if( row_index < d_cache_size ) {
			d_wrapper.cache[row_index].word_bits = temp_word;
			d_wrapper.cache[row_index].row_bits = temp_row;
			d_wrapper.cache[row_index].tag_bits = temp_tag;
			d_wrapper.cache[row_index].valid = 0;
			d_wrapper.cache[row_index].dirty = 0;
		} else {	// cache is too small, add block to first index
			add_block(temp_word, temp_row, temp_tag, 0, 'D');
			d_compulsory_miss = d_compulsory_miss + d_wpb;
		}
		break;
	}
}

/* replace word in a specified block using specified replacement type for cache */
void replace_block(ReplacementType R, char* temp_word, char* temp_row, char* temp_tag, int row_index, char cache_type) {
	int Replace_Block_index = 0;
	if( cache_type == 'I' ) {
		struct Block Replace_Block = wrapper.cache2D[row_index][0];
		switch(R)
		{
			case Replacement_RANDOM:
			if( row_index < cache_size ) {
				wrapper.cache2D[row_index][rand()].word_bits = temp_word;
				wrapper.cache2D[row_index][rand()].row_bits = temp_row;
				wrapper.cache2D[row_index][rand()].tag_bits = temp_tag;
				wrapper.cache2D[row_index][rand()].valid = 0;
				wrapper.cache2D[row_index][rand()].dirty = 0;
			}	else {
				add_block(temp_row, temp_row, temp_tag, 0, 'I');
				compulsory_miss++;
			}
			break;
			case Replacement_LRU:
			for( int i = 0; i < associativity-1; i++ ) {	// find the block to replace, lowest used_last means LRu
				if( Replace_Block.used_last > wrapper.cache2D[row_index][i+1].used_last ) {
					Replace_Block = wrapper.cache2D[row_index][i];
					Replace_Block_index++;
				}
			}
			if( row_index < cache_size ) {
				wrapper.cache2D[row_index][Replace_Block_index].word_bits = temp_word;
				wrapper.cache2D[row_index][Replace_Block_index].row_bits = temp_row;
				wrapper.cache2D[row_index][Replace_Block_index].tag_bits = temp_tag;
				wrapper.cache2D[row_index][Replace_Block_index].valid = 0;
				wrapper.cache2D[row_index][Replace_Block_index].dirty = 0;
			}	else {
				add_block(temp_row, temp_row, temp_tag, 0, 'I');
				compulsory_miss = compulsory_miss + wpb;
			}
			break;
		}
	} else {
		struct Block Replace_Block = d_wrapper.cache2D[row_index][0];
		switch(R)
		{
			case Replacement_RANDOM:
			if( row_index < d_cache_size ) {
				d_wrapper.cache2D[row_index][rand()].word_bits = temp_word;
				d_wrapper.cache2D[row_index][rand()].row_bits = temp_row;
				d_wrapper.cache2D[row_index][rand()].tag_bits = temp_tag;
				d_wrapper.cache2D[row_index][rand()].valid = 0;
				d_wrapper.cache2D[row_index][rand()].dirty = 0;
			}	else {
				add_block(temp_row, temp_row, temp_tag, 0, 'D');
				d_compulsory_miss = d_compulsory_miss + d_wpb;
			}
			break;
			case Replacement_LRU:
			for( int i = 0; i < d_associativity-1; i++ ) {	// find the block to replace, lowest used_last means LRu
				if( Replace_Block.used_last > d_wrapper.cache2D[row_index][i+1].used_last ) {
					Replace_Block = d_wrapper.cache2D[row_index][i];
					Replace_Block_index++;
				}
			}
			if( row_index < d_cache_size ) {
				d_wrapper.cache2D[row_index][Replace_Block_index].word_bits = temp_word;
				d_wrapper.cache2D[row_index][Replace_Block_index].row_bits = temp_row;
				d_wrapper.cache2D[row_index][Replace_Block_index].tag_bits = temp_tag;
				d_wrapper.cache2D[row_index][Replace_Block_index].valid = 0;
				d_wrapper.cache2D[row_index][Replace_Block_index].dirty = 0;
			}	else {
				add_block(temp_row, temp_row, temp_tag, 0, 'D');
				d_compulsory_miss = d_compulsory_miss + d_wpb;
			}
			break;
		}
	}
}

//write to d_cache based on alloation scheme
void write_to_block(char* temp_word, char* temp_row, char* temp_tag, int row_index, int word_index, AllocateType A) {
	int Replace_Block_index = 0;
	struct Block Replace_Block = d_wrapper.cache2D[row_index][0];
	// remember to set dirty bit to 0 if new block, and 1 if not new/in memory
	switch(A)
	{
		case Allocate_ALLOCATE:
		// implement write_allocate
		if( associativity == 1 ) {
			add_block(temp_word, temp_row, temp_tag, row_index, 'D');
		} else {
			add_block_2(temp_word, temp_row, temp_tag, row_index, word_index, 'D');
		}
		break;
		case Allocate_NO_ALLOCATE:
		// implement write-No-allocate, only reads being cached
		printf("Allocate_NO_ALLOCATE, handled in read access\n");
		break;
	}
}

//adds a block to 2D cache
void add_block_2(char* temp_word, char* temp_row, char* temp_tag, int row_index, int word_index, char cache_type) {
	switch(cache_type)
	{
		case 'I':
		if( row_index < cache_size ) {
			wrapper.cache2D[row_index][word_index].word_bits = temp_word;
			wrapper.cache2D[row_index][word_index].row_bits = temp_row;
			wrapper.cache2D[row_index][word_index].tag_bits = temp_tag;
			wrapper.cache2D[row_index][word_index].valid = 0;
			wrapper.cache2D[row_index][word_index].dirty = 0;
		}	else {
			compulsory_miss = compulsory_miss + wpb;
			add_block_2(temp_word, temp_row, temp_tag, row_index, word_index, 'I');
		}
		break;
		case 'D':
		if( row_index < d_cache_size ) {
			d_wrapper.cache2D[row_index][word_index].word_bits = temp_word;
			d_wrapper.cache2D[row_index][word_index].row_bits = temp_row;
			d_wrapper.cache2D[row_index][word_index].tag_bits = temp_tag;
			d_wrapper.cache2D[row_index][word_index].valid = 0;
			d_wrapper.cache2D[row_index][word_index].dirty = 0;
		}	else {
			d_compulsory_miss = d_compulsory_miss + d_wpb;
			add_block_2(temp_word, temp_row, temp_tag, row_index, word_index, 'D');
		}
		break;
	}
}

void handle_access(AccessType type, memaddr_t address)
{
	int row_index = 0;
	int word_index = 0;
	char row[row_bits+1];
	char word[word_bits+1];
	char tag[tag_bits+1];

	//convert address from hex to binary and store it in binary_address
	hex_binary_converter(address, binary, binary_address);

	// Fill tag array
	address_decompress('t', tag, binary_address);

	//fill word array
	address_decompress('w', word, binary_address);

	//fill row array
	address_decompress('r', row, binary_address);

	//convert row[] to row_index
	row_index = row_index_converter(row);

	//convert word[] to word_index
	word_index = word_index_converter(word);

	char temp_word[sizeof(word)];
	char temp_row[sizeof(row)];
	char temp_tag[sizeof(tag)];

	strcpy(temp_word, word);
	strcpy(temp_row, row);
	strcpy(temp_tag, tag);

	/* This is where all the fun stuff happens! This function is called to
	simulate a memory access. You figure out what type it is, and do all your
	fun simulation stuff from here. */
	switch(type)
	{
		case Access_I_FETCH:
		if( associativity == 1 ) {
			if( wrapper.cache[row_index].tag_bits != '\0' ) {
				if( !strcmp(wrapper.cache[row_index].tag_bits, tag) ) {
					read_cache++;
				} else {
					mem_reads++;
					conflict_miss++;
				}
			} else {
				add_block(temp_word, temp_row, temp_tag, row_index, 'I');
				compulsory_miss++;
				mem_reads++;
			}
		} else {	// associativity > 1, not direct-mapped
			if( wrapper.cache2D[row_index][word_index].tag_bits != '\0' ) {
				if( !strcmp(wrapper.cache2D[row_index][word_index].tag_bits, tag) ) {
					if( !strcmp(wrapper.cache2D[row_index][word_index].word_bits, word) ) {
						wrapper.cache2D[row_index][word_index].used_last++;
						read_cache++;
					} else {
						replace_block(icache_info.replacement, temp_word, temp_row, temp_tag, row_index, 'I');
						mem_reads = mem_reads + wpb;
						conflict_miss++;
					}
				}
			} else {
				add_block_2(temp_word, temp_row, temp_tag, row_index, word_index, 'I');
				compulsory_miss = compulsory_miss + wpb;
				mem_reads = mem_reads + wpb;
			}
		}
		break;
		/************************************************************************************/
		case Access_D_READ:
		switch(w_scheme) {
			/**********************************************************/
			case Write_WRITE_BACK:
			if( d_associativity == 1 ) {	// direct-mapped
				if( d_wrapper.cache[row_index].tag_bits != '\0' ) {
					if( !strcmp(d_wrapper.cache[row_index].tag_bits, tag) ) {
						d_read_cache++;
					} else if( d_wrapper.cache[row_index].dirty == 1 ) {
						d_mem_reads++;
						d_conflict_miss++;
						words_written_to_mem++;
						add_block(temp_word, temp_row, temp_tag, row_index, 'D');
					} else {
						d_mem_reads++;
						d_conflict_miss++;
						add_block(temp_word, temp_row, temp_tag, row_index, 'D');
					}
				} else {
					add_block(temp_word, temp_row, temp_tag, row_index, 'D');
					d_compulsory_miss++;
					d_mem_reads++;
				}
			} else {	// associativity > 1, not direct-mapped
				if( d_wrapper.cache2D[row_index][word_index].tag_bits != '\0' ) {
					if( !strcmp(d_wrapper.cache2D[row_index][word_index].tag_bits, tag) ) {
						if( !strcmp(d_wrapper.cache2D[row_index][word_index].word_bits, word) ) {
							d_wrapper.cache2D[row_index][word_index].used_last++;
							d_read_cache++;
						} else if( d_wrapper.cache2D[row_index][word_index].dirty == 1 ) {	//checking if word is dirty
							replace_block(dcache_info[0].replacement, temp_word, temp_row, temp_tag, row_index, 'D');
							d_mem_reads = d_mem_reads + d_wpb;
							d_conflict_miss = d_conflict_miss + d_wpb;
							words_written_to_mem = words_written_to_mem + d_wpb;
						} else {
							replace_block(dcache_info[0].replacement, temp_word, temp_row, temp_tag, row_index, 'D');
							d_mem_reads = d_mem_reads + d_wpb;
							d_conflict_miss= d_conflict_miss + d_wpb;
						}
					}
				} else {
					add_block_2(temp_word, temp_row, temp_tag, row_index, word_index, 'D');
					d_compulsory_miss = d_compulsory_miss + d_wpb;
					d_mem_reads = d_mem_reads + d_wpb;
				}
			}
			break;
			/************************************************************************************/
			case Write_WRITE_THROUGH:
			if( d_associativity == 1) {
				if( d_wrapper.cache[row_index].tag_bits != '\0' ) {
					if( !strcmp(d_wrapper.cache[row_index].tag_bits, tag) ) {	//	checking tag
						d_read_cache++;
					} else {
						d_conflict_miss++;
						write_to_block(temp_word, temp_row, temp_tag, row_index, word_index, a_type);
						writes_to_cache++;
						d_read_cache++;
						words_written_to_mem++;
					}
				} else {
					d_mem_reads++;
					d_read_cache++;
					d_compulsory_miss++;
					add_block(temp_word, temp_row, temp_tag, row_index, 'D');
				}
			} else {	// not direct-mapped associativity > 1
				if( d_wrapper.cache2D[row_index][word_index].tag_bits != '\0' ) {	//seeing if block is null/empty
					if( !strcmp(d_wrapper.cache2D[row_index][word_index].tag_bits, tag) ) {	//checking tag
						d_read_cache++;
					} else {
						d_conflict_miss = d_conflict_miss + d_wpb;
						d_mem_reads = d_mem_reads + d_wpb;
						write_to_block(temp_word, temp_row, temp_tag, row_index, word_index, a_type);
						writes_to_cache = writes_to_cache + d_wpb;
						d_read_cache++;
						words_written_to_mem = words_written_to_mem + d_wpb;
					}
				} else {
					d_read_cache++;
					d_compulsory_miss = d_compulsory_miss + d_wpb;
					d_mem_reads = d_mem_reads + d_wpb;
					add_block_2(temp_word, temp_row, temp_tag, row_index, word_index, 'D');
				}
			}
			break;
		}
		break;
		/********************************************************************************************/
		case Access_D_WRITE:
		switch(w_scheme) {
			case Write_WRITE_BACK:
			if( d_associativity == 1) {	//direct-mapped
				if( d_wrapper.cache[row_index].tag_bits != '\0' ) {
					if( !strcmp(d_wrapper.cache[row_index].tag_bits, tag) ) {	//check tag
						d_read_cache++;
					} else {
						d_read_cache++;
						if( d_wrapper.cache[row_index].dirty == 1 ) {	//check if block is dirty
							words_written_to_mem++;
							write_to_block(temp_word, temp_row, temp_tag, row_index, word_index, a_type);
							writes_to_cache++;
						} else {	//write to mem and fill in new block
							words_written_to_mem++;
							d_mem_reads++;
							write_to_block(temp_word, temp_row, temp_tag, row_index, word_index, a_type);
							writes_to_cache++;
						}
					}
				} else {
					d_compulsory_miss++;
					writes_to_cache++;
					d_mem_reads++;
					add_block(temp_word, temp_row, temp_tag, row_index, 'D');
				}
			} else {	//not direct mapped
				if( d_wrapper.cache2D[row_index][word_index].tag_bits != '\0' ) {
					if( !strcmp(d_wrapper.cache2D[row_index][word_index].tag_bits, tag) ) {
						d_read_cache++;
					} else {	// bad data
						d_read_cache++;
						d_conflict_miss = d_conflict_miss + d_wpb;
						if( d_wrapper.cache2D[row_index][word_index].dirty == 1 ) {	//check if block is dirty
							words_written_to_mem = words_written_to_mem + d_wpb;
							write_to_block(temp_word, temp_row, temp_tag, row_index, word_index, a_type);
							writes_to_cache = writes_to_cache + d_wpb;
						} else {
							words_written_to_mem = words_written_to_mem + d_wpb;
							write_to_block(temp_word, temp_row, temp_tag, row_index, word_index, a_type);
							writes_to_cache = writes_to_cache + d_wpb;
						}
					}
				} else { // nothing is in the cache
					d_compulsory_miss = d_compulsory_miss + d_wpb;
					writes_to_cache = writes_to_cache + d_wpb;
					add_block_2(temp_word, temp_row, temp_tag, row_index, word_index, 'D');
				}
			}
			break;
			/*******************************************************************************************8*/
			case Write_WRITE_THROUGH:
			if( d_associativity == 1) {	 //dirct mapped
				if( d_wrapper.cache[row_index].tag_bits != '\0' ) {
					if( !strcmp(d_wrapper.cache[row_index].tag_bits, tag) ) {
						d_read_cache++;
					} else {
						write_to_block(temp_word, temp_row, temp_tag, row_index, word_index, a_type);
						words_written_to_mem++;
						d_conflict_miss++;
					}
				} else {
					d_compulsory_miss++;
					writes_to_cache++;
					add_block(temp_word, temp_row, temp_tag, row_index, 'D');
				}
			} else {	// not direct mapped
				if( d_wrapper.cache2D[row_index][word_index].tag_bits != '\0' ) {
					if( !strcmp(d_wrapper.cache2D[row_index][word_index].tag_bits, tag) ) {
						d_read_cache++;
					} else {	// bad data
						write_to_block(temp_word, temp_row, temp_tag, row_index, word_index, a_type);
						words_written_to_mem = words_written_to_mem + d_wpb;
						d_conflict_miss = d_conflict_miss + d_wpb;
					}
				} else { // nothing is in the cache
					d_compulsory_miss = d_compulsory_miss + d_wpb;
					writes_to_cache = writes_to_cache + d_wpb;
					add_block_2(temp_word, temp_row, temp_tag, row_index, word_index, 'D');
				}
			}
			break;
		}
		break;
	}
}

void print_statistics()
{
	/* Finally, after all the simulation happens, you have to show what the
	results look like. Do that here.*/
	/************i-cache stats**************************/
	printf("Instruction cache:\n");
	printf("\tNumber of reads from the cache: %d\n", read_cache);
	printf("\tNumber of conflict misses: %d\n", conflict_miss);
	printf("\tNumber of words loaded from memory: %d\n", mem_reads);
	printf("\tcompulsory_misses: %d\n", compulsory_miss);
	miss_rate = (float)(conflict_miss + compulsory_miss)/(float)read_cache;
	printf("\tRead miss rate (with compulsory): %.2f\n", miss_rate);
	miss_rate = (float)(conflict_miss)/(float)read_cache;
	printf("\tRead miss rate (without compulsory): %.2f\n", miss_rate);

	/*******************d-cache stats****************************/
	printf("Data cache\n");
	printf("\tNumber of reads from the cache: %d\n", d_read_cache);
	printf("\tMemory reads: %d\n", d_mem_reads);
	printf("\tNumber of writes to cache: %d\n", writes_to_cache);
	printf("\tNumber of words written to memory: %d\n", words_written_to_mem);
	printf("\tcompulsory misses: %d\n", d_compulsory_miss);
	printf("\tConflict misses: %d\n", d_conflict_miss);
	miss_rate = (float)(d_conflict_miss + d_compulsory_miss)/(float)d_read_cache;
	printf("\tRead miss rate (with compulsory): %.2f\n", miss_rate);
	miss_rate = (float)(d_conflict_miss)/(float)d_read_cache;
	printf("\tRead miss rate (without compulsory): %.2f\n", miss_rate);
}
/*******************************************************************************
*
*
*
* DO NOT MODIFY ANYTHING BELOW THIS LINE!
*
*
*
*******************************************************************************/

void dump_cache_info()
{
	int i;
	CacheInfo* info;

	printf("Instruction cache:\n");
	printf("\t%d blocks\n", icache_info.num_blocks);
	printf("\t%d word(s) per block\n", icache_info.words_per_block);
	printf("\t%d-way associative\n", icache_info.associativity);

	if(icache_info.associativity > 1)
	{
		printf("\treplacement: %s\n\n",
		icache_info.replacement == Replacement_LRU ? "LRU" : "Random");
	}
	else
	printf("\n");

	for(i = 0; i < 3 && dcache_info[i].num_blocks != 0; i++)
	{
		info = &dcache_info[i];

		printf("Data cache level %d:\n", i);
		printf("\t%d blocks\n", info->num_blocks);
		printf("\t%d word(s) per block\n", info->words_per_block);
		printf("\t%d-way associative\n", info->associativity);

		if(info->associativity > 1)
		{
			printf("\treplacement: %s\n", info->replacement == Replacement_LRU ?
			"LRU" : "Random");
		}

		printf("\twrite scheme: %s\n", info->write_scheme == Write_WRITE_BACK ?
		"write-back" : "write-through");

		printf("\tallocation scheme: %s\n\n",
		info->allocate_scheme == Allocate_ALLOCATE ?
		"write-allocate" : "write-no-allocate");
	}
}

void read_trace_line(FILE* trace)
{
	char line[100];
	memaddr_t address;
	char type;

	if(fgets(line, sizeof(line), trace) == NULL)
	{
		return;
	}

	if(sscanf(line, "0x%lx %c", &address, &type) < 2)
	{
		return;
	}

	switch(type)
	{
		case 'R': handle_access(Access_D_READ, address);  break;
		case 'W': handle_access(Access_D_WRITE, address); break;
		case 'I': handle_access(Access_I_FETCH, address); break;
		default:
		fprintf(stderr, "Malformed trace file: invalid access type '%c'.\n",
		type);
		exit(1);
		break;
	}
}

static void bad_params(const char* msg)
{
	fprintf(stderr, msg);
	fprintf(stderr, "\n");
	exit(1);
}

#define streq(a, b) (strcmp((a), (b)) == 0)

FILE* parse_arguments(int argc, char** argv)
{
	int i;
	int have_inst = 0;
	int have_data[3] = {};
	FILE* trace = NULL;
	int level;
	int num_blocks;
	int words_per_block;
	int associativity;
	char write_scheme;
	char alloc_scheme;
	char replace_scheme;
	int converted;

	for(i = 1; i < argc; i++)
	{
		if(streq(argv[i], "-I"))
		{
			if(i == (argc - 1))
			bad_params("Expected parameters after -I.");

			if(have_inst)
			bad_params("Duplicate I-cache parameters.");
			have_inst = 1;

			i++;
			converted = sscanf(argv[i], "%d:%d:%d:%c",
			&icache_info.num_blocks,
			&icache_info.words_per_block,
			&icache_info.associativity,
			&replace_scheme);

			if(converted < 4)
			bad_params("Invalid I-cache parameters.");

			if(icache_info.associativity > 1)
			{
				if(replace_scheme == 'R')
				icache_info.replacement = Replacement_RANDOM;
				else if(replace_scheme == 'L')
				icache_info.replacement = Replacement_LRU;
				else
				bad_params("Invalid I-cache replacement scheme.");
			}
		}
		else if(streq(argv[i], "-D"))
		{
			if(i == (argc - 1))
			bad_params("Expected parameters after -D.");

			i++;
			converted = sscanf(argv[i], "%d:%d:%d:%d:%c:%c:%c",
			&level, &num_blocks, &words_per_block, &associativity,
			&replace_scheme, &write_scheme, &alloc_scheme);

			if(converted < 7)
			bad_params("Invalid D-cache parameters.");

			if(level < 1 || level > 3)
			bad_params("Inalid D-cache level.");

			level--;
			if(have_data[level])
			bad_params("Duplicate D-cache level parameters.");

			have_data[level] = 1;

			dcache_info[level].num_blocks = num_blocks;
			dcache_info[level].words_per_block = words_per_block;
			dcache_info[level].associativity = associativity;

			if(associativity > 1)
			{
				if(replace_scheme == 'R')
				dcache_info[level].replacement = Replacement_RANDOM;
				else if(replace_scheme == 'L')
				dcache_info[level].replacement = Replacement_LRU;
				else
				bad_params("Invalid D-cache replacement scheme.");
			}

			if(write_scheme == 'B')
			dcache_info[level].write_scheme = Write_WRITE_BACK;
			else if(write_scheme == 'T')
			dcache_info[level].write_scheme = Write_WRITE_THROUGH;
			else
			bad_params("Invalid D-cache write scheme.");

			if(alloc_scheme == 'A')
			dcache_info[level].allocate_scheme = Allocate_ALLOCATE;
			else if(alloc_scheme == 'N')
			dcache_info[level].allocate_scheme = Allocate_NO_ALLOCATE;
			else
			bad_params("Invalid D-cache allocation scheme.");
		}
		else
		{
			if(i != (argc - 1))
			bad_params("Trace filename should be last argument.");

			break;
		}
	}

	if(!have_inst)
	bad_params("No I-cache parameters specified.");

	if(have_data[1] && !have_data[0])
	bad_params("L2 D-cache specified, but not L1.");

	if(have_data[2] && !have_data[1])
	bad_params("L3 D-cache specified, but not L2.");

	trace = fopen(argv[argc - 1], "r");

	if(trace == NULL)
	bad_params("Could not open trace file.");

	return trace;
}

int main(int argc, char** argv)
{
	FILE* trace = parse_arguments(argc, argv);

	setup_caches();

	while(!feof(trace))
	read_trace_line(trace);

	fclose(trace);

	print_statistics();
	return 0;
}
