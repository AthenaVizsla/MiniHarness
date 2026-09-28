# Design Log — Project 2 (The Conversation Loop)

## Growth factor and amortized O(1) `append`

`Conversation` doubles its capacity whenever it is full, rather than
growing by a fixed amount:

```cpp
std::size_t new_cap = capacity_ ? capacity_ * 2 : 4;
```

Capacity therefore follows the sequence 4, 8, 16, 32, .... For `n` total
appends starting from empty, a reallocation only occurs when size crosses
one of these thresholds, so the total number of reallocations is
approximately `log2(n/4)` — far smaller than `n`.

To show that `append` is amortized O(1), consider the total cost of `n`
appends, split into two parts: the `n` individual insertions (each O(1)),
and the cost of every resize along the way. A resize at capacity `c`
copies `c` existing elements, and resizes occur at capacities
4, 8, 16, ..., up to the final capacity `C`. The total copying cost across
all resizes is:

```
4 + 8 + 16 + ... + C  <  2C
```

because this is a geometric series with ratio 2, and the sum of such a
series is bounded by twice its largest term. Since `C` is at most
proportional to `n` (capacity never exceeds roughly `2n`), the total
resize cost across all `n` appends is O(n). Dividing that total cost by
`n` gives an average cost per append of O(1) — this is the definition of
amortized O(1): not every individual append is fast, but the cost of the
occasional expensive resize is spread evenly across the many appends that
preceded it, so the average never grows with `n`.

A growth factor of 2 is used because it keeps the number of resizes
logarithmic in `n` while bounding wasted space to a constant factor. A
smaller growth factor (e.g., 1.1) would keep memory tighter but increase
the number of resizes; a much larger growth factor would reduce resizes
further but waste more memory per allocation.

## Rule of Five 

The two points of failure to avoid are: (1) two `Conversation` objects sharing
the same `Message*` buffer, leading to a double free when both destructors
run, and (2) a copy that duplicates only the pointer rather than the
underlying data, so mutating one object silently mutates the other.

The implementation separates copy and move so that only one of these
operations ever allocates:

- **Copy** (constructor and `operator=`) allocates a new array with
  `new Message[...]` and copies each element individually. After any copy,
  `a.data() != b.data()` holds unconditionally. This is verified directly
  in `test_copy_is_deep`, which also copy-assigns into an already-populated
  `Conversation` and performs a self-assignment (`c = c;`) to confirm the
  "allocate the new buffer before freeing the old one" ordering in
  `operator=` does not free memory that is still required.
- **Move** never allocates. It transfers the pointer and zeroes the
  source's `data_`, `size_`, and `capacity_`. The moved-from object's
  destructor subsequently executes `delete[] nullptr`, which is
  well-defined and a no-op. `test_move_steals` confirms this by recording
  the source pointer before the move and asserting the destination holds
  that exact pointer (no reallocation occurred) while the source is fully
  zeroed.

Because the destructor is simply `delete[] data_;`, and every code path
above leaves `data_` pointing at memory uniquely owned by that object, or
at `nullptr`, no double free or leak is possible. AddressSanitizer
confirms this with zero errors across every test run.

## Bound on the `pending_` buffer

`SentinelScanner::pending_` stores the longest suffix of characters seen so
far that could still be a prefix of the sentinel. For each incoming
character `c`, `match_length` computes the longest suffix of
`pending_ + c` that matches a prefix of `sentinel_`; call this length
`len`. Two facts hold for every call:

1. `len <= sentinel_.size()`, since no match can exceed the sentinel's
   length.
2. If `len == sentinel_.size()`, the full sentinel has been matched;
   `feed()` returns immediately in this case and clears `pending_`, so this
   value of `len` never reaches the buffering step below.

Consequently, on every character that does not complete the sentinel,
`len <= sentinel_.size() - 1`. After processing that character, `pending_`
is set to exactly the last `len` characters, so
`pending_.size() <= sentinel_.size() - 1` holds at all times. This bound is
independent of the total input length, which is what keeps memory usage
constant rather than growing with the stream — avoiding the O(N²) buildup
the specification warns against.

## Item for future revision

`SentinelScanner::match_length` could be replaced with a precomputed
Knuth-Morris-Pratt failure function. The current implementation is correct
and satisfies the bounded-memory requirement, but for each character it may
re-compare up to `sentinel_.size()` characters, giving roughly O(N·M)
total work rather than O(N), where N is stream length and M is sentinel
length. For a short, fixed sentinel such as `<|end_conversation|>` this
difference is negligible in practice, but a KMP-based implementation would
remove the repeated re-comparison and scale better for longer sentinels or
higher-throughput streams.