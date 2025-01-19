#include "chatserver.hpp"
#include "json.hpp"
#include "chatservice.hpp"

#include <iostream>
#include <functional>
#include <string>
using namespace std;
using namespace placeholders;
using json = nlohmann::json;

// 初始化聊天服务器对象
ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &nameArg)
    : _server(loop, listenAddr, nameArg), _loop(loop)
{
    // 注册链接回调
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));

    // 注册消息回调
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

    // 设置线程数量
    _server.setThreadNum(4);
}

// 启动服务
void ChatServer::start()
{
    _server.start();
}

// 上报链接相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    // 客户端断开链接
    if (!conn->connected())
    {
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}

// 上报读写事件相关信息的回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn,
                           Buffer *buffer,
                           Timestamp time)
{
   while (buffer->readableBytes() >= sizeof(uint32_t))
    {
        // 窥探报文前四个字节
        uint32_t length = buffer->peekInt32();
        if (buffer->readableBytes() >= length + sizeof(uint32_t)) // 确保缓冲区里的数据足够
        {
            buffer->retrieve(sizeof(uint32_t)); // 移除长度字符串
            json js;
            try
            {
                string buf = buffer->retrieveAsString(length);
                js = json::parse(buf);
            }
            catch (const nlohmann::json::parse_error &e)
            {
                LOG_ERROR << "JSON parse error at byte " << e.byte << ": " << e.what();
                break;
                // 处理解析失败的情况，例如记录错误、返回默认值等
            }
            auto msgHander = ChatService::instance()->getHandler(js["msgid"].get<int>());
            msgHander(conn, js, time);
        }
        else
        {
            break;
        }
    }
}