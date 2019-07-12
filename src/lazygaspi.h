#ifndef __H_LAZYGASPI
#define __H_LAZYGASPI

#include <GASPI.h>

#define SEGMENT_ID_INFO 0 
#define SEGMENT_ID_ROWS 1
#define SEGMENT_ID_CACHE 2

typedef unsigned long lazygaspi_id_t;

struct LazyGaspiProcessInfo{
    gaspi_rank_t id, n;
};

struct LazyGaspiRowData{

};

/** Initializes LazyGASPI.
 *  Parameters:
 *  table_size   - The amount of rows in one table.
 *  table_amount - The amount of tables.
 *  row_size     - The size of a row, in bytes.
 */
gaspi_return_t lazygaspi_init(lazygaspi_id_t table_size, lazygaspi_id_t table_amount, gaspi_size_t row_size);

#endif