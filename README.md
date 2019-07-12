# redis-zinterrange

redis-zinterrange is a module for redis that provides new commands for
performant, range-limited intersections on sorted sets.

The core redis sorted set commands require storing the resulting set in order
to do any intersection or union (with ZINTERSTORE and ZUNIONSTORE). This is
unnecessarily slow for applications that simply need to query the top N items
within a given score range that are also present in another set. Additionally,
for intersections that are needed only once to fulfill a given query, use of
ZINTERSTORE adds the requirement that you must delete or expire the new key
after fetching the result.

This module provides:
* `ZINTERRANGEBYSCORE key1 key2 min max [WITHSCORES] [LIMIT offset count]`
* `ZINTERREVRANGEBYSCORE key1 key2 max min [WITHSCORES] [LIMIT offset count]`,

where `key1`'s scores are used for the range and result scores, and `key2` is
only used for membership checks to compute the intersection. `max` and `min`
are both treated as inclusive. Other than that, these commands behave as
documented in the [ZRANGEBYSCORE docs](https://redis.io/commands/zrangebyscore).

The module may be extended in the future to also provide ZINTERRANGE
lexicographically, or to support more than two sets.

## Testing

Tests are provided in tcl to be used with the core
[redis](https://github.com/antirez/redis) test suite. In order to run them,
first build the zinterrange module, then clone and build redis, then run the
following from the redis repository root:
```
./runtest --single ../relative/path/to/redis-zinterrange/tests/zinterrange
```

## Benchmarks

ZINTERRANGE has been benchmarked against ZINTERSTORE using the core redis
benchmark tool. Our benchmark simply compares the two commands head-to-head.
It does not additionally evalute the time to fetch results from the new key
created by ZINTERSTORE, and then to delete it.

We benchmarked for the following cases:
1. *Small non-intersecting sets*: Both keys contain small sets (10 elements)
   with a null intersection.
1. *Small intersecting sets*: Both keys contain small sets (10 elements) with
   a full intersection.
1. *First key large*: The first key contains a large set (1000 elements in
   range) and the second contains a small set (10 elements).
1. *Second key large*: The first key contains a small set (10 elements in
   range) and the second contains a large set (1000 elements).
1. *Large non-intersecting sets*: Both keys contain large sets (1000 elements)
   with a null intersection.
1. *Large intersecting sets*: Both keys contain large sets (1000 elements)
   with a full intersection.
1. *Large sets, small range*: Both keys contain large sets (1000 elements)
   with a full intersection, but only 10 elements have scores in range.

For each of these, we ran one benchmark with default settings (100000 requests
in parallel from 50 clients) and one with 10000 requests from a single client.

The script to reproduce these benchmarks is provided in
[utils/benchmark](utils/benchmark). Note that this script requires a redis
server to be running with the zinterrange module loaded.

### Results

This is the output from the benchmark script run on a
MacBook Pro 2.9 GHz Core i7 (I7-7820HQ):

```
$ utils/benchmark
===== small non-intersecting sets =====
Parallel clients:
zinterstore c 2 small1a small11 weights 1 0: 75642.96 requests per second
zinterrangebyscore small1a small11 1 10: 70077.09 requests per second
Single client:
zinterstore c 2 small1a small11 weights 1 0: 24390.24 requests per second
zinterrangebyscore small1a small11 1 10: 24213.08 requests per second

===== small intersecting sets =====
Parallel clients:
zinterstore c 2 small1a small1b weights 1 0: 35829.45 requests per second
zinterrangebyscore small1a small1b 1 10: 71022.73 requests per second
Single client:
zinterstore c 2 small1a small1b weights 1 0: 20325.20 requests per second
zinterrangebyscore small1a small1b 1 10: 23474.18 requests per second

===== first set large, second small =====
Parallel clients:
zinterstore c 2 large1a small1b weights 1 0: 37439.16 requests per second
zinterrangebyscore large1a small1b 1 1000: 1735.48 requests per second
Single client:
zinterstore c 2 large1a small1b weights 1 0: 20408.16 requests per second
zinterrangebyscore large1a small1b 1 1000: 1649.08 requests per second

===== second set large, first small =====
Parallel clients:
zinterstore c 2 small1a large1a weights 1 0: 36818.85 requests per second
zinterrangebyscore small1a large1a 1 10: 63211.12 requests per second
Single client:
zinterstore c 2 small1a large1a weights 1 0: 20833.33 requests per second
zinterrangebyscore small1a large1a 1 1000: 23809.53 requests per second

===== large non-intersecting sets =====
Parallel clients:
zinterstore c 2 large1a large1001 weights 1 0: 6413.96 requests per second
zinterrangebyscore large1a large1001 1 1000: 3172.89 requests per second
Single client:
zinterstore c 2 large1a large1001 weights 1 0: 7757.95 requests per second
zinterrangebyscore large1a large1001 1 1000: 2909.51 requests per second

===== large intersecting sets =====
Parallel clients:
zinterstore c 2 large1a large1b weights 1 0: 590.10 requests per second
zinterrangebyscore large1a large1b 1 1000: 2613.97 requests per second
Single client:
zinterstore c 2 large1a large1b weights 1 0: 580.11 requests per second
zinterrangebyscore large1a large1b 1 1000: 1186.24 requests per second

===== large sets, small range =====
Parallel clients:
zinterstore c 2 large1a large1b weights 1 0: 578.37 requests per second
zinterrangebyscore large1a large1b 1 10: 83822.30 requests per second
Single client:
zinterstore c 2 large1a large1b weights 1 0: 619.16 requests per second
zinterrangebyscore large1a large1b 1 10: 25445.29 requests per second
```
