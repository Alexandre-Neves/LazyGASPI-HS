#include "lazygaspi.h"
#include "gaspi_utils.h"

#define ERROR_CHECK if(r != GASPI_SUCCESS) return r

gaspi_return_t lazygaspi_get_info(LazyGaspiProcessInfo** info){
    gaspi_pointer_t ptr;
    auto r = gaspi_segment_ptr(SEGMENT_ID_INFO, &ptr);
    *info = (LazyGaspiProcessInfo*)ptr;
    return r;
}

gaspi_return_t lazygaspi_clock(){
    LazyGaspiProcessInfo* info;
    auto r = lazygaspi_get_info(&info); ERROR_CHECK;
    info->age++;
    return GASPI_SUCCESS;
}

gaspi_return_t lazygaspi_term(){
    auto r = gaspi_wait(0, GASPI_BLOCK); ERROR_CHECK;

    r = GASPI_BARRIER; ERROR_CHECK;

    return gaspi_proc_term(GASPI_BLOCK);
}
