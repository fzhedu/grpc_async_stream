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

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>

#ifdef BAZEL_BUILD
#include "examples/protos/helloworld.grpc.pb.h"
#else
#include "hellostreamingworld.grpc.pb.h"
#endif

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
    explicit GreeterClient(std::shared_ptr<Channel> channel)
            : stub_(MultiGreeter::NewStub(channel)) {}

    // Assembles the client's payload, sends it and presents the response back
    // from the server.
    std::string SayHello(const std::string& user) {
        // Data we are sending to the server.
        HelloRequest request;
        request.set_name(user);
        request.set_num_greetings(12);

        // Container for the data we expect from the server.
        HelloReply reply;

        // Context for the client. It could be used to convey extra information to
        // the server and/or tweak certain RPC behaviors.
        ClientContext context;

        // The producer-consumer queue we use to communicate asynchronously with the
        // gRPC runtime.
        CompletionQueue cq;

        // Storage for the status of the RPC upon completion.
        std::unique_ptr<ClientAsyncReader<HelloReply> > reader(stub_->AsyncsayHello(&context, request ,&cq,(void*)1));
        void* got_tag;
        bool ok;
        Status status;
        bool ret = cq.Next(&got_tag, &ok);

        if (ret && ok && got_tag == (void*)1) {
            while (1) {
                reader->Read(&reply,(void*)1);
                ok = false;
                ret = cq.Next(&got_tag, &ok);
                if (!ret || !ok || got_tag != (void*)1) {
                    break;
                }
                request.set_num_greetings(request.num_greetings()-1);
                std::cout << "reply "
                          << reply.message() <<" rest "<< request.num_greetings()<< std::endl;
            }
        }

        return request.num_greetings() == 0 ? "OK":" FAILED";
    }
private:
    // Out of the passed in Channel comes the stub, stored here, our view of the
    // server's exposed services.
    int times=0;
    std::unique_ptr<MultiGreeter::Stub> stub_;
};

int main(int argc, char** argv) {
    // Instantiate the client. It requires a channel, out of which the actual RPCs
    // are created. This channel models a connection to an endpoint (in this case,
    // localhost at port 50051). We indicate that the channel isn't authenticated
    // (use of InsecureChannelCredentials()).
    GreeterClient greeter(grpc::CreateChannel(
            "localhost:50051", grpc::InsecureChannelCredentials()));
    std::string user("world");
    std::string reply = greeter.SayHello(user);  // The actual RPC call!
    std::cout << "Greeter received: " << reply << std::endl;

    return 0;
}