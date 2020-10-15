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
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>
#include <thread>

#ifdef BAZEL_BUILD
#include "examples/protos/hellostreamingworld.grpc.pb.h"
#else
#include "hellostreamingworld.grpc.pb.h"
#endif

using std::vector;
using std::string;
using grpc::Channel;
using grpc::ClientAsyncReader;
using grpc::ClientAsyncWriter;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
using hellostreamingworld::HelloRequest;
using hellostreamingworld::HelloReply;
using hellostreamingworld::MultiGreeter;


class GreeterClient {
public:
    explicit GreeterClient(std::shared_ptr<Channel> channel,CompletionQueue*cq, int id)
            : stub_(MultiGreeter::NewStub(channel)),cq_(cq),client_id_(id) {}

    // Assembles the client's payload and sends it to the server.
    void SayMoreHello(const std::string& user, const int num_greetings)
    {
        // Call object to store rpc data
        AsyncClientCall* call = new AsyncClientCall;
        call->request.set_name(user);
        call->request.set_num_greetings(num_greetings);
        call->writer = stub_->AsyncsayMoreHello(&call->context, &call->reply ,cq_,(void*)call);
        call->state_type = AsyncClientCall::CONNECTED;
        call->times= 0;
        call->client_id= this->client_id_;
        std::cout<<"connected!!"<<std::endl;
    }

    // Loop while listening for completed responses.
    // Prints out the response from the server.
    void AsyncCompleteRpc() {
        void* got_tag;
        bool ok = false;

        // Block until the next result is available in the completion queue "cq".
        while (cq_->Next(&got_tag, &ok)) {
            // The tag in this example is the memory location of the call object
            AsyncClientCall* call = static_cast<AsyncClientCall*>(got_tag);
            std::cout<<"client id = "<< call->client_id<<" : ";
            // Verify that the request was completed successfully. Note that "ok"
            // corresponds solely to the request for updates introduced by Finish().
            if(!ok) {
                call->state_type = AsyncClientCall::DONE;
            }
            switch (call->state_type) {
                case AsyncClientCall::CONNECTED: {
                    call->request.set_name("client "+std::to_string(call->client_id)+" hello " + std::to_string(call->times));
                    call->writer->Write(call->request,(void*)call);
                    call->state_type = AsyncClientCall::TOREAD;
                    std::cout << call->request.num_greetings() <<" connected"<< std::endl;
                }break;
                case AsyncClientCall::TOREAD: {
                    call->times++;
                    std::cout << "Greeter received: " << call->reply.message() << " times = "
                                  << call->times <<" of total "<< call->request.num_greetings() << std::endl;


                    if(call ->times >= call->request.num_greetings()) {
                        call->writer->Finish(&call->status,call);
                        call->state_type = AsyncClientCall::DONE;
                    }else {
                        call->request.set_name("client "+std::to_string(call->client_id)+" hello "+ std::to_string(call->times));
                        call->writer->Write(call->request,(void*)call);
                    }
                    //  std::cout <<"begin to to read again times = " << call->times << std::endl;
                }break;
                case AsyncClientCall::DONE: {
                    std::cout << call->request.num_greetings() <<" begin to done "<< call->reply.message() << std::endl;
                    delete call;
                }
            }
        }
    }

private:

    // struct for keeping state and data information
    struct AsyncClientCall {
        // Container for the data we expect from the server.
        HelloReply reply;
        HelloRequest request;
        // Context for the client. It could be used to convey extra information to
        // the server and/or tweak certain RPC behaviors.
        ClientContext context;

        // Storage for the status of the RPC upon completion.
        Status status;

        std::unique_ptr<ClientAsyncWriter<HelloRequest> > writer;
        enum StateType {CONNECTED,TOREAD,DONE};
        StateType state_type;
        std::atomic_int times;
        int client_id;
    };

    // Out of the passed in Channel comes the stub, stored here, our view of the
    // server's exposed services.
    std::unique_ptr<MultiGreeter::Stub> stub_;

    // The producer-consumer queue we use to communicate asynchronously with the
    // gRPC runtime.
    CompletionQueue* cq_;
    int client_id_;
};

int main(int argc, char** argv) {

    int client_num=3;
    int req_num=5;
    CompletionQueue cq;
    std::vector<GreeterClient*> clients;
    for (int i = 0; i< client_num; ++i) {
        clients.emplace_back(new GreeterClient(grpc::CreateChannel(
                "localhost:5005"+std::to_string(i), grpc::InsecureChannelCredentials()),&cq,i));
    }

    // Spawn reader thread that loops indefinitely
    std::thread thread_ = std::thread(&GreeterClient::AsyncCompleteRpc, clients[0]);



    for (int i = 5; i <= req_num; i++) {
        for(int j=0;j<client_num;++j) {
            std::string user("world req id = " + std::to_string(i) + " client id = " + std::to_string(j));
            clients[j]->SayMoreHello(user,i);  // The actual RPC call!
        }
    }


    std::cout << "Press control-c to quit" << std::endl << std::endl;
    thread_.join();  //blocks forever

    return 0;
}
