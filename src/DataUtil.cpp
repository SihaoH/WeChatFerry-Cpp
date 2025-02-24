#include "DataUtil.h"
#include "pb_util.h"
#include "Logger.h"

constexpr size_t DEFAULT_BUF_SIZE = 16 * 1024 * 1024;

static void releaseRequest(Request* req)
{
    pb_release(Request_fields, req);
}
static void releaseResponse(Response* rsp)
{
    pb_release(Response_fields, rsp);
}

extern "C" static bool decode_contacts(pb_istream_t* stream, const pb_field_t* field, void** arg)
{
    RpcContact_t contact;
    RpcContact message = RpcContact_init_default;

    message.wxid.funcs.decode = decode_string;
    message.wxid.arg          = (void *)&contact.wxid;

    message.code.funcs.decode = decode_string;
    message.code.arg          = (void *)&contact.code;

    message.remark.funcs.decode = decode_string;
    message.remark.arg          = (void *)&contact.remark;

    message.name.funcs.decode = decode_string;
    message.name.arg          = (void *)&contact.name;

    message.country.funcs.decode = decode_string;
    message.country.arg          = (void *)&contact.country;

    message.province.funcs.decode = decode_string;
    message.province.arg          = (void *)&contact.province;

    message.city.funcs.decode = decode_string;
    message.city.arg          = (void *)&contact.city;

    if (!pb_decode_ex(stream, RpcContact_fields, &message, PB_DECODE_NOINIT)) {
        LOG(err) << "Encoding failed: " << PB_GET_ERROR(stream);
        return false;
    }
    contact.gender = message.gender;

    // 将解码后的 contact 添加到原始的 contacts vector 中
    std::vector<RpcContact_t>* contacts = (std::vector<RpcContact_t>*)*arg;
    contacts->push_back(contact);

    return true;
}

extern "C" static bool decode_bytes(pb_istream_t* stream, const pb_field_t* field, void** arg)
{
    std::vector<uint8_t>* bytes = (std::vector<uint8_t>*)*arg;
    size_t len = stream->bytes_left;
    bytes->resize(len);
    if (!pb_read(stream, (uint8_t *)bytes->data(), len)) {
        LOG(err) << "Decoding failed: " << PB_GET_ERROR(stream);
        return false;
    }
    return true;
}

extern "C" static bool decode_fields(pb_istream_t* stream, const pb_field_t* field, void** arg)
{
    DbField_t db_field;
    DbField message = DbField_init_default;

    message.column.funcs.decode = decode_string;
    message.column.arg          = (void *)&db_field.column;
    
    message.content.funcs.decode = decode_bytes;
    message.content.arg          = (void *)&db_field.content;
    

    if (!pb_decode_ex(stream, DbField_fields, &message, PB_DECODE_NOINIT)) {
        LOG(err) << "Encoding failed: " << PB_GET_ERROR(stream);
        return false;
    }
    db_field.type = message.type;

    // 将解码后的 row 添加到原始的 row vector 中
    std::vector<DbField_t>* row = (std::vector<DbField_t>*)*arg;
    row->push_back(db_field);

    return true;
}

extern "C" static bool decode_rows(pb_istream_t* stream, const pb_field_t* field, void** arg)
{
    DbRow_t row;
    DbRow message = DbRow_init_default;

    message.fields.funcs.decode = decode_fields;
    message.fields.arg          = (void *)&row;

    if (!pb_decode_ex(stream, DbRow_fields, &message, PB_DECODE_NOINIT)) {
        LOG(err) << "Encoding failed: " << PB_GET_ERROR(stream);
        return false;
    }

    // 将解码后的 row 添加到原始的 rows vector 中
    std::vector<DbRow_t>* rows = (std::vector<DbRow_t>*)*arg;
    rows->push_back(row);

    return true;
}

QByteArray DataUtil::encode(const Request& req)
{
    std::vector<uint8_t> buf(DEFAULT_BUF_SIZE);
    pb_ostream_t stream = pb_ostream_from_buffer(buf.data(), buf.size());
    pb_encode(&stream, Request_fields, &req);
    return QByteArray((const char*)buf.data(), stream.bytes_written);
}

QByteArray DataUtil::encode(const Response& rsp)
{
    std::vector<uint8_t> buf(DEFAULT_BUF_SIZE);
    pb_ostream_t stream = pb_ostream_from_buffer(buf.data(), buf.size());
    pb_encode(&stream, Response_fields, &rsp);
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

vector<RpcContact_t> DataUtil::getContacts(const QByteArray& data)
{
    vector<RpcContact_t> contacts;
    QSharedPointer<Response> rsp_ptr = QSharedPointer<Response>(new Response, releaseResponse);
    rsp_ptr->func = Functions_FUNC_GET_CONTACTS;
    rsp_ptr->which_msg = Response_contacts_tag;
    rsp_ptr->msg.contacts.contacts.funcs.decode = decode_contacts;
    rsp_ptr->msg.contacts.contacts.arg = &contacts;
    pb_istream_t stream = pb_istream_from_buffer((const pb_byte_t*)data.data(), data.size());
    pb_decode_ex(&stream, Response_fields, rsp_ptr.get(), PB_DECODE_NOINIT);
    return contacts;
}

vector<DbRow_t> DataUtil::getDatabaseRows(const QByteArray& data)
{
    vector<DbRow_t> rows;
    QSharedPointer<Response> rsp_ptr = QSharedPointer<Response>(new Response, releaseResponse);
    rsp_ptr->func = Functions_FUNC_EXEC_DB_QUERY;
    rsp_ptr->which_msg = Response_rows_tag;
    rsp_ptr->msg.rows.rows.funcs.decode = decode_rows;
    rsp_ptr->msg.rows.rows.arg = &rows;
    pb_istream_t stream = pb_istream_from_buffer((const pb_byte_t*)data.data(), data.size());
    pb_decode_ex(&stream, Response_fields, rsp_ptr.get(), PB_DECODE_NOINIT);
    return rows;
}
