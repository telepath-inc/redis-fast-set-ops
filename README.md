# redis-fast-set-ops

redis-fast-set-ops is a module for redis that provides new commands for
performant, range-limited operations on sets and sorted sets.

This module is well tested and used in production at Telepath. All provided
commands are readonly operations on your data, so you can safely load this
module into an existing redis installation and try the new commands out on
your use case.

> **:book: User Guide**
>
> - [Background](#background)
> - [Command Reference](#command-reference)
> - [Example: Sorted set intersection prefix](#example)
> - [Installation](#installation)
>
> **:hammer_and_wrench: Development**
>
> - [Benchmarks](#benchmarks)
> - [Testing](#testing)
> - [Future Commands](#future-commands)

## :book: User Guide

#### Background

The core redis sorted set commands are not designed for efficiently computing
partial set operations; they require storing the resulting set in order to do
any intersection or union (with ZINTERSTORE and ZUNIONSTORE). This is
unnecessarily slow for applications that simply need to query the top N items
within a given score range that are also present in another set. Additionally,
for intersections that are needed only once to fulfill a given query, use of
ZINTERSTORE adds the requirement that you must delete or expire the new key
after fetching the result.

For applications that only need a small range of the resulting set or a
derivative such as the cardinality of the result, there's a need for commands
that:
* only compute the required portion of the set operation
* don't require a write to the redis database for results with ephemeral utility
* decrease the amount of network I/O required to infer the partial result

This module provides commands that accomplish these goals.

#### Command Reference

**Sorted Sets:**

`ZINTERRANGEBYSCORE key1 key2 min max [WITHSCORES] [LIMIT offset count]`
> *Time complexity: O(M), where M is the cardinality of key1.*

Returns a subset of the intersection of two sorted sets that falls in the range
denoted by `min` and `max`. The first set (denoted by `key1`) is treated as the
source of truth for scores, and scores from the second set are disregarded. All
other arguments function exactly as documented in the built-in command
[ZRANGEBYSCORE](https://redis.io/commands/zrangebyscore).

This command is significantly faster than the built-in `ZINTERSTORE` for the
use cases it supports, because:
* it only scans the subset of `key1`'s set that is needed to compute the
  partial result
* it avoids the write to the redis dataset, and the need to delete the written
  set if the result didn't need to be permanently stored and maintained.

`ZINTERREVRANGEBYSCORE key1 key2 max min [WITHSCORES] [LIMIT offset count]`
> *Time complexity: O(M), where M is the cardinality of key1.*

Performs exactly as `ZINTERRANGEBYSCORE`, but in reverse order.

**Sets:**

`SINTERCARD key [key ...]`
> *Time complexity: O(NM), where N is the cardinality of the smallest set, and M is the number of sets.*

Returns the set cardinality of the result of the intersection of all given sets.

Typically you would need to either retrieve the entire set intersection and
compute the length in your application (wasting network I/O), or first write
the result to the redis database and then delete it.

`SDIFFCARD key [key ...]`
> *Time complexity: O(N), where N is the total number of elements in all given sets.*

Returns the set cardinality of the result of the difference between the first
set and all of the successive sets.

`SUNIONCARD key [key ...]`
> *Time complexity: O(N), where N is the total number of elements in all given sets.*

Returns the set cardinality of the result of the union of all the given sets.

#### Example

User-facing applications often filter and sort user actions by their
relationships to other actions. For example, it might be desirable to show
someone specifically the people the follow who liked a given post. Further,
we may only have room in the UI to show five of these people, so we wouldn't
want to waste resources computing the part of the intersection past the five
most recent likers that we're going to be able to display.

Let's say that you are storing the data in:
1. A sorted set, `likes:<post_id>` containing the user IDs that liked a post,
   scored by the unix timestamp of the like action.
1. A sorted set, `follow:<user_id>` containing the user IDs that a user follows,
   scored by the unix timestamp of the follow action.

You want to quickly compute which users you follow that also liked the post,
then show only the most recent five likes from the resulting intersection.

**Using built-in commands:**

1. First, compute the entirety of the set operation and write it into a new set:
   `ZINTERSTORE results 2 likes:1 follow:1 WEIGHTS 1 0`
   *Complexity: O(NK)+O(M log(M))* [(source)](https://redis.io/commands/zinterstore)
1. Issue a range query by timestamp on the computed results:
   `ZREVRANGEBYSCORE results 1523456789 0 limit 5`
   *Complexity: O(log(N) + M)* [(source)](https://redis.io/commands/zrangebyscore)
1. Delete the results set once you no longer need it, using either `DEL` (which
   does the work inline) or `UNLINK` (which does the same computational work
   asynchronously).
   `DEL results`
   *Complexity: O(N)* [(source)](https://redis.io/commands/del)

It's impractical to perform a write and delete for an operation that is
fundamentally a read and is performed so frequently. Additionally, redis
wastes resources computing the entirety of the intersection, which may be
very large.

**Using the `redis-fast-set-ops` module:**

1. `ZINTERREVRANGEBYSCORE likes:1 follow:1 1523456789 0 limit 5`
   *Complexity: O(M), where M is the cardinality of likes:1.*

You have the same result in only one, faster step! Redis didn't need to write
the result to the database and then subsequently delete it. It also only needed
to scan set `likes:1` up to the point where it found 5 results that were also
present in `follow:1`, instead of the entire intersection which could be much
larger.

Note that the O(M) worst case complexity occurs in the case where the range
covers the entire first set, and the intersection is smaller than the limit.
The case where `ZINTERRANGEBYSCORE` really shines in comparison to the
`ZINTERSTORE` method is when the intersection is large (driving a big
`O(M log(M))` component in its computational cost) and the limit is small,
as highlighted in the last of the (benchmarks)[#benchmarks] below.

You can page through the results by using the minimum score from the previous
query as an offset, and end up computing the entire intersection in a series
of requests without any single request having to do the work of computing the
complete intersection.

#### Installation

1. Install redis 4.0 or higher.
1. Clone and build `redis-fast-set-ops`:
   ```
   $ git clone https://github.com/telepath-inc/redis-fast-set-ops
   $ cd redis-fast-set-ops
   $ make
   ```
1. Load the module into a currently-running redis server
   ```
   $ redis-cli
   > MODULE LOAD /absolute/path/to/redis-fast-set-ops/src/redis-fast-set-ops.so
   >
   > ZADD s1 1 a
   (integer) 1
   > ZADD s1 2 b
   (integer) 1
   > ZADD s2 12 a
   (integer) 1
   > ZINTERRANGEBYSCORE s1 s2 1 2
   1) "a"
   > DEL s1 s2
   ```
1. Add the module to your redis.conf so it will load every time your server
   starts:
   ```
   # echo "loadmodule /absolute/path/to/redis-fast-set-ops/redis-fast-set-ops.so" >> /absolute/path/to/redis.conf
   ```

## **:hammer_and_wrench: Development**

We welcome feedback and contributions from the community! If you have a use
case for partial operations on sets or sorted sets that isn't currently
supported, let us know or send us a pull request and we'll check it out!

If you're building on top of this module, feel free to use and extend the
tests and benchmarking scripts described below:

#### Testing

Tests are provided in tcl to be used with the core
[redis](https://github.com/antirez/redis) test suite. In order to run them,
first build the zinterrange module, then clone and build redis, then run the
following from the redis repository root:
```
./runtest --single ../relative/path/to/redis-fast-set-ops/tests/zinterrange
```

#### Benchmarks

ZINTERRANGE has been benchmarked against ZINTERSTORE using the core redis
benchmark tool. Our benchmark simply compares the two commands head-to-head.
It does not additionally evalute the time to fetch results from the new key
created by ZINTERSTORE, and then to delete it, nor does it incorporate the
network I/O cost of fetching extraneous results from a remote server. So these
benchmarks should be interpreted to slightly underestimate the efficiency gains
achieved by ZINTERRANGE.

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

**Benchmark Results:**

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

#### Future Commands

There are a ton of possibilities for additional set and sorted set commands
to compute partial set operations more efficiently than the built-in commands.
Here are a few that we've talked about at Telepath, some of which we intend
to build for near-future use cases.

* `ZDIFFRANGEBYSCORE` could provide similar efficiency gains as
  `ZINTERRANGEBYSCORE` for related use cases that require set differences, like
  "the five most recent members of a public group, excluding people I block."
* `ZINTERCARD`/`ZUNIONCARD`/`ZDIFFCARD` would be analogous to the corresponding
  `SINTERCARD` etc., and would remove the need to redundantly store sets and
  sorted sets of the same information, which we currently do for some use cases.
* We have considered a more expressive `Z*RANGEBYSCORE` that would allow us to
  combine multiple set operations into a single pass through the first sorted
  set, e.g. take the intersection with the second set, then the diff against
  the third set.
