#include "redismodule.h"

int ZDiffRangeByScore_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **, int);
int ZDiffRevRangeByScore_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **, int);
int ZInterRangeByScore_RedisCommand(RedisModuleCtx *, RedisModuleString **, int);
int ZInterRevRangeByScore_RedisCommand(RedisModuleCtx *, RedisModuleString **, int);

int SDiffCard_RedisCommand(RedisModuleCtx *, RedisModuleString **, int);
int SInterCard_RedisCommand(RedisModuleCtx *, RedisModuleString **, int);
int SUnionCard_RedisCommand(RedisModuleCtx *, RedisModuleString **, int);
