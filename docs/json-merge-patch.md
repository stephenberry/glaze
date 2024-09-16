# JSON Merge Patch

JSON Merge Patch is an [IETF RFC](https://datatracker.ietf.org/doc/html/rfc7386), it describes how JSON documents can be patched (or merged) together.

Since Glaze interacts with C++ structures, the behavior of merging JSON documents depends on the underlying structures. Where it makes sense, the behavior of JSON Merge Patch is implemented.

