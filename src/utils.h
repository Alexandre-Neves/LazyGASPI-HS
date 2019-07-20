/** Utility header for the current LazyGASPI implementation. */

#ifndef __H_UTILS
#define __H_UTILS

#include <GASPI.h>
#include <cstdlib>
#include <cassert>
#include <iostream>
#include <thread>
#include <utility>

#include "typedefs.h"
#include "lazygaspi.h"

#define NOTIF_ID_ROW_WRITTEN 0

#if (defined (DEBUG) || defined (DEBUG_INTERNAL))
#define PRINT_DEBUG_INTERNAL(msg) if(info->out) timestamp(*info->out) << " Rank " << info->id << " => " << msg << std::endl
#else
#define PRINT_DEBUG_INTERNAL(msg)
#endif

#if (defined (DEBUG) || defined (DEBUG_TEST))
#define PRINT_DEBUG_TEST(msg) if(info->out) timestamp(*info->out) << " Rank " << info->id << " => " << msg << std::endl
#else
#define PRINT_DEBUG_TEST(msg)
#endif

#if (defined (DEBUG) || defined (DEBUG_PERF))
#define PRINT_DEBUG_PERF(msg) if(info->out) timestamp(*info->out) << " Rank " << info->id << " => " << msg << std::endl
#else
#define PRINT_DEBUG_PERF(msg)
#endif

//How many different row IDs will be in a cache.
#define DIFF_ROW_ID_PER_CACHE 4

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

static inline lazygaspi_age_t get_min_age(lazygaspi_age_t current, lazygaspi_age_t slack){
    return (current < slack + 2) ? 1 : (current - slack - 1);
}

static inline gaspi_rank_t get_rank_of_table(lazygaspi_id_t table_id, gaspi_rank_t n){
    return table_id % n;
};

static inline unsigned long get_table_amount(gaspi_offset_t table_amount, gaspi_rank_t rank_amount, gaspi_rank_t rank){
    return table_amount / rank_amount + (unsigned long)(rank < table_amount % rank_amount);
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
                                           gaspi_rank_t rank){
    return get_table_amount(table_amount, rank_amount, rank) * table_size;
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
    auto flag = (lazygaspi_age_t*)((bool*)rows + sizeof(LazyGaspiRowData) + info->row_size + rank * sizeof(lazygaspi_age_t)); 
    if(*flag){
        auto temp = *flag;
        *flag = 0;
        return temp;
    }
    return 0;
}

static inline gaspi_size_t get_cache_size(gaspi_size_t row_size, lazygaspi_id_t table_amount){
    return DIFF_ROW_ID_PER_CACHE * table_amount * (sizeof(LazyGaspiRowData) + row_size);
}

/** Returns the offset, in bytes, of a given row, including metadata, in the cache.
 *  Parameters:
 *  info - A pointer to the "info" segment.
 *  row_id - The row's ID.
 *  table_id - The table's ID.
 */
static inline gaspi_offset_t get_offset_in_cache(LazyGaspiProcessInfo* info, lazygaspi_id_t row_id, lazygaspi_id_t table_id){
    return ((row_id % DIFF_ROW_ID_PER_CACHE) * info->table_amount + table_id) * (sizeof(LazyGaspiRowData) + info->row_size);
}

static inline gaspi_offset_t get_offset_in_rows_segment(LazyGaspiProcessInfo* info, lazygaspi_id_t row_id, lazygaspi_id_t table_id){
    return (info->table_size * (table_id / info->n) + row_id) * 
           (sizeof(LazyGaspiRowData) + info->row_size + info->n * sizeof(lazygaspi_age_t));
}

struct Location{
    //The offset of the intended row inside the rows table, in bytes.
    gaspi_offset_t offset;
};

/**Gets the location (server and offset) of a row wanted by the client.
 * Parameters:
 * info     - The LazyGaspiProcessInfo of the current process.
 * table_id - The row's table ID.
 * row_id   - The row's ID.
 * Returns:
 * The Location (rank; offset, in bytes) of the row.
 */
Location lazygaspi_get_location(LazyGaspiProcessInfo* info, lazygaspi_id_t table_id, lazygaspi_id_t row_id, size_t row_size);

/**Serves as a trapdoor for server processes, so that they don't run the user code. */
void lazygaspi_server_trapdoor(LazyGaspiProcessInfo* info, size_t row_size);

#endif