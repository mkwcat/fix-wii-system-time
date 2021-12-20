// main.cpp
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

#include "util.h"
#include <gctypes.h>
#include <stdio.h>
#include <stdlib.h>
LIBOGC_SUCKS_BEGIN
#include <wiiuse/wpad.h>
LIBOGC_SUCKS_END
#include "json.hpp"
#include "os.h"
#include <cassert>
#include <cstring>
#include <ctime>
#include <network.h>
#include <new>
#include <ogc/conf.h>
#include <string>
#include <unistd.h>

class SysConf
{
public:
    SysConf()
    {
        IOS::File file("/shared2/sys/SYSCONF", IOS::Mode::Read);
        assert(file.fd() >= 0);
        assert(file.size() == 0x4000);

        const s32 ret = file.read(sysconf, 0x4000);
        assert(ret == 0x4000);
    }

    void save()
    {
        IOS::File file("/shared2/sys/SYSCONF", IOS::Mode::Write);
        assert(file.fd() >= 0);

        const s32 ret = file.write(sysconf, 0x4000);
        assert(ret == 0x4000);
    }

    u16 count() const
    {
        return *((u16*)(&sysconf[4]));
    }

    u16* offset() const
    {
        return (u16*)&sysconf[6];
    }

    u8* find(const char* name)
    {
        s32 len = strlen(name);
        u16* offs = offset();
        u16 cnt = count();

        while (cnt--) {
            if (len == ((sysconf[*offs] & 0x0F) + 1) &&
                !memcmp(name, &sysconf[*offs + 1], len))
                return &sysconf[*offs];
            offs++;
        }

        return nullptr;
    }

    static s32 getLength(const char* name, const u8* elem)
    {
        switch (*elem >> 5) {
        case 1:
            return *((u16*)&elem[strlen(name) + 1]) + 1;
        case 2:
            return elem[strlen(name) + 1] + 1;
        case 3:
            return 1;
        case 4:
            return 2;
        case 5:
            return 4;
        case 7:
            return 1;
        }
        return -1;
    }

    s32 getLength(const char* name)
    {
        return getLength(name, find(name));
    }

    template <class T>
    void replace(const char* name, T value)
    {
        u8* elem = find(name);
        assert(elem);

        assert(getLength(name, elem) == sizeof(T));
        s32 len = getLength(name, elem);

        switch (*elem >> 5) {
        case CONF_BIGARRAY:
            memcpy(reinterpret_cast<void*>(&value), &elem[strlen(name) + 3],
                   len);
            break;
        case CONF_SMALLARRAY:
            memcpy(reinterpret_cast<void*>(&value), &elem[strlen(name) + 2],
                   len);
            break;
        case CONF_BYTE:
        case CONF_SHORT:
        case CONF_LONG:
        case CONF_BOOL:
            memcpy(&elem[strlen(name) + 1], reinterpret_cast<void*>(&value),
                   len);
            break;
        default:
            assert(0);
        }
    }

private:
    u8 sysconf[0x4000] ATTRIBUTE_ALIGN(32);
};

class Network
{
public:
    Network()
    {
        printf("Initializing network...\n");

        net_init();

        s32 ret;
        for (s32 i = 0; i < 8; i++) {
            ret = if_config(m_localip, m_netmask, m_gateway, 1, 20);
            if (ret >= 0)
                break;
            printf("Retrying...\n");
        }

        if (ret < 0) {
            printf("Failed to initialize network\n");
            abort();
        }
        printf("Network initialized\n");
    }

