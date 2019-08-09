# LazyGASPI-HS

This is an implementation of the LazyGASPI library where all processes have the same role (**homogeneous**) and data is **sharded** among them.\
LazyGASPI is the implementation of bounded staleness (https://www.usenix.org/system/files/conference/atc14/atc14-paper-cui.pdf) using GASPI (http://www.gaspi.de/).

## Table of Contents

[How it works](#How-it-works)\
[ID's/Macros, Structures/Typedefs and Functions](#idsMacStrTypFunc)
- [ID's/Macros](#idsMac)
  - [`LAZYGASPI_ID_INFO`](#idInfo)
  - [`LAZYGASPI_ID_ROW_LOCATION_TABLE`](#idRowLoc)
  - [`LAZYGASPI_ID_CACHE`](#idCache)
  - [`LAZYGASPI_ID_AVAIL`](#idAvail)
  - [`LAZYGASPI_SC_HASH_ROW`](#macro_hrow)
  - [`LAZYGASPI_SC_HASH_TABLE`](#macro_htable)
  - [`LAZYGASPI_SC_BARRIER`](#macro_barrier)
- [Structures/Typedefs](#strTyp)
  - [`CachingOptions (struct)`](#co)
  - [`CacheHash (typedef)`](#ch)
  - [`LazyGaspiProcessInfo (struct)`](#lgpi)
  - [`LazyGaspiRowData (struct)`](#lgrd)
  - [`SizeDeterminer (typedef)`](#sd)
  - [`OutputCreator (typedef)`](#oc)
- [Functions](#Functions)
  - [`lazygaspi_init`](#fInit)
  - [`lazygaspi_get_info`](#fInfo)
  - [`lazygaspi_prefetch`](#fPrefetch)
  - [`lazygaspi_prefetch_all`](#fPrefetchAll)
  - [`lazygaspi_read`](#fRead)
  - [`lazygaspi_write`](#fWrite)
  - [`lazygaspi_clock`](#fClock)
  - [`lazygaspi_term`](#fTerm)

[Implementation Locations](#Implementation-Locations)\
[Notes](#Notes)
- [A note on the amount of ranks](#A-note-on-the-amount-of-ranks)
- [A note on barriers](#A-note-on-barriers)


## How it works

The predetermined amount of tables, amount of rows per table, and amount of bytes per row, determine the total amount of data that must be stored.\
Depending on how much memory is available per process, a single one might not be enough to store all data (i.e., to be the only server). Therefore, the initialization procedure consists of allocating as much space as possible in rank `i` and, if some data was left unallocated, process `i+1` will receive the leftover, and so on, starting at rank 0.\
The remaining ranks will be clients, where the user's code will actually run. Because of this, the amount of processes to be considered is different than usual, which raises some caveats (see notes on [the amount of ranks](#A-note-on-the-amount-of-ranks) and on [barriers](#A-note-on-barriers)).

<a id="idsMacStrTypFunc"></a>
## ID's/Macros, Structures/Typedefs and Functions
<a id="idsMac"></a>
### ID's/Macros
| Segment ID | Explanation |
| ---------- | ----------- |
| <a id="idInfo"></a>`LAZYGASPI_ID_INFO = 0` | Stores the `LazyGaspiProcessInfo` of the current rank | 
| <a id="idRowLoc"></a>`LAZYGASPI_ID_ROW_LOCATION_TABLE = 1` | Stores a table of the first row stored in each server |
| <a id="idCache"></a>`LAZYGASPI_ID_CACHE = 2` | Stores the cache |
| <a id="idAvail"></a>`LAZYGASPI_ID_AVAIL = 3` | The first available segment ID for allocation (not an actual segment)|

| Macro | Explanation |
| ----- | ----------- | 
| <a id="macro_hrow"></a>`LAZYGASPI_SC_HASH_ROW` | A [`CacheHash`](#ch) lambda that hashes entries by row (rows of the same table will (usually) have different positions) |
| <a id="macro_htable"></a>`LAZYGASPI_SC_HASH_TABLE` | A [`CacheHash`](#ch) lambda that hashes entries by table (rows with the same ID of different tables will (usually) have different positions) | 
| <a id="macro_barrier"></a>`LAZYGASPI_SC_BARRIER` | See [a note on barriers](#a-note-on-barriers) |

<a id="strTyp"></a>
### Structures/Typedefs
<a id="co"></a>
#### `CachingOptions (struct)`
| Type | Member | Explanation |
| ---- | ------ | ----------- |
| `CacheHash` | `hash` | Function used to hash an entry to insert into the `LAZYGASPI_ID_CACHE` segment, or `nullptr` to use [`LAZYGASPI_SC_HASH_ROW`](#macro_hrow) instead |
| `gaspi_size_t` | `size` | The amount of rows to be allocated for the cache, or `0` to allocate as many as possible, while at the same time leaving the amount of memory specified in [`lazygaspi_init`](#fInit) free |

<a id="ch"></a>
#### `CacheHash (typedef)`
Takes 3 parameters: the row ID, the table ID, and a pointer to the `LazyGaspiProcessInfo` in the `LAZYGASPI_ID_INFO` segment. `CacheHash` should then return a `gaspi_offset_t` indicating the index of the new entry in the cache.\
Value can be higher or equal to the cache's `size` (modulo `size` is used afterward).

<a id="lgpi"></a>
#### `LazyGaspiProcessInfo (struct)`

| Type | Member | Explanation |
| ---- | ------ | ----------- |
| `gaspi_rank_t`    | `id`               | The current rank, as outputted by `gaspi_proc_rank` |
| `gaspi_rank_t`    | `n`                | The total amount of ranks, as outputted by `gaspi_proc_num` |
| `lazygaspi_age_t` | `age`              | The age of the current process. This corresponds to how many times `lazygaspi_clock` has been called |
| `bool`            | `isServer`         | `true` if the current process is a **server** and `false` if it is a **client** |
| `gaspi_group_t`   | `group_id_clients` | The ID of the group of clients, or `GASPI_GROUP_ALL` if there is only one client (\*) |
| `gaspi_group_t`   | `group_id_servers` | The ID of the group of servers, or `GASPI_GROUP_ALL` if there is only one server (\*) |
| `unsigned int`    | `num_clients`      | The amount of client ranks |
| `unsigned int`    | `num_servers`      | The amount of servers |
| `lazygaspi_id_t`  | `table_amount`     | The total amount of tables stored by all servers |
| `lazygaspi_id_t`  | `table_size`       | The amount of rows in each table (same for all) |
| `gaspi_size_t`    | `row_size`         | The size of each row (same for all), in bytes |
| `lazygaspi_age_t` | `communicator`     | Member used for communication with servers | 
| `CachingOptions`  | `cacheOpts`        | The user options for how to cache read rows |
| `std::ostream*`   | `out`              | A pointer to the output stream for debugging. See [OutputCreator](#oc). |
| `bool`            | `offset_slack`     | `true` if accetable age range should be calculated from the previous age (iteration); `false` if it should be calculated from the current age (\*\*) |

(\*) In this case, collective operations should not be used. Always check either if `num_(clients/servers)` is `1` or if 
group ID is `GASPI_GROUP_ALL`.\
(\*\*) For example, if current age is 7, slack is 2 and `offset_slack` is `true`, the minimum acceptable age for a read row is 7 - 2 - 1 = 4; if `offset_slack` is `false`, the minimum age is 7 - 2 = 5.

<a id="lgrd"></a>
#### `LazyGaspiRowData (struct)`
This is the metadata associated to each row stored in the **server**.

| Type | Member | Explanation |
| ---- | ------ | ----------- |
| `lazygaspi_age_t` | `age` | The amount of times `lazygaspi_clock` had been called before row was written to server |
| `lazygaspi_id_t` | `row_id` | The ID of the associated row |
| `lazygaspi_id_t` | `table_id` | The ID of associated row's table |

<a id="sd"></a>
#### `SizeDeterminer (typedef)`
Can determine one of these: `LazyGaspiProcessInfo::table_amount`, `LazyGaspiProcessInfo::table_size` or `LazyGaspiProcessInfo::row_size`, which are henceforth considered "sizes".\
This function is called by `lazygaspi_init` after GASPI is initialized, `LazyGaspiProcessInfo::id` and `LazyGaspiProcessInfo::n` have been set and after output is created (see [OutputCreator](#oc)). This means that not only does a `SizeDeterminer` already know the rank of the current process and the total amount of ranks, but it can also output to the process's output stream.\
If a "size" is passed as a non-zero value to `lazygaspi_init` then its corresponding `SizeDeterminer` will never be called, even if it not a `nullptr` (see [`lazygaspi_init`](#fInit)), for example:
```
#define TABLE_AMOUNT 5
...
lazygaspi_init(TABLE_AMOUNT, TABLE_SIZE, ROW_SIZE, MyOutputCreator, MyCacheOptions, FREE_MEM, MyTableAmountDeterminer, ...);
//Since TABLE_AMOUNT != 0, MyTableAmountDeterminer will never be called.
```

Parameters:
- `gaspi_rank_t rank` - The rank of the current process, as outputted by `gaspi_proc_rank`.
- `gaspi_rank_t total` - The total amount of ranks, as outputted by `gaspi_proc_num`.
- `void * data` - A pointer to the data that will be used by the function. This is only used by the function itself.

\
Returns:
- The size of the corresponding member, as a `gaspi_size_t`.

<a id="oc"></a>
#### `OutputCreator (typedef)`
Responsible for creating the output stream and storing a pointer to it in this process's `LAZYGASPI_ID_INFO` segment.
This function is called by `lazygaspi_init` after GASPI is initialized, `LazyGaspiProcessInfo::id` and `LazyGaspiProcessInfo::n` are set. If `nullptr` is passed, the output stream is `std::cout`.

Parameters:
- `LazyGaspiProcessInfo*` - A pointer to the process's `LAZYGASPI_ID_INFO` segment. `LazyGaspiProcessInfo::out` must be set.

### Functions

<a id="fInit"></a>
#### `lazygaspi_init` 
Initializes the LazyGASPI library, including GASPI itself. This function must be called before any other LazyGASPI functions. 

| Type | Parameter | Explanation |
| ---- | --------- | ----------- |
| `lazygaspi_id_t` | `table_amount` | The amount of tables to be allocated, or `0` if value is to be determined by `det_amount` |
| `lazygaspi_id_t` | `table_size` | The size of each table (\*), in amount of rows, or `0` if value is to be determined by `det_tablesize` |
| `gaspi_size_t` | `row_size` | The size of a single row, in bytes, or `0` if value is to be determined by `det_rowsize` |
| [`OutputCreator`](#oc) | `creator` | Used to create the process's output stream for debug messages. Use `nullptr` to indicate `std::cout` should be used |
| [`CachingOptions`](#co) | `cache_options` | Indicates how to cache data. |
| `gaspi_size_t` | `freeMemory` | The minimum amount of memory guaranteed to be left unallocated for client processes, in bytes; default is 1 MB |
| [`SizeDeterminer`](#sd) | `det_amount` | A `SizeDeterminer` for the amount of tables. Will only be called if `table_amount` is `0` |
| `void*` | `data_amount` | A pointer passed to `det_amount` when it is called |
| [`SizeDeterminer`](#sd) | `det_tablesize` | A `SizeDeterminer` for the amount of rows in a table. Will only be called if `table_size` is `0` |
| `void*` | `data_tablesize` | A pointer passed to `det_tablesize` when it is called |
| [`SizeDeterminer`](#sd) | `det_rowsize` | A `SizeDeterminer` for the size of a row, in bytes. Will only be called if `row_size` is `0` |
| `void*` | `data_rowsize` | A pointer passed to `det_rowsize` when it is called |

Returns:
- `GASPI_SUCCESS` on success;
- `GASPI_ERROR` on unknown error thrown by GASPI (or another error code);
- `GASPI_TIMEOUT` on timeout;
- `GASPI_ERR_INV_RANK` if running in a single node (can also be thrown by GASPI for other reasons);
- `GASPI_ERR_INV_NUM` if a "size" was `0` and its corresponding `SizeDeterminer` was a `nullptr`.

\
(\*) All tables have the same size.

<a id="fInfo"></a>
#### `lazygaspi_get_info`

Outputs a pointer to the [`LAZYGASPI_ID_INFO`](#idInfo) segment.

| Type | Parameter | Explanation |
| ---- | --------- | ----------- |
| [`LazyGaspiProcessinfo**`](#lgpi) | `info` |  The output parameter that will receive the pointer |

Returns:
- `GASPI_SUCCESS` on success;
- `GASPI_ERROR` on unknown error thrown by GASPI (or another error code);
- `GASPI_TIMEOUT` on timeout;
- `GASPI_ERR_NULLPTR` if a `nullptr` is passed as the value of `info`. 


<a id="fPrefetch"></a>
#### `lazygaspi_prefetch`

Writes prefetch requests on the proper *server(s)*. The two arrays ought to have a size of `size`. For a given index `i`, `row_vec[i]` from `table_vec[i]` will be requested for prefetching.

| Type | Parameter | Explanation |
| ---- | --------- | ----------- |
| `lazygaspi_id_t*` | `row_vec` |  An array of row ID's to prefetch |
| `lazygaspi_id_t*` | `table_vec` | An array containing the table ID's of the corresponding row for each index |
| `size_t`          | `size` | The size of **both** arrays |
| `lazygaspi_slack_t` | `slack` | The amount of slack to be used when prefetching back to the *client* |

For example, calling `lazygaspi_prefetch` with  `row_vec = {0, 1, 0, 3}`, `table_vec = {0, 0, 1, 1}` and `size = 4` would be valid (from table `0`, rows `0` and `1` would be prefetched; from table `1`, rows `0` and `3` would be prefetched).

Returns:
- `GASPI_SUCCESS` on success;
- `GASPI_ERROR` on unknown error thrown by GASPI (or another error code);
- `GASPI_TIMEOUT` on timeout;
- `GASPI_ERR_INV_RANK` if calling process is a *server* (or thrown by GASPI for another reason);
- `GASPI_ERR_INV_NUM` if either `row_id` or `table_id` is not a valid ID (or thrown by GASPI for another reason);
- `GASPI_ERR_NOINIT` if `lazygaspi_clock` has not been called even once (or thrown by GASPI for another reason). 


<a id="fPrefetchAll"></a>
#### `lazygaspi_prefetch_all`
Writes prefetch requests on all *servers* for all rows.

| Type | Parameter | Explanation |
| ---- | --------- | ----------- |
| `lazygaspi_slack_t` | `slack` | The amount of slack to be used when prefetching back to the *client* |

Returns:
- `GASPI_SUCCESS` on success;
- `GASPI_ERROR` on unknown error thrown by GASPI (or another error code);
- `GASPI_TIMEOUT` on timeout;
- `GASPI_ERR_INV_RANK` if calling process is a *server* (or thrown by GASPI for another reason);
- `GASPI_ERR_NOINIT` if `lazygaspi_clock` has not been called even once (or thrown by GASPI for another reason).

<a id="fRead"></a>
#### `lazygaspi_read`

Reads a row. Row's age is guaranteed to be at least the current rank's age minus the slack (minus one if `LazyGaspiProcessInfo::offset_slack` is `true`).

| Type | Parameter | Explanation |
| ---- | --------- | ----------- |
| `lazygaspi_id_t` | `row_id` | The ID of the row to be read |
| `lazygaspi_id_t` | `table_id` | The ID of the row's table |
| `lazygaspi_slack_t` | `slack` | The amount of slack allowed for the returned row's age |
| `void*` | `row` | Output parameter for the row data. Will write `LazyGaspiProcessInfo::row_size` bytes |
| `LazyGaspiRowData*` | `data` |  Output parameter for the row's metadata (see [LazyGaspiRowData](#lgrd)), or `nullptr` to ignore |

Returns:
- `GASPI_SUCCESS` on success;
- `GASPI_ERROR` on unknown error thrown by GASPI (or another error code);
- `GASPI_TIMEOUT` on timeout;
- `GASPI_ERR_INV_RANK` if calling process is a *server* (or thrown by GASPI for another reason);
- `GASPI_ERR_INV_NUM` if either `row_id` or `table_id` is not a valid ID (or thrown by GASPI for another reason).
- `GASPI_ERR_NOINIT` if `lazygaspi_clock` has not been called even once (or thrown by GASPI for another reason).

<a id="fWrite"></a>
#### `lazygaspi_write`

Writes the given row to the proper *server*.

| Type | Parameter | Explanation |
| ---- | --------- | ----------- |
| `lazygaspi_id_t` | `row_id` | The ID of the row to be read |
| `lazygaspi_id_t` | `table_id` | The ID of the row's table |
| `void*` | `row` | Output parameter for the row data. Will write `LazyGaspiProcessInfo::row_size` bytes |

Returns:
- `GASPI_SUCCESS` on success;
- `GASPI_ERROR` on unknown error thrown by GASPI;
- `GASPI_TIMEOUT` on timeout;
- `GASPI_ERR_INV_RANK` if calling process is a *server* (or thrown by GASPI for another reason);
- `GASPI_ERR_INV_NUM` if either `row_id` or `table_id` is not a valid ID (or thrown by GASPI for another reason);
- `GASPI_ERR_NOINIT` if `lazygaspi_clock` has not been called even once (or thrown by GASPI for another reason).

<a id="fClock"></a>
#### `lazygaspi_clock`

Increases the age of the current process by one. Must be called at least once before reading, writing or prefetching.

Returns:
- `GASPI_SUCCESS` on success;
- `GASPI_ERROR` on unknown error thrown by GASPI (or another error code);
- `GASPI_TIMEOUT` on timeout;

<a id="fTerm"></a>
#### `lazygaspi_term`

Terminates LazyGASPI for the current process. Deletes the output stream whose pointer can be found in the current process's `LAZYGASPI_ID_INFO` segment (unless it points to `std::cout`).\
*Server* will also terminate if `lazygaspi_terminate` is called by all *clients*.

Returns:
- `GASPI_SUCCESS` on success;
- `GASPI_ERROR` on unknown error thrown by GASPI (or another error code);
- `GASPI_TIMEOUT` on timeout;

## Implementation locations

| Implementation of         | at            |
| ------------------------- | ------------- |
| `lazygaspi_init`          | init.cpp      |
| `lazygaspi_get_info`      | general.cpp   |
| `lazygaspi_prefetch`      | general.cpp   |
| `lazygaspi_prefetch_all`  | general.cpp   |
| `lazygaspi_read`          | read.cpp      |
| `lazygaspi_write`         | write.cpp     |
| `lazygaspi_clock`         | general.cpp   |
| `lazygaspi_term`          | general.cpp   |

## Notes

### A note on the amount of ranks

Since there are less *client* processes than running processes, `LazyGaspiProcessInfo::num_clients` should be used instead of `gaspi_proc_num` (or more easily `LazyGaspiProcessInfo::n`) for work distribution, etc...

### A note on barriers

A call to a barrier with `GASPI_GROUP_ALL` will not work (unless it times out), since servers never return from `lazygaspi_init`.
The macro `LAZYGASPI_SC_BARRIER` is defined to facilitate what would otherwise be a global barrier in other implementations:
```
...
SUCCESS_OR_DIE(LAZYGASPI_SC_BARRIER); //Replaces SUCCESS_OR_DIE(gaspi_barrier(GASPI_GROUP_ALL, GASPI_BLOCK))
...
```
