#ifndef MYRPC_FIBER_SYNC_H
#define MYRPC_FIBER_SYNC_H

#include <atomic>
#include <unistd.h>
#include <set>
#include <queue>

#include "noncopyable.h"
#include "debug.h"
#include "spinlock.h"

#include "debug.h"

#include "fiber/fiber.h"

#include "fiber/fiber_pool.h"

namespace MyRPC{
    namespace FiberSync {
        /**
         * @brief 协程级互斥锁
         */
        class Mutex : public NonCopyable {
        public:
            Mutex();
            ~Mutex();

            void lock();

            void unlock();

            bool tryLock() {
                return !m_lock.test_and_set(std::memory_order_acquire);
            }

#if MYRPC_DEBUG_LEVEL >= MYRPC_DEBUG_LOCK_LEVEL
            int64_t _debug_lock_owner = -10; // 锁持有者的Fiber id
#endif
            // 清空等待队列，但需要确保等待队列中的所有协程都处于停止的状态
            void Clear(){
                while(!m_wait_queue.empty()){
                    auto [fb, tid] = m_wait_queue.front();
                    MYRPC_ASSERT(fb->GetStatus() == Fiber::TERMINAL);
                    m_wait_queue.pop();
                }
            }

        private:
            std::atomic_flag m_lock = ATOMIC_FLAG_INIT;

            int m_mutex_id;
            std::queue<std::pair<Fiber::ptr, int>> m_wait_queue; // Fiber ThreadID
            SpinLock m_wait_queue_lock;
        };

        /*
         * @brief 协程级读写锁
         */
        class RWMutex{
        public:
            void lock() {
                write_lock.lock();
            }
            void unlock() {
                write_lock.unlock();
            }

            void lock_shared(){
                //read_lock.lock();
                while(!read_lock.tryLock()){
                    if(reader_blocked){ // 如果存在写操作，则阻塞直到获得读锁
                        read_lock.lock();
                        break;
                    } // 如果不存在写操作，则以自旋锁的方式获得读锁
                }
                if(++m_reader == 1){
                    // 存在线程读操作时，写加锁
                    if(!write_lock.tryLock()) {
                        reader_blocked = true;
                        write_lock.lock();
                        reader_blocked = false;
                    }
                }
                read_lock.unlock();
            }

            void unlock_shared(){
                //read_lock.lock();
                while(!read_lock.tryLock()); // 以自旋锁的方式获得读锁
                if(--m_reader == 0){
                    write_lock.unlock(); // 没有线程读操作时，释放写锁
                }
                read_lock.unlock();
            }

            void Clear(){
                write_lock.Clear();
                read_lock.Clear();
            }
        private:
            Mutex write_lock;
            Mutex read_lock;

            int m_reader = 0;

            std::atomic<bool> reader_blocked = {false};
        };

        class ConditionVariable : public NonCopyable{
        public:
            ~ConditionVariable();

            void wait(Mutex& mutex);

            void notify_one();

            void notify_all();

            // 清空等待队列，但需要确保等待队列中的所有协程都处于停止的状态
            void Clear(){
                // 清空等待队列，但需要确保等待队列中的所有协程都处于停止的状态
                while(!m_wait_queue.empty()){
                    auto [fb, tid] = m_wait_queue.front();
                    MYRPC_ASSERT(fb->GetStatus() == Fiber::TERMINAL);
                    m_wait_queue.pop();
                }
            }

        private:
            std::atomic<bool> m_notify_all = {false};

            std::queue<std::pair<Fiber::ptr, int>> m_wait_queue; // Fiber ThreadID
            SpinLock m_wait_queue_lock;
        };
    }
}

#endif //MYRPC_FIBER_SYNC_H
