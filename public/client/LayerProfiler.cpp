#include "LayerProfiler.h"
#include <thread>
#include <chrono>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/websocket.h>
#include <emscripten/threading.h>
#endif

namespace tracy
{

static LayerProfiler s_layer_profiler;

void LayerProfiler::SendLayerData( const std::vector<uint8_t>& data ) { s_layer_profiler.setData( data ); }
void LayerProfiler::SetLayerCallBack( std::function<void( const std::vector<uint8_t>& )> callback )
{
    s_layer_profiler.setCallBack(callback);
}

LayerProfiler::LayerProfiler()
{
#ifdef __EMSCRIPTEN__
  m_WebSocket = nullptr;
#endif
    spawnWorkTread();
}

void LayerProfiler::SendWork()
{
#ifdef __EMSCRIPTEN__
    // create websocket
    while( true )
    {
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
        m_WebSocket = std::make_shared<WebSocketClient>( "ws://localhost:8085" );
        if( m_WebSocket->isConnect )
        {
            printf( "web socket create success!\n" );
            break;
        }
        printf( "web socket create failure!\n" );
        m_WebSocket.reset();
    }

    while( true )
    {
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
        if( !m_WebSocket->isConnect ) break;
        if( !m_Queue.empty() )
        {
            auto data = *m_Queue.front();
            m_Queue.pop();
            m_WebSocket->sendMessage( (char*)data.data(), data.size() );
        }
    }
#else
    int port = 8084;
    printf("Start listen port: %d!\n", port);
    bool isListen = m_ListenSocket.Listen(port, 4);
    if(!isListen)
    {
        printf("Listen port: %d return false!\n", port);
    }
    while(true)
    {
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
        m_Socket = m_ListenSocket.Accept();
        if(m_Socket)
        {
            printf("tcp already connect!\n");
            break;
        }
    }

    while(true)
    {
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
        if(!m_Queue.empty())
        {
            auto data = *m_Queue.front();
            m_Queue.pop();
            m_Socket->Send(data.data(), data.size());
        }
    }

#endif
}

void LayerProfiler::recvWork()
{
#ifdef __EMSCRIPTEN__
    while(true)
    {
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
        WebSocketClient::Message message;
        if(m_WebSocket->recvMssageImmdiately(message))
        {
            printf("recive message type: %d \n", message.type);
            if((message.type == WebSocketClient::Binary) && m_Callback)
            {
                std::vector<uint8_t> data(message.data.data(), message.data.data() + message.data.size());
                m_Callback(data);
            }
        }
    }
#else
    while(true)
    {
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
        if(m_Socket && m_Socket->HasData())
        {
            std::vector<uint8_t> data(9);
            m_Socket->ReadUpTo(data.data(), data.size());
            messages.push(std::move(data));
        }
        if(!messages.empty())
        {
            if(m_Callback){
                m_Callback(messages.front());
                messages.pop();
            }
        }
    }
#endif
}

void LayerProfiler::spawnWorkTread()
{
    m_SendThread = std::make_shared<std::thread>(&LayerProfiler::SendWork, this);
    m_RecvThread = std::make_shared<std::thread>(&LayerProfiler::recvWork, this);
}

void LayerProfiler::setData( const std::vector<uint8_t>& data ) { m_Queue.push( data ); }
void LayerProfiler::setCallBack( std::function<void( const std::vector<uint8_t>& )> callback )
{
    if(!m_Callback)
    {
        m_Callback = callback;
    }

}
}


