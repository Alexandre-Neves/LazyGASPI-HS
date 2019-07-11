/**Utility header for GASPI. 
 * Uses code from https://github.com/ExaScience/bpmf
 */

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

#define OUTPUT &std::cout

#define DIE_ON_ERROR(function, msg)                                                                                 \
    do {                                                                                                            \
        *OUTPUT << "\n\nERROR: " << function << "[" << __FILE__ << ":" << __LINE__ << "]: " << msg << std::flush;   \
        abort();                                                                                                    \
    } while(0);

#define SUCCESS_OR_DIE(f...)            \
    do {                                \
        const gaspi_return_t r = f;     \
        if (r == GASPI_ERROR)           \
            DIE_ON_ERROR(#f, r);        \
    } while(0); 


#define ASSERT(expr, function) { if(!(expr)) DIE_ON_ERROR(function, "Failed to assert that " << #expr) }

#define GASPI_BARRIER SUCCESS_OR_DIE(gaspi_barrier(GASPI_GROUP_ALL, GASPI_BLOCK))

/** Prints a timestamp onto the stream with the format `[HH:MM:SS]` */
static std::ostream* println_timestamp(std::ostream* stream){
    const auto t = time(nullptr);
    auto time_m = localtime(&t);
    *stream << '[' << time_m->tm_hour << ':' << time_m->tm_min << ':' << time_m->tm_sec << ']';
    return stream;
}

#define println(p) { *println_timestamp(OUTPUT) << info->id << ": " << p << std::endl << std::flush; } 

/** Returns the amount of seconds since epoch. */
inline double get_time(){
    return std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now().time_since_epoch()).count(); 
}

#define CREATE_OUTFILE(name, var, pid) {std::stringstream ofname; ofname << name << pid << ".out"; var = new std::ofstream(ofname.str()); }

#define SETUP_OUTPUT(name, var, pid) std::ostream* var;                                                                 \
                                    { std::stringstream s; s << "rm -f " << name << "*.out"; system(s.str().c_str()); } \
                                    GASPI_BARRIER;                                                                      \
                                    CREATE_OUTFILE(name, var, pid)

static int gaspi_free(int k = 0) {
    gaspi_number_t queue_size, queue_max;
    gaspi_queue_size_max(&queue_max); 
    gaspi_queue_size(k, &queue_size); 
    ASSERT(queue_size <= queue_max, "gaspi_free");
    return (queue_max - queue_size);
}

static int gaspi_wait_for_queue(int cap = 1, int k = 0) {
    int free = gaspi_free(k);
    while ((free = gaspi_free(k)) < cap) SUCCESS_OR_DIE(gaspi_wait(k, GASPI_BLOCK));
    return free;
}

#define CREATE_SEG(id, size) SUCCESS_OR_DIE(gaspi_segment_create(id, size, GASPI_GROUP_ALL, GASPI_BLOCK, GASPI_MEM_UNINITIALIZED))

#define POINTER(var, type, id) SUCCESS_OR_DIE(gaspi_segment_ptr(id, &ptr)); auto var = (type*)ptr

#define READ_OFFSETS(from, to, size, offset_from, offset_to, rank) { int free = gaspi_wait_for_queue();                                      \
                                                    SUCCESS_OR_DIE(gaspi_read(to, offset_to, rank, from, offset_from, size, 0, GASPI_BLOCK));\
                                                    assert((--free) == gaspi_free());                                                        \
                                                    if (free == 0) gaspi_wait_for_queue(); }

#define READ(from, to, size, offset, rank) READ_OFFSETS(from, to, size, offset, offset, rank)

#define READCOPY(seg, size, offset, rank) READ(seg, seg, size, offset, rank)

#define READWAIT_OFFSETS(from, to, size, offset_from, offset_to, rank) READ_OFFSETS(from, to, size, offset_from, offset_to, rank);\
                                                                        SUCCESS_OR_DIE(gaspi_wait(0, GASPI_BLOCK))

#define READWAIT(from, to, size, offset, rank) READWAIT_OFFSETS(from, to, size, offset, offset, rank)

#define WRITE_OFFSETS(from, to, size, offset_from, offset_to, rank) { int free = gaspi_wait_for_queue();                              \
                                            SUCCESS_OR_DIE(gaspi_write(from, offset_from, rank, to, offset_to, size, 0, GASPI_BLOCK));\
                                            assert((--free) == gaspi_free());                                                         \
                                            if (free == 0) gaspi_wait_for_queue(); }

#define WRITE(from, to, size, offset, rank) WRITE_OFFSETS(from, to, size, offset, offset, rank)

#define WRITECOPY(seg, size, offset, rank) WRITE(seg, seg, size, offset, rank)

#define NOTIF(seg, rank, id, val) { int free = gaspi_wait_for_queue();                                  \
                                    SUCCESS_OR_DIE(gaspi_notify(seg, rank, id, val, 0, GASPI_BLOCK));   \
                                    assert((--free) == gaspi_free());                                   \
                                    if(free == 0) gaspi_wait_for_queue();}

#define WRITE_NOTIF_OFFSETS(from, to, size, offset_from, offset_to, rank, id, val)                                                           \
                                    { int free = gaspi_wait_for_queue(2);                                                                    \
                                        SUCCESS_OR_DIE(gaspi_write_notify(from, offset_from, rank, to, offset_to, size, id, val, 0, GASPI_BLOCK));   \
                                        free -= 2; assert(free == gaspi_free());                                                             \
                                        if(free == 0) gaspi_wait_for_queue(2); }

