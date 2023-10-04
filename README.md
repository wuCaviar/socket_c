# Linux 下 socket 编程 C语言 版

包含单线程版、多线程版、线程池版

```bash
.
├── README.md
├── socket
│   ├── client.c
│   └── server.c
├── socket_thread
│   ├── client.c
│   └── server_thread.c
└── socket_threadpoll
    ├── client.c
    ├── server_threadpoll.c
    ├── threadpoll.c
    └── threadpoll.h
```

- socket: 单线程版
- socket_thread: 多线程版
- socket_threadpoll: 线程池版
