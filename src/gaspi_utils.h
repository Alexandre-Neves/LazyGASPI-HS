#ifndef __H_GASPI_UTILS
#define __H_GASPI_UTILS

#include <GASPI.h>
#include <iostream>
#include <ctime>
#include <thread>
#include <sstream>
#include <cstdlib>
#include <fstream>
#include <cassert>

#include "lazygaspi_hs.h"

#define DIE_ON_ERROR_OUT(function, msg, out)                                                                    \
    do {                                                                                                        \
        out << "ERROR: " << function << "[" << __FILE__ << ":" << __LINE__ << "]: " << msg << std::endl;        \
        abort();                                                                                                \
    } while(0);

#define DIE_ON_ERROR(function, msg) DIE_ON_ERROR_OUT(function, msg, *info->out)

#define SUCCESS_OR_DIE(f...)                        \
    do {                                            \
        const gaspi_return_t r = f;                 \
        if (r != GASPI_SUCCESS)                     \
            DIE_ON_ERROR_OUT(#f, r, *info->out);    \
    } while(0); 
    
#define SUCCESS_OR_DIE_COUT(f...)                   \
    do {                                            \
        const gaspi_return_t r = f;                 \
        if (r != GASPI_SUCCESS)                     \
            DIE_ON_ERROR_OUT(#f, r, std::cout);     \
    } while(0); 

#define ASSERT_OUT(out, expr, function) { if(!(expr)) DIE_ON_ERROR_OUT(function, "Failed to assert that " << #expr, out) }

#define ASSERT(expr, function) ASSERT_OUT(*info->out, expr, function)

#if defined DEBUG || defined DEBUG_INTERNAL
#define PRINT_ON_ERROR(msg) { *info->out << "Error [" << __FILE__ << ':' << __LINE__ << "] " << msg << std::endl; }
#define PRINT_ON_ERROR_COUT(msg) { std::cout << "Error [" << __FILE__ << ':' << __LINE__ << "] " << msg << std::endl; }
#else
#define PRINT_ON_ERROR(msg)
#define PRINT_ON_ERROR_COUT(msg) 
#endif

#define ERROR_CHECK_COUT { if(r != GASPI_SUCCESS) { PRINT_ON_ERROR_COUT(r); return r; }}
#define ERROR_CHECK {if(r != GASPI_SUCCESS) { PRINT_ON_ERROR(r); return r; }}

#define GASPI_BARRIER gaspi_barrier(GASPI_GROUP_ALL, GASPI_BLOCK)

/** Prints a timestamp into the stream with the format `[HH:MM:SS]`. */
static std::ostream& timestamp(std::ostream& stream){
    const auto t = time(nullptr);
    auto time_m = localtime(&t);
    return stream << '[' << time_m->tm_hour << ':' << time_m->tm_min << ':' << time_m->tm_sec << "] ";
}

/** Returns the amount of seconds since epoch. */
inline double get_time(){
    return std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now().
                                                                     time_since_epoch()).count(); 
}

/** Sets up the default file output stream for print messages.
 *  Hits a barrier of the provided group. Default is all ranks.
 * 
 *  Parameters:
 *  identifier - A C string containing the identifier of the program. Does not need to be unique to each rank.
 *  id         - The rank of the current process.
 *  stream     - Output parameter for the newly created output stream.
 * 
 *  Returns:
 *  GASPI_SUCCESS on success, GASPI_ERROR (or other error codes) on error, or GASPI_TIMEOUT on timeout.
 *  GASPI_ERR_MEMALLOC if stream could not be allocated.
 */
static gaspi_return_t gaspi_setup_output(const char* identifier, gaspi_rank_t id, std::ostream** stream){
    auto s = std::stringstream();
    s << identifier << '_' << id << ".out"; 
    *stream = new std::ofstream(s.str());
    if(*stream == nullptr){
        PRINT_ON_ERROR_COUT("Failed to allocate output stream");
        return GASPI_ERR_MEMALLOC;
    }

    return GASPI_SUCCESS;
}

/** Returns how many requests can be posted to a given queue.
 * 
 *  Parameters:
 *  q    - The queue.
 *  free - Output parameters for the amount of requests that can be posted.
 * 
 *  Returns:
 *  GASPI_SUCCESS on success, GASPI_ERROR (or other error codes) on error, or GASPI_TIMEOUT on timeout.
 */
static gaspi_return_t gaspi_free(gaspi_queue_id_t q, int* free) {
    gaspi_number_t queue_size, queue_max;

    auto r = gaspi_queue_size_max(&queue_max); 
    ERROR_CHECK_COUT;

    r = gaspi_queue_size(q, &queue_size);
    ERROR_CHECK_COUT;

    if(queue_size > queue_max){
        PRINT_ON_ERROR_COUT("Queue " << q << " overflow");
        return GASPI_QUEUE_FULL;
    }
    *free = queue_max - queue_size;
    return GASPI_SUCCESS;
}
/** Waits for a queue to have enough room for new requests.
 * 
 *  Parameters:
 *  q    - The queue to wait for.
 *  min  - The minimum amount of requests that can be posted to the queue after returning.
 *  free - Output parameter for the amount of requests that can actually be posted. Use nullptr to ignore
 *  
 *  Returns:
 *  GASPI_SUCCESS on success, GASPI_ERROR (or other error codes) on error, or GASPI_TIMEOUT on timeout.
 */
static gaspi_return_t gaspi_wait_for_queue(gaspi_queue_id_t q, int min, int* free = nullptr) {
    int f;
    auto r = gaspi_free(q, &f);
    for (; f < min && r == GASPI_SUCCESS; r = gaspi_free(q, &f)) {
        r = gaspi_wait(q, GASPI_BLOCK);
        if(r != GASPI_SUCCESS) break;
    }
    ERROR_CHECK_COUT;
    if(free) *free = f;
    return GASPI_SUCCESS;
}

/** Creates a segment and returns a pointer to it.
 * 
 * Parameters:
 * seg - The segment's ID.
 * size - The size of the segment.
 * 
 * Returns:
 * A pointer to the segment.
 */
template<typename T>
static gaspi_return_t gaspi_malloc(gaspi_segment_id_t seg, gaspi_size_t size, T* ptr) {
    if(size == 0){
        PRINT_ON_ERROR_COUT("Tried to allocate segment of size 0");
        return GASPI_ERR_INV_SEGSIZE;
    }
    auto r = gaspi_segment_create(seg, size, GASPI_GROUP_ALL, GASPI_BLOCK, GASPI_MEM_UNINITIALIZED); ERROR_CHECK_COUT;
         r = gaspi_segment_ptr(seg, (gaspi_pointer_t*)ptr);                                          ERROR_CHECK_COUT;

    return GASPI_SUCCESS;
}

/**Allocates a new segment and registers it with all other ranks. Does not hit a barrier, unlike 
 * gaspi_segment_create.
 * 
 * Parameters:
 * seg   - The segment's ID.
 * size  - The size of the segment.
 * alloc - The memory allocation policy.
 * 
 * Returns:
 * GASPI_SUCCESS on success, GASPI_ERROR (or other error codes) on error, or GASPI_TIMEOUT on timeout.
 */
static gaspi_return_t gaspi_segment_create_noblock(gaspi_segment_id_t seg, gaspi_size_t size, 
                                                   gaspi_alloc_policy_flags policy = GASPI_MEM_UNINITIALIZED){
    if(size == 0){
        PRINT_ON_ERROR_COUT("Tried to create segment of size 0");
        return GASPI_ERR_INV_SEGSIZE;
    }
    auto r = gaspi_segment_alloc(seg, size, policy);
    ERROR_CHECK_COUT;

    gaspi_rank_t n; 
    r = gaspi_proc_num(&n);
    for(int i = 0; r == GASPI_SUCCESS && i < n; i++) {
        r = gaspi_segment_register(seg, i, GASPI_BLOCK);
    }
    ERROR_CHECK_COUT;
    return GASPI_SUCCESS;
}

/**Allocates a segment and returns its pointer without hitting a barrier.
 * 
 * Parameters:
 * seg  - The ID of the new segment.
 * size - The size of the new segment.
 * ptr  - Output parameter for the pointer.
 * 
 * Returns:
 * GASPI_SUCCESS on success, GASPI_ERROR (or another error code) on error, GASPI_TIMEOUT on timeout. 
 */
template<typename T>
static gaspi_return_t gaspi_malloc_noblock(gaspi_segment_id_t seg, gaspi_size_t size, T* ptr,
                                           gaspi_alloc_policy_flags policy = GASPI_MEM_UNINITIALIZED){
    auto r = gaspi_segment_create_noblock(seg, size, policy); ERROR_CHECK_COUT;
         r = gaspi_segment_ptr(seg, (gaspi_pointer_t*)ptr);   ERROR_CHECK_COUT;
    return GASPI_SUCCESS;
}
/** A SizeReductor function is meant to reduce the size of an attempted allocation.
 * 
 *  Parameters:
 *  size - The original size of the allocation.
 *  data - A pointer to data that will be used by the reductor to reduce the size.
 *  
 *  Returns:
 *  The new size (assumed to be less than `size`). Must return 0 at some point.
 */
typedef gaspi_size_t (*SizeReductor)(gaspi_size_t size, void* data);

/**Allocates as much memory as possible for a segment. Uses last available segment ID for testing.
 * 
 * Parameters:
 * seg       - The ID of the new segment.
 * size      - The desired/maximum size of the new segment.
 * red       - A SizeReductor function, used to reduce the size that will be allocated, in case the previous size failed.
 * margin    - The minimum amount of memory guaranteed to be left unallocated after segment is allocated.
 * allocated - Output parameter for the amount of bytes actually allocated (the actual size of the segment).
 * ptr       - Output parameter for the pointer to the newly created segment. Use nullptr to ignore.
 * data      - Data passed to the SizeReductor function.
 * policy    - The memory allocation policy.
 * 
 * Returns:
 * GASPI_SUCCESS on success, GASPI_ERROR (or other error codes) on error, or GASPI_TIMEOUT on timeout.
 * GASPI_ERR_MEMALLOC is returned if size was reduced to 0 by `red`.
 */
static gaspi_return_t gaspi_malloc_amap(gaspi_segment_id_t seg, gaspi_size_t size, SizeReductor red, gaspi_size_t margin, 
                                        gaspi_size_t* allocated, gaspi_pointer_t* ptr = nullptr, void* data = nullptr, 
                                        gaspi_alloc_policy_flags policy = GASPI_MEM_UNINITIALIZED){
    auto r = gaspi_segment_create_noblock(seg, size, policy);
    while(r == GASPI_ERR_MEMALLOC || r == GASPI_ERROR) {
        size = red(size, data);
        if(size == 0){
            PRINT_ON_ERROR_COUT("Tried to allocate segment but size was reduced to 0");
            return GASPI_ERR_MEMALLOC; 
        }

        r = gaspi_segment_create_noblock(seg, size, policy);
        if(margin != 0 && r == GASPI_SUCCESS){
            gaspi_number_t max;
            r = gaspi_segment_max(&max);
            ERROR_CHECK_COUT;
            r = gaspi_segment_create_noblock(max - 1, margin);
            ERROR_CHECK_COUT;
            r = gaspi_segment_delete(max - 1);
            ERROR_CHECK_COUT;
        }
    }
    ERROR_CHECK_COUT;
    *allocated = size;
    return ptr ? gaspi_segment_ptr(seg, ptr) : GASPI_SUCCESS;
}

struct Notification{
    gaspi_notification_id_t first;
    gaspi_notification_t val;
};

/**Waits for a notification and resets its value. Returns the ID and value.
 * On timeout, returns 0 for both the ID and the value.
 * Parameters:
 * seg   - The ID of the segment that will wait for the notification.
 * begin - The first notification ID to wait for.
 * range - The range of ID's to wait for. Will wait for any ID between `begin` and `begin + range - 1`.
 * notif - Output parameter for the notification.
 * timeout - The timeout to use for the wait.
 * 
 * Returns:
 * GASPI_SUCCESS on success, GASPI_ERROR (or another error code) on error, GASPI_TIMEOUT on timeout.
 */
static gaspi_return_t get_notification(gaspi_segment_id_t seg, gaspi_notification_id_t begin, gaspi_number_t range, 
                                       Notification* notif, gaspi_timeout_t timeout = GASPI_BLOCK){
    auto r = gaspi_notify_waitsome(seg, begin, range, &(notif->first), timeout);
    switch(r){
    case GASPI_TIMEOUT:
        notif->first = notif->val = 0;
        break;
    case GASPI_SUCCESS:
        return gaspi_notify_reset(seg, notif->first, &(notif->val));
    default:
        ERROR_CHECK_COUT;
    }        
    return GASPI_SUCCESS;  
}

/** Sends a notification. 
 * 
 *  Parameters:
 *  seg      - The segment that the notification will be sent to.
 *  rank      - The rank that contains the segment.
 *  id       - The notification's ID.
 *  val      - The notification's value.
 *  q        - The queue into which the notification will be posted.
 *  timeout  - The timeout used in `gaspi_notify`.
 * 
 *  Returns:
 *  GASPI_SUCCESS on success, GASPI_ERROR (or another error code) on error, GASPI_TIMEOUT on timeout.
 */
static gaspi_return_t send_notification(gaspi_segment_id_t seg, gaspi_rank_t rank, gaspi_notification_id_t id, 
                                        gaspi_notification_t val = 1, gaspi_queue_id_t q = 0, gaspi_timeout_t timeout = GASPI_BLOCK){
    int free;
    auto r = gaspi_wait_for_queue(q, 1, &free); ERROR_CHECK_COUT;
    r = gaspi_notify(seg, rank, id, val, q, timeout); ERROR_CHECK_COUT;
    return GASPI_SUCCESS;
}

/** Reads from one segment to another. Waits for the given queue to empty, guaranteeing  that the read request was fulfilled.
 * 
 *  Parameter:
 *  from         - The segment from which the data will be read.
 *  to           - The segment into which the data will be read.
 *  offset_from  - The offset of the data inside the `from` segment, in bytes.
 *  offset_to    - The offset of the data inside the `to` segment, in bytes.
 *  size         - The size of the data, in bytes.
 *  rank         - The rank of the `from` segment.
 *  timeout      - The timeout used in `gaspi_read`.
 *  q            - The queue into which the read request will be posted.
 * 
 *  Returns:
 *  GASPI_SUCCESS on success, GASPI_ERROR (or another error code) on error, GASPI_TIMEOUT on timeout.
 */
static gaspi_return_t read(gaspi_segment_id_t from, gaspi_segment_id_t to, gaspi_offset_t offset_from, gaspi_offset_t offset_to,
                           gaspi_size_t size, gaspi_rank_t rank, gaspi_timeout_t timeout = GASPI_BLOCK, gaspi_queue_id_t q = 0){
    auto r = gaspi_wait_for_queue(1, q); ERROR_CHECK_COUT;

    r = gaspi_read(to, offset_to, rank, from, offset_from, size, q, timeout); ERROR_CHECK_COUT;

    r = gaspi_wait(q, GASPI_BLOCK); ERROR_CHECK_COUT;
    return GASPI_SUCCESS;
}

/** Reads data from a segment to another of the same ID, at the same offset.
 * 
 * Parameters:
 * seg      - The segment ID where the data will be read from and into.
 * offset   - The offset of the data inside the segment, in bytes.
 * size     - The size of the data, in bytes.
 * rank     - The rank from which the data will be read.
 * timeout  - The timeout used in 'gaspi_read`.
 * q        - The queue into which the read request will be posted.
 * 
 * Returns:
 * GASPI_SUCCESS on success, GASPI_ERROR (or another error code) on error, GASPI_TIMEOUT on timeout.
 */
static gaspi_return_t readcopy(gaspi_segment_id_t seg, gaspi_offset_t offset, gaspi_size_t size, gaspi_rank_t rank, 
                               gaspi_timeout_t timeout = GASPI_BLOCK, gaspi_queue_id_t q = 0){
    return read(seg, seg, offset, offset, size, rank, timeout, q);
}

/**Writes from one segment to another. 
 * Waits for the given queue to free up if it is full.
 * Make sure data is not changed before write request is fulfilled (by using gaspi_wait).
 * 
 * Parameters:
 * from         - The segment that contains the data that will be written.
 * to           - The segment that will receive the data.
 * offset_from  - The offset of the data in the `from` segment, in bytes.
 * offset_to    - The offset of the data in the `to` segment, in bytes.
 * size         - The size of the data, in bytes.
 * rank         - The rank that contains the `to` segment.
 * timeout      - The timeout used in `gaspi_write`.
 * q            - The queue into which the write request will be posted.
 * 
 * Returns:
 * GASPI_SUCCESS on success, GASPI_ERROR (or another error code) on error, GASPI_TIMEOUT on timeout.
 */
static gaspi_return_t write(gaspi_segment_id_t from, gaspi_segment_id_t to, gaspi_offset_t offset_from, gaspi_offset_t offset_to,
                           gaspi_size_t size, gaspi_rank_t rank, gaspi_timeout_t timeout = GASPI_BLOCK, gaspi_queue_id_t q = 0){
    auto r = gaspi_wait_for_queue(q, 1); ERROR_CHECK_COUT;

    r = gaspi_write(from, offset_from, rank, to, offset_to, size, q, timeout); ERROR_CHECK_COUT;

    return GASPI_SUCCESS;
}

/**Writes from one segment to another of the same ID, at the same offset. 
 * Waits for the given queue to free up if it is full.
 * Make sure data is not changed before write request is fulfilled (by using gaspi_wait).
 * 
 * Parameters:
 * seg          - The segment ID.
 * offset       - The offset of the data in either segment, in bytes.
 * size         - The size of the data, in bytes.
 * rank         - The rank that contains the `to` segment, in bytes.
 * timeout      - The timeout used in `gaspi_write`.
 * q            - The queue into which the write request will be posted.
 * 
 * Returns:
 * GASPI_SUCCESS on success, GASPI_ERROR (or another error code) on error, GASPI_TIMEOUT on timeout.
 */
static gaspi_return_t writecopy(gaspi_segment_id_t seg, gaspi_offset_t offset, gaspi_size_t size, gaspi_rank_t rank,
                                gaspi_timeout_t timeout = GASPI_BLOCK, gaspi_queue_id_t q = 0){
    return write(seg, seg, offset, offset, size, rank, timeout, q);
}

/** Writes from a segment to another and notifies the receiving segment.
 *  Waits for the given queue to free up if it is full.
 *  Make sure data is not changed before write request is fulfilled (by using gaspi_wait).
 * 
 * Parameters:
 * from         - The segment where the data resides.
 * to           - The segment that the data will be written to.
 * offset_from  - The offset of the data in the `from` segment, in bytes.
 * offset_to    - The offset of the data in the `to` segment, in bytes.
 * size         - The size of the data, in bytes.
 * rank         - The rank that contains the `to` segment.
 * notif_id     - The ID of the notification to send.
 * notif_val    - The value of the notification to send.
 * timeout      - The timeout used in `gaspi_write_notify`.
 * q            - The queue in which to post the notification and write request.
 * 
 * Returns:
 * GASPI_SUCCESS on success, GASPI_ERROR (or another error code) on error, GASPI_TIMEOUT on timeout.
 */
static gaspi_return_t writenotify(gaspi_segment_id_t from, gaspi_segment_id_t to, gaspi_offset_t offset_from, 
                                  gaspi_offset_t offset_to, gaspi_size_t size, gaspi_rank_t rank, 
                                  gaspi_notification_id_t notif_id, gaspi_notification_t notif_val = 1,
                                  gaspi_timeout_t timeout = GASPI_BLOCK, gaspi_queue_id_t q = 0){
    auto r = gaspi_wait_for_queue(q, 2); ERROR_CHECK_COUT;

    r = gaspi_write_notify(from, offset_from, rank, to, offset_to, size, notif_id, notif_val, q, timeout);
    ERROR_CHECK_COUT;

    return GASPI_SUCCESS;
}

static gaspi_return_t writewait(gaspi_segment_id_t from, gaspi_segment_id_t to, gaspi_offset_t offset_from, gaspi_offset_t offset_to,
                           gaspi_size_t size, gaspi_rank_t rank, gaspi_timeout_t timeout = GASPI_BLOCK, gaspi_queue_id_t q = 0){
    auto r = write(from, to, offset_from, offset_to, size, rank, timeout, q);
    ERROR_CHECK_COUT;
    r = gaspi_wait(q, GASPI_BLOCK); ERROR_CHECK_COUT;
    return GASPI_SUCCESS;
}

/**Returns a pointer to a local segment. Dies on error.
 * 
 * Parameters:
 * id              - The segment ID.
 * notFoundMessage - A C string containing a `not found` message. Use nullptr for default message.
 */
static gaspi_pointer_t get_pointer(gaspi_segment_id_t id, const char* notFoundMessage = nullptr){
    gaspi_pointer_t ptr;
    auto r = gaspi_segment_ptr(id, &ptr);
    if(r != GASPI_SUCCESS){
        if(r == GASPI_ERR_INV_SEG){
            if(notFoundMessage) {   DIE_ON_ERROR_OUT("gaspi_get_pointer", notFoundMessage, std::cout); }
            else                    DIE_ON_ERROR_OUT("gaspi_get_pointer", "Segment ID " << id << " was not found.", std::cout);
        } else                      DIE_ON_ERROR_OUT("gaspi_get_pointer", r, std::cout);
    }
    return ptr;
};

#endif