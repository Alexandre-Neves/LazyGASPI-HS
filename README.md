# LazyGASPI-HS

LazyGASPI-HS is an implementation of the LazyGASPI library where all processes have the same role (**homogeneous**) and data is **sharded** among them.\
LazyGASPI is the implementation of bounded staleness (https://www.usenix.org/system/files/conference/atc14/atc14-paper-cui.pdf) using GASPI (http://www.gaspi.de/).  
For compilation and installation instructions, see INSTALL.

## Table of Contents

[Compilation](#Compilation)\
[How it works](#How-it-works)\
[ID's/Macros, Structures/Typedefs and Functions](#idsMacStrTypFunc)
- [ID's/Macros](#idsMac)
  - [`LAZYGASPI_ID_INFO`](#idInfo)
  - [`LAZYGASPI_ID_ROWS`](#idRows)
  - [`LAZYGASPI_ID_CACHE`](#idCache)
  - [`LAZYGASPI_ID_AVAIL`](#idAvail)
  - [`LAZYGASPI_HS_HASH_ROW`](#macro_hrow)
  - [`LAZYGASPI_HS_HASH_TABLE`](#macro_htable)
- [Structures/Typedefs](#strTyp)
  - [`ShardingOptions (struct)`](#so)
  - [`CachingOptions (struct)`](#co)
  - [`CacheHash (typedef)`](#ch)
  - [`LazyGaspiProcessInfo (struct)`](#lgpi)
  - [`LazyGaspiRowData (struct)`](#lgrd)
  - [`SizeDeterminer (typedef)`](#sd)
  - [`OutputCreator (typedef)`](#oc)
- [Functions](#Functions)
  - [`lazygaspi_init`](#fInit)
  - [`lazygaspi_get_info`](#fInfo)
  - [`lazygaspi_fulfill_prefetches`](#fFulfillPrefetches)
  - [`lazygaspi_prefetch`](#fPrefetch)
  - [`lazygaspi_prefetch_all`](#fPrefetchAll)
  - [`lazygaspi_read`](#fRead)
  - [`lazygaspi_write`](#fWrite)
  - [`lazygaspi_clock`](#fClock)
  - [`lazygaspi_term`](#fTerm)

[Locks](#Locks)\
\
[Tests](#Tests)
 - [Test 0](#Test-0)
## Compilation
During compilation, the following macros can be defined (through the `configure.sh` script):

| Macro | Explanation |
| ----- | ----------- | 
| `DEBUG` | Same as defining all of the macros below |
| <a id="macroDebugInternal"></a>`DEBUG_INTERNAL` | Prints debug information for all LazyGASPI function calls |
| `DEBUG_ERRORS` | Prints error output whenever an error occurs |

Some macros were left out since they are explained in [Tests](#Tests).

## How it works

Data (in the form of rows) is sharded and distributed among all processes (see [ShardingOptions](#so)).\
Each process acts as a server for the rows that were distributed to it (will send rows that were prefetched to the requesting client).\
[`lazygaspi_fulfill_prefetches`](#fFulfill) must be called (ideally at the end of the current iteration) in order for prefetching to occur. If the program does not resort to prefetching, there is no need to call [`lazygaspi_fulfill_prefetches`](#fFulfill).

<a id="idsMacStrTypFunc"></a>
## ID's/Macros, Structures/Typedefs and Functions
<a id="idsMac"></a>
### ID's/Macros
| Segment ID | Explanation |
| ---------- | ----------- |
| <a id="idInfo"></a>`LAZYGASPI_ID_INFO = 0` | Stores the [`LazyGaspiProcessInfo`](#lgpi) of the current rank | 
| <a id="idRows"></a>`LAZYGASPI_ID_ROWS = 1` | Stores the rows assigned to the current rank |
| <a id="idCache"></a>`LAZYGASPI_ID_CACHE = 2` | Stores the cache |
| <a id="idAvail"></a>`LAZYGASPI_ID_AVAIL = 3` | The first available segment ID for allocation (not an actual segment)|

| Macro | Explanation |
| ----- | ----------- | 
| <a id="macro_hrow"></a>`LAZYGASPI_HS_HASH_ROW` | A [`CacheHash`](#ch) lambda that hashes entries by row (rows of the same table will (usually) have different positions) |
| <a id="macro_htable"></a>`LAZYGASPI_HS_HASH_TABLE` | A [`CacheHash`](#ch) lambda that hashes entries by table (rows with the same ID of different tables will (usually) have different positions) | 

<a id="strTyp"></a>
### Structures/Typedefs
<a id="so"></a>
#### `ShardingOptions (struct)`
| Type | Member | Explanation |
| ---- | ------ | ----------- |
| `lazygaspi_id_t` | `block_size` | How many rows will be assigned to a given process at a time. For example, a value of one means rows are distributed one at a time through all processes, while a value equal to the size of a table means tables are assigned one at a time (almost like how many "cards" are dealt at a time to each "player". |

<a id="co"></a>
#### `CachingOptions (struct)`
| Type | Member | Explanation |
| ---- | ------ | ----------- |
| `CacheHash` | `hash` | Function used to hash an entry to insert into the [`LAZYGASPI_ID_CACHE`](#idCache) segment, or `nullptr` to use [`LAZYGASPI_HS_HASH_ROW`](#macro_hrow) instead |
| `gaspi_size_t` | `size` | The amount of rows to be allocated for the cache, or `0` to allocate as many as possible, while at the same time leaving the amount of memory specified in [`lazygaspi_init`](#fInit) free |

<a id="ch"></a>
#### `CacheHash (typedef)`
Takes 3 parameters: the row ID, the table ID, and a pointer to the [`LazyGaspiProcessInfo`](#lgpi) in the [`LAZYGASPI_ID_INFO`](#idInfo) segment. `CacheHash` should then return a `gaspi_offset_t` indicating the index of the new entry in the cache.\
Value can be higher or equal to the cache's `size` (modulo `size` is used afterward).

<a id="lgpi"></a>
#### `LazyGaspiProcessInfo (struct)`

| Type | Member | Explanation |
| ---- | ------ | ----------- |
| `gaspi_rank_t`    | `id`               | The current rank, as outputted by `gaspi_proc_rank` |
| `gaspi_rank_t`    | `n`                | The total amount of ranks, as outputted by `gaspi_proc_num` |
| `lazygaspi_age_t` | `age`              | The age of the current process. This corresponds to how many times `lazygaspi_clock` has been called |
| `lazygaspi_id_t`  | `table_amount`     | The total amount of tables that have been distributed among all processes. Not the same as the amount of tables stored by the current rank|
| `lazygaspi_id_t`  | `table_size`       | The amount of rows in each table (same for all) |
| `gaspi_size_t`    | `row_size`         | The size of each row (same for all), in bytes |
| `lazygaspi_age_t` | `communicator`     | Member used for communication with servers | 
| `std::ostream*`   | `out`              | A pointer to the output stream for debugging. See [OutputCreator](#oc). |
| `bool`            | `offset_slack`     | `true` if accetable age range should be calculated from the previous age (iteration); `false` if it should be calculated from the current age (\*) |
| `ShardingOptions` | `shardOpts`        | The user options for how to shard the data among the processes. See [`ShardingOptions`](#so) for more information |
| `CachingOptions`  | `cacheOpts`        | The user options for how to cache read rows. See [`CachingOptions`](#co) for more information |

(\*) For example, if current age is 7, slack is 2 and `offset_slack` is `true`, the minimum acceptable age for a read row is 7 - 2 - 1 = 4; if `offset_slack` is `false`, the minimum age is 7 - 2 = 5.

<a id="lgrd"></a>
#### `LazyGaspiRowData (struct)`
This is the metadata associated to a row stored by a process or read from a process.

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
- `LazyGaspiProcessInfo*` - A pointer to the process's `LAZYGASPI_ID_INFO` segment.

### Functions

<a id="fInit"></a>
#### `lazygaspi_init` 
Initializes the LazyGASPI library, including GASPI itself. This function must be called before any other LazyGASPI functions.\
Also initializes MPI (before GASPI) if library was compiled with MPI support. 

| Type | Parameter | Explanation |
| ---- | --------- | ----------- |
| `lazygaspi_id_t` | `table_amount` | The amount of tables to be allocated, or `0` if value is to be determined by `det_amount` |
| `lazygaspi_id_t` | `table_size` | The size of each table (\*), in amount of rows, or `0` if value is to be determined by `det_tablesize` |
| `gaspi_size_t` | `row_size` | The size of a single row, in bytes, or `0` if value is to be determined by `det_rowsize` |
| [`ShardingOptions`](#so) | `shard_options` | The sharding options to be used |
| [`CachingOptions`](#co) | `cache_options` | Indicates how to cache data. |
| [`OutputCreator`](#oc) | `creator` | Used to create the process's output stream for debug messages. Use `nullptr` to indicate `std::cout` should be used |
| `gaspi_size_t` | `freeMemory` | The minimum amount of memory guaranteed to be left unallocated for client processes, in bytes; default is 1 MB |
| [`SizeDeterminer`](#sd) | `det_amount` | A `SizeDeterminer` for the amount of tables. Will only be called if `table_amount` is `0` |
| `void*` | `data_amount` | A pointer passed to `det_amount` when it is called |
| [`SizeDeterminer`](#sd) | `det_tablesize` | A `SizeDeterminer` for the amount of rows in a table. Will only be called if `table_size` is `0` |
| `void*` | `data_tablesize` | A pointer passed to `det_tablesize` when it is called |
| [`SizeDeterminer`](#sd) | `det_rowsize` | A `SizeDeterminer` for the size of a row, in bytes. Will only be called if `row_size` is `0` |
| `void*` | `data_rowsize` | A pointer passed to `det_rowsize` when it is called |

Returns:
- `GASPI_SUCCESS` on success
- `GASPI_ERROR` on unknown error thrown by GASPI (or another error code)
- `GASPI_TIMEOUT` on timeout
- `GASPI_ERR_INV_RANK` if GASPI failed to obtain the amount of ranks (returned 0), or if MPI is supported, if it assigned a different rank or determined a different amount of ranks from GASPI (can also be thrown by GASPI for other reasons)
- `GASPI_ERR_INV_NUM` if a "size" was `0` and its corresponding `SizeDeterminer` was a `nullptr` (can also be thrown by GASPI for other reasons)
- `GASPI_ERR_NULLPTR` if the OutputCreator failed to set a value for `LazyGaspiProcessInfo::out`

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

<a id="fFulfillPrefetches"></a>
#### `lazygaspi_fulfill_prefetches`

Fulfills the prefetch requests posted to the current process by other processes.

Returns:
- `GASPI_SUCCESS` on success;
- `GASPI_ERROR` on unknown error thrown by GASPI (or another error code);
- `GASPI_TIMEOUT` on timeout;
- `GASPI_ERR_NOINIT` if `lazygaspi_init` has not been called yet.

<a id="fPrefetch"></a>
#### `lazygaspi_prefetch`

Writes prefetch requests on the proper **client(s)**. The two arrays ought to have a size of `size`. For a given index `i`, `row_vec[i]` from `table_vec[i]` will be requested for prefetching.

| Type | Parameter | Explanation |
| ---- | --------- | ----------- |
| `lazygaspi_id_t*` | `row_vec` |  An array of row ID's to prefetch |
| `lazygaspi_id_t*` | `table_vec` | An array containing the table ID's of the corresponding row for each index |
| `size_t`          | `size` | The size of **both** arrays |
| `lazygaspi_slack_t` | `slack` | The amount of slack to be used when prefetching back to the requester |

For example, calling `lazygaspi_prefetch` with  `row_vec = {0, 1, 0, 3}`, `table_vec = {0, 0, 1, 1}` and `size = 4` would be valid (from table `0`, rows `0` and `1` would be prefetched; from table `1`, rows `0` and `3` would be prefetched).

Returns:
- `GASPI_SUCCESS` on success;
- `GASPI_ERROR` on unknown error thrown by GASPI (or another error code);
- `GASPI_TIMEOUT` on timeout;
- `GASPI_ERR_INV_NUM` if either `row_id` or `table_id` is not a valid ID (or thrown by GASPI for another reason);
- `GASPI_ERR_NOINIT` if `lazygaspi_clock` has not been called even once (or thrown by GASPI for another reason). 


<a id="fPrefetchAll"></a>
#### `lazygaspi_prefetch_all`
Writes prefetch requests on all **clients** for all rows.

| Type | Parameter | Explanation |
| ---- | --------- | ----------- |
| `lazygaspi_slack_t` | `slack` | The amount of slack to be used when prefetching back to the *client* |

Returns:
- `GASPI_SUCCESS` on success;
- `GASPI_ERROR` on unknown error thrown by GASPI (or another error code);
- `GASPI_TIMEOUT` on timeout;
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
- `GASPI_ERR_NULLPTR` if passed pointer was a `nullptr`;
- `GASPI_ERR_INV_NUM` if either `row_id` or `table_id` is not a valid ID (or thrown by GASPI for another reason);
- `GASPI_ERR_NOINIT` if `lazygaspi_clock` has not been called even once (or thrown by GASPI for another reason).

<a id="fWrite"></a>
#### `lazygaspi_write`

Writes the given row to the proper *client*. 

| Type | Parameter | Explanation |
| ---- | --------- | ----------- |
| `lazygaspi_id_t` | `row_id` | The ID of the row to be read |
| `lazygaspi_id_t` | `table_id` | The ID of the row's table |
| `void*` | `row` | Output parameter for the row data. Will write `LazyGaspiProcessInfo::row_size` bytes |

Returns:
- `GASPI_SUCCESS` on success;
- `GASPI_ERROR` on unknown error thrown by GASPI;
- `GASPI_TIMEOUT` on timeout;
- `GASPI_ERR_NULLPTR` if passed row pointer was a `nullptr` (or thrown by GASPI for another reason);
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
Also terminates MPI if library was compiled with MPI support.

Returns:
- `GASPI_SUCCESS` on success;
- `GASPI_ERROR` on unknown error thrown by GASPI (or another error code);
- `GASPI_TIMEOUT` on timeout;

## Locks

Row operations (`lazygaspi_read`, `lazygaspi_write` and `lazygaspi_prefetch`) can be locked. For that, configuration must be called with the `--with-lock` option.\
These locks ensure that only one write occurs at a time on a row and when reads are occurring, a write can't happen (and vice-versa).

## Safety Checks

Calls to LazyGASPI functions can be checked for their parameter validity (indices out of bounds, passed nullptr, etc...). For that, configuration must be called with the `--with-safety-checks` option. This validity must be ensured by the application, otherwise the functions will have undefined behaviour.

## Tests

The following macros are used by the provided tests. See each specific test to see which macros are actually used.

| Macro | Explanation |
| ----- | ----------- |
| <a id="macroDebugPerf"></a>`DEBUG_PERFORMANCE` | Prints time results (time for every iteration, total time, etc...) |
| <a id="macroDebugTest"></a>`DEBUG_TEST` | Prints detailed debug output. Similar to [`DEBUG_INTERNAL`](#macroDebugInternal) |

### Test 0

A goal is set when the test is run, by either using the `-g` flag or the `-2` flag. The `-g` flag sets the value of the goal and the `-2` flag sets it to 2 to the power of the option's value. For example, `-g 20` sets the goal to 20 and `-2 4` sets it to 16.\
The flags `-n`, `-k` and `-r` set: the amount of tables; rows per table; and amount of elements in each row, respectively.\
The rows elements will be `doubles`.\
\
The test assigns one table to each process (`ShardingOptions::block_size` will be the amount of rows in a table).\
Then, for each row of a table of the current process, the average of the values of all the rows with the same index from other tables (and from the current one) is added to the current value of the row.\
This is done until all of the rows reach the goal.\
The program then waits for the other processes to reach their goal.\
Used macros: [`DEBUG_PERFORMANCE`](#macroDebugPerf), [`DEBUG_TEST`](#macroDebugTest)























