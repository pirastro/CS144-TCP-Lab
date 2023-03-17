# Lab 0: webget 

#### 1. Program Structure and Design:

使用Sponge中的`TCPSocket`和`Address`类，通过http协议的请求格式和socket接受网页信息。

读取并输出server的所以信息直到socket达到“EOF”。

#### 2. Implementation Challenges:

根据tutorial中的指导，在本次实验中未遇到较大挑战。在第一次build测试webget功能时出现了报错，修改了test中`webget_t.sh`的权限后，解决了这一问题。





# Lab 2: byte_stream stream_reassembler

*实现byte_stream和stream_reassembler*

#### 1. Program Structure and Design:

##### (1) byte_stream: 

使用了元素类型为 char 的 deque 作为容器实现byte stream 的buffer，数据`data` 的每一个字节当做`_buffer` 的一个元素。
因此可以直接使用deque 的pop_front(), push_back() 实现byte stream 的读和写。

##### (2) stream_reassembler:

同样使用元素类型为 char 的 deque 队列 `_unassembled_storage` 作为容器来存储"unassembled substrings"，一个字节作为一个元素。
初始化 `_unassembled_storage` 时，其大小由"capacity" 决定，暂时将其元素设置为 `"\0"`， 由于收到的数据也可能有 `"\0"`，因此额外使用了一个同样大小的bool 类型队列 `_empty_flag` 来记录`_unassembled_storage` 的某个位置是否为空。

由于byte stream 中也可能有尚未被读取的数据需要占用"capacity"，因此 `_unassembled_storage` 中实际可存储的空间需要用"capacity" 减去byte stream 的大小。在实验中，通过对传入数据的 `index` 加以限制而实现对"unassembled bytes" 占用空间的限制。即只接受在 "**first unassembled**" 和"**first unacceptable**" 之间的数据。

在处理重叠数据方面，因为收到的substring中每个字节的index 与 `_unassembled_storage` 的元素位置一一对应（ `_unassembled_storage`的队头永远对应"first unassembled" 的字节），所以只需遍历一遍在"first unassembled" 和"first unacceptable" 之间的部分，若`_unassembled_storage`的该位置为空就把该字符写入。

在写入byte stream 方面，只要 `_unassembled_storage` 的队头不为空则一直写入stream，将队头元素写入后则删除队头并在队尾加入空字符元素保持队列长度不变，`_empty_flag` 队列也做相同处理。


#### 2. Implementation Challenges:

在开始设计存储 unassembled strings时，我本想以string 的方式存储数据，因此用`map<size_t, string>` 实现了 `_unassembled_storage`：key用以存储子串第一个字节的index，value存储substring的整个字符串。这样导致了将子串存入map 的处理十分复杂，必须考虑接收到的子串 `data` 与map中已有子串的重叠情况，分别对map 中最靠近 `data` 的前后的子串进行分类讨论，还需要遍历找到 `data` 两侧最近的子串。实现后在测试时出现了超时的情况，最后只好放弃这种方法。

用队列实现 `_unassembled_storage` 后，测试时出现了EOF的错误，检查后发现是判断 `data` 是否与已输出数据重复的条件没有考虑到空字符的情况：用 `index + data.size() < _first_unassembled` 判断是否重复，则会忽略掉`data = "", eof = true` 的输入，导致了"The reassembler was expected to be at EOF, but was not" 的错误。把 < 改成 <= 就解决了这一问题。





Lab 3: wrapping_integers tcp_receiver
=============

#### 1. Program Structure and Design:
##### wrapping integers
1. **wrap:** 
wrap的实现较为简单，先将absolute sequence number `n` 转换为 WrappingInt32 类型，再使用wrapping_integers.hh 中的helper函数`operator+()` 将其与`isn` 相加即可。
2. **unwrap:** 
先用`operator-(n, isn)` 计算sqn 相较于isn 的偏移量`offset`，观察到实际的absolute sqn 等于`offset` 或者 `offset` + $2^{32 + n} (n = 0, 1, 2, 3...)$。为了确定目标absolute sqn，需要借助checkpoint 确定范围，即找到距离checkpoint 最近的满足条件的absolute sqn。
首先，如果checkpoint 小于等于`offset`，说明`offset` 即为目标absolute sqn，直接返回即可；如果checkpoint 大于`offset`，说明目标absolute sqn 与`offset` 之间可能相差$2^{32 + n}$，目标的范围在checkpoint 左右两侧$2^{31}$ 之内，因此只需计算`offset` 与checkpoint + $2^{31}$ 之间相差多少个$2^{32}$ ，最后给`offset` 加上相差的$2^{32}$即为目标absolute sqn。

