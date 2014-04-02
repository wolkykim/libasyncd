libasyncd
=========

Embeddable Event-based Asynchronous Message/HTTP Server library in C.

## What is libasyncd?

Libasyncd is an embeddable fully event-driven asynchronous message server in C
which is in flexible architecture to add/support multiple different protocols.

Libasyncd project was started from my need of embeddable HTTP server. There is
a couple of choices there, I tried all of them. Each has pros and cons.
I couldn't pick my favorite at the end and here's my insight.

* libevent provides one - good choice for light use but doesn't provide a way
  of fine controls over large data.
* mongoose - nice API, good abstraction, but since it uses select() and realloc
  based i/o buffer, wouldn't be a promising solution for serious users.
  Also its dual lisencing is restricting.
* libevhtp - fine but doesn't provide a way to hook on METHOD(Verb, a command
  on the 1st line)
* GNU's libmicrohttpd - good feature set but little too complicated to use.

What I was looking for was.

* Must be easily embeddable.
* Simple and nice abstraction that cut down the complexity.
* Generic enough to add and customize lighter weight custom protocols.
* Practical level of support in HTTP handler.
* Flexible hook supports on both Method hook and Gradual way.
* Snap-on SSL feature.

So with these feature set, applications can expose RESTful API as customer facing
interface and also use more lighter weight protocols for inter communication
without doing integration work between this modules and that modules.

Asynchronous way of programming can easily go quite complicated as more things
you handle in nonblocking passion. So the goal of Libasyncd project is to make
a flexible and fast event-based message server base with good abstraction.

Hope you find libasyncd is useful and help us to make a fast event server
without thinking much about the underneath complexity.

## Why libasyncd?

* Stands as a generic event-based server library.
* Not only for HTTP server but also as a RPC server, as a Protocol Buffer channel,
  as a Message transforming layer...
* Embeddable library module - you write main().
* Simple to use.
* Support HTTP(coming soon) and BYPASS handler.
* Support of multiple hooks.
* Fine control - Provides gradual hooks as well as method hooks.
* Supports SSL - Just flip the switch on.

## Compile & Install.
```
$ git clone git clone https://github.com/wolkykim/libasyncd
$ cd libasyncd
$ (cd lib; run2init-submodules.sh; cd qlibc; ./configure; make) 
$ ./configure
$ make
$ src/main_example
wolkykim@arena:/home/wolkykim/ws/libasyncd $ src/main_example
[DEBUG] Created a server object. [ad_server_new(),ad_server.c:53]
[DEBUG] Starting a server. [ad_server_start(),ad_server.c:62]
[DEBUG] server.port=2222 [ad_server_start(),ad_server.c:70]
[DEBUG] server.addr=0.0.0.0 [ad_server_start(),ad_server.c:70]
[DEBUG] server.backlog=128 [ad_server_start(),ad_server.c:70]
[DEBUG] server.timeout=5 [ad_server_start(),ad_server.c:70]
[DEBUG] server.enable_ssl=0 [ad_server_start(),ad_server.c:70]
[DEBUG] server.ssl_cert=/usr/local/etc/ad_server/ad_server.cert [ad_server_start(),ad_server.c:70]
[DEBUG] server.protocol_handler=bypass [ad_server_start(),ad_server.c:70]
[DEBUG] server.start_detached=0 [ad_server_start(),ad_server.c:70]
[DEBUG] server.free_on_stop=1 [ad_server_start(),ad_server.c:70]
[DEBUG] Listening on 0.0.0.0:2222 [ad_server_start(),ad_server.c:131]
```

## Super simple "Hello World" server example.
```
int my_bypass_handler(short event, void *conn, void *userdata) {
    evbuffer_add(req->out, "Hello World.");
    return AD_CLOSE;
}

int main(int argc, char **argv) {
    ad_server_t *server = ad_server_new();
    ad_server_set_option(server, "server.port", "2222");
    ad_server_register_hook(server, 0, my_bypass_handler, NULL);
    return ad_server_start(server);
}
```

## Looking for more controls? Echo server example here.

