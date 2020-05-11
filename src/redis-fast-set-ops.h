#include "redismodule.h"

int ZInterRangeByScore_RedisCommand(RedisModuleCtx *, RedisModuleString **, int);
int ZInterRevRangeByScore_RedisCommand(RedisModuleCtx *, RedisModuleString **, int);

int SDiffCard_RedisCommand(RedisModuleCtx *, RedisModuleString **, int);
int SInterCard_RedisCommand(RedisModuleCtx *, RedisModuleString **, int);
int SUnionCard_RedisCommand(RedisModuleCtx *, RedisModuleString **, int);