##### tcp receiver
1. **segment_received:** 
若尚未设置isn，即`_syn_flag` 为false，且收到的segment 的header 中SYN flag 也为false，说明尚不能确定元素的sqn 以及stream idx，直接舍弃即可。
若isn已被设置，则可以接收。注意到absolute sqn 比stream idx 大1，因此将下一个需要写入的stream idx 加1作为absolute sqn 的check point。调用wrapping integers 中实现的`unwrap` 函数，即可将segment header 中的sqn 转化为absolute sqn，再减一即为stream idx。此外还需要判断此segment 的payload 中是否有syn 位，若有需要给stream idx 加一避免syn 的sqn引起混淆。

2. **ackno:** 
若尚未设置isn，直接返回空。
若isn已被设置，则需要返回下一个需要被写入的数据的sqn，即窗口的左边界（stream reassembler 中的“first unassembled”）的sqn，调用wrap函数即可：`wrap(_reassembler.stream_out().bytes_written() + 1, _isn)`，需要注意的是，若输入以及到达eof，返回的sqn需要再加一位FIN 的占位。

3. **window_size:** 
直接返回`_capacity` 减去byte stream 的大小即可。

#### 2. Implementation Challenges:

wrapping integers 中的难点主要在于根据checkpoint 确定absolute sqn 的范围。解决方法是需要先给checkpoint 加上$2^{31}$ 再减去`offset`，继而除以$2^{32}$，计算得到相差多少个$2^{32}$。

tcp receiver 实现的难点主要在于需要考虑sqn、absolute sqn、stream idx 之间相互转换的细节，比如payload 中的SYN 位并不是stream 的一部分，因此如果当前segment 的元素含有SYN 的话需要给stream idx 加一。



Lab 4: tcp_sender
=============

##### Program Structure and Design of the TCPSender:
使用了`map<size_t, TCPSegment> _in_flight_map`用以暂时存储已经发出但尚未被确认的segments，每个在`fill_window()` 中发送的segment 都被存入 `_in_flight_map`，直到收到receiver 的ackno，即可以删去`_in_flight_map` 中seqno 小于 ackno 的segment（说明已被确认）。需要注意receiver 发出的 ackno 的类型是`WrappingInt32`，需要先调用unwrap 函数`unwrap(ackno, _isn, _next_seqno)` 将其转换为 absolute_ackno，才能与segment 的seqno 相比较。

在本次实验中，共有三种情况需要重新设置计时器`_timer`。首先是在`fill_window()` 中，如果发送当前segment 时，没有已发送但未被确认的segment（即`_in_flight_map`）为空，则需要重设定时器：`_timer = 0`；其次是在`ack_received()` 中，收到receiver 的ackno，并且`_in_flight_map` 中有被确认（删去）的segment 时；最后是在`tick` 中，如果出现超时并且重发了segment，也需要重设定时器。需要注意的是，如果在重发时receiver 的window size 不为0，说明可能出现网络拥塞，需要将超时的界限`_rto` 乘2。

##### Implementation Challenges:
本次实验的难点主要在于细节较多，例如需要在不同情况下对计时器做出不同处理，容易出现错误。
一开始在test 13中出现了错误：`The test "Retx SYN twice at the right times, then ack" failed`. 当syn需要被发送两次然后确认是出现了错误，经检查发现是由于window size 的初始值被设为0，导致这种情况下在超时没有将`_rto` 乘2，造成了这一错误。窗口初始值经改正为1后正确运行。





Lab 5: tcp_connection
=============

#### 1. Program Structure and Design:
##### Receiving segments
1. 如果segment 设置了reset 标志位，需要执行reset 操作：将inbound 和outbound stream 都设置为error 状态，并且切断连接（即将指示连接的bool 变量`_active_flag`赋值false）。
2. 将segment 传递给receiver，由于receiver 足够鲁棒，可以处理具有乱序到达等问题的segment，此处不需做其他处理。
3. 如果到达的segment 占用了sequence number（即seg.length_in_sequence_space()），则需要向另一端发送具有ack 的segment。此时sender 可能会因为收到了新的ack 和窗口大小而重新fill window，因此sender 的输出队列不为空，这种情况下直接加上ack 相关field 并发送即可。但也有可能此时sender 的输出队列为空，这时需要sender 发送一个空segment，并添加相关field。

##### Sending segments
发送segment的实现较为简单，只需将sender 的输出队列中所有segment 出队并加上ack 相关field，再发送即可。

