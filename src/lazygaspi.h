#ifndef __H_LAZYGASPI
#define __H_LAZYGASPI

#include <GASPI.h>

#define SEGMENT_ID_INFO 0 

typedef enum{
    LG_SUCCESS,
    LG_GASPI_ERROR,
    LG_NOT_ENOUGH_MEMORY
} lazygaspi_return_t;


/** Initializes LazyGASPI.
 *  Parameters:
 *  table_size   - The amount of rows in one table.
 *  table_amount - The amount of tables.
 *  row_size     - The size of a row, in bytes.
 */
lazygaspi_return_t lazygaspi_init(gaspi_offset_t table_size, gaspi_offset_t table_amount, gaspi_size_t row_size);

#endif