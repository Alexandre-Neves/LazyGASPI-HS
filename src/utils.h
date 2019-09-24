/** Utility header for the current LazyGASPI implementation. */

#ifndef __H_UTILS
#define __H_UTILS

#include <GASPI.h>
#include <cstdlib>
#include <cassert>
#include <iostream>
#include <thread>
#include <utility>

#include "lazygaspi_hs.h"

typedef unsigned int uint;
typedef unsigned char byte;
typedef unsigned long ulong;

#define subsizeof(STRUCT, MEMBER_BEGIN, MEMBER_END) (offsetof(STRUCT, MEMBER_END) + sizeof(decltype(std::declval<STRUCT>().MEMBER_END)) - offsetof(STRUCT, MEMBER_BEGIN))
#define sizeofmember(STRUCT, MEMBER) sizeof((( STRUCT *)0)-> MEMBER)

#define NOTIF_ID_ROW_WRITTEN 0

#if (defined (DEBUG) || defined (DEBUG_INTERNAL))
#define PRINT_DEBUG_INTERNAL(msg) *info->out << msg << std::endl
#define PRINT_DEBUG_INTERNAL_COUT(msg) std::cout << msg << std::endl
#else
#define PRINT_DEBUG_INTERNAL(msg)
#define PRINT_DEBUG_INTERNAL_COUT(msg)
#endif

#if (defined (DEBUG) || defined (DEBUG_TEST))
#define PRINT_DEBUG_TEST(msg) *info->out << msg << std::endl
#else
#define PRINT_DEBUG_TEST(msg)
#endif

#if (defined (DEBUG) || defined (DEBUG_PERF))
#define PRINT_DEBUG_PERF(msg) *info->out << msg << std::endl
#else
#define PRINT_DEBUG_PERF(msg)
#endif

#if defined DEBUG || defined DEBUG_PERF || defined DEBUG_TEST || defined DEBUG_INTERNAL
#define PRINT_DEBUG(msg) *info->out << msg << std::endl
#define PRINT_DEBUG_COUT(msg) std::cout << msg << std::endl
#else
#define PRINT_DEBUG(msg)
#define PRINT_DEBUG_COUT(msg)
#endif

#ifdef WITH_MPI
#include <mpi.h>
#define ERROR_MPI_CHECK_COUT(msg) {if(ret != MPI_SUCCESS){ std::cout << "Error " << ret << " at [" << __FILE__ << ':' << __LINE__ << "] \
                              from MPI: " << msg << std::endl; return GASPI_ERROR; }}
#endif

#define ROW_SIZE_IN_TABLE (sizeof(LazyGaspiRowData) + info->row_size + info->n * sizeof(lazygaspi_age_t))
#define ROW_SIZE_IN_CACHE (sizeof(LazyGaspiRowData) + info->row_size)

#ifdef LOCKED_OPERATIONS
    #define LOCK_MASK_WRITE (((gaspi_atomic_value_t)1) << (sizeof(gaspi_atomic_value_t) * 8 - 1))
    #define LOCK_MASK_READ (LOCK_MASK_WRITE - 1)

    struct Lock{ gaspi_atomic_value_t val; };

    gaspi_return_t lock_row_for_read(const LazyGaspiProcessInfo* info, const gaspi_segment_id_t seg, const gaspi_offset_t offset, 
                       const gaspi_rank_t rank);
    gaspi_return_t unlock_row_from_read(const LazyGaspiProcessInfo* info, const gaspi_segment_id_t seg, const gaspi_offset_t offset,
                          const gaspi_rank_t rank);
    gaspi_return_t lock_row_for_write(const LazyGaspiProcessInfo* info, const gaspi_segment_id_t seg, const gaspi_offset_t offset, 
                        const gaspi_rank_t rank);
    gaspi_return_t unlock_row_from_write(LazyGaspiProcessInfo* info, const gaspi_segment_id_t seg, const gaspi_offset_t offset,
                           const gaspi_rank_t rank, const gaspi_queue_id_t q = 0, bool wait_on_q = true);

    #define ROW_LOCK_OFFSET 0
    #define ROW_METADATA_OFFSET (ROW_LOCK_OFFSET + sizeof(Lock))
    #define ROW_SIZE_IN_TABLE_WITH_LOCK (ROW_SIZE_IN_TABLE + sizeof(Lock))
    #define ROW_SIZE_IN_CACHE_WITH_LOCK (ROW_SIZE_IN_CACHE + sizeof(Lock))

#else
    #define ROW_METADATA_OFFSET 0
    #define ROW_SIZE_IN_TABLE_WITH_LOCK ROW_SIZE_IN_TABLE
    #define ROW_SIZE_IN_CACHE_WITH_LOCK ROW_SIZE_IN_CACHE
#endif

#define ROW_DATA_OFFSET (ROW_METADATA_OFFSET + sizeof(LazyGaspiRowData))
#define ROW_REQUEST_OFFSET(rank) (ROW_DATA_OFFSET + info->row_size + rank * sizeof(lazygaspi_age_t))

struct RowLocationEntry{
    gaspi_rank_t rank;
    lazygaspi_id_t table_id;
    lazygaspi_id_t row_id;
    RowLocationEntry() = default;
    RowLocationEntry(lazygaspi_id_t table, lazygaspi_id_t row) : table_id(table), row_id(row) {}
};

struct IntraComm {
    gaspi_size_t currentTable, currentRow;
    IntraComm(gaspi_size_t currentTable, gaspi_size_t currentRow) : currentTable(currentTable), currentRow(currentRow) {}
    IntraComm() = default;
};

static inline lazygaspi_age_t get_min_age(lazygaspi_age_t current, lazygaspi_age_t slack, bool offset){
    return (current < slack + 1 + (int)offset) ? 1 : (current - slack - (int)offset);
}

/** Offset is in rows, not bytes. */
static inline std::pair<gaspi_rank_t, gaspi_offset_t> get_row_location(LazyGaspiProcessInfo* info, lazygaspi_id_t row_id, 
                                                                       lazygaspi_id_t table_id){
    const auto absIndex = table_id * info->table_size + row_id;
    const auto absBlock = absIndex / info->shardOpts.block_size;
    const auto rank = absBlock % info->n;
    const auto offsetBlock = absBlock / info->n;
    const auto offsetInternal = absIndex - absBlock * info->shardOpts.block_size;
    const auto offset = offsetBlock * info->shardOpts.block_size + offsetInternal;

    return std::make_pair((gaspi_rank_t)rank, (gaspi_offset_t)offset);
}

/** Returns the amount of rows in the given rank's rows segment. 
 *  Table amount can be obtained by dividing by the amount of rows in one table.
 * 
 *  Parameters:
 *  table_size   - The size of one table, in rows.
 *  table_amount - The total amount of tables that will exist throughout all processes.
 *  rank_amount  - The total amount of ranks, per gaspi_proc_num.
 *  rank         - The current rank.
 */
static inline unsigned long get_row_amount(gaspi_offset_t table_size, gaspi_offset_t table_amount, gaspi_rank_t rank_amount, 
                                           gaspi_rank_t rank, ShardingOptions opts){
    const auto block_amount = table_amount * table_size / opts.block_size;
            //Full blocks for all ranks + leftover blocks for first few ranks
    return (block_amount / rank_amount + (rank < (block_amount % rank_amount)) ) * opts.block_size +
            //Last partial block.
           ((rank == block_amount % rank_amount) ? ((table_amount * table_size) % (block_amount * opts.block_size)) : 0);
}

/** Offset is in rows, not bytes. */
static inline gaspi_offset_t get_offset_in_cache(LazyGaspiProcessInfo* info, lazygaspi_id_t row_id, lazygaspi_id_t table_id){
    return info->cacheOpts.hash(row_id, table_id, info) % info->cacheOpts.size;
}

/** Returns the minimum age for the current prefetch. 0 indicates no prefetching should occur. Resets flag to 0.
 * 
 *  Parameters:
 *  info - A pointer to the "info" segment.
 *  rows - A pointer to the "rows" segment.
 *  row  - The offset, in bytes, of the rows table entry that contains the row and prefetch flags.
 *  rank - The rank to check for the prefetch flag.
 */
static inline lazygaspi_age_t get_prefetch(const LazyGaspiProcessInfo* info, const gaspi_pointer_t rows, const gaspi_offset_t row, const gaspi_rank_t rank){
    auto flag = (lazygaspi_age_t*)((bool*)rows + row + ROW_REQUEST_OFFSET(rank)); 
    //TODO: prefetch setting/resetting need to be made atomic
    if(*flag){
        auto temp = *flag;
        *flag = 0;
        return temp;
    }
    return 0;
}

//To prevent overflow
static inline bool is_atomic_size_enough(LazyGaspiProcessInfo* info){
    #ifdef LOCKED_OPERATIONS
    return info->max_threads <= LOCK_MASK_READ / info->n;
    #else
    return true;
    #endif
}

#endif