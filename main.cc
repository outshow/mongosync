#include "mongosync.h"
#include "util.h"
#include "log.h"

#include <iostream>

int shards_num = 1;
void *sync_oplog_thread(void *args) {
  MongoSync *mongosync = reinterpret_cast<MongoSync *>(args);
  mongosync->MongosSyncOplog();

  delete mongosync;
  pthread_exit(NULL);
}

int cloning_thread = 0;
void *clone_db_thread(void *args) {
  MongoSync *mongosync = reinterpret_cast<MongoSync *>(args);
  mongosync->MongosCloneDb();

  delete mongosync;
  cloning_thread--;
  pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	/*
	 *  first set default log level to INFO
	 */
	mlog::Init(mlog::kInfo, "./log", "mongosync");
	LOG(INFO) << util::GetFormatTime() << "\tmonogosync started, first set log level to INFO" << std::endl;

  mongo::client::GlobalInstance instance;
  if (!instance.initialized()) {
		LOG(FATAL) << util::GetFormatTime() << "\tfailed to initialize the client driver: " << instance.status() << std::endl;
    exit(-1);
  }
	
  Options opt;
  if (argc == 3 && (strncmp(argv[1], "-c", 2) == 0)) {
    opt.LoadConf(argv[2]);
  } else {
    opt.ParseCommand(argc, argv);	
  }

	if (!opt.log_level.empty()) {
		LOG(INFO) << util::GetFormatTime() << "\twith log level option, set log level to " << opt.log_level << std::endl; 
		if (!mlog::SetLogLevel(opt.log_level)) {
			LOG(WARN) << util::GetFormatTime() << "\tlog level option value invalid, set log level to default to INFO" << std::endl;
		}
	}
    // check opt
    if (!opt.ValidCheck()) {
        exit(-1);
    }
    
  // mongos -> mongos
  if (opt.is_mongos) {
    std::vector<std::string> all_dbs;
    if (!opt.no_shard_auth && (opt.shard_user.empty() || opt.shard_passwd.empty())) {
      LOG(FATAL)
        << util::GetFormatTime()
        << "\tShard username or password should not be empty when src is mongos\n"
        << std::endl;
      return -1;
    }
    // Get mongos shards host info
    MongoSync *mongos_mongosync = MongoSync::NewMongoSync(&opt);
    if (!mongos_mongosync) {
      LOG(FATAL) << util::GetFormatTime() << "\tCreate mongosync instance failed" << std::endl;
      return -1;
    }
    std::vector<std::string> shards = mongos_mongosync->GetShards();
    if (mongos_mongosync->IsBalancerRunning()) {
      LOG(FATAL) << util::GetFormatTime() << "\tBalancer is running" << std::endl;
      return -1;
    }
    if (mongos_mongosync->IsBigChunkExist()) {
      LOG(FATAL) << util::GetFormatTime() << "\tBig chunk exist" << std::endl;
      return -1;
    }

    mongos_mongosync->GetAllDb(&all_dbs);
    delete mongos_mongosync;

    // Create connection between shard and dst mongos
    int ret;
    pthread_t tid;
    std::vector<pthread_t> tids;
    std::vector<std::string> shard_ips;
    std::vector<MongoSync*> shard_mongosync;
    Options shard_opt(opt);
    shard_opt.src_user = opt.shard_user;
    shard_opt.src_passwd = opt.shard_passwd;
    shards_num = shards.size();
    for (int i = 0; i < shards.size(); i++) {
      size_t slash_pos = shards[i].find('/');
      size_t comma_pos = shards[i].find(',');
      std::string shard_addr =
        shards[i].substr(slash_pos + 1, comma_pos - slash_pos - 1);

      // find a SECONDARY src mongodb
      MongoSync *mongosync = NULL;
      shard_opt.src_ip_port = shard_addr;
      mongosync = MongoSync::NewMongoSync(&shard_opt); // delete in thread
      std::string readable_host = shard_addr;
      if (!mongosync || !mongosync->GetReadableHost(&readable_host)) {
        LOG(FATAL) << util::GetFormatTime() << "\tCreate shard mongosync instance failed" << std::endl;
        return -1;
      }
      if (readable_host != shard_addr) {
        delete mongosync;
        shard_opt.src_ip_port = readable_host;
        mongosync = MongoSync::NewMongoSync(&shard_opt);
      }

      mongosync->MongosGetOplogOption();
      shard_mongosync.push_back(mongosync);
    }

    if (opt.only_sync_oplog) {
        // only sync oplog
        LOG(INFO) << util::GetFormatTime() << "\tSkip data cloned, only sync oplog start at: " << opt.oplog_start.sec<< "," << opt.oplog_start.no << " and stop at: " << opt.oplog_end.sec << "," << opt.oplog_end.no << std::endl;
    }else{
        // cloned data
        LOG(INFO) << util::GetFormatTime() << "\tStart cloeing data ... " << std::endl;
        for (int i = 0; i < all_dbs.size(); i++) {
          Options subopt(opt);
          subopt.db = all_dbs[i];
          if (!opt.db.empty() && subopt.db != opt.db) {
            continue;
          }
          cloning_thread++;
          mongos_mongosync = MongoSync::NewMongoSync(&subopt);
          ret = pthread_create(&tid, NULL, clone_db_thread, (void *)mongos_mongosync);
          if (ret != 0)
            return -1;
          LOG(INFO) << util::GetFormatTime() << "\tNew thread cloning db: " << all_dbs[i] << std::endl;
          pthread_setname_np(tid, "clone_db_thread");
          tids.push_back(tid);
          while(cloning_thread > 10) {
            sleep(1);
          }
        }

        for (int i = 0; i < tids.size(); i++) {
          pthread_join(tids[i], NULL);
        }

        LOG(INFO) << util::GetFormatTime() << "\tAll db have been cloned ... " << std::endl;
    }

    tids.clear();
    for (int i = 0; i < shard_mongosync.size(); i++) {
      MongoSync* mongosync = shard_mongosync[i];
      ret = pthread_create(&tid, NULL, sync_oplog_thread, (void *)mongosync);
      if (ret != 0)
        return -1;
      pthread_setname_np(tid, "sync_oplog_thread");
      tids.push_back(tid);
    }

    for (int i = 0; i < tids.size(); i++) {
      pthread_join(tids[i], NULL);
    }
  } else {
    // Others sync methods
    MongoSync *mongosync = MongoSync::NewMongoSync(&opt);
    if (!mongosync) {
      return -1;
    }
    mongosync->Process();
    delete mongosync;
  }

  return 0;
}
