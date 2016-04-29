libasyncd
=========

Embeddable Event-based Asynchronous Message/HTTP Server library for C/C++.

## What is libasyncd?

Libasyncd is an embeddable event-driven asynchronous message server for C/C++.
It supports HTTP protocol by default and you can add your own protocol handler(hook)
to build your own high performance server.

Asynchronous way of programming can easily go quite complicated since you need to
handle every thing in non-blocking way. So the goal of Libasyncd project is
to make a flexible and fast asynchronous server framework with nice abstraction that
can cut down the complexity.

## Why libasyncd?

libasyncd is a light-weight single-threaded asynchronous RPC server. It is efficient
especially when server needs to handle a large number of concurrent connections where
connection-per-thread or connection-per-process model is not appropriate to consider.

* Stands as a generic event-based server library. (single-thread)
* Embeddable library module - you write main().
* Pluggable protocol handlers.
* Complete HTTP protocol handler. (support chunked transfer-encoding)
* Support request pipelining.
* Support multiple hooks.
* Support SSL - Just flip the switch on.
* Support IPv4, IPv6 and Unix Socket.
* Simple to use - Check out examples.

## Compile & Install.
```
$ git clone https://github.com/wolkykim/libasyncd
$ cd libasyncd
$ ./configure
# If using system wide install of qlibc, add QLIBC=system to make install command
$ make install
```

## API Reference

* [libasyncd API reference](http://wolkykim.github.io/libasyncd/doc/html/globals_func.html)

## "Hello World", Asynchronous Socket Server example.
```c
int my_conn_handler(short event, ad_conn_t *conn, void *userdata) {
    if (event & AD_EVENT_WRITE) {
        evbuffer_add_printf(conn->out, "Hello World.");
        return AD_CLOSE;
    }
    return AD_OK;
}

int main(int argc, char **argv) {
    ad_log_level(AD_LOG_DEBUG);
    ad_server_t *server = ad_server_new();
    ad_server_set_option(server, "server.port", "2222");
    ad_server_register_hook(server, my_conn_handler, NULL);
    return ad_server_start(server);
}
```

## "Hello World", Asynchronous HTTPS Server example.
```c
int my_http_get_handler(short event, ad_conn_t *conn, void *userdata) {
    if (event & AD_EVENT_READ) {
        if (ad_http_get_status(conn) == AD_HTTP_REQ_DONE) {
            ad_http_response(conn, 200, "text/html", "Hello World", 11);
            return ad_http_is_keepalive_request(conn) ? AD_DONE : AD_CLOSE;
        }
    }
    return AD_OK;
}

int my_http_default_handler(short event, ad_conn_t *conn, void *userdata) {
    if (event & AD_EVENT_READ) {
        if (ad_http_get_status(conn) == AD_HTTP_REQ_DONE) {
            ad_http_response(conn, 501, "text/html", "Not implemented", 15);
            return AD_CLOSE; // Close connection.
        }
    }
    return AD_OK;
}

int main(int argc, char **argv) {

    SSL_load_error_strings();
    SSL_library_init();

    ad_log_level(AD_LOG_DEBUG);
    ad_server_t *server = ad_server_new();
    ad_server_set_option(server, "server.port", "8888");
    ad_server_set_ssl_ctx(server, ad_server_ssl_ctx_create_simple("ssl.cert", "ssl.pkey"));
    ad_server_register_hook(server, ad_http_handler, NULL); // HTTP Parser is also a hook.
    ad_server_register_hook_on_method(server, "GET", my_http_get_handler, NULL);
    ad_server_register_hook(server, my_http_default_handler, NULL);

    return ad_server_start(server);
}
```

Please refer sample codes such as echo example in examples directory for more details.

## References

* [C10K problem](http://en.wikipedia.org/wiki/C10k_problem)
* [libevent library - an event notification library](http://libevent.org/)
* [qLibc library - a STL like C library](http://wolkykim.github.io/qlibc/)

## Contributors

The following people have helped with suggestions, ideas, code or fixing bugs:
(in alphabetical order by first name)

* [Carpentier Pierre-Francois](https://github.com/kakwa)
* [Dmitri Toubelis](https://github.com/dtoubelis)
* [Seungyoung Kim](https://github.com/wolkykim) - The author
