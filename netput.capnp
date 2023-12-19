@0x95f270d1e351654f

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("netput::internal");

struct Book {
    title @0 :Text;
    # Title of the book.

    pageCount @1 :Int32;
    # Number of pages in the book.
}