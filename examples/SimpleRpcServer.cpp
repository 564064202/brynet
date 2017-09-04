#include <iostream>
#include <string>

#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/http/HttpService.h>
#include <brynet/net/http/HttpFormat.h>
#include <brynet/net/http/WebSocketFormat.h>  
#include <brynet/net/WrapTCPService.h>
#include <brynet/utils/systemlib.h>

#include "google/protobuf/service.h"
#include "google/protobuf/message.h"
#include "google/protobuf/util/json_util.h"
#include "echo_service.pb.h"

using namespace brynet;
using namespace brynet::net;

class TypeService
{
public:
    typedef std::shared_ptr<TypeService>    PTR;

    TypeService(std::shared_ptr<google::protobuf::Service> service) : mService(service)
    {}
    virtual ~TypeService()
    {}

    std::shared_ptr<google::protobuf::Service>  getService()
    {
        return  mService;
    }

    void    registerMethod(const ::google::protobuf::MethodDescriptor* desc)
    {
        mMethods[desc->name()] = desc;
    }

    const ::google::protobuf::MethodDescriptor* findMethod(std::string name)
    {
        auto it = mMethods.find(name);
        if (it != mMethods.end())
        {
            return (*it).second;
        }

        return nullptr;
    }

private:
    std::shared_ptr<google::protobuf::Service>  mService;
    std::unordered_map<std::string, const ::google::protobuf::MethodDescriptor*>   mMethods;
};

class RPCServiceMgr
{
public:
    typedef std::shared_ptr<RPCServiceMgr> PTR;
    
    virtual ~RPCServiceMgr()
    {}

    bool    registerService(std::shared_ptr<google::protobuf::Service> service)
    {
        auto typeService = std::make_shared<TypeService>(service);

        auto serviceDesc = service->GetDescriptor();
        for (auto i = 0; i < serviceDesc->method_count(); i++)
        {
            auto methodDesc = serviceDesc->method(i);
            typeService->registerMethod(methodDesc);
        }

        mServices[service->GetDescriptor()->full_name()] = typeService;

        return true;
    }

    TypeService::PTR    findService(const std::string& name)
    {
        auto it = mServices.find(name);
        if (it != mServices.end())
        {
            return (*it).second;
        }

        return nullptr;
    }

private:
    std::unordered_map<std::string, TypeService::PTR>   mServices;
};

class MyClosure : public ::google::protobuf::Closure
{
public:
    MyClosure(std::function<void(void)> callback) : mCallback(callback), mValid(true)
    {}

private:
    void    Run() override final
    {
        assert(mValid);
        assert(mCallback);

        //�����ε���Run����
        if (mValid && mCallback)
        {
            mCallback();
            mValid = false;
        }
    }

private:
    std::function<void(void)>   mCallback;
    bool                        mValid;
};

static void processHTTPRPCRequest(RPCServiceMgr::PTR rpcServiceMgr, 
    HttpSession::PTR session, 
    const std::string & serviceName, 
    const std::string& methodName, 
    const std::string& body)
{
    auto typeService = rpcServiceMgr->findService(serviceName);
    if (typeService == nullptr)
    {
        return;
    }

    auto method = typeService->findMethod(methodName);
    if (method == nullptr)
    {
        return;
    }

    auto requestMsg = typeService->getService()->GetRequestPrototype(method).New();
    requestMsg->ParseFromArray(body.c_str(), body.size());

    auto responseMsg = typeService->getService()->GetResponsePrototype(method).New();

    auto clouse = new MyClosure([=]() {
        //�����ͨ��HTTPЭ�鷵�ظ��ͻ���
        //TODO::delete clouse

        std::string jsonMsg;
        google::protobuf::util::MessageToJsonString(*responseMsg, &jsonMsg);

        HttpResponse httpResponse;
        httpResponse.setStatus(HttpResponse::HTTP_RESPONSE_STATUS::OK);
        httpResponse.setContentType("application/json");
        httpResponse.setBody(jsonMsg.c_str());

        auto result = httpResponse.getResult();
        session->send(result.c_str(), result.size(), nullptr);

        /*  TODO::���Ͽ�����,�ɿ��ſͻ��������Ͽ�,���ߵײ㿪ʼ�������(�Ͽ���������) */
        session->postShutdown();

        delete requestMsg;
        delete responseMsg;
    });

    //TODO::support controller(��ʱ�Լ�������)
    typeService->getService()->CallMethod(method, nullptr, requestMsg, responseMsg, clouse);
}

//  ʵ��Echo����
class MyEchoService : public sofa::pbrpc::test::EchoServer
{
public:
    MyEchoService() : mIncID(0)
    {
    }

private:
    void Echo(::google::protobuf::RpcController* controller,
        const ::sofa::pbrpc::test::EchoRequest* request,
        ::sofa::pbrpc::test::EchoResponse* response,
        ::google::protobuf::Closure* done) override
    {
        mIncID++;
        response->set_message(std::to_string(mIncID) + " is the current id");
        done->Run();
    }

private:
    int     mIncID;
};

int main(int argc, char **argv)
{
    RPCServiceMgr::PTR rpcServiceMgr = std::make_shared<RPCServiceMgr>();
    rpcServiceMgr->registerService(std::make_shared<MyEchoService>());

    auto tcpService = std::make_shared<WrapTcpService> ();
    tcpService->startWorkThread(ox_getcpunum());

    auto listenThread = ListenThread::Create();
    listenThread->startListen(false, "0.0.0.0", 8080, [tcpService, rpcServiceMgr](sock fd) {
        tcpService->addSession(fd, [rpcServiceMgr](const TCPSession::PTR& session) {
            HttpService::setup(session, [rpcServiceMgr](const HttpSession::PTR& httpSession) {
                httpSession->setHttpCallback([=](const HTTPParser& httpParser, HttpSession::PTR session) {
                    auto queryPath = httpParser.getPath();
                    std::string::size_type pos = queryPath.rfind('.');
                    if (pos != std::string::npos)
                    {
                        auto serviceName = queryPath.substr(1, pos - 1);
                        auto methodName = queryPath.substr(pos + 1);
                        processHTTPRPCRequest(rpcServiceMgr, session, serviceName, methodName, httpParser.getBody());
                    }
                });

                httpSession->setWSCallback([=](HttpSession::PTR session, WebSocketFormat::WebSocketFrameType opcode, const std::string& payload) {
                    //TODO::support websocketЭ��,payload�Ͷ�����Э��һ��
                });
            });
        }, false, nullptr, 1024 * 1024);
    });

    std::cin.get();
    return 0;
}
