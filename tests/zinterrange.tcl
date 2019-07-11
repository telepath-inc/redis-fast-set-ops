# get the path of the test module in order to reference the absolute path of the tested module
set moduleLocation [file dirname [file normalize [info script]]]
dict set options overrides "loadmodule ${moduleLocation}/../src/zinterrange.so"
start_server $options {
    proc create_zset {key items} {
        r del $key
        foreach {score entry} $items {
            r zadd $key $score $entry
        }
    }

    proc run {encoding} {
        if {$encoding == "ziplist"} {
            r config set zset-max-ziplist-entries 128
            r config set zset-max-ziplist-value 64
        } elseif {$encoding == "skiplist"} {
            r config set zset-max-ziplist-entries 0
            r config set zset-max-ziplist-value 0
        } else {
            puts "Unknown sorted set encoding"
            exit
        }

        test "Check encoding - $encoding" {
            r del ztmp
            r zadd ztmp 10 x
            assert_encoding $encoding ztmp
        }

        proc create_default_zset {} {
            create_zset zset {-inf a 1 b 2 c 3 d 4 e 5 f +inf g}
        }

        proc create_default_interset {} {
            # zset, but with only one shared score (shouldn't matter)
            # and lacking members a & g
            create_zset interset {-inf b -inf c 3 d +inf e +inf f}
        }

        test "ZINTERRANGEBYSCORE/ZINTERREVRANGEBYSCORE basics" {
            create_default_zset
            create_default_interset

            assert_equal {b c} [r zinterrangebyscore zset interset -inf 2]
            assert_equal {b c d} [r zinterrangebyscore zset interset 0 3]
            assert_equal {d e f} [r zinterrangebyscore zset interset 3 6]
            assert_equal {e f} [r zinterrangebyscore zset interset 4 +inf]
            assert_equal {c b} [r zinterrevrangebyscore zset interset 2 -inf]
            assert_equal {d c b} [r zinterrevrangebyscore zset interset 3 0]
            assert_equal {f e d} [r zinterrevrangebyscore zset interset 6 3]
            assert_equal {f e} [r zinterrevrangebyscore zset interset +inf 4]

            # test empty ranges
            assert_equal {} [r zinterrangebyscore zset interset 4 2]
            assert_equal {} [r zinterrangebyscore zset interset 6 +inf]
            assert_equal {} [r zinterrangebyscore zset interset -inf -6]
            assert_equal {} [r zinterrevrangebyscore zset interset +inf 6]
            assert_equal {} [r zinterrevrangebyscore zset interset -6 -inf]

            # empty inner range
            assert_equal {} [r zinterrangebyscore zset interset 2.4 2.6]
        }

        test "ZINTERRANGEBYSCORE with WITHSCORES" {
            create_default_zset
            create_default_interset
            assert_equal {b 1 c 2 d 3} [r zinterrangebyscore zset interset 0 3 withscores]
            assert_equal {d 3 c 2 b 1} [r zinterrevrangebyscore zset interset 3 0 withscores]
        }

        test "ZINTERRANGEBYSCORE with LIMIT" {
            create_default_zset
            create_default_interset
            assert_equal {b c}   [r zinterrangebyscore zset interset -inf 10 LIMIT 0 2]
            assert_equal {b c}   [r zinterrangebyscore zset interset 0 10 LIMIT 0 2]
            assert_equal {d e f} [r zinterrangebyscore zset interset 0 10 LIMIT 2 3]
            assert_equal {d e f} [r zinterrangebyscore zset interset 0 10 LIMIT 2 10]
            assert_equal {d e f} [r zinterrangebyscore zset interset 0 +inf LIMIT 2 10]
            assert_equal {}      [r zinterrangebyscore zset interset 0 10 LIMIT 20 10]
            assert_equal {f e}   [r zinterrevrangebyscore zset interset 10 -inf LIMIT 0 2]
            assert_equal {f e}   [r zinterrevrangebyscore zset interset 10 0 LIMIT 0 2]
            assert_equal {d c b} [r zinterrevrangebyscore zset interset 10 0 LIMIT 2 3]
            assert_equal {d c b} [r zinterrevrangebyscore zset interset 10 0 LIMIT 2 10]
            assert_equal {d c b} [r zinterrevrangebyscore zset interset +inf 0 LIMIT 2 10]
            assert_equal {}      [r zinterrevrangebyscore zset interset 10 0 LIMIT 20 10]
        }

        test "ZINTERRANGEBYSCORE with LIMIT and WITHSCORES" {
            create_default_zset
            create_default_interset
            assert_equal {e 4 f 5} [r zinterrangebyscore zset interset 2 5 LIMIT 2 3 WITHSCORES]
            assert_equal {d 3 c 2} [r zinterrevrangebyscore zset interset 5 2 LIMIT 2 3 WITHSCORES]
        }

        test "ZINTERRANGEBYSCORE with non-value min or max" {
            assert_error "*not*float*" {r zrangebyscore fooz str 1}
            assert_error "*not*float*" {r zrangebyscore fooz 1 str}
            assert_error "*not*float*" {r zrangebyscore fooz 1 NaN}
        }
    }

    run ziplist
    run skiplist
}