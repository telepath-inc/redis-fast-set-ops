#include "redismodule.h"
#include <stdlib.h>
#include <string.h>

int zinterrangebyscoreGenericCommand(RedisModuleCtx *ctx,
                                     RedisModuleString **argv,
                                     int argc, int reverse) {
    RedisModuleKey *zset = NULL, *interset = NULL;
    double start;
    double end;
    int withscores = 0;
    long long limit = -1;
    long long offset = 0;
    long long rangelen = 0;
    RedisModuleString *elem;
    double zscore, interscore;

    if (argc < 5) {
        // ZINTERRANGE only supports exactly two input keys with a range
        return RedisModule_WrongArity(ctx);
    } else if (argc >= 6) {
        RedisModuleString **suffix_args = argv + 5;
        int suffixargc = argc - 5;

        RedisModule_Log(ctx, "warning",RedisModule_StringPtrLen(suffix_args[0], NULL));

        if (strcasecmp(RedisModule_StringPtrLen(suffix_args[0], NULL),
                       "withscores") == 0) {
            withscores = 1;
            suffix_args++;
            suffixargc--;
        }

        if (suffixargc >= 3 &&
                strcasecmp(RedisModule_StringPtrLen(suffix_args[0], NULL),
                           "limit") == 0) {
            if (RedisModule_StringToLongLong(suffix_args[1], &offset)
                    == REDISMODULE_ERR) {
                RedisModule_ReplyWithError(
                        ctx, "ERR offset arg is not a valid integer");
                return REDISMODULE_ERR;
            }
            if (RedisModule_StringToLongLong(suffix_args[2], &limit)
                    == REDISMODULE_ERR) {
                RedisModule_ReplyWithError(
                        ctx, "ERR limit arg is not a valid integer");
                return REDISMODULE_ERR;
            }

            suffix_args += 3;
            suffixargc -= 3;
        }

        // support WITHSCORES after LIMIT as well
        if (suffixargc == 1 &&
                strcasecmp(RedisModule_StringPtrLen(suffix_args[0], NULL),
                           "withscores") == 0) {
            withscores = 1;
            suffix_args++;
            suffixargc--;
        } else if (suffixargc > 0) {
            return RedisModule_WrongArity(ctx);
        }

    }

    /* Get and sanitize indexes. Note: does not support exclusive ranges. */
    if ((RedisModule_StringToDouble(argv[3], &start) != REDISMODULE_OK) ||
        (RedisModule_StringToDouble(argv[4], &end) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, "ERR range args are not valid floats");
        return REDISMODULE_ERR;
    }
    /* The range is empty when start > end, or the inverse if the reverse
     * flag is on. */
    if (reverse ^ (start > end)) {
        RedisModule_ReplyWithArray(ctx, 0);
        return REDISMODULE_OK;
    }

    /* read keys to be used for input */
    if ((zset = RedisModule_OpenKey(ctx,argv[1],REDISMODULE_READ)) != NULL
         && RedisModule_KeyType(zset) != REDISMODULE_KEYTYPE_ZSET) {
        RedisModule_CloseKey(zset);
        RedisModule_ReplyWithError(ctx, "ERR first key is not a sorted set");
        return REDISMODULE_ERR;
    }
    if ((interset = RedisModule_OpenKey(ctx,argv[2],REDISMODULE_READ)) != NULL
         && RedisModule_KeyType(interset) != REDISMODULE_KEYTYPE_ZSET) {
        RedisModule_CloseKey(zset);
        RedisModule_CloseKey(interset);
        RedisModule_ReplyWithError(ctx, "ERR second key is not a sorted set");
        return REDISMODULE_ERR;
    }

    /* If either key doesn't exist, the intersection is empty. */
    if (zset == NULL || interset == NULL) {
        RedisModule_CloseKey(zset);
        RedisModule_CloseKey(interset);
        RedisModule_ReplyWithArray(ctx, 0);
        return REDISMODULE_OK;
    }

    /* set up iterator for scored input */
    if (reverse) {
        RedisModule_ZsetLastInScoreRange(zset, end, start, 0, 0);
    } else {
        RedisModule_ZsetFirstInScoreRange(zset, start, end, 0, 0);
    }

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    while ((limit == -1 || rangelen < limit) &&
            RedisModule_ZsetRangeEndReached(zset) == 0) {
        elem = RedisModule_ZsetRangeCurrentElement(zset, &zscore);
        // could consider swapping loop order based on size
        if (RedisModule_ZsetScore(interset, elem, &interscore)
                == REDISMODULE_OK) {
            if (offset-- <= 0) {
                RedisModule_ReplyWithString(ctx, elem);
                if (withscores) {
                    RedisModule_ReplyWithDouble(ctx, zscore);
                }
                rangelen++;
            }
        }

        RedisModule_FreeString(ctx, elem);

        // advance the iterator
        if (reverse) {
            RedisModule_ZsetRangePrev(zset);
        } else {
            RedisModule_ZsetRangeNext(zset);
        }
    }

    RedisModule_ReplySetArrayLength(ctx, rangelen * (1 + withscores));

    // cleanup
    RedisModule_ZsetRangeStop(zset);
    RedisModule_CloseKey(zset);
    RedisModule_CloseKey(interset);

    return REDISMODULE_OK;
}

int ZInterRangeByScore_RedisCommand(RedisModuleCtx *ctx,
                                    RedisModuleString **argv,
                                    int argc) {
    return zinterrangebyscoreGenericCommand(ctx, argv, argc, 0);
}

int ZInterRevRangeByScore_RedisCommand(RedisModuleCtx *ctx,
                                       RedisModuleString **argv,
                                       int argc) {
    return zinterrangebyscoreGenericCommand(ctx, argv, argc, 1);
}

int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (RedisModule_Init(ctx,"zinterrange",1,REDISMODULE_APIVER_1) == REDISMODULE_ERR)
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

    return REDISMODULE_OK;
}