    static s32 connect(const char* address, s32 port = 80)
    {
        s32 sock = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (sock == INVALID_SOCKET) {
            printf("Failed to create a socket\n");
            abort();
        }

        struct hostent* host;
        for (s32 i = 0; i < 8; i++) {
            printf("Getting host name...\n");
            host = net_gethostbyname(address);
            if (host != nullptr)
                break;
        }
        if (host == nullptr) {
            printf("Failed to get host name\n");
            abort();
        }

        struct sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(port);
        server.sin_addr.s_addr = *(u32*)host->h_addr;

        s32 ret;
        for (s32 i = 0; i < 8; i++) {
            printf("Connecting to server...\n");
            ret = net_connect(sock, (struct sockaddr*)&server, sizeof(server));
            if (ret >= 0)
                break;
        }
        if (ret < 0) {
            printf("Failed to connect to server\n");
            abort();
        }

        return sock;
    }

    static void send(s32 sock, const void* data, s32 len)
    {
        const s32 ret = net_send(sock, data, len, 0);
        assert(ret == len);
    }

    static s32 receive(s32 sock, void* data, s32 len)
    {
        const s32 ret = net_recv(sock, data, len, 0);
        return ret;
    }

    static void close(s32 sock)
    {
        net_close(sock);
    }

private:
    char m_localip[16] = {0};
    char m_gateway[16] = {0};
    char m_netmask[16] = {0};
};

static s32 MakeHTTPRequest(char* out, const char* host, const char* path)
{
    return sprintf(out,
                   "GET %s HTTP/1.1\r\n"
                   "Host: %s\r\n"
                   "User-Agent: Wii\r\n"
                   "Cache-Control: no-cache\r\n\r\n",
                   path, host);
}

static u64 ProcessWorldTime(char* json_str)
{
    auto json = nlohmann::json::parse(json_str);

    if (!json.contains("unixtime") || !json.contains("utc_offset")) {
        printf("Invalid date and time JSON\n");
        abort();
    }

    if (json.contains("abbreviation") && json.contains("timezone")) {
        printf("Timezone: %s (%s)\n",
               json["abbreviation"].get<std::string>().c_str(),
               json["timezone"].get<std::string>().c_str());
    }

    if (json.contains("datetime")) {
        printf("Date and Time: %s\n",
               json["datetime"].get<std::string>().c_str());
    }

    u64 unixTime = json["unixtime"].get<u64>();
    std::string utcOffset = json["utc_offset"].get<std::string>();

    /* Calculate for time zone */
    int difHour, difMin;
    /* starts with either + or - */
    sscanf(utcOffset.c_str() + 1, "%02d:%02d", &difHour, &difMin);
    int difTime = difHour * 3600 + difMin * 60;
    unixTime += utcOffset[0] == '-' ? -difTime : difTime;

    return unixTime;
}

static u64 GetWorldTime()
{
    char request[2048];
    s32 len = MakeHTTPRequest(request, "worldtimeapi.org", "/api/ip");

    s32 sock = Network::connect("worldtimeapi.org", 80);
    Network::send(sock, request, len);
    len = Network::receive(sock, request, 2048);
    u64 wiiTime = time(NULL);
    Network::close(sock);

    request[len] = 0;
    // printf("received: %s\n", request);

    char* json_start = strstr(request, "\r\n\r\n");
    if (json_start == nullptr) {
        printf("Invalid HTTP response\n");
        abort();
    }

    json_start += 4;
    return ProcessWorldTime(json_start) - wiiTime;
}

s32 main()
{
    static void* xfb = nullptr;
    static GXRModeObj* rmode = nullptr;

    VIDEO_Init();
    rmode = VIDEO_GetPreferredMode(nullptr);
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight,
                 rmode->fbWidth * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();
    printf("\x1b[2;0H");

    VIDEO_WaitVSync();

    new Network();
    u64 timeDif = GetWorldTime();

    SysConf* conf = new SysConf();

    u32 bias;
    s32 ret = CONF_GetCounterBias(&bias);
    assert(ret == 0);

    conf->replace<u32>("IPL.CB", bias + timeDif);
    conf->save();
    printf("Wrote new counter bias to SYSCONF\n");
    delete conf;

    sleep(1);
    return 0;
}
