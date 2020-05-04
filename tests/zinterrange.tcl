# get the path of the test module in order to reference the absolute path of the tested module
set moduleLocation [file dirname [file normalize [info script]]]
dict set options overrides "loadmodule ${moduleLocation}/../src/zinterrange.so"
start_server $options {

    proc runs {encoding} {
        if {$encoding == "intset"} {
            r config set set-max-intset-entries 512
        } elseif {$encoding == "hashtable"} {
            r config set set-max-intset-entries 0
        } else {
            puts "Unknown set encoding"
            exit
        }

        test "Check encoding - $encoding" {
            r del stmp
            r sadd stmp 2
            assert_encoding $encoding stmp
        }

        test "SDIFFCARD/SINTERCARD/SUNIONCARD non-existent keys" {
            assert_equal 0 [r sdiffcard nonset]
            assert_equal 0 [r sintercard nonset]
            assert_equal 0 [r sunioncard nonset]
        }

        proc create_nonsets {} {
            r del t
            r set t t
            r del l
            r lpush l 1
            r del h
            r hset h f v
        }

        test "SDIFFCARD/SINTERCARD/SUNIONCARD non-set single keys" {
            create_nonsets
            assert_error "*WRONGTYPE*" {r sdiffcard t}
            assert_error "*WRONGTYPE*" {r sdiffcard l}
            assert_error "*WRONGTYPE*" {r sdiffcard h}
            assert_error "*WRONGTYPE*" {r sintercard t}
            assert_error "*WRONGTYPE*" {r sintercard l}
            assert_error "*WRONGTYPE*" {r sintercard h}
            assert_error "*WRONGTYPE*" {r sunioncard t}
            assert_error "*WRONGTYPE*" {r sunioncard l}
            assert_error "*WRONGTYPE*" {r sunioncard h}
        }

        proc create_default_set {} {
            r del set
            r sadd set a b c d e f g
        }

        proc create_default_otherset {} {
            r del otherset
            r sadd otherset b d f h
        }

        test "SDIFFCARD/SINTERCARD/SUNIONCARD basic" {
            create_default_set
            create_default_otherset

            assert_equal 4 [r sdiffcard set otherset]
            assert_equal 1 [r sdiffcard otherset set]
            assert_equal 3 [r sintercard set otherset]
            assert_equal 3 [r sintercard otherset set]
            assert_equal 8 [r sunioncard set otherset]
            assert_equal 8 [r sunioncard otherset set]
        }

        test "SDIFFCARD/SINTERCARD/SUNIONCARD duplicate" {
            create_default_set
            create_default_otherset

            assert_equal 4 [r sdiffcard set otherset otherset]
            assert_equal 1 [r sdiffcard otherset set set]
            assert_equal 3 [r sintercard set otherset otherset]
            assert_equal 3 [r sintercard otherset set set]
            assert_equal 8 [r sunioncard set otherset otherset]
            assert_equal 8 [r sunioncard otherset set set]
        }

        test "SDIFFCARD/SINTERCARD/SUNIONCARD self" {
            create_default_set
            create_default_otherset

            assert_equal 0 [r sdiffcard set set]
            assert_equal 0 [r sdiffcard otherset otherset]
            assert_equal 7 [r sintercard set set]
            assert_equal 4 [r sintercard otherset otherset]
            assert_equal 7 [r sunioncard set set]
            assert_equal 4 [r sunioncard otherset otherset]
        }

        test "SDIFFCARD/SINTERCARD/SUNIONCARD multi" {
            create_default_set
            create_default_otherset

            r del third
            r sadd third c z

            assert_equal 3 [r sdiffcard set otherset third]
            assert_equal 1 [r sdiffcard otherset set third]
            assert_equal 0 [r sintercard set otherset third]
            assert_equal 0 [r sintercard otherset set third]
            assert_equal 9 [r sunioncard set otherset third]
            assert_equal 9 [r sunioncard otherset set third]

            r del fourth
            r sadd fourth f

            assert_equal 3 [r sdiffcard set otherset third fourth]
            assert_equal 1 [r sdiffcard otherset set third fourth]
            assert_equal 0 [r sintercard set otherset third fourth]
            assert_equal 0 [r sintercard otherset set third fourth]
            assert_equal 9 [r sunioncard set otherset third fourth]
            assert_equal 9 [r sunioncard otherset set third fourth]
        }

        test "SDIFFCARD/SINTERCARD/SUNIONCARD empty other keys" {
            create_default_set
            create_default_otherset

            assert_equal 4 [r sdiffcard set otherset nonset]
            assert_equal 1 [r sdiffcard otherset set nonset]
            assert_equal 0 [r sintercard set otherset nonset]
            assert_equal 0 [r sintercard otherset set nonset]
            assert_equal 8 [r sunioncard set otherset nonset]
            assert_equal 8 [r sunioncard otherset set nonset]
        }

        test "SDIFFCARD/SINTERCARD/SUNIONCARD non-set other keys" {
            create_nonsets
            assert_error "*WRONGTYPE*" {r sdiffcard set otherset t}
            assert_error "*WRONGTYPE*" {r sdiffcard set t otherset}
            assert_error "*WRONGTYPE*" {r sdiffcard t set otherset}
            assert_error "*WRONGTYPE*" {r sdiffcard set otherset l}
            assert_error "*WRONGTYPE*" {r sdiffcard set l otherset}
            assert_error "*WRONGTYPE*" {r sdiffcard l otherset set}
            assert_error "*WRONGTYPE*" {r sdiffcard set otherset h}
            assert_error "*WRONGTYPE*" {r sdiffcard set h otherset}
            assert_error "*WRONGTYPE*" {r sdiffcard h set otherset}
            assert_error "*WRONGTYPE*" {r sintercard set otherset t}
            assert_error "*WRONGTYPE*" {r sintercard set t otherset}
            assert_error "*WRONGTYPE*" {r sintercard t set otherset}
            assert_error "*WRONGTYPE*" {r sintercard set otherset h}
            assert_error "*WRONGTYPE*" {r sintercard set h otherset}
            assert_error "*WRONGTYPE*" {r sintercard h set otherset}
            assert_error "*WRONGTYPE*" {r sintercard set otherset l}
            assert_error "*WRONGTYPE*" {r sintercard set l otherset}
            assert_error "*WRONGTYPE*" {r sintercard l set otherset}
            assert_error "*WRONGTYPE*" {r sunioncard set otherset t}
            assert_error "*WRONGTYPE*" {r sunioncard set t otherset}
            assert_error "*WRONGTYPE*" {r sunioncard t set otherset}
            assert_error "*WRONGTYPE*" {r sunioncard set otherset h}
            assert_error "*WRONGTYPE*" {r sunioncard set h otherset}
            assert_error "*WRONGTYPE*" {r sunioncard h set otherset}
            assert_error "*WRONGTYPE*" {r sunioncard set otherset l}
            assert_error "*WRONGTYPE*" {r sunioncard set l otherset}
            assert_error "*WRONGTYPE*" {r sunioncard l set otherset}
        }
    }

    runs intset
    runs hashtable

    proc create_zset {key items} {
        r del $key
        foreach {score entry} $items {
            r zadd $key $score $entry
        }
    }

    proc runz {encoding} {
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

        proc create_nonsets {} {
            r del t
            r set t t
            r del l
            r lpush l 1
            r del h
            r hset h f v
        }

        test "ZINTERRANGEBYSCORE/ZINTERREVRANGEBYSCORE basics" {
            create_default_zset
            create_default_interset

            # non-existenet keys
            assert_equal {} [r zinterrangebyscore zset nonset -inf +inf]
            assert_equal {} [r zinterrangebyscore nonset zset -inf +inf]
            assert_equal {} [r zinterrangebyscore nonset nonset -inf +inf]

            assert_equal {b c} [r zinterrangebyscore zset interset -inf 2]
            assert_equal {b c d} [r zinterrangebyscore zset interset 0 3]
            assert_equal {d e f} [r zinterrangebyscore zset interset 3 6]
            assert_equal {e f} [r zinterrangebyscore zset interset 4 +inf]
            assert_equal {c b} [r zinterrevrangebyscore zset interset 2 -inf]
            assert_equal {d c b} [r zinterrevrangebyscore zset interset 3 0]
            assert_equal {f e d} [r zinterrevrangebyscore zset interset 6 3]
            assert_equal {f e} [r zinterrevrangebyscore zset interset +inf 4]

            # exclusive ranges
            assert_equal {b} [r zinterrangebyscore zset interset -inf (2]
            assert_equal {b} [r zinterrangebyscore zset interset (-inf (2]
            assert_equal {b c} [r zinterrangebyscore zset interset 0 (3]
            assert_equal {e f} [r zinterrangebyscore zset interset (3 (6]
            assert_equal {f} [r zinterrangebyscore zset interset (4 +inf]
            assert_equal {f} [r zinterrangebyscore zset interset (4 (+inf]
            assert_equal {b} [r zinterrevrangebyscore zset interset (2 -inf]
            assert_equal {b} [r zinterrevrangebyscore zset interset (2 (-inf]
            assert_equal {c b} [r zinterrevrangebyscore zset interset (3 (0]
            assert_equal {f e} [r zinterrevrangebyscore zset interset (6 (3]
            assert_equal {f} [r zinterrevrangebyscore zset interset +inf (4]
            assert_equal {f} [r zinterrevrangebyscore zset interset (+inf (4]

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
            create_default_zset
            create_default_interset
            assert_error "*not*float*" {r zinterrangebyscore zset interset str 1}
            assert_error "*not*float*" {r zinterrangebyscore zset interset (str 1}
            assert_error "*not*float*" {r zinterrangebyscore zset interset 1 str}
            assert_error "*not*float*" {r zinterrangebyscore zset interset 1 (str}
            assert_error "*not*float*" {r zinterrangebyscore zset interset 1 NaN}
            assert_error "*not*float*" {r zinterrangebyscore zset interset 1 (NaN}
        }

        test "ZINTERRANGEBYSCORE with non-zset" {
            create_default_zset
            create_nonsets
            assert_error "*WRONGTYPE*" {r zinterrangebyscore zset t -inf inf}
            assert_error "*WRONGTYPE*" {r zinterrangebyscore zset l -inf inf}
            assert_error "*WRONGTYPE*" {r zinterrangebyscore zset h -inf inf}
            assert_error "*WRONGTYPE*" {r zinterrangebyscore t zset -inf inf}
            assert_error "*WRONGTYPE*" {r zinterrangebyscore l zset -inf inf}
            assert_error "*WRONGTYPE*" {r zinterrangebyscore h zset -inf inf}
            assert_error "*WRONGTYPE*" {r zinterrangebyscore t l -inf inf}
            assert_error "*WRONGTYPE*" {r zinterrevrangebyscore zset t inf -inf}
            assert_error "*WRONGTYPE*" {r zinterrevrangebyscore zset l inf 1}
            assert_error "*WRONGTYPE*" {r zinterrevrangebyscore zset h inf 2}
            assert_error "*WRONGTYPE*" {r zinterrevrangebyscore t zset inf 3.5}
            assert_error "*WRONGTYPE*" {r zinterrevrangebyscore l zset inf -inf}
            assert_error "*WRONGTYPE*" {r zinterrevrangebyscore h zset inf -inf}
            assert_error "*WRONGTYPE*" {r zinterrevrangebyscore h l inf -inf}
        }

        test "ZDIFFSTORE non-set single keys" {
            create_nonsets
            assert_error "*WRONGTYPE*" {r zdiffstore out t}
            assert_error "*WRONGTYPE*" {r zdiffstore out l}
            assert_error "*WRONGTYPE*" {r zdiffstore out h}
        }

        test "ZDIFFSTORE non-set other keys" {
            create_default_zset
            create_default_interset
            create_nonsets
            assert_error "*WRONGTYPE*" {r zdiffstore out zset interset t}
            assert_error "*WRONGTYPE*" {r zdiffstore out zset t interset}
            assert_error "*WRONGTYPE*" {r zdiffstore out t zset interset}
            assert_error "*WRONGTYPE*" {r zdiffstore out zset interset l}
            assert_error "*WRONGTYPE*" {r zdiffstore out zset l interset}
            assert_error "*WRONGTYPE*" {r zdiffstore out l interset zset}
            assert_error "*WRONGTYPE*" {r zdiffstore out zset interset h}
            assert_error "*WRONGTYPE*" {r zdiffstore out zset h interset}
            assert_error "*WRONGTYPE*" {r zdiffstore out h zset interset}
        }
    }

    runz ziplist
    runz skiplist
}
