# DNSNameResolver

A multi-threaded code that resolves domain names to IP addresses

Authors:
- Bradley Arnot


## Files
### multi-lookup.c
The main code that reads in the input files, creates threads, resolves names to IP's, and writes the results to an output file.

### queue.c
An implemntation of a simple queue

### util.c
Contains the dnslookup function for resolving domain names to IP addresses


## How to Run
`$> make`

`$> ./multi-lookup input/* output.txt`

NOTE: This uses the pthread library and only works on POSIX systems
