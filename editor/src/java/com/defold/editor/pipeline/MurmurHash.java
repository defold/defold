package com.defold.editor.pipeline;

/**
 * NOTE This version has been modified by Defold to mimick the C implementation.
 *
 * murmur hash 2.0.
 *
 * The murmur hash is a relative fast hash function from
 * http://murmurhash.googlepages.com/ for platforms with efficient
 * multiplication.
 *
 * This is a re-implementation of the original C code plus some
 * additional features.
 *
 * Public domain.
 *
 * @author Viliam Holub
 * @version 1.0.2
 *
 */
public final class MurmurHash {

    // all methods static; private constructor.
    private MurmurHash() {}

    /**
     * Generates 32 bit hash from byte array of the given length and
     * seed.
     *
     * @param data byte array to hash
     * @param length length of the array to hash
     * @param seed initial seed value
     * @return 32 bit hash of the given array
     */
    public static int hash32( final byte[] data, int length) {
        // 'm' and 'r' are mixing constants generated offline.
        // They're not really 'magic', they just happen to work well.
        final int m = 0x5bd1e995;
        final int r = 24;

        int h = /*seed*/ 0;
        int length4 = length/4;

        for (int i=0; i<length4; i++) {
            final int i4 = i*4;
            int k = (data[i4+0]&0xff) +((data[i4+1]&0xff)<<8)
                    +((data[i4+2]&0xff)<<16) +((data[i4+3]&0xff)<<24);
            k *= m;
            k ^= k >>> r;
            k *= m;
            h *= m;
            h ^= k;
        }

        int t = 0;
        // Handle the last few bytes of the input array
        switch (length%4) {
        case 3: t ^= (data[(length&~3) +2]&0xff) << 16;
        case 2: t ^= (data[(length&~3) +1]&0xff) << 8;
        case 1: t ^= (data[length&~3]&0xff);
        }

        t *= m;
        t ^= t >>> r;
        t *= m;
        h *= m;
        h ^= t;

        int l = length;
        l *= m;
        l ^= l >>> r;
        l *= m;
        h *= m;
        h ^= l;

        h ^= h >>> 13;
        h *= m;
        h ^= h >>> 15;

        return h;
    }

    /**
     * Generates 32 bit hash from a string.
     *
     * @param text string to hash
     * @return 32 bit hash of the given string
     */
    public static int hash32( final String text) {
        final byte[] bytes = text.getBytes();
        return hash32( bytes, bytes.length);
    }


    /**
     * Generates 32 bit hash from a substring.
     *
     * @param text string to hash
     * @param from starting index
     * @param length length of the substring to hash
     * @return 32 bit hash of the given string
     */
    public static int hash32( final String text, int from, int length) {
        return hash32( text.substring( from, from+length));
    }


    /**
     * Generates 64 bit hash from byte array of the given length and seed.
     *
     * @param data byte array to hash
     * @param length length of the array to hash
     * @param seed initial seed value
     * @return 64 bit hash of the given array
     */
    public static long hash64( final byte[] data, int length) {
        final long m = 0xc6a4a7935bd1e995L;
        final int r = 47;

        long h = /*seed*/ 0;

        int length8 = length/8;

        for (int i=0; i<length8; i++) {
            final int i8 = i*8;
            long k =  ((long)data[i8+0]&0xff)      +(((long)data[i8+1]&0xff)<<8)
                    +(((long)data[i8+2]&0xff)<<16) +(((long)data[i8+3]&0xff)<<24)
                    +(((long)data[i8+4]&0xff)<<32) +(((long)data[i8+5]&0xff)<<40)
                    +(((long)data[i8+6]&0xff)<<48) +(((long)data[i8+7]&0xff)<<56);

            k *= m;
            k ^= k >>> r;
            k *= m;
            h *= m;
            h ^= k;
        }

        long t = 0;

        switch (length%8) {
        case 7: t ^= (long)(data[(length&~7)+6]&0xff) << 48;
        case 6: t ^= (long)(data[(length&~7)+5]&0xff) << 40;
        case 5: t ^= (long)(data[(length&~7)+4]&0xff) << 32;
        case 4: t ^= (long)(data[(length&~7)+3]&0xff) << 24;
        case 3: t ^= (long)(data[(length&~7)+2]&0xff) << 16;
        case 2: t ^= (long)(data[(length&~7)+1]&0xff) << 8;
        case 1: t ^= (long)(data[length&~7]&0xff);
        };

        t *= m;
        t ^= t >>> r;
        t *= m;
        h *= m;
        h ^= t;

        long l = length;
        l *= m;
        l ^= l >>> r;
        l *= m;
        h *= m;
        h ^= l;

        h ^= h >>> r;
        h *= m;
        h ^= h >>> r;

        return h;
    }

    /**
     * Generates 64 bit hash from a string.
     *
     * @param text string to hash
     * @return 64 bit hash of the given string
     */
    public static long hash64( final String text) {
        final byte[] bytes = text.getBytes();
        return hash64( bytes, bytes.length);
    }


    /**
     * Generates 64 bit hash from a substring.
     *
     * @param text string to hash
     * @param from starting index
     * @param length length of the substring to hash
     * @return 64 bit hash of the given array
     */
    public static long hash64( final String text, int from, int length) {
        return hash64( text.substring( from, from+length));
    }
}
