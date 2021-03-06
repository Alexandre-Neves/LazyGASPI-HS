#include "lazygaspi_hs.h"
#include "gaspi_utils.h"
#include "utils.h"

gaspi_return_t lazygaspi_get_info(LazyGaspiProcessInfo** info){
    #ifdef SAFETY_CHECKS
    if(info == nullptr){
        PRINT_ON_ERROR_COUT("Tried to get info segment with nullptr.");
        return GASPI_ERR_NULLPTR;
    }
    #endif
    return gaspi_segment_ptr(LAZYGASPI_ID_INFO, (gaspi_pointer_t*)info);
}

gaspi_return_t lazygaspi_set_max_threads(unsigned int max_threads){
    LazyGaspiProcessInfo* info;
    auto r = lazygaspi_get_info(&info); ERROR_CHECK_COUT;
    #ifdef SAFETY_CHECKS
    if(max_threads == 0){
        PRINT_ON_ERROR("Tried to set maximum number of threads to 0.");
        return GASPI_ERR_INV_NUM;
    }
    #endif
    info->max_threads = max_threads;
    if(!is_atomic_size_enough(info)){
        PRINT_ON_ERROR("Amount of total possible threads is too high to prevent overflow on the read lock."
                       "\nReduce amount of ranks, or maximum threads per rank, or disable LOCKED_OPERATIONS.");
        return GASPI_ERR_INV_RANK;
    }
    return GASPI_SUCCESS;
}

gaspi_return_t lazygaspi_clock(){
    LazyGaspiProcessInfo* info;
    auto r = lazygaspi_get_info(&info); ERROR_CHECK_COUT;
    info->age++;
    PRINT_DEBUG_INTERNAL("Increased age to " << info->age);
    return GASPI_SUCCESS;
}

gaspi_return_t lazygaspi_term(){
    LazyGaspiProcessInfo* info;
    auto r = lazygaspi_get_info(&info); ERROR_CHECK_COUT;

    PRINT_DEBUG_INTERNAL("Started to terminate LazyGASPI for current process. Waiting for outstanding requests in queue 0...");

    r = gaspi_wait(0, GASPI_BLOCK); ERROR_CHECK;
    r = GASPI_BARRIER;              ERROR_CHECK;

    PRINT_DEBUG_INTERNAL("Terminating...\n\n");

    if(info->out && info->out != &std::cout) delete info->out;
    
    #ifdef WITH_MPI
    r = gaspi_proc_term(GASPI_BLOCK); ERROR_CHECK_COUT;
    auto ret = MPI_Finalize(); ERROR_MPI_CHECK_COUT("Failed to finalize.");
    return GASPI_SUCCESS;
    #else
    return gaspi_proc_term(GASPI_BLOCK);
    #endif
}
