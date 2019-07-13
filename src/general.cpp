#include "lazygaspi.h"

#define ERROR_CHECK if(r != GASPI_SUCCESS) return r

gaspi_return_t lazygaspi_get_info(LazyGaspiProcessInfo** info){
    gaspi_pointer_t ptr;
    auto r = gaspi_segment_ptr(SEGMENT_ID_INFO, &ptr);
    *info = (LazyGaspiProcessInfo*)ptr;
    return r;
}