#include "fiber/synchronization.h"

#include <sys/eventfd.h>
#include <fcntl.h>

using namespace MyRPC;
using namespace MyRPC::FiberSync;


Mutex::Mutex() {
    // 初始化event_fd
    m_event_fd = eventfd(0, O_NONBLOCK);
    MYRPC_SYS_ASSERT(m_event_fd > 0);

#if MYRPC_DEBUG_LEVEL >= MYRPC_DEBUG_LOCK_LEVEL
    Logger::debug("New mutex event_fd: {}", m_event_fd);
#endif
}

Mutex::~Mutex() {
    MYRPC_SYS_ASSERT(close(m_event_fd) == 0);

#if MYRPC_DEBUG_LEVEL >= MYRPC_DEBUG_LOCK_LEVEL
    Logger::debug("Mutex event_fd: {} closed", m_event_fd);
#endif
}