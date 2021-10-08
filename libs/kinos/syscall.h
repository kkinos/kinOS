#ifdef __cplusplus
#include <cstddef>
#include <cstdint>

extern "C" {
#else
#include <stddef.h>
#include <stdint.h>
#endif

#include "logger.hpp"

struct SyscallResult {
    uint64_t value;
    int error;
};

struct SyscallResult SyscallLogString(enum LogLevel level, const char *message);
struct SyscallResult SyscallPutString(int fd, const char *s, size_t len);
void SyscallExit(int exit_code);
struct SyscallResult SyscallGetCurrentTick();
#define TIMER_ONESHOT_REL 1
#define TIMER_ONESHOT_ABS 0
struct SyscallResult SyscallCreateTimer(unsigned int type, int timer_value,
                                        unsigned long timeout_ms);

struct SyscallResult SyscallOpenFile(const char *path, int flags);
struct SyscallResult SyscallReadFile(int fd, void *buf, size_t count);

struct SyscallResult SyscallDemandPages(size_t num_pages, int flags);
struct SyscallResult SyscallMapFile(int fd, size_t *file_size, int flags);

struct SyscallResult SyscallNewTask();

struct SyscallResult SyscallCopyToTaskBuffer(uint64_t id, void *buf,
                                             size_t offset, size_t len);
struct SyscallResult SyscallSetArgument(uint64_t id, char *buf);
/*--------------------------------------------------------------------------
 * プロセス間通信用システムコール
 *--------------------------------------------------------------------------
 */

/**
 * @brief オープン受信 どこからのメッセージも受け取る
 *
 * @param msg
 * @param len
 * @return struct SyscallResult
 */
struct SyscallResult SyscallOpenReceiveMessage(struct Message *msg, size_t len);

/**
 * @brief クローズド受信 指定されたタスク以外のメッセージにはType Errorを返す
 *
 * @param msg
 * @param len
 * @param target_task_id
 * @return struct SyscallResult
 */
struct SyscallResult SyscallClosedReceiveMessage(struct Message *msg,
                                                 size_t len,
                                                 uint64_t target_id);
struct SyscallResult SyscallSendMessage(struct Message *msg, uint64_t id);

/*--------------------------------------------------------------------------
 * GUIサーバ用システムコール
 *--------------------------------------------------------------------------
 */
struct SyscallResult SyscallWritePixel(int x, int y, int r, int g, int b);
struct SyscallResult SyscallFrameBufferWidth();
struct SyscallResult SyscallFrameBufferHeight();
struct SyscallResult SyscallCopyToFrameBuffer(const uint8_t *src_buf,
                                              int start_x, int start_y,
                                              int bytes_per_copy_line);

/*--------------------------------------------------------------------------
 * ファイルシステムサーバ用システムコール
 *--------------------------------------------------------------------------
 */

/**
 * @brief
 * ブートローダによってボリュームされたイメージを指定されたものにコピーする
 *
 * @param buf コピーさせたいもののアドレス
 * @param offset 512バイトを単位とする
 * @param len 512バイトを単位とする
 * @return struct SyscallResult
 */
struct SyscallResult SyscallReadVolumeImage(void *buf, size_t offset,
                                            size_t len);

/*--------------------------------------------------------------------------
 * すべてのサーバ用システムコール
 *--------------------------------------------------------------------------
 */

/**
 * @brief command_lineで指定された名前のサーバを探す
 *
 * @param command_line
 * @return struct SyscallResult
 */
struct SyscallResult SyscallFindServer(const char *command_line);

/**
 * @brief kernel logを読み込む
 *
 * @param buf
 * @param len
 * @return struct SyscallResult
 */
struct SyscallResult SyscallReadKernelLog(char *buf, size_t len);

/**
 * @brief kernel logに書き込む
 *
 * @param buf
 * @return struct SyscallResult
 */
struct SyscallResult SyscallWriteKernelLog(char *buf);

#ifdef __cplusplus
}  // extern "C"
#endif