#include "redismodule.h"
#include "redis-fast-set-ops.h"

int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (RedisModule_Init(ctx,"redis-fast-set-ops",1,REDISMODULE_APIVER_1) == REDISMODULE_ERR)
		return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx, "zinterrangebyscore",
                                  ZInterRangeByScore_RedisCommand,
                                  "readonly",1,2,1)
            == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx, "zinterrevrangebyscore",
                                  ZInterRevRangeByScore_RedisCommand,
                                  "readonly",1,2,1)
            == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx, "sintercard",
                                  SInterCard_RedisCommand,
                                  "readonly",1,-1,1)
            == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx, "sdiffcard",
                                  SDiffCard_RedisCommand,
                                  "readonly",1,-1,1)
            == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx, "sunioncard",
                                  SUnionCard_RedisCommand,
                                  "readonly",1,-1,1)
            == REDISMODULE_ERR)
        return REDISMODULE_ERR;


    return REDISMODULE_OK;
}
