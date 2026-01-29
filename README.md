# SMoLKVM

```

┌┐┌┬┐┌┐┬ ┬┌\   /┬┐
└┐││││││ ├┴┐\ /│││
└┘┴ ┴└┘┴─┴ └┘× ┴ ┴
 ~dgp

```

## What

This is a single header implementation of a very crappy "virtual machine".

## Features

- One vcpu, its basically impossible to do any better without threads.
- Dumb console emulation to a unix domain socket so you can connect minicom.
- Direct entry into long mode.
- meh grade gdb stub.
- lots of bugs
- It should, eventually, compile to a completely self standing static binary
  with nolibc at some point but nolibc needs to get wrappers for the socket
  stuff and signal support.
