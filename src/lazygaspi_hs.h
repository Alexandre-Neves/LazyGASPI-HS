/** LazyGASPI-HS
 *  LazyGASPI library with a homogeneous and sharded implementation.
 */

#ifndef LAZYGASPI_HS
#define LAZYGASPI_HS

#include <GASPI.h>
#include <fstream>

#define SEGMENT_ID_INFO 0 
#define SEGMENT_ID_ROWS 1
#define SEGMENT_ID_CACHE 2

typedef unsigned long lazygaspi_id_t;
typedef unsigned long lazygaspi_age_t;
typedef unsigned long lazygaspi_slack_t;

struct LazyGaspiProcessInfo;

struct ShardingOptions{
    //How many rows will be assigned to a given process at a time. For example, a value of one means rows are distributed one at 
    //a time through all processes, while a value equal to the size of a table means tables are assigned one at a time.
    lazygaspi_id_t block_size;
    ShardingOptions(lazygaspi_id_t size) : block_size(size){};
};

struct CachingOptions{
    typedef gaspi_offset_t (*CacheHash)(lazygaspi_id_t row_id, lazygaspi_id_t table_id, LazyGaspiProcessInfo* info);
    CacheHash hash;
    //The size of the cache, in rows.
    gaspi_size_t size;
    CachingOptions(CacheHash hash, gaspi_size_t size) : hash(hash), size(size) {};
};

//None of the fields in this structure should be altered, except for the out and offset_slack fields.
struct LazyGaspiProcessInfo{
    //Value returned by gaspi_proc_rank.
    gaspi_rank_t id;
    //Value returned by gaspi_proc_num. Should be equal to all other processes and different from 0.
    gaspi_rank_t n;
    //The age of the current process.
    lazygaspi_age_t age;
    //The amount of tables that have been distributed evenly among all processes.
    //This is not the same as the amount of tables stored by this process.
    gaspi_offset_t table_amount;
    //The amount of rows in a table. Not the same as the size of a table, in bytes.
    gaspi_offset_t table_size;
    //The size of a row as defined by the user, in bytes.
    gaspi_size_t row_size;
    //Field used for communication with other segments.
    lazygaspi_age_t communicator;
    //Stream used to output lazygaspi debug messages. Use nullptr to ignore lazygaspi output.
    std::ofstream* out;
    //True if minimum age for read rows will be the current age minus the slack minus 1.
    //If false, minimum age for read rows will be the current age minus the slack.
    bool offset_slack = true;

    ShardingOptions shardOpts;
    CachingOptions cacheOpts;
};

struct LazyGaspiRowData{
    lazygaspi_age_t age;
    lazygaspi_id_t row_id;
    lazygaspi_id_t table_id;

    LazyGaspiRowData(lazygaspi_age_t age, lazygaspi_id_t row_id, lazygaspi_id_t table_id) : 
                     age(age), row_id(row_id), table_id(table_id) {};
    LazyGaspiRowData() : LazyGaspiRowData(0, 0, 0) {}
};

/** A function used to determine a given size that depends on the current rank and/or total amount of ranks.
 *  Parameters:
 *  rank - The current rank, as given by gaspi_proc_rank.
 *  total - The total number of ranks, as given by gaspi_proc_num.
 *  data  - User data passed to lazygaspi_init.
 */
typedef gaspi_size_t (*SizeDeterminer)(gaspi_rank_t rank, gaspi_rank_t total, void* data);

/**A function that creates and sets the output file stream for the info segment.
 * Info is guaranteed to have fields `id` and `n` filled before this function is called. 
 */
typedef void (*OutputCreator)(LazyGaspiProcessInfo* info);

/** Initializes LazyGASPI.
 * 
 *  Parameters:
 *  table_amount    - The amount of tables, or 0 if size is to be determined by a SizeDeterminer.
 *  table_size      - The amount of rows in one table, or 0 if size is to be determined by a SizeDeterminer.
 *  row_size        - The size of a row, in bytes, or 0 if size is to be determined by a SizeDeterminer.
 *  shard_options   - Indicates how to shard data among processes. Use block_size = 0 to indicate default sharding (by table).
 *  caching_options - Indicates how to cache data. Use hash = nullptr or size = 0 to indicate default hashing (stores as many rows
 *                    as a table can hold).
 *  output          - Output file stream for debug messages. Use nullptr to ignore.
 *  det_amount      - Determines the amount of tables to be allocated. Use nullptr to ignore.
 *  det_tablesize   - Determines the size of each table. Use nullptr to ignore.
 *  det_rowsize     - Determines the size of a row, in bytes. Use nullptr to ignore.
 * 
 *  Returns:
 *  GASPI_SUCCESS on success, GASPI_ERROR (or another error code) on error, GASPI_TIMEOUT on timeout.
 *  GASPI_ERR_INV_NUM indicates that at least one of the three parameters was 0 and its SizeDeterminer was a nullptr or returned 0.
 */
gaspi_return_t lazygaspi_init(lazygaspi_id_t table_amount, lazygaspi_id_t table_size, gaspi_size_t row_size, 
                              ShardingOptions shard_options = ShardingOptions(0), 
                              CachingOptions cache_options = CachingOptions(nullptr, 0),
                              OutputCreator outputCreator = nullptr,
                              SizeDeterminer det_amount = nullptr, void* data_amount = nullptr, 
                              SizeDeterminer det_tablesize = nullptr, void* data_tablesize = nullptr, 
                              SizeDeterminer det_rowsize = nullptr, void* data_rowsize = nullptr);

/** Outputs a pointer to the "info" segment.
 *  
 *  Parameters:
 *  info - Output parameter for a pointer to the "info" segment.
 * 
 *  Returns:
 *  GASPI_SUCCESS on success, GASPI_ERROR (or another error code) on error, GASPI_TIMEOUT on timeout.
 */
gaspi_return_t lazygaspi_get_info(LazyGaspiProcessInfo** info);

/** Fulfils prefetch requests from other ranks. 
 *  Must be called by all processes at the end of each iteration for prefetching to work properly.
 *  
 *  Returns:
 *  GASPI_SUCCESS on success, GASPI_ERROR (or another error code) on error, GASPI_TIMEOUT on timeout. 
 */
gaspi_return_t lazygaspi_fulfil_prefetches();

/** Prefetches a given row.
 *  
 *  Parameters:
 *  row_id   - The row's ID.
 *  table_id - The ID of the row's table.
 *  slack    - The slack allowed for the row that will be prefetched.
 * 
 *  Returns:
 *  GASPI_SUCCESS on success, GASPI_ERROR (or another error code) on error, GASPI_TIMEOUT on timeout.
 *  GASPI_ERR_INV_NUM is returned if either row_id or table_id are invalid.
 */
gaspi_return_t lazygaspi_prefetch(lazygaspi_id_t row_id, lazygaspi_id_t table_id, lazygaspi_slack_t slack);

/** Reads a row, whose age is within the given slack.
 * 
 *  Parameters:
 *  row_id - The row's ID.
 *  table_id - The ID of the row's table.
 *  slack    - The slack allowed for the row that will be read.
 *  row      - Output parameter for the row. Size of the data read will be the same as the parameter passed to lazygaspi_init.
 *  data     - Output parameter for the metadata tag associated with the read row. Use nullptr to ignore.
 *  Returns:
 *  GASPI_SUCCESS on success, GASPI_ERROR (or another error code) on error, GASPI_TIMEOUT on timeout.
 *  GASPI_ERR_INV_NUM is returned if either row_id or table_id are invalid.
 *  GASPI_ERR_NULLPTR is returned if row is a nullptr.
 */
gaspi_return_t lazygaspi_read(lazygaspi_id_t row_id, lazygaspi_id_t table_id, lazygaspi_slack_t slack, void* row, LazyGaspiRowData* data = nullptr);

/** Writes the given row in the appropriate server.
 *  
 *  Parameters:
 *  row_id   - The row's ID.
 *  table_id - The ID of the row's table.
 *  row      - A pointer to the row's data. Size is assumed to be the same as the size passed to lazygaspi_init.
 * 
 *  Returns:
 *  GASPI_SUCCESS on success, GASPI_ERROR (or another error code) on error, GASPI_TIMEOUT on timeout.
 *  GASPI_ERR_INV_NUM is returned if either row_id or table_id are invalid.
 *  GASPI_ERR_NULLPTR is returned if row is a nullptr.
 */
gaspi_return_t lazygaspi_write(lazygaspi_id_t row_id, lazygaspi_id_t table_id, void* row);

/** Increments the current process's age by 1.
 * 
 *  Returns:
 *  GASPI_SUCCESS on success, GASPI_ERROR (or another error code) on error, GASPI_TIMEOUT on timeout.
 */
gaspi_return_t lazygaspi_clock();

/* Terminates LazyGASPI. 
 *
 * Returns:
 * GASPI_SUCCESS on success, GASPI_ERROR (or another error code) on error, GASPI_TIMEOUT on timeout.
 */
gaspi_return_t lazygaspi_term();

#define LAZYGASPI_HS_HASH_ROW [](lazygaspi_id_t row_id, lazygaspi_id_t table_id, LazyGaspiProcessInfo* info){\
                                return info->table_size * table_id + row_id; }

#define LAZYGASPI_HS_HASH_TABLE [](lazygaspi_id_t row_id, lazygaspi_id_t table_id, LazyGaspiProcessInfo* info){\
                                    return info->table_amount * row_id + table_id; }

#endif