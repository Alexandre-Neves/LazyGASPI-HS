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

#define PROC0 0
#define QUEUE0 0

#define NOTIF_ID_ISSERVER       1
#define NOTIF_ID_ISCLIENT       2
#define NOTIF_ID_LAST_SERVER    3
#define NOTIF_ID_INTRACOMM      4
#define NOTIF_ID_UPDATE_SUBMITTED 5
#define NOTIF_ID_TERM_PROGRAM   6

#define NOTIF_AMOUNT_SERVER 2

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

struct Location{
    //The rank of the server that contains the row.
    gaspi_rank_t rank;
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