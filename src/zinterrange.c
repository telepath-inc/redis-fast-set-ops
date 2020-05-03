#include "redismodule.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SET_COMMAND_DIFF 0
#define SET_COMMAND_INTER 1
#define SET_COMMAND_UNION 2

/*  copied from redis/src/util.c since unfortunately there isn't
    something similar exposed by the modules API    */
int string2d(const char *s, size_t slen, double *dp) {
    errno = 0;
    char *eptr;
    *dp = strtod(s, &eptr);
    if (slen == 0 ||
        isspace(((const char*)s)[0]) ||
        (size_t)(eptr-(char*)s) != slen ||
        (errno == ERANGE &&
            (*dp == HUGE_VAL || *dp == -HUGE_VAL || *dp == 0)) ||
        isnan(*dp))
        return 0;
    return 1;
}

int zinterrangebyscoreGenericCommand(RedisModuleCtx *ctx,
                                     RedisModuleString **argv,
                                     int argc, int reverse) {
    RedisModuleKey *zset = NULL, *interset = NULL;
    const char *startstr;
    size_t startstrlen;
    int startex = 0;
    double start;
    const char *endstr;
    size_t endstrlen;
    int endex = 0;
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

    /* Get and sanitize indexes. */
    startstr = RedisModule_StringPtrLen(argv[3], &startstrlen);
    if (startstr[0] == '(') {
        /* this marks an exlusive interval */
        startex = 1;
        startstr++;
        startstrlen--;
    }
    if (!string2d(startstr, startstrlen, &start)) {
        RedisModule_ReplyWithError(ctx, "ERR min or max is not a float");
        return REDISMODULE_ERR;
    }
    endstr = RedisModule_StringPtrLen(argv[4], &endstrlen);
    if (endstr[0] == '(') {
        /* this marks an exlusive interval */
        endex = 1;
        endstr++;
        endstrlen--;
    }
    if (!string2d(endstr, endstrlen, &end)) {
        RedisModule_ReplyWithError(ctx, "ERR min or max is not a float");
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
        RedisModule_ReplyWithError(ctx,
                                   "WRONGTYPE Operation against a key holding the wrong kind of value");
        return REDISMODULE_ERR;
    }
    if ((interset = RedisModule_OpenKey(ctx,argv[2],REDISMODULE_READ)) != NULL
         && RedisModule_KeyType(interset) != REDISMODULE_KEYTYPE_ZSET) {
        RedisModule_CloseKey(zset);
        RedisModule_CloseKey(interset);
        RedisModule_ReplyWithError(ctx,
                                   "WRONGTYPE Operation against a key holding the wrong kind of value");
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
        RedisModule_ZsetLastInScoreRange(zset, end, start, endex, startex);
    } else {
        RedisModule_ZsetFirstInScoreRange(zset, start, end, startex, endex);
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

int SDiffInterUnionCard_GenericCommand(RedisModuleCtx *ctx,
                                       RedisModuleString **argv,
                                       int argc,
                                       int cmdid) {
    const char *cmd;
    RedisModuleCallReply *reply;

    if (cmdid == SET_COMMAND_DIFF) {
        cmd = "SDIFF";
    } else if (cmdid == SET_COMMAND_INTER) {
        cmd = "SINTER";
    } else if (cmdid == SET_COMMAND_UNION) {
        cmd = "SUNION";
    }
    reply = RedisModule_Call(ctx,cmd,"v",argv+1,argc-1);

    if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_ERROR) {
      RedisModule_ReplyWithCallReply(ctx, reply);
      RedisModule_FreeCallReply(reply);
      return REDISMODULE_ERR;
    }

    size_t card = RedisModule_CallReplyLength(reply);
    RedisModule_FreeCallReply(reply);
    RedisModule_ReplyWithLongLong(ctx, card);
    return REDISMODULE_OK;
}

int SDiffCard_RedisCommand(RedisModuleCtx *ctx,
                           RedisModuleString **argv,
                           int argc) {
    return SDiffInterUnionCard_GenericCommand(ctx, argv, argc, SET_COMMAND_DIFF);
}

int SInterCard_RedisCommand(RedisModuleCtx *ctx,
                            RedisModuleString **argv,
                            int argc) {
    return SDiffInterUnionCard_GenericCommand(ctx, argv, argc, SET_COMMAND_INTER);
}

int SUnionCard_RedisCommand(RedisModuleCtx *ctx,
                            RedisModuleString **argv,
                            int argc) {
    return SDiffInterUnionCard_GenericCommand(ctx, argv, argc, SET_COMMAND_UNION);
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
