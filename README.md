For simplicity just run
```bash
./simple_run.sh
```
It will do everything.

But if you want to compile it and run by yourself, then run
```bash
gcc sender.c -o sender
gcc receiver.c -o receiver
```
It will give two executables.\
"./receiver" runs with arguments such as <receiver_port> <drop_probability>\
"sender" runs with arguments such as <sender_port> <receiver_ip> <receiver_port> <timeout> <filename> <drop_probability>
