#include "DataUtil.h"
#include "pb_util.h"

constexpr size_t DEFAULT_BUF_SIZE = 16 * 1024 * 1024;

static void releaseRequest(Request* req) {
    pb_release(Request_fields, req);
}
static void releaseResponse(Response* rsp) {
    pb_release(Response_fields, rsp);
}

QByteArray DataUtil::encode(const Request& req)
{
    std::vector<uint8_t> buf(DEFAULT_BUF_SIZE);
    pb_ostream_t stream = pb_ostream_from_buffer(buf.data(), buf.size());
    pb_encode(&stream, Request_fields, &req);
    return QByteArray((const char*)buf.data(), stream.bytes_written);
}

QByteArray DataUtil::encode(const Response& req)
{
    std::vector<uint8_t> buf(DEFAULT_BUF_SIZE);
    pb_ostream_t stream = pb_ostream_from_buffer(buf.data(), buf.size());
    pb_encode(&stream, Response_fields, &req);
    return QByteArray((const char*)buf.data(), stream.bytes_written);
}

QSharedPointer<Request> DataUtil::toRequest(const QByteArray& data)
{
    QSharedPointer<Request> req_ptr = QSharedPointer<Request>(new Request, releaseRequest);
    pb_istream_t stream = pb_istream_from_buffer((const pb_byte_t*)data.data(), data.size());
    pb_decode(&stream, Request_fields, req_ptr.get());
    return req_ptr;
}

QSharedPointer<Response> DataUtil::toResponse(const QByteArray& data)
{
    QSharedPointer<Response> rsp_ptr = QSharedPointer<Response>(new Response, releaseResponse);
    pb_istream_t stream = pb_istream_from_buffer((const pb_byte_t*)data.data(), data.size());
    pb_decode(&stream, Response_fields, rsp_ptr.get());
    return rsp_ptr;
}