#define WRITE_NOTIF(from, to, size, offset, rank, id, val) WRITE_NOTIF_OFFSETS(from, to, size, offset, offset, rank, id, val)

#define QUICK_TERM if(outstream != &std::cout) delete outstream; SUCCESS_OR_DIE(gaspi_proc_term(GASPI_BLOCK))

/** Creates a segment and returns a pointer to it.
 * 
 * Parameters:
 * seg - The segment's ID.
 * size - The size of the segment.
 * 
 * Returns:
 * A pointer to the segment.
 */
static gaspi_pointer_t gaspi_malloc(gaspi_segment_id_t seg, gaspi_size_t size) {
    SUCCESS_OR_DIE(gaspi_segment_create(seg, size, GASPI_GROUP_ALL, 
                                  GASPI_BLOCK, GASPI_MEM_UNINITIALIZED));
    gaspi_pointer_t ptr;
    SUCCESS_OR_DIE(gaspi_segment_ptr(seg, &ptr));
    return ptr;
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
    auto r = gaspi_segment_alloc(seg, size, policy);
    if(r != GASPI_SUCCESS) return r;
    gaspi_rank_t n; 
    r = gaspi_proc_num(&n);
    for(int i = 0; r == GASPI_SUCCESS && i < n; i++) {
        r = gaspi_segment_register(seg, i, GASPI_BLOCK);
    }
    return r;
}

/**Allocates a segment and returns its pointer without hitting a barrier.
 * 
 * Parameters:
 * seg  - The ID of the new segment.
 * size - The size of the new segment.
 * 
 * Returns:
 * A pointer to the new segment. 
 */
static gaspi_pointer_t gaspi_malloc_noblock(gaspi_segment_id_t seg, gaspi_size_t size){
    SUCCESS_OR_DIE(gaspi_segment_create_noblock(seg, size));
    gaspi_pointer_t ptr;
    SUCCESS_OR_DIE(gaspi_segment_ptr(seg, &ptr));
    return ptr;
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

/**Allocates as much memory as possible for a segment.
 * 
 * Parameters:
 * seg    - The ID of the new segment.
 * size   - The desired/maximum size of the new segment.
 * red    - A SizeReductor function, used to reduce the size that will be allocated, in case the previous size failed.
 * margin - The minimum amount of memory guaranteed to be left unallocated after segment is allocated.
 * policy - The memory allocation policy.
 * 
 * Returns:
 * A pointer to the newly allocated segment.
 */
static gaspi_pointer_t gaspi_malloc_amap(gaspi_segment_id_t seg, gaspi_size_t size, SizeReductor red, gaspi_size_t margin, 
                                        gaspi_size_t* allocated, void* data = nullptr, gaspi_alloc_policy_flags policy = GASPI_MEM_UNINITIALIZED){
    auto r = gaspi_segment_create_noblock(seg, size, policy);
    while(r == GASPI_ERR_MEMALLOC || r == GASPI_ERROR) {
        size = red(size, data);
        if(size == 0){
            DIE_ON_ERROR("gaspi_malloc_amap", "Process did not have any memory available for allocation.");
        }
        r = gaspi_segment_create_noblock(seg, size, policy);
        if(margin != 0 && r == GASPI_SUCCESS){
            r = gaspi_segment_create_noblock(SEGMENT_ID_TEST, margin);
            if(r == GASPI_SUCCESS) SUCCESS_OR_DIE(gaspi_segment_delete(SEGMENT_ID_TEST));
        }
    }
    *allocated = size;
    gaspi_pointer_t ptr;
    SUCCESS_OR_DIE(gaspi_segment_ptr(seg, &ptr));
    return ptr;
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
 * timeout - The timeout to use for the wait.
 */
static Notification get_notification(gaspi_segment_id_t seg, gaspi_notification_id_t begin, gaspi_number_t range, 
                                     gaspi_timeout_t timeout = GASPI_BLOCK){
    Notification n;
    auto r = gaspi_notify_waitsome(seg, begin, range, &n.first, timeout);
    switch(r){
    case GASPI_TIMEOUT:
        n.first = n.val = 0; break;
    case GASPI_SUCCESS:
        gaspi_notify_reset(seg, n.first, &n.val); break;
    default:
        DIE_ON_ERROR("get_notification", r);
    }        
    return n;  
}

/**Returns a pointer to a local segment.
 * Parameters:
 * id              - The segment ID.
 * notFoundMessage - A C string containing a `not found` message. Use nullptr for default message.
 */
static gaspi_pointer_t lazygaspi_get_pointer(gaspi_segment_id_t id, const char* notFoundMessage = nullptr){
    gaspi_pointer_t ptr;
    auto r = gaspi_segment_ptr(id, &ptr);
    if(r != GASPI_SUCCESS){
        if(r == GASPI_ERR_INV_SEG){
            if(notFoundMessage) {   DIE_ON_ERROR("gaspi_get_pointer", notFoundMessage); }
            else                    DIE_ON_ERROR("gaspi_get_pointer", "Segment ID " << id << " was not found.");
        } else                      DIE_ON_ERROR("gaspi_get_pointer", r);
    }
    return ptr;
};

#endif