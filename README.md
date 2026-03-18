# Operating Systems Labs – Kiwi & LKL Projects

## 📌 Overview

This repository contains multiple laboratory assignments for the **Operating Systems course**, focusing on:

* Thread synchronization (mutex & readers-writers)
* Storage engine benchmarking
* Linux Kernel Library (LKL)
* Filesystem operations & logging

---

# 🧪 Lab 1 – Mutex Synchronization (Kiwi Storage Engine)

## 🧠 Description

In this assignment, we implemented **basic thread synchronization using mutex (Pthreads)** in the Kiwi storage engine.

The goal was to prevent conflicts when multiple threads perform:

* Read operations
* Write operations

👉 Only **one thread can access the database at a time** (either read or write). 

---

## ⚙️ Implementation

### 🔐 Mutex Integration

We added:

* `pthread_mutex_t db_mutex` in `db.h`
* Lock/Unlock logic in:

  * `db_add`
  * `db_get`
  * `db_remove`

Also:

* Initialized mutex in `db_open_ex`
* Destroyed mutex in `db_close`

---

## 🔄 Behavior

* Every operation locks the mutex before execution
* Ensures **mutual exclusion**
* Prevents race conditions
* No parallel reads/writes allowed

---

## 🧪 Benchmark Extension

Added new command:

```bash
./kiwi-bench write_read <count> <write%> <read%>
```

Supports:

* Sequential execution
* Random execution (flag)

---

## 📊 Results

* Sequential workloads perform faster than random
* Balanced workloads (e.g. 40-60) showed better performance

---

---

# 🧪 Lab 2 – Filesystem Logging (LKL – VFAT)

## 🧠 Description

In this assignment, we implemented a **logging system for filesystem operations (VFAT)** inside the **Linux Kernel Library (LKL)**.

The system records:

* File creation
* Directory creation
* File modifications

👉 Logs are stored in files like `/tmp/lkl_cptofs.log` 

---

## ⚙️ Key Components

### 📄 cptofs.c

Main responsibilities:

* File copy (host ↔ LKL filesystem)
* Logging system
* Argument parsing (argp)

#### Logging System

* `log_init()`
* `log_message()`
* `log_info()`
* `log_warn()`
* `log_error()`

Includes:

* Timestamp
* Log level (INFO, WARN, ERROR)

---

### 💾 disk.c

Handles:

* Disk image loading
* Mount/Unmount operations
* Interaction with LKL kernel

Key features:

* Add/remove disk (`lkl_disk_add`)
* Mount filesystem
* Read directory contents

---

## 🔄 Core Functionality

### 📁 File Operations

* Open source/destination files
* Read/write data using buffers
* Copy files efficiently (4096 buffer)

### 📂 Directory Handling

* Recursive directory traversal
* Create destination directories
* Copy entire directory trees

### 🔗 Symbolic Links

* Read symlinks
* Create symlinks
* Copy symbolic links

---

## 🧪 Execution

```bash
make -j8
make run-tests ./tests/run.py
```

Additional commands:

```bash
./disk.sh -t vfat
./cptofs -p -I vfat file /
cat /tmp/lkl_cptofs.log
```

---

## 📊 Features

* ✔ Real-time logging
* ✔ File & directory copy
* ✔ LKL filesystem interaction
* ✔ Recursive operations
* ✔ Error handling & reporting

---

---

# 🧪 Lab 3 – Readers-Writers Synchronization (Kiwi)

*(Add your latest assignment here – already implemented in your newer repo)*

👉 You can include:

* Condition variables
* Multiple readers support
* Writer exclusivity

---

## 🧱 Project Structure

```bash
.
├── lab1/   # Mutex synchronization
├── lab2/   # LKL filesystem logging
├── lab3/   # Readers-Writers (new)
├── README.md
```

---

## 🛠️ Technologies

* C
* Pthreads
* Linux
* LKL (Linux Kernel Library)
* File Systems (VFAT)
* Concurrency

---

## 📚 Notes

This repository demonstrates:

* Evolution from simple mutex → advanced synchronization
* From local DB → kernel-level filesystem operations

---
