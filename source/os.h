// os.h
//
// Copyright (c) 2021 TheLordScruffy
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "util.h"
#include <gctypes.h>
LIBOGC_SUCKS_BEGIN
#include <ogc/ipc.h>
LIBOGC_SUCKS_END
#include <cassert>

#define DASSERT assert
#define ASSERT assert

namespace IOSErr
{
enum
{
    OK = 0,
    NoAccess = -1,
    Invalid = -4,
    NotFound = -6,
};
} // namespace IOSErr

namespace ISFSError
{
enum
{
    OK = 0,
    Invalid = -101,
    NoAccess = -102,
    Corrupt = -103,
    NotReady = -104,
    Exists = -105,
    NotFound = -106,
    MaxOpen = -109,
    MaxDepth = -110,
    Locked = -111,
    Unknown = -117,
};
} // namespace ISFSError

namespace IOS
{

typedef s32 (*IPCCallback)(s32 result, void* userdata);

namespace Mode
{
enum Mode
{
    None = 0,
    Read = 1,
    Write = 2,
    RW = Read | Write,
};
} // namespace Mode

typedef struct _ioctlv Vector;

template <u32 in_count, u32 out_count>
struct IOVector {
    struct {
        const void* data;
        u32 len;
    } in[in_count];
    struct {
        void* data;
        u32 len;
    } out[out_count];
};

template <u32 in_count>
struct IVector {
    struct {
        const void* data;
        u32 len;
    } in[in_count];
};

template <u32 out_count>
struct OVector {
    struct {
        void* data;
        u32 len;
    } out[out_count];
};

class Resource
{
public:
#define IPC_TO_CALLBACK_INIT()
#define IPC_TO_CALLBACK(cb, userdata) (cb), (userdata)
#define IPC_TO_CB_CHECK_DELETE(ret)

    Resource()
    {
        this->m_fd = -1;
    }

    Resource(s32 fd)
    {
        this->m_fd = fd;
    }

    explicit Resource(const char* path, u32 mode = 0)
    {
        this->m_fd = IOS_Open(path, mode);
    }

    Resource(const Resource& from) = delete;

    Resource(Resource&& from)
    {
        this->m_fd = from.m_fd;
        from.m_fd = -1;
    }

    ~Resource()
    {
        if (this->m_fd >= 0)
            close();
    }

    s32 close()
    {
        const s32 ret = IOS_Close(this->m_fd);
        if (ret >= 0)
            this->m_fd = -1;
        return ret;
    }

    s32 read(void* data, u32 length)
    {
        return IOS_Read(this->m_fd, data, length);
    }

    s32 write(const void* data, u32 length)
    {
        return IOS_Write(this->m_fd, data, length);
    }

    s32 seek(s32 where, s32 whence)
    {
        return IOS_Seek(this->m_fd, where, whence);
    }

    s32 readAsync(void* data, u32 length, IPCCallback cb, void* userdata)
    {
        IPC_TO_CALLBACK_INIT();
        s32 ret = IOS_ReadAsync(this->m_fd, data, length,
                                IPC_TO_CALLBACK(cb, userdata));
        IPC_TO_CB_CHECK_DELETE(ret);
        return ret;
    }

    s32 writeAsync(const void* data, u32 length, IPCCallback cb, void* userdata)
    {
        IPC_TO_CALLBACK_INIT();
        s32 ret = IOS_WriteAsync(this->m_fd, data, length,
                                 IPC_TO_CALLBACK(cb, userdata));
        IPC_TO_CB_CHECK_DELETE(ret);
        return ret;
    }

    s32 seekAsync(s32 where, s32 whence, IPCCallback cb, void* userdata)
    {
        IPC_TO_CALLBACK_INIT();
        s32 ret = IOS_SeekAsync(this->m_fd, where, whence,
                                IPC_TO_CALLBACK(cb, userdata));
        IPC_TO_CB_CHECK_DELETE(ret);
        return ret;
    }

protected:
    s32 m_fd;
};

template <typename Ioctl>
class ResourceCtrl : public Resource
{
public:
    using Resource::Resource;

    s32 ioctl(Ioctl cmd, void* input, u32 inputLen, void* output, u32 outputLen)
    {
        return IOS_Ioctl(this->m_fd, static_cast<u32>(cmd), input, inputLen,
                         output, outputLen);
    }

    s32 ioctlv(Ioctl cmd, u32 inputCnt, u32 outputCnt, Vector* vec)
    {
        return IOS_Ioctlv(this->m_fd, static_cast<u32>(cmd), inputCnt,
                          outputCnt, vec);
    }

    template <u32 in_count, u32 out_count>
    s32 ioctlv(Ioctl cmd, IOVector<in_count, out_count>& vec)
    {
        return IOS_Ioctlv(this->m_fd, static_cast<u32>(cmd), in_count,
                          out_count, reinterpret_cast<Vector*>(&vec));
    }

    template <u32 in_count>
    s32 ioctlv(Ioctl cmd, IVector<in_count>& vec)
    {
        return IOS_Ioctlv(this->m_fd, static_cast<u32>(cmd), in_count, 0,
                          reinterpret_cast<Vector*>(&vec));
    }

    template <u32 out_count>
    s32 ioctlv(Ioctl cmd, OVector<out_count>& vec)
    {
        return IOS_Ioctlv(this->m_fd, static_cast<u32>(cmd), 0, out_count,
                          reinterpret_cast<Vector*>(&vec));
    }

    s32 ioctlAsync(Ioctl cmd, void* input, u32 inputLen, void* output,
                   u32 outputLen, IPCCallback cb, void* userdata)
    {
        IPC_TO_CALLBACK_INIT();
        s32 ret =
            IOS_IoctlAsync(this->m_fd, static_cast<u32>(cmd), input, inputLen,
                           output, outputLen, IPC_TO_CALLBACK(cb, userdata));
        IPC_TO_CB_CHECK_DELETE(ret);
        return ret;
    }

    s32 ioctlvAsync(Ioctl cmd, u32 inputCnt, u32 outputCnt, Vector* vec,
                    IPCCallback cb, void* userdata)
    {
        IPC_TO_CALLBACK_INIT();
        s32 ret =
            IOS_IoctlvAsync(this->m_fd, static_cast<u32>(cmd), inputCnt,
                            outputCnt, vec, IPC_TO_CALLBACK(cb, userdata));
        IPC_TO_CB_CHECK_DELETE(ret);
        return ret;
    }

    template <u32 in_count, u32 out_count>
    s32 ioctlvAsync(Ioctl cmd, IOVector<in_count, out_count>& vec,
                    IPCCallback cb, void* userdata)
    {
        IPC_TO_CALLBACK_INIT();
        s32 ret = IOS_IoctlvAsync(this->m_fd, static_cast<u32>(cmd), in_count,
                                  out_count, reinterpret_cast<Vector*>(&vec),
                                  IPC_TO_CALLBACK(cb, userdata));
        IPC_TO_CB_CHECK_DELETE(ret);
        return ret;
    }

    template <u32 in_count>
    s32 ioctlvAsync(Ioctl cmd, IVector<in_count>& vec, IPCCallback cb,
                    void* userdata)
    {
        IPC_TO_CALLBACK_INIT();
        s32 ret = IOS_IoctlvAsync(this->m_fd, static_cast<u32>(cmd), in_count,
                                  0, reinterpret_cast<Vector*>(&vec),
                                  IPC_TO_CALLBACK(cb, userdata));
        IPC_TO_CB_CHECK_DELETE(ret);
        return ret;
    }

    template <u32 out_count>
    s32 ioctlvAsync(Ioctl cmd, OVector<out_count>& vec, IPCCallback cb,
                    void* userdata)
    {
        IPC_TO_CALLBACK_INIT();
        s32 ret = IOS_IoctlvAsync(this->m_fd, static_cast<u32>(cmd), 0,
                                  out_count, reinterpret_cast<Vector*>(&vec),
                                  IPC_TO_CALLBACK(cb, userdata));
        IPC_TO_CB_CHECK_DELETE(ret);
        return ret;
    }

    s32 fd() const
    {
        return this->m_fd;
    }
};

/* Only one IOCTL for specific files */
enum class FileIoctl
{
    GetFileStats = 11
};

class File : public ResourceCtrl<FileIoctl>
{
public:
    struct Stat {
        u32 size;
        u32 pos;
    };

    using ResourceCtrl::ResourceCtrl;

    u32 tell()
    {
        Stat stat;
        const s32 ret = this->stats(&stat);
        ASSERT(ret == IOSErr::OK);
        return stat.pos;
    }

    u32 size()
    {
        Stat stat;
        const s32 ret = this->stats(&stat);
        ASSERT(ret == IOSErr::OK);
        return stat.size;
    }

    s32 stats(Stat* stat)
    {
        return this->ioctl(FileIoctl::GetFileStats, nullptr, 0,
                           reinterpret_cast<void*>(stat), sizeof(Stat));
    }

    s32 statsAsync(Stat* stat, IPCCallback cb, void* userdata)
    {
        return this->ioctlAsync(FileIoctl::GetFileStats, nullptr, 0,
                                reinterpret_cast<void*>(stat), sizeof(Stat), cb,
                                userdata);
    }
};

} // namespace IOS