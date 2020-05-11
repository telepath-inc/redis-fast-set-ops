#include "redismodule.h"

#define SET_COMMAND_DIFF 0
#define SET_COMMAND_INTER 1
#define SET_COMMAND_UNION 2

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
