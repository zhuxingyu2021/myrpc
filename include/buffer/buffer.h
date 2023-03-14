#ifndef MYRPC_BUFFER_H
#define MYRPC_BUFFER_H

#include <string>
#include "net/exception.h"
#include "noncopyable.h"

namespace MyRPC {
    class ReadBuffer : public NonCopyable{
    public:
        virtual ~ReadBuffer() = default;

        /**
             * @brief 获取读指针的当前位置
             */
        int GetPos() const { return m_read_idx; }

        /**
         * @brief 获得下一个字符，并将读指针向后移动一个字符
         */
        virtual unsigned char GetChar() = 0;

        /**
         * @brief 读指针前进sz个字符
         */
        virtual void Forward(size_t sz) = 0;

        /**
         * @brief 读指针回退sz个字符
         */
        virtual void Backward(size_t sz) = 0;

        /**
         * @brief 获得下一个字符，但不移动读指针
         */
        virtual unsigned char PeekChar(){return '\0';}

        /**
        * @brief 获得之后的N个字符，但不移动读指针
        */
        virtual void PeekString(std::string& s, size_t N) = 0;

        virtual void Commit() = 0;

        unsigned char GetCharF(){
            char t = GetChar();
            while((t == ' ') || (t == '\t') || (t == '\n') || (t == '\r')){ // 去除空白字符
                t = GetChar();
            }
            return t;
        }

        unsigned char PeekCharF(){
            char t = GetChar();
            while((t == ' ') || (t == '\t') || (t == '\n') || (t == '\r')){ // 去除空白字符
                t = GetChar();
            }
            --m_read_idx;
            return t;
        }

        /**
         * @brief 获得从当前读指针到字符c之前的所有字符，并移动读指针
         */
        template<char ...c>
        void ReadUntil(std::string& s) {
            unsigned long prev_read_idx = m_read_idx;
            while (true) {
                try {
                    char t = GetChar();
                    if (((c == t) || ...)) {
                        --m_read_idx;
                        break;
                    }
                } catch (const NetException &e) {
                    // 发生异常，则恢复read指针
                    m_read_idx = prev_read_idx;
                    throw e;
                }
            }
            _read_to_str(s, prev_read_idx, m_read_idx);
        }

    protected:
        unsigned long m_read_idx = 0;

        virtual void _read_to_str(std::string &s, unsigned long begin, unsigned long end) = 0;
    };

    class WriteBuffer: public NonCopyable{
    public:
        virtual ~WriteBuffer() = default;

        /**
         * @brief 向字符缓冲区中添加字符串数据
         * @param str 要添加的字符串
         */
        virtual void Append(const std::string &str) = 0;

        /**
         * @brief 向字符缓冲区中添加字符数据
         * @param str 要添加的字符串
         */
        virtual void Append(unsigned char c) = 0;

        /**
         * @brief 写入指针回退size个字符
         */
        virtual void Backward(size_t size) = 0;

        virtual void Commit() = 0;
    };
}

#endif //MYRPC_BUFFER_H