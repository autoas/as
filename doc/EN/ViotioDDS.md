
# VDDS - The Virtio Ring Buffer & Shared Memory based DDS

This is a concept project inspired by iceoryx, but I see iceoryx is too complex and it need a center daemon Roudi.
Here I think the [vitrio](https://docs.kernel.org/driver-api/virtio/virtio.html) is so perfect and why not based on the virtio ring buffer to implement a virtio type DDS that dedicated for inter-process communication, so I named it VDDS.

Below is the simple diagram to show the architecture and the shared named semaphore was used to sync between reader and writer. And the atomic was used to manage the reference counter of the used DESC, thus ensure that if multiply consumers/readers, that last one decrease the reference counter to 0 do the real action to put the DESC back to the avaiable ring of the producer/writer. And atomic based spinlock was used to protect some atomic multiply actions.

The code footprint is very small, just about 1000 lines of code, good for you to study.

The idea was perfect as I think, it can be expanded to be used widely for large data sharing for inter-process communication.

```txt
The VRing memory layout: N = capability - 1, K = maxReaders - 1

  +--------------------------------+
  |           META                 |
  +--------------------------------+
  |  DESC[0]  DESC[1] ... DESC[N]  |
  +--------------------------------+
  |           AVAIL                |
  |  lastIdx              idx      |
  |  ring[0] ring[1] ... ring[N]   |
  +--------------------------------+
  |           USED[0]              |
  |  lastIdx    idx                |
  |  ring[0] ring[1] ... ring[N]   |
  +--------------------------------+
  |           USED[1]              |
  |  lastIdx    idx                |
  |  ring[0] ring[1] ... ring[N]   |
  +--------------------------------+
  |          ...                   |
  +--------------------------------+
  |           USED[K]              |
  |  lastIdx    idx                |
  |  ring[0] ring[1] ... ring[N]   |
  +--------------------------------+

The Writer from AVAIL get a free DESC, and put the DESC to those onlined reader used ring.
The DESC manage a reference counter. e.g as below shows, 2 Readers online:

            get        +-----------------+   put the DESC[0] back to the AVAIL ring
      +----------------|   AVAIL ring    |<---------------------------------------------------+
      |                +-----------------+                                                    |
      | DESC[0] is returned                                                                   |
      V           DESC[0].ref = 2                                                             |
+------------+  put     +-----------------+  get   +---------------+ put: DESC[0].ref--,      |
|   Writer   |----+---->|  USED[0] ring   |------->|   Reader[0]   |------------------[END]   |
+------------+    |     +-----------------+        +---------------+ ref > 0, stop            |
                  |                                                                           |
                  |     +-----------------+  get   +---------------+ put: DESC[0].ref--,      |
                  +---->|  USED[1] ring   |------->|   Reader[0]   |--------------------------+
                        +-----------------+        +---------------+ ref == 0, release
The Writer.put must ensure no Reader.put happens for the same DESC

The Writer.get update the AVAIL ring "lastIdx". Only the Writer will update this "lastIdx",
no need atomic action for this "lastIdx".

The Reader.put update the AVAIL ring "idx". It is possible that 2 or more readers put different
DESC at the same time thus race to update the AVAIL ring "idx". Need atomic action for this
"idx".

The Writer.put update the USED ring "idx". Only the Writer will update this "idx", no need
atomic.

The Reader.get update the used ring "lastIdx". Only the Reader will update this "lastIdx", no
need atomic.
```

```sh
scons --app=VDDSHwPub
scons --app=VDDSHwSub

# launch 1 pub
build/posix/GCC/VDDSHwPub/VDDSHwPub

# launch 3 sub in different shell
build/posix/GCC/VDDSHwSub/VDDSHwSub
build/posix/GCC/VDDSHwSub/VDDSHwSub
build/posix/GCC/VDDSHwSub/VDDSHwSub
```

On my windows WSL linux sub system, I see performance was very good.

![hello_world_sample](../images/virtio-DDS-helloworld-sample.png)