mini_gzip
=========

[![Build Status](https://travis-ci.org/wkoszek/mini_gzip.svg?branch=master)](https://travis-ci.org/wkoszek/mini_gzip)

Embeddable, minimal GZIP functionality. Only supports decompression for now.

I wrote it when I needed a small piece of code to give me decompression in
an embedded system I was working with.

The `mini_gzip` is based on `miniz` library
(https://code.google.com/p/miniz/), which provides an API for operating on
data compressed according to deflate algorithm. Added is a layer which
provides a container for the GZIP files and let me do some verification.

# To build

Everything should be fairly self-contained, and built with:

	make

# To test

Simple test is provided:

	make test

To test by yourself, you can do:

~~~shell
	wk:/w/repos/mini_gzip> ls -la miniz.o
	-rw-r--r--  1 wk  staff  114348 20 paź 09:46 miniz.o
	wk:/w/repos/mini_gzip> md5 miniz.o
	MD5 (miniz.o) = e6199aade2020b6040fa160baee47d68
	wk:/w/repos/mini_gzip> gzip miniz.o
	wk:/w/repos/mini_gzip> ls -la miniz.o.gz
	-rw-r--r--  1 wk  staff  44965 20 paź 09:46 miniz.o.gz
	wk:/w/repos/mini_gzip> ./mini_gzip miniz.o.gz miniz.o
	flag_c: 0 is_gzipped: 1
	in_fn: miniz.o.gz out_fn: miniz.o level 6
	--- testing decompression --
	out_len = 114348
	ret = 114348
	wk:/w/repos/mini_gzip> md5 miniz.o
	MD5 (miniz.o) = e6199aade2020b6040fa160baee47d68
~~~
