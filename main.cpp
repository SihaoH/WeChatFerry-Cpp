#include <windows.h>
#include "sdk.h"
#include "wcf.pb.h"
#include "pb_util.h"
#include <nng/nng.h>
#include <nng/protocol/pair1/pair.h>
#include <vector>
#include <iostream>
#include <tlhelp32.h>

// 从util.cpp拷过来的，因为原则上客户端不应该编译sdk相关的源文件
DWORD GetWeChatPid()
{
    DWORD pid           = 0;
    HANDLE hSnapshot    = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe32 = { sizeof(PROCESSENTRY32) };
    while (Process32Next(hSnapshot, &pe32)) {
        std::wstring strProcess = pe32.szExeFile;
        if (strProcess == L"WeChat.exe") {
            pid = pe32.th32ProcessID;
            break;
        }
    }
    CloseHandle(hSnapshot);
    return pid;
}

int main(int argc, char *argv[])
{
    int pid = GetWeChatPid();
    // 微信没打开，才需要重新打开并注入
    if (pid == 0) {
        int ret = 0;
    #if _DEBUG
        ret = WxInitSDK(true, 10086);
    #else
        ret = WxInitSDK(false, 10086);
    #endif
        if (ret != 0) {
            return ret;
        }
    }

    constexpr size_t DEFAULT_BUF_SIZE = 16 * 1024 * 1024;
    nng_socket func_socket = NNG_SOCKET_INITIALIZER;
    Request req = Request_init_default;
    req.func = Functions_FUNC_ENABLE_RECV_TXT;
    req.which_msg = Request_func_tag;
    std::vector<uint8_t> msgBuffer(DEFAULT_BUF_SIZE);

    pb_ostream_t stream = pb_ostream_from_buffer(msgBuffer.data(), msgBuffer.size());
    pb_encode(&stream, Request_fields, &req);

    // 连接nng服务端，发送监听wx信息的请求
    std::string url = "tcp://127.0.0.1:10086";
    nng_pair1_open(&func_socket);
    nng_dial(func_socket, url.c_str(), NULL, 0);
    nng_setopt_ms(func_socket, NNG_OPT_SENDTIMEO, 5000);
    nng_send(func_socket, msgBuffer.data(), stream.bytes_written, 0);

    // 连接nng服务端，循环接收wx信息
    url = "tcp://127.0.0.1:10087";
    nng_socket msg_socket = NNG_SOCKET_INITIALIZER;
    nng_pair1_open(&msg_socket);
    nng_dial(msg_socket, url.c_str(), NULL, 0);
    system("chcp 65001"); // 支持中文输出
    for (;;) {
        // 获取wx接收的信息
        uint8_t *in = NULL;
        size_t in_len = 0;
        size_t out_len = DEFAULT_BUF_SIZE;
        nng_recv(msg_socket, &in, &in_len, NNG_FLAG_ALLOC);
        Response rsp = Response_init_default;
        pb_istream_t stream2 = pb_istream_from_buffer(in, in_len);
        pb_decode(&stream2, Response_fields, &rsp);

        // 可以在这里打断点调试，查看更多信息
        std::cout << rsp.msg.wxmsg.content <<std::endl;

        // 匹配关键词，固定回复
        if (rsp.msg.wxmsg.content == std::string("aabb99999")) {
            req = Request_init_default;
            req.func = Functions_FUNC_SEND_TXT;
            req.which_msg = Request_txt_tag;
            req.msg.txt.receiver = rsp.msg.wxmsg.sender;
            req.msg.txt.msg = "666";
            std::vector<uint8_t> msg_buff(DEFAULT_BUF_SIZE);
            pb_ostream_t stream = pb_ostream_from_buffer(msg_buff.data(), msg_buff.size());
            pb_encode(&stream, Request_fields, &req);
            nng_send(func_socket, msg_buff.data(), stream.bytes_written, 0);
        }

        pb_release(Response_fields, &rsp);
        nng_free(in, in_len);
    }
    nng_close(msg_socket);
    nng_close(func_socket);

    return 0;
}
