#ifdef WITH_MPI
#include <mpi.h>
#endif

#include "lazygaspi_hs.h"
#include "gaspi_utils.h"
#include "utils.h"

/* Allocates: rows; cache. Sets n and id for info. Hits barrier for all. */
gaspi_return_t allocate_segments(LazyGaspiProcessInfo* info);

gaspi_return_t lazygaspi_init(lazygaspi_id_t table_amount, lazygaspi_id_t table_size, gaspi_size_t row_size, 
                              ShardingOptions shard_options, CachingOptions cache_options, OutputCreator outputCreator,
                              SizeDeterminer det_amount, void* data_amount, SizeDeterminer det_tablesize, void* data_tablesize, 
                              SizeDeterminer det_rowsize, void* data_rowsize){

    #ifdef WITH_MPI
    PRINT_DEBUG_INTERNAL_COUT("Initializing MPI...");
    int mpi_rank, mpi_rank_amount;
    {
        int provided; 
        auto ret = MPI_Init_thread(0, 0, MPI_THREAD_SERIALIZED, &provided); ERROR_MPI_CHECK_COUT("Failed to init");
        if(provided != MPI_THREAD_SERIALIZED) ERROR_MPI_CHECK_COUT("Tried to initialize with " << (int)MPI_THREAD_SERIALIZED 
                                                                  << " but " << provided << " was provided.");
        ret = MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank); ERROR_MPI_CHECK_COUT("Failed to get rank");
        ret = MPI_Comm_size(MPI_COMM_WORLD, &mpi_rank_amount); ERROR_MPI_CHECK_COUT("Failed to get rank amount");
        PRINT_DEBUG_INTERNAL_COUT("MPI determined this to be rank " << mpi_rank << ", with " << mpi_rank_amount << " total.");
        ret = MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN); ERROR_MPI_CHECK_COUT("Failed to set error handler.");
    }
    #endif

    PRINT_DEBUG_INTERNAL_COUT("Initializing LazyGASPI...");

    auto r = gaspi_proc_init(GASPI_BLOCK); ERROR_CHECK_COUT;

    LazyGaspiProcessInfo* info;
    r = gaspi_malloc_noblock(LAZYGASPI_ID_INFO, sizeof(LazyGaspiProcessInfo), &info, GASPI_MEM_INITIALIZED); 
    ERROR_CHECK_COUT;

    r = gaspi_proc_num(&(info->n)); ERROR_CHECK_COUT;
    r = gaspi_proc_rank(&(info->id)); ERROR_CHECK_COUT;

    #ifdef WITH_MPI
    if(mpi_rank != info->id) {
        PRINT_ON_ERROR_COUT("MPI and GASPI ranks did not match!");
        return GASPI_ERR_INV_RANK;
    }
    if(mpi_rank_amount != info->n){
        PRINT_ON_ERROR_COUT("MPI and GASPI rank amounts did not match!");
        return GASPI_ERR_INV_RANK;
    }
    #endif

    if(info->n == 0) return GASPI_ERR_INV_RANK;
    if(outputCreator){
        outputCreator(info);
        if(info->out == nullptr){
            PRINT_ON_ERROR_COUT("Output stream was nullptr (not created).");
            return GASPI_ERR_NULLPTR;
        }
    }
    else info->out = &std::cout;
    
    PRINT_DEBUG_INTERNAL("Allocated info segment for rank " << info->id << ". Total amount of ranks: " << info->n);
    if(table_amount == 0) { 
        if(!det_amount) return GASPI_ERR_INV_NUM;
        if(!(table_amount = det_amount(info->id, info->n, data_amount))) return GASPI_ERR_INV_NUM;
    }
    if(table_size == 0) { 
        if(!det_tablesize) return GASPI_ERR_INV_NUM;
        if(!(table_size = det_tablesize(info->id, info->n, data_tablesize))) return GASPI_ERR_INV_NUM;
    }
    if(row_size == 0) { 
        if(!det_rowsize) return GASPI_ERR_INV_NUM;
        if(!(row_size = det_rowsize(info->id, info->n, data_rowsize))) return GASPI_ERR_INV_NUM;
    }

    if(shard_options.block_size == 0) shard_options.block_size = table_size;
    if(cache_options.hash == nullptr || cache_options.size == 0) 
        cache_options = CachingOptions(LAZYGASPI_HS_HASH_ROW, table_size);

    PRINT_DEBUG_INTERNAL("Table amount: " << table_amount << " | Table size: " << table_size << " | Row size: " << row_size);

    info->shardOpts = shard_options;
    info->cacheOpts = cache_options;
    info->row_size = row_size;
    info->table_amount = table_amount;
    info->table_size = table_size;
    info->offset_slack = true;

    r = allocate_segments(info); ERROR_CHECK;

    return GASPI_SUCCESS;
}

gaspi_return_t allocate_segments(LazyGaspiProcessInfo* info){
    auto row_amount = get_row_amount(info->table_size, info->table_amount, info->n, info->id, info->shardOpts);
    auto rows_table_size = ROW_SIZE_IN_TABLE * row_amount;
    auto cache_size = info->cacheOpts.size * ROW_SIZE_IN_CACHE;

    PRINT_DEBUG_INTERNAL("Allocating cache with " << cache_size << " bytes (" << info->cacheOpts.size << " entries) and rows with "
                         << rows_table_size << " bytes (" << row_amount << " entries)... Sharding options block size was "
                         << info->shardOpts.block_size);

    //An entry for this segment is a metadata tag, the row itself, and the pending prefetch requests (n slots).
    gaspi_return_t r;
    if(row_amount){
        r = gaspi_segment_create_noblock(LAZYGASPI_ID_ROWS, rows_table_size, GASPI_MEM_INITIALIZED);
        ERROR_CHECK;
    }

    //An entry for this segment is a metadata tag and the row itself.
    r = gaspi_segment_create_noblock(LAZYGASPI_ID_CACHE, cache_size, GASPI_MEM_INITIALIZED);
    ERROR_CHECK;

    return GASPI_BARRIER;
}