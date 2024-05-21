# README

[中文wiki前往](https://github.com/Qihoo360/mongosync/wiki)

## 1.Introduction 

This simpe program is for transfering data between two mongo nodes, including:

* data cloning;
* oplog syncing(applying one mongo databbase's oplog to another mongo database) ;
* oplog cloning(not applying, just storing the oplogs from source mongo database to another one)

## 2.Usage

* Requires
  ```
  gcc < 11

  gcc==4.4 or gcc==4.8 is also good

  ```

* Get the source code

  ```shell
  $ git clone https://github.com/outshow/mongosync.git
  ```

* install depenncies

  ```
  yum install -y scons
  yum install -y boost boost-devel
  yum install -y openssl-devel

  ```

* Compile

  Enter the source code and typewrite:

  ```shell
  $ make
  ```

  after that, a "mongosync" named execution file appears in the $(MONGOSYNC_ROOT)/output directory, and also a "mongosync.conf" named config file

  *SP:*

  a. maybe some depenncies not installed in your machine, install them according to the tips（mostly sons, boost and ssl is needed）

  b. don't use c++11 to compile this program, otherwise will occur error

* Exectution

  The program can launches with command line or config file，and the specified usage can be obtained by:

  ```shell
  $ cd output
  $ ./mongosync --help
  Follow is the mongosync-surpported options:
  --help                   to get the help message
  -c conf.file             use config file to start mongosync
  --src_srv arg            the source mongodb server's ip port
  --src_user arg           the source mongodb server's logging user
  --src_passwd arg         the source mongodb server's logging password
  --src_auth_db arg        the source mongodb server's auth db
  --src_use_mcr            force source connection to use MONGODB-CR password machenism
  --dst_srv arg            the destination mongodb server's ip port
  --dst_user arg           the destination mongodb server's logging user
  --dst_passwd arg         the destination mongodb server's logging password
  --dst_auth_db arg        the destination mongodb server's auth db
  --is_mongos              the source mongodb server is mongos
  --no_shard_auth          if the source mongos server has auth setting
  --sd_sync_mode           the sync mode of stock data, normal or snapshot
  --op_sync_mode           the sync mode of oplog, full or incr
  --shard_user arg         the source mongos server's shard username
  --shard_passwd arg       the source mongos server's shard password
  --dst_use_mcr            force destination connection to use MONGODB-CR password machenism
  --db arg                 the source database to be cloned
  --dst_db arg             the destination database
  --coll arg               the source collection to be cloned
  --colls arg              the source collection list name
  --dst_coll arg           the destination collection
  --oplog                  whether to sync oplog
  --raw_oplog              whether to only clone oplog
  --op_start arg           the start timestamp to sync oplog
  --op_end arg             the start timestamp to sync oplog
  --dst_op_ns arg          the destination namespace for raw oplog mode
  --no_index               whether to clone the db or collection corresponding index
  --filter arg             the bson format string used to filter the records to be transfered
  --bg_num arg             the background thread number for cloning data(not oplog syncing and oplog storing)
  --batch_size arg         the data grouping size criterion in cloning data(0-16M, default to 16M), unit is Byte
  --log_level arg          specify the log level(INFO, WARN, FATAL)

  ```

## 3.Performance

Rough testing under the situation:

* Ping from this program's exectution's machine to the mongo db server's machine(two machines) is about **1.5ms**；
* total about **36,400,000** documents；
* database size about **1.09GB**

transfering time is about **379 seconds**(even qps: 96.04k).

**SP**: The bottleneck is writing, So using multiple threads(multi-conn) to accelerate the speed of that(but also limited by database server's writing speed). 

 
