# The 1.0.27 line's canonical partners for master's sequence vectors

`c4m_sequence_vector_bytes_test.cpp` reads the "## Round trip" tables of the
sequence vectors copied from c4 master b82ec07. Two kinds of row state 1.1
rules that the 1.0.x line does not have. Those rows are checked here against
what the Go library on core's `release/1.0.27` (9236ab1) writes. No file here
is composed by hand.

| Vector file                                         | Master's row | On this line                               | Why                                                                                      |
| --------------------------------------------------- | ------------ | ------------------------------------------ | ---------------------------------------------------------------------------------------- |
| `range-data-vector/add.c4m`                         | itself       | `line-1.0.27/range-data-vector/add.c4m`    | 1.0.x drops an external base line on re-encode; keeping it is 1.1 (c4 #145)              |
| `range-data-vector/remove.c4m`                      | itself       | `line-1.0.27/range-data-vector/remove.c4m` | the same                                                                                 |
| `sequence-null-size-vector/frames-zero-spelled.c4m` | `frames.c4m` | itself                                     | on 1.0.x a `0` beside a non-empty C4 ID is a known 0; reading it as null is 1.1 (c4 #68) |

Both files here are `Marshal(Unmarshal(file))` by the `c4m` package at core
`release/1.0.27` 9236ab1, run on master's vector files. They are
byte-identical to the partners c4ts's `release/1.0.27` carries, which the
same library wrote at 3ecd6ea. On core 9236ab1 every other row of the three
tables holds as master states it, including the two chains
(`add-chain.c4m` to `folded.c4m`, `remove-chain.c4m` to `no-plate.c4m`),
measured 2026-11-16.

The test decodes every file with `Manifest::Parse`, which resolves a chain
as core release/1.0.27's decoder does. Until that change, Parse concatenated a
chain's sections, and the test resolved chains through `DecodePatchChain` and
`ResolvePatchChain` instead.
