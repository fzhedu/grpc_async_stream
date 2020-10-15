# grpc_async_stream
Two folds:

ServerRead: server read streaming from clients

ServerWrite: server write streaming to clients

multiple clients share a completion queue.
If we want to send more messages to a server, we can create more channels to that server.

adapted from examples for a G-Research's [blog post](https://www.gresearch.co.uk/2019/03/20/lessons-learnt-from-writing-asynchronous-streaming-grpc-services-in-c/) about gRPC. They are modified versions of the examples from [gRPC's repository](https://github.com/grpc/grpc).


## command
PKG_CONFIG_PATH=/usr/local/opt/openssl/lib/pkgconfig make
