#ifdef __cplusplus
#include <cstddef>
#include <cstdint>

extern "C" {
#else
#include <stddef.h>
#include <stdint.h>
#endif

#include "../../common/message.hpp"

struct SyscallResult {
    uint64_t value;
    int error;
};

void SyscallExit(int exit_code);
struct SyscallResult SyscallGetCurrentTick();
#define TIMER_ONESHOT_REL 1
#define TIMER_ONESHOT_ABS 0
struct SyscallResult SyscallCreateTimer(unsigned int type, int timer_value,
                                        unsigned long timeout_ms);

struct SyscallResult SyscallDemandPages(size_t num_pages, int flags);
/*--------------------------------------------------------------------------
 * system calls for task
 *--------------------------------------------------------------------------
 */

struct SyscallResult SyscallCreateNewTask();
struct SyscallResult SyscallCopyToTaskBuffer(uint64_t id, void *buf,
                                             size_t offset, size_t len);
struct SyscallResult SyscallSetArgument(uint64_t id, char *buf);
struct SyscallResult SyscallSetCommand(uint64_t id, char *buf);

/*--------------------------------------------------------------------------
 * system calls for ipc
 *--------------------------------------------------------------------------
 */

struct SyscallResult SyscallOpenReceiveMessage(struct Message *msg, size_t len);
struct SyscallResult SyscallClosedReceiveMessage(struct Message *msg,
                                                 size_t len,
                                                 uint64_t target_id);
struct SyscallResult SyscallSendMessage(struct Message *msg, uint64_t id);

/*--------------------------------------------------------------------------
 * system calls for GUI server
 *--------------------------------------------------------------------------
 */

struct SyscallResult SyscallWritePixel(int x, int y, int r, int g, int b);
struct SyscallResult SyscallGetFrameBufferWidth();
struct SyscallResult SyscallGetFrameBufferHeight();
struct SyscallResult SyscallCopyToFrameBuffer(const uint8_t *src_buf,
                                              int start_x, int start_y,
                                              int bytes_per_copy_line);

/*--------------------------------------------------------------------------
 * system calls for file system server
 *--------------------------------------------------------------------------
 */

struct SyscallResult SyscallReadVolumeImage(void *buf, size_t offset_by_sector,
                                            size_t len_by_sector);
struct SyscallResult SyscallCopyToVolumeImage(void *buf,
                                              size_t offset_by_sector,
                                              size_t len_by_sector);

/*--------------------------------------------------------------------------
 * common system calls for application and server
 *--------------------------------------------------------------------------
 */
struct SyscallResult SyscallFindServer(const char *name);
struct SyscallResult SyscallReadKernelLog(char *buf, size_t len);
struct SyscallResult SyscallWriteKernelLog(char *buf);

#ifdef __cplusplus
}  // extern "C"
#endif