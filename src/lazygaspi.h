#ifndef __H_LAZYGASPI
#define __H_LAZYGASPI

#include <GASPI.h>

#define SEGMENT_ID_INFO 0 
#define SEGMENT_ID_ROWS 1
#define SEGMENT_ID_CACHE 2

typedef unsigned long lazygaspi_id_t;
typedef unsigned long lazygaspi_age_t;
typedef unsigned long lazygaspi_slack_t;

//None of the fields in this structure should be altered, except for the out field.
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
};

struct LazyGaspiRowData{
    lazygaspi_age_t age;
    lazygaspi_id_t row_id;
    lazygaspi_id_t table_id;

    LazyGaspiRowData(lazygaspi_id_t row_id, lazygaspi_id_t table_id, lazygaspi_age_t age) : 
                    row_id(row_id), table_id(table_id), age(age) {};
};

/** Initializes LazyGASPI.
 * 
 *  Parameters:
 *  table_size   - The amount of rows in one table.
 *  table_amount - The amount of tables.
 *  row_size     - The size of a row, in bytes.
 * 
 *  Returns:
 *  GASPI_SUCCESS on success, GASPI_ERROR (or another error code) on error, GASPI_TIMEOUT on timeout.
 *  GASPI_ERR_INV_NUM indicates that at least one of the three parameters was 0.
 */
gaspi_return_t lazygaspi_init(lazygaspi_id_t table_size, lazygaspi_id_t table_amount, gaspi_size_t row_size);

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
#endif