
A simple web server written in C.

The structure of the server:
- HTTP request parser
- HTTP response builder
- LRU cache
    - Doubly linked list
    - Hash table

Completed:
- Main Goals
    - 1. HTTP request parser and HTTP response builder which support method
         `GET` and `POST`
    - 2. LRU cache which use doubly linked list to organize entries and hash
         table to retrieve entries.
- Stretch Goals
    - 1. Post a file
    - 2. Automatic `index.html` serving

To be continued:
- Stretch Goals:
    - 3. Expire cache entries
    - 4. Concurrency
         - multi-process, multi-threads or i/o multiplexing (use `epoll`).
