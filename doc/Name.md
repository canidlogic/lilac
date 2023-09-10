# Lilac name identifiers

Lilac is an extensible framework, which means it needs a naming system where names defined by one extension won't conflict with names defined by a different extension, even when the two extensions are not aware of each other's existence.

## Name identifier format

Name identifiers in Lilac are strings of 1 to 255 ASCII alphanumerics, underscores, and periods.  Periods are intended as hierarchical separators, so neither the first nor last character of a name identifier may be a period, nor may a period occur immediately after another period.

Using the period as a separator, name identifiers can be split into an array of one or more components.  Each component has at least one character, and components only contain ASCII alphanumerics and underscores.  Each component prior to the last is a _package,_ with packages organized in a hierarchical tree.  The last component is the _proper name._  For example, consider the following name identifier:

    com.example.library.Identifier

This name identifier has three packages:  `com` `example` and `library`.  Its proper name is `Identifier`.  The `example` package is part of the `com` package, the `library` package is part of the `example` package, and the `Identifier` proper name is part of the `library` package.

It is possible for a name identifier to have no packages, which occurs if there is not a single period character in the identifier.  However, identifiers always have a proper name.  If there are no packages, the proper name is equivalent to the whole name identifier.

Matching of name identifiers is case sensitive.  Therefore, `Identifier` is a completely different name from `identifier`.

## Package conventions

The package `lilac` is reserved for use by name identifiers defined by the Lilac core program and the Lilac base plug-ins.

Name identifiers with no packages are not recommended.  Lilac only defines the special name identifier `null` without any packages.

Extensions outside of Lilac are recommended to start with a domain name in reverse order, where the domain is unique to the specific developer.  For example, if the developer controls a domain name `developer.example.com` then the Lilac name identifiers created by that developer should begin with the packages `com.example.developer`.  This ensures that names defined by one developer will not conflict with names defined by a different developer, even if the two developers do not know about each other.

However, Lilac does not enforce these conventions.  They are merely recommendations for avoiding name conflicts.

## Namespaces

Lilac name identifiers are used in different contexts within Lilac, such as for identifying a data type versus identifying a specific compressor to use with a deep bitamp.  A _namespace_ is a set of names specific to a particular context.

Names only need to be unique within the namespace they belong to.  For example, the namespace of data types and the namespace of deep bitmap compressors are separate.  This means that each data type needs a unique name within the set of data types and each deep bitmap compressor needs a unique name within the set of deep bitmap compressors.  However, it is possible for a data type and a deep bitmap compressor to have the exact same name because they are in separate namespaces.

To avoid confusion, however, names should be kept unique across all namespaces as much as possible, though Lilac will not complain if this convention is not upheld.