##### Deciding when to fully terminate
首先可以明确，TCP connection 终止只会出现在两种情况下：**当receiver 收到新的segment**（**a.** 收到了具有rst flag 的segment, **b.** 或收到的包使receiver 和sender 的状态发生更新）；**当tick() 被调用时**（**a.** 调用sender 的tick 方法后，发现重传次数超出限制`TCPConfig::MAX_RETX_ATTEMPTS`，需要立刻reset 并通知远端， **b.** 发现距离上次重传已经过限制时间`10 * _cfg.rt_timeout`，并且receiver 和receiver 的状态以达到终止要求）。

1. **在segment_received中**：
若收到的segment 具有reset flag，则直接调用`_reset()` 方法，将输入输出流均设为error 状态，并将`_active_flag` 赋为false，`active()` 会返回false，这是一个unclean shutdown。
在状态更新后：如果输入流比输出流先达到EOF，则无需linger，因此将`_linger_after_streams_finish` 赋为false；如果输入输出流均已达到EOF 并且此时`_linger_after_streams_finish` 为false，则直接终止，即把`_active_flag` 赋为false，这是一个clean shutdown。

1. **在tick中**：
若发现重传次数超出限制`TCPConfig::MAX_RETX_ATTEMPTS`，需要立刻reset：将输入输出流均设为error 状态，并将`_active_flag` 赋为false，并向远端发送一个具有reset flag 的segment，这是一个unclean shutdown。
若输入输出流均已达到EOF，并需要linger，此时若距离上次重传已超过限制时间`10 * _cfg.rt_timeout`，则直接终止，即把`_active_flag` 赋为false，这是一个clean shutdown。

##### Results

 实现的吞吐量结果为：
CPU-limited throughput: 0.15 Gbits/s
CPU-limited throughput with reordering: 0.16 Gbits/s


#### 2. Implementation Challenges:
本次实验的主要难点在于实现的细节，比如实现某个方法时，调用其他方法的先后顺序等问题。
* 在`segment_receiver()` 中，由于带有reset flag 的segment 仍可能具有其他有效信息，因此需要先调用sender 的`segment_received()`，再判断是否需要reset。
* 在`segment_receiver()` 中，一开始只考虑了收到占用sequence number 的segment 才需要发送segment。但其实即使收到的segment 不占用sequence number，也可能具有ack flag，传达新的ack 信息，通知sender新的window size 和ackno，如果窗口有新的空间，sender也会发送新的segment。因此，在调用`_sender.ack_received()` 后，如果sender 的输出队列不为空，也需要在tcp connection 进行发送。
* 在`tick()` 中，调用`_sender.tick()`方法后，sender 可能会重新发送一个in-flight segment，因此也需要在tcp connection 中进行发送。






# Lab 6: network_interface

#### 1. Program Structure and Design:

使用`unordered_map<uint32_t, EthernetAddress> _mapping`来记录next-hop IP address与Ethernet address的映射：IP address作为key，Ethernet address作为value。由于映射具有有效期限，超时的映射需要被删除，因此再利用一个`unordered_map<uint32_t, size_t> _timing`来记录每个映射已经被缓存的时间：IP address作为key，存在时间作为value。调用`tick()`方法后，增加`_timing`中记录的时间，若超出30s的时间限制，则两个unordered_map中的映射均删去。

#### 2. Implementation Challenges:

在发送datagram时，若Ethernet address未知(`_mapping`中不存在key为`next_hop_ip`的映射)，则需要先将datagram和next_hop_ip都存储起来，以供收到目标Ethernet address后再发送。在该实验中使用了两个vector分别存储datagram和next_hop_ip，在后续的查找和删除操作中，只需要对两个vector做相同的操作即可。

需要注意的是，为了避免向网络中发送过多的ARP request，如果5s内已经发送过当前ip address的ARP request，则不可继续发送，只需等待第一个的回复即可。在实现中，使用了`unordered_map<uint32_t, size_t> _arpRequest`来记录每个ARP request距离发送过去的时间：IP address作为key，距离发送过去的时间作为value。调用`tick()`方法后，增加记录的时间，若超出5s则已无意义，删去。





# Lab 7: router

####  1. Program Structure and Design:

用结构体`struct routeTableItem`作为路由表表项，然后用`vector<routeTableItem>`实现路由表`_route_table`。在匹配地址时，只需遍历路由表，并比较目标地址与路由前缀`route_prefix`均右移(32-子网前缀长度)即可。

#### 2. Implementation Challenges:

一开始没有考虑到前缀长度为0的默认匹配路由情况，导致了错误，增加了`iter->route_prefix == 0`的条件后可以正常运行。

