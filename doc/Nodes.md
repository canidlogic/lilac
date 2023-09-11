# Lilac rendering nodes

Lilac scripts use _nodes_ to specify the details of the rendering process.

## Node indices

Nodes are stored in an internal array and are added to this array in the order they are defined.

Each node has a unique integer index that is zero or greater, corresponding to their position in the array.  The first two nodes (node index zero and one) are predefined and already present before script interpretation begins.  The first node defined by the script will therefore have node index two.

## Ports

Each node has zero or more _ports_ that are identified with an integer index that is zero or greater.  Each port has a data type code associated with it, which indicates what type of data is available at the port.  The data type system is extensible and described in a separate specification.

The special `null` data type is used to represents ports that are not in use by a specific node.  Ports may be opened up by changing their value from `null` to some other data type while the script is being interpreted.  However, once a port has been assigned a data type other than `null`, the assigned type should be permanent and never change.

## Callback functions

Each node must define three callback functions:

1. Compute function
2. Scan function
3. Query function

The _compute function_ causes the node to recompute its state.  The node may invoke the query functions of other nodes during this computation, but only for nodes that have an index less than that of the node currently being computed.

The _scan function_ is given a specific port number.  The node then returns a data type code associated with that port.  If the scan function is used for a port that is not in use by the node, the node returns `null` as the data type associated with that unused port.

The _query function_ is given a specific port number, a data type code, and a method for returning a value.  The data type code is verified to match the type of the port, with an error if there is no match.  Primitive types return the port data as a `double` value.  Reference types return the port data as a void pointer.  It is an error to query a port that has the `null` data type associated with it.

For each pixel that is being rendered, Lilac keeps track of the maximum node index that can be queried.  An error occurs if a query is made to a node index beyond this limit.  When nodes are computed, the limit is always one node index below the current node, so that nodes that are already computed can be queried, but the current node and future nodes can not be queried.

## Parameter node

The first predefined node (at node index zero) is the _parameter node._  The ports of the parameter node allow current rendering data to be accessible from other nodes.  The following ports are currently defined:

- __Port 0:__  Current pixel offset (`lilac.int`)
- __Port 1:__  Current X coordinate (`lilac.int`)
- __Port 2:__  Current Y coordinate (`lilac.int`)
- __Port 3:__  Image width (`lilac.int`)
- __Port 4:__  Image height (`lilac.int`)

The current pixel offset is zero at the top-left corner of the image and is one less than the width multiplied by the height at the bottom-right corner.  The current pixel starts at zero and increments at each pixel as rendering moves through the image from left to right in each scanline, and from top to bottom scanlines.

The current X and Y coordinates are in range zero up to one less than the width or height respectively.  The following relationship between current pixel offset, X, and Y always holds:

    offset = (y * WIDTH) + x

## Layer node

The second predefined node (at node index one) is the _layer node._  The ports of the layer node correspond to the different layers defined within each deep pixel of the input deep pixel bitmap.  (See the separate documentation for more about deep bitmaps.)

Each port in the layer node maps to a layer in the deep bitmap pixel with the equivalent offset.  `null` is used for ports that do not correspond to any layer.  `lilac.ch` is used for compressed layers.  `lilac.argb` is used for full-color layers.

## Constructor operations

Instead of requiring each specific node type to declare its own Shastina operations, Lilac has a unified operation interface for constructing nodes that uses the following operations defined by the core program:

- `node`
- `begin`
- `d`
- `end`

The `node` operation is used for nodes that are simple enough to specify all construction information in a single operation.  For more complex nodes, an accumulator is used.  The `begin` operation starts building a node in the accumulator.  There are then zero or more `d` operations that declare various parameters of the node being constructed.  Finally, the `end` operation finishes construction, clears the accumulator, and yields the finished node.

Only one node can be constructed at a time in the accumulator.  Nested construction is not allowed.

Atoms are used as arguments to these operations to identify the specific kinds of nodes that are being constructed and the specific kinds of operations that are requested.  This allows these operations to act as a kind of escape that indirectly select from a far larger set of operations specific to particular nodes.  This allows nodes to not have to worry about name collisions with registering their own operation functions.

The constructor operations are specified in detail in separate documentation about the core operations provided by the Lilac interpreter.
