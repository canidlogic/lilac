# Lilac data types

The Lilac script interpreter uses an extensible set of data types.  These data types are both used during Shastina script interpretation and also for passing data between different rendering nodes.

## Data type classes

There are three basic classes of data types:

1. Null class
2. Primitive class
3. Reference class

The _null class_ only contains a single data type, which is the `null` data type.  This `null` data type only has a single possible value, which is used to represent "no data."  The null class is not extensible.

The _primitive class_ contains data types that are passed around by fully copying their values.  Lilac primitives must fit within the C `double` floating-point data type with finite values only.  For integer types, this `double` data type is guaranteed to be able to exactly represent all integers in the following range:

    Minimum exact integer: -(2^53 - 1) = -9007199254740991
    Maximum exact integer:   2^53 - 1  =  9007199254740991

The _reference class_ contains data types that are passed around by reference.  They are not constrained to fit within a `double`.  However, they have the overhead of a separate memory allocation to store the actual data object.

## Data type names

Each Lilac data type must have a unique name.  Data type names may only consist of US-ASCII alphanumerics, underscores, and period.  The length of data type names must be in range 1 to 63 characters.  Neither the first nor last character may be a period, and periods may not immediately follow other periods.  Data type names are case sensitive, such that `null` and `Null` are different data types.

The period is designed to be used for hierarchical namespaces.  Extensions can avoid namespace collisions by beginning with a domain name in reverse order, such as:

    com.example.MyDataType

The special null data type has the name `null`.  All other data types defined by core or base plug-ins of Lilac start with the prefix `lilac.` such as:

    lilac.float

The recommended convention is that the last component of the hierarchical name begin with a lowercase letter if it is a primitive type or an uppercase letter if it is a reference type.  However, Lilac does not enforce these conventions.

## Type codes

Lilac internally uses unique integer codes to refer to specific data types.  The `type` module allows data type names to be registered and unique integer codes to be received.  It also allows the integer codes of registered data type strings to be queried.

The `null` data type always has an integer code of zero.  Primitive types are assigned unique integers greater than zero.  Reference types are assigned unique integers less than zero.

Except for the mapping of `null` to zero, the mapping of data type names to numeric codes is not fixed and must be determined at runtime.