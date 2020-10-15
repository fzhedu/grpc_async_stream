/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

// This is a modification of greeter_async_server.cc from grpc examples.
// Comments have been removed to make it easier to follow the code.
// For comments please refer to the original example.

#include <memory>
#include <iostream>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>

#include "hellostreamingworld.grpc.pb.h"

using std::string;
using grpc::Server;
using grpc::ServerAsyncWriter;
using grpc::ServerAsyncReader;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;
using hellostreamingworld::HelloRequest;
using hellostreamingworld::HelloReply;
using hellostreamingworld::MultiGreeter;

class ServerImpl final
{
public:
    ~ServerImpl()
    {
        server_->Shutdown();
        cq_->Shutdown();
    }

    void Run(string port)
    {
        std::string server_address("0.0.0.0:"+port);

        ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service_);

        cq_ = builder.AddCompletionQueue();
        server_ = builder.BuildAndStart();
        std::cout << "Server listening on " << server_address << std::endl;

        HandleRpcs();
    }

private:
    class CallData
    {
    public:
        CallData(MultiGreeter::AsyncService* service, ServerCompletionQueue* cq)
            : service_(service)
            , cq_(cq)
            , reader_(&ctx_)
            , status_(CREATE)
            , times_(0)
        {
            Proceed();
        }

        void Proceed()
        {
            if (status_ == CREATE)
            {
                status_ = PROCESS;
                service_->RequestsayMoreHello(&ctx_,  &reader_, cq_, cq_, this);
            }
            else if (status_ == PROCESS)
            {
                // Now that we go through this stage multiple times, 
                // we don't want to create a new instance every time.
                // Refer to gRPC's original example if you don't understand 
                // why we create a new instance of CallData here.
                if (times_ == 0)
                {
                    new CallData(service_, cq_);
                    reader_.Read(&request_, this);
                    times_++;
                    return;
                }
                std::cout<<times_<<" th server receive request : "<< request_.name() <<"   "<< request_.num_greetings() << std::endl;

                if (times_ >= request_.num_greetings())
                {
                    status_ = FINISH;
                    std::string prefix("Hello ");
                    reply_.set_message(prefix + request_.name() + ", no " + std::to_string(times_) );
                    std::cout<<"read finish!!!"<< std::endl;
                    reader_.Finish(reply_,Status::OK, this);
                }
            	else
            	{
                    // read one more
                    ++times_;
                    reader_.Read(&request_, this);
                }
            }
            else
            {
                std::cout<<"delete this!!!"<< std::endl;
                GPR_ASSERT(status_ == FINISH);
                delete this;
            }
        }

    private:
        MultiGreeter::AsyncService* service_;
        ServerCompletionQueue* cq_;
        ServerContext ctx_;

        HelloRequest request_;
        HelloReply reply_;

        ServerAsyncReader<HelloReply, HelloRequest> reader_;

        int times_;

        enum CallStatus
        {
            CREATE,
            PROCESS,
            FINISH
        };
        CallStatus status_; // The current serving state.
    };

    void HandleRpcs()
    {
        new CallData(&service_, cq_.get());
        void* tag; // uniquely identifies a request.
        bool ok;
        while (true)
        {
            GPR_ASSERT(cq_->Next(&tag, &ok));
            GPR_ASSERT(ok);
            static_cast<CallData*>(tag)->Proceed();
        }
    }

    std::unique_ptr<ServerCompletionQueue> cq_;
    MultiGreeter::AsyncService service_;
    std::unique_ptr<Server> server_;
};

int main(int argc, char** argv)
{
    ServerImpl server;
    assert(argc==2);
    server.Run((argv[1]));

    return 0;
}