```
/**
 * User data for per-connection custom information for non-blocking operation.
 */
struct my_cdata {
    int counter;
};

/**
 * User callback example.
 *
 * This is a simple echo handler.
 * It response on input line up to 3 times then close connection.
 *
 * @param event event type. see ad_server.h for details
 * @param conn  connection object. type is vary based on
 *              "server.protocol_handler" option.
 * @userdata    given user-data registered at the
 */
int my_bypass_handler(short event, void *conn, void *userdata) {
    DEBUG("my_bypass_callback: %x", event);
    ad_bypass_t *req = conn;

    /*
     * AD_EVENT_INIT event is like a constructor method.
     * It happens only once at the beginning of connection.
     * This is a good place to create a per-connection base
     * resources. You can attach it into this connection to
     * use at the next callback cycle.
     */
    if (event & AD_EVENT_INIT) {
        DEBUG("==> AD_EVENT_READ");
        // Allocate a counter container for this connection.
        struct my_cdata *cdata = (struct my_cdata *)calloc(1, sizeof(struct my_cdata));

        // Attach to this connection.
        ad_bypass_set_userdata(conn, cdata);
    }

    /*
     * AD_EVENT_READ event happens whenever data comes in.
     */
    else if (event & AD_EVENT_READ) {
        DEBUG("==> AD_EVENT_READ");

        // Get my per-connection data.
        struct my_cdata *cdata = (struct my_cdata *)ad_bypass_get_userdata(conn);

        // Try to read one line.
        char *data = evbuffer_readln(req->in, NULL,  EVBUFFER_EOL_ANY);
        if (data) {
            if (!strcmp(data, "SHUTDOWN")) {
                //return AD_SHUTDOWN;
            }
            cdata->counter++;
            evbuffer_add_printf(req->out, "%s, counter:%d, userdata:%s\n", data, cdata->counter, (char*)userdata);
            free(data);
        }

        // Close connection after 3 echos.
        return (cdata->counter < 3) ? AD_OK : AD_CLOSE;
    }

    /*
     * AD_EVENT_WRITE event happens whenever out-buffer has lesser than certain
     * amount of data.
     *
     * Default watermark is 0 meaning this will happens when out-buffer is empty.
     * For reasonable size of message, you can send it all at once but for a large
     * amount of data, you need to send it out through out multiple callbacks.
     *
     * To maximize the performance, you will also want to set higher watermark
     * so whenever the level goes below the watermark you will be called for the
     * refill work before the buffer gets empty. So it's just faster if you can
     * fill up gas while you're driving without a stop.
     */
    else if (event & AD_EVENT_WRITE) {
        DEBUG("==> AD_EVENT_WRITE");
        // We've sent all the data in out-buffer.
    }

    /*
     * AD_EVENT_CLOSE event happens right before closing connection.
     * This will be the last callback for this connection.
     * So if you have created per-connection data then release the resource.
     */
    else if (event & AD_EVENT_CLOSE) {
        DEBUG("==> AD_EVENT_CLOSE=%x (TIMEOUT=%d, SHUTDOWN=%d)",
              event, event & AD_EVENT_TIMEOUT, event & AD_EVENT_SHUTDOWN);
        // Get per-connection data and set it to NULL, otherwise server will complain
        // about possible memory leak.
        struct my_cdata *cdata = (struct my_cdata *)ad_bypass_set_userdata(conn, NULL);
        if (cdata) {
            free(cdata);
        }
    }

    // Return AD_OK will let the hook loop to continue.
    return AD_OK;
}

int main(int argc, char **argv) {
    // Example shared user data.
    char *userdata = "SHARED-USERDATA";

    //
    // Create a server.
    //
    ad_server_t *server = ad_server_new();

    //
    // Set server options.
    //
    // Usually you need to override a few default options and that's it
    // but it's lengthy here for a demonstration purpose.
    //
    ad_server_set_option(server, "server.port", "2222");
    ad_server_set_option(server, "server.addr", "0.0.0.0");
    ad_server_set_option(server, "server.timeout", "5");

    // Set protocol handler.
    //   - bypass : Use bypass handler. This is a transparent handler for
    //              build an custom protocols.
    //   - http   : Use HTTP handler. Request message will be parsed by
    //              the handler. You can put your hooks on method name or
    //              on each phase of parsing process like "AFTER_HEADER".
    //   - euca   : Use EUCA handler. This handler is for EUCA message.
    //              light weight messaging protocol designed for the
    //              blasting fast performance in data exchange.
    ad_server_set_option(server, "server.protocol_handler", "bypass");

    // Register custom hooks. When there are multiple hooks, it will be
    // executed in the same order as it registered.
    ad_server_register_hook(server, 0, my_bypass_handler, userdata);

    // SSL options. - Not implemented yet.
    ad_server_set_option(server, "server.server.enable_ssl", "0");
    ad_server_set_option(server, "server.ssl_cert", "/usr/local/etc/ad_server/ad_server.cert");

    // Run server in a separate thread. If you want to run multiple
    // server instances or if you want to run it in background, this
    // is the option.
    ad_server_set_option(server, "server.start_detached", "0");

    // Call ad_server_free() internally when server is shutting down.
    ad_server_set_option(server, "server.free_on_stop", "1");

    //
    // Start server.
    //
    int retstatus = ad_server_start(server);

    //
    // That is it!!!
    //
    return retstatus;
}
```

## References

* [C10K problem](http://en.wikipedia.org/wiki/C10k_problem)
* [libevent library - an event notification library](http://libevent.org/)
* [qLibc library - a STL like C library](http://wolkykim.github.io/qlibc/)
