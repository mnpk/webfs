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
