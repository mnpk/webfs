# WEB-FS

Virtual File System bind to web servers.

```bash
# nothing in mnt
$ ls mnt
$

# mount mnt to http://alice
$ ./webfs mnt

$ curl http://alice/hello.txt
Hello, world!

$ cat mnt/hello.txt
Hello, world!

```

## Build
- Need C++11 Support (tested on gcc 4.9.2)

```bash
$ mkdir build
$ cd build
$ cmake ..
$ make
```



