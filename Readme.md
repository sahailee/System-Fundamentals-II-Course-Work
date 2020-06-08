# System Fundamentals II Coursework
#### Stony Brook University
This repo contains all of my course work for CSE 320, System Fundamentals II.
All the code is done in C and the assignment instructions are included as Readmes for each assignment.
## Summary of Assignments
### HW1 - Compression/Decompression Utility
The goal of this assignment was to create a command line utility to compress and decompress text with the Sequitur algorithm. With this assignment I learned about pointers, manipulating bits in C, and how to deal with input and output in C.
### HW2 - Debugging in C
The task is to update an old piece of software, vtree, so that there are no errors or memory leaks. Besides the issues with the outdated software, the professor introduced some more bugs into the code needing to be fixed. This assignment taught me how to debug and test in C using gdb debugger and valgrind.
### HW3 - Dynamic Memory Allocator
The goal of this assignment was to better understand dynamic memory allocators and to increase familiarity with C. In this assignment I had to implement my own version of C's malloc, free, realloc, and memalign functions based on the professors' requirements.
### HW4 - Multi-process Problem Solver
Here the objective of the program I have to create is a problem solver that can take in many different problems of various types and concurrently solve them. The time to solve each problem varies each time thus making concurrent safety extremely important. A master keeps track of all the problems that needs to be completed and assigns it to idle workers. The master and workers communicate through pipes which is installed by the master. The program executes until all problems are solved, all the processes safely exited, and all pipes have been safely closed.
### HW5 - Private Branch Exhange Telephone System
The goal of this assignment is to simulate the behavior of Private Branch Exhange telephone systems. A server must be created to handle the connections of multiple clients and assign them unique id's. For each client, a new thread is started and acts as a worker to allow concurrent communication between the specific client and the server. In this program, a client represents a phone, and these phones can dial other phones and chat with them. It is the server's responsibility to safely connect these clients and allow the clients to communicate with each other through the server.

This assignment was definitely my favorite because we essentially created an instant messaging program using concurrency and networking in C.
