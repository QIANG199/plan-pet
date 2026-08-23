const test = require("node:test");
const assert = require("node:assert");
const { stripQuotes, toUnixSec } = require("../src/lib/text");

test("stripQuotes removes matching outer quotes", () => {
  assert.equal(stripQuotes('"abc"'), "abc");
  assert.equal(stripQuotes("'abc'"), "abc");
  assert.equal(stripQuotes("abc"), "abc");
  assert.equal(stripQuotes('"a\'b"'), "a'b");
});

test("stripQuotes leaves mismatched or short values alone", () => {
  assert.equal(stripQuotes('"abc'), '"abc');
  assert.equal(stripQuotes('abc"'), 'abc"');
  assert.equal(stripQuotes('"'), '"');
  assert.equal(stripQuotes(null), "");
});

test("toUnixSec normalises ms and s timestamps", () => {
  assert.equal(toUnixSec(1755900000), 1755900000);
  assert.equal(toUnixSec(1755900000123), 1755900000);
  assert.equal(toUnixSec("1755900000"), 1755900000);
  assert.equal(toUnixSec(null), null);
  assert.equal(toUnixSec(""), null);
  assert.equal(toUnixSec(0), null);
  assert.equal(toUnixSec(-5), null);
  assert.equal(toUnixSec("nope"), null);
});
