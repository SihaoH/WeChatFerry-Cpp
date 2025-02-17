#include "sdk.h"
#include "wcf.pb.h"
#include "pb_util.h"
#include <nng/nng.h>
#include <nng/protocol/pair1/pair.h>
#include <vector>
#include <iostream>


int main(int argc, char *argv[])
{
    int ret = 0;
#if _DEBUG
    ret = WxInitSDK(true, 10086);
#else
    ret = WxInitSDK(false, 10086);
#endif

    constexpr size_t DEFAULT_BUF_SIZE = 16 * 1024 * 1024;
    nng_socket func_socket = NNG_SOCKET_INITIALIZER;
    Request req = Request_init_default;
    req.func = Functions_FUNC_ENABLE_RECV_TXT;
    req.which_msg = Response_wxmsg_tag;
    std::vector<uint8_t> msgBuffer(DEFAULT_BUF_SIZE);

    pb_ostream_t stream = pb_ostream_from_buffer(msgBuffer.data(), msgBuffer.size());
    pb_encode(&stream, Request_fields, &req);

    // 连接nng服务端，发送监听wx信息的请求
    std::string url = "tcp://127.0.0.1:10086";
    nng_pair1_open(&func_socket);
    nng_dial(func_socket, url.c_str(), NULL, 0);
    nng_setopt_ms(func_socket, NNG_OPT_SENDTIMEO, 5000);
    nng_send(func_socket, msgBuffer.data(), stream.bytes_written, 0);
    nng_close(func_socket);
    pb_release(Request_fields, &req);

    // 连接nng服务端，循环接收wx信息
    url = "tcp://127.0.0.1:10087";
    nng_socket msg_socket = NNG_SOCKET_INITIALIZER;
    nng_pair1_open(&msg_socket);
    nng_dial(msg_socket, url.c_str(), NULL, 0);
    system("chcp 65001"); // 支持中文输出
    for (;;) {
        uint8_t *in = NULL;
        size_t in_len = 0;
        size_t out_len = DEFAULT_BUF_SIZE;
        nng_recv(msg_socket, &in, &in_len, NNG_FLAG_ALLOC);
        Response rsp = Response_init_default;
        pb_istream_t stream2 = pb_istream_from_buffer(in, in_len);
        pb_decode(&stream2, Response_fields, &rsp);

        // 可以在这里打断点调试，查看更多信息
        std::cout << rsp.msg.wxmsg.content <<std::endl;

        pb_release(Response_fields, &rsp);
        nng_free(in, in_len);
    }

    ret = WxDestroySDK();

    return ret;
}
