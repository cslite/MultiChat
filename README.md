# MultiChat

A Multicast based chat application that works in a LAN environment. Any user can join multiple courses. A user can send a message to any of his/her registered course and will also receive messages sent to those courses.

## Getting Started

### Compilation

Use `make` to compile the code. It will generate an executable named `mchat`.
 
### Running the MultiChat

```bash
$ ./mchat <path to course-data file>
```

For example, if the course-data file is named `course-info.txt` and is stored in the current directory, to run the application,
```bash
$ ./mchat course-info.txt
```

#### Optional loopback argument
The `mchat` application also accepts an optional argument to set loopback. (Loopback is _disabled_ by default).

For example, if the course-data file named `course-info.txt` is stored in the current directory and you want to allow loopback, run application as,
```bash
$ ./mchat course-info.txt 1
```

