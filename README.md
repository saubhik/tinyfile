# Inter-process Communication Services

The goal of the project is to create a service domain that can be accessed from any process in the system. In order to support this service, you will be creating a library of calls to access this service. 
This service must allow blocking (synchronous) calls, non-blocking (asynchronous) calls, 
and QoS mechanisms (optional for extra credits).

### Background
In Xen, there is a domain called *Dom0* which, among other things, 
handles requests from guest operating systems for performing I/O. 
The guest operating systems (clients) place their data for the operation (e.g., a file write) 
on a ring buffer, and Dom0 handles the requests in turn. 
You will be creating an analog of Dom0 which will handle requests for a service. 
Specifically, your task is to implement a shared memory based file compression service (TinyFile) and a client library.

* Clients submit requests which include the file name, file-content, etc. to the TinyFile service over shared memory.

* The TinyFile service will respond to clients with a compressed version of the file.
* The original file content and the compressed file content (and any other service data) must be exchanged between clients and service via shared memory.
* The TinyFile service is capable of handling any kind of file content. (e.g. text, binary, etc)
* TinyFile service supports:
  * synchronous calls - the caller has to block until the call is complete and the resultis delivered.
  
  * asynchronous calls - the caller will call the service and then continue to run whilethe service request is being processed. The asynchronous call must have some method to deliver the results back to the caller while the caller is still executing, or potentially for the caller to retrieve or even wait on the results, once it has nothing else to do.
  
  * Optionally, Quality of Service (QoS) mechanisms which improves fairness among the processes.

## 1. Project Details

### TinyFile service

* A service process/daemon that will manage the input requests and send back a response
* Your design of TinyFile service must incorporate the concept of a service process that actually executes the service.
* A TinyFile server has 2 modes of requests: SYNC (blocking) / ASYNC (nonblocking)

- there may be structures (including the response itself) that must be cleaned up once the response has been received, 
or certain application logic may start up or continue once the recipient has acknowledged the response.

    * For this part of the project you will need to implement asynchronous communication. You have a choice as to how the service process can choose to determine that a response has been delivered. One of the design options to implement async process is to have a blocking function call in the caller that at a later time retrieves the result and initiates the cleanup. The second choice is to let the caller register a callback function that is called by the service process when a response is ready. This also requires that the callback function is called with a reference to the original response. In either case, you will need to think about how to notify the calling process that the request is complete and to deliver any data in the response.

* The service process can use one or more queues on which the requests are queued up.
  * How many you choose to use depends on your design, but must be justified.

* Note that you will need to use shared memory for all the data is shepherded back and forth.
* The service process can be started as a daemon, meaning that it is an independent process can be started from the command line.
* The service process implements a file compression functionality. Coupled with QoS functionality, this non-trivial computation kernel allows you to demonstrate the queue management of your service process.
  * Use the [snappy-c library](https://github.com/andikleen/snappy-c) to implement file compression functionality.

### TinyFile Library
* You need to implement a library for accessing the TinyFile service—a library that your application can link in.
* This library will consist of an API for sending requests and getting back a response (in a same function call) by two ways:

  * **Synchronous (SYNC)**
    * The library you write will need an initialization function that establishes the control structures that you may want to use in your library - queue(s), any common shared memory region you want to use in your library, and soon. 
Next, you will need functions for your library that allow blocking (synchronous) calls to the TinyFile service. 
Here, a blocking call means that the sender will block until the caller has received a response from the service

  * **Asynchronous (ASYNC)**
    * There are times when a process will send a request, but it does not want to wait for the response. 
This form of request is called asynchronous because the process does not have a specific time ordering with respect to its computations and the receipt of a response. At first glance this may seem easier than synchronous (blocking) communication because the sender does not have to wait for a response, but that is not the case. 
The service process must still potentially need to know when a response has been received back by the calling process 

* The library you implement must have some way to specify a blocking call versus a non-blocking call. 
Ideally, it should be possible to have both uncompleted synchronous and uncompleted asynchronous calls pending completion at the same time in your application. As before, you have a lot of latitude on your design and you must document it, its shortcomings (if any), and shortfalls in implementation.

### Sample Application
* You will need to implement a Sample Application which will be used to demonstrate the functionality of the TinyFile service.
* The sample application must exercise all the functionality of your library and service—synchronous requests, asynchronous requests and optionally QoS, must all be demonstrated.

### Dependencies
* snappy-c library: should be installed along with your codebase.

### Argument Synopsis

* The service program/application should accept the configuration filepath/arguments as a runtime parameter.
  
  * Under directory ``bin/input``, there are sample files for compression.

* TinyFile service

  * Working mode should be a configurable runtime parameter.
  * parameters:
    * ``--n_sms``: # shared memory segments
    * ``--sms_size``: the size of shared memory segments, in the unit of bytes.
    * e.g. running TinyFile service in blocking mode: `./tinyfile --n_sms 5 --sms_size 32`

* A sample Application to use the service using TinyFile Library

  * The number and size of the shared memory segments should be as configurable runtime parameters.
  * The path to indicate the file(s) to be compressed
  
  * parameters:
    * ``--state``: SYNC | ASYNC
    * ``--file``: specify the file path to be compressed
    * ``--files``: specify the file containing the list of files to compressed. You will want to take advantage of this argument to test QoS.
    
    * For example, if you were to run a sample application: 
    
      * for single file request: `./sample_app --file ./aos_is_fun.txt --state SYNC`
    
      * for multi-file request: `./sample_app --files ./filelist.txt --state SYNC`
    
      * the files put in the filelist file should be listed in a column:
    
        ```
        $ cat filelist.txt
        input/Large.txt
        input/Huge.jpg
        :
        ```
    
  * You can possibly add other arguments as you need, just make sure putting some note in your report. Also, add `README` file with instruction.
  
    

## 2. Experiments-Inputs and Outputs

* Inputs: You can find sample files in ``bin/input`` directory in the repo, which you can use when you develop your service and application.

    |File Types|Size|Description|
    |:-:|:-:|:-:|
    |Tiny|34 KB|A Text file with lots of text|
    |Small|233 KB|A picture of a Kitten|
    |Medium|533 KB|A pdf file|
    |Large|1 MB|A file with random numbers|
    |Huge|1.9 MB|A picture of a Bird|

* Please place flow charts describing how the communication management works.


## 3. Hints and Tips
First, you will need some sort of queue to send messages to another process, and you will need to lock access to that queue. For that it is recommended that you check out the System V or POSIX library's message queue and semaphores. For example, in System V library, the message queue is available via functions that start with ``msg``:
* **msgget**: create the queue
* **msgsnd**: place a message on a queue
* **msgrcv**: pull a request off of a queue
The semaphore access is similar, with semget being the primary function of interest.

You should also become familiar with shared memory functions such as the following:
* **shmget**: create a file for shared memory use OR give a handle (called a key) to access it
* **ftok**: map a filename to a key for use in shmget
* **shmat**: allow(attach) a piece of shared memory to the calling process's address space
* **shmdt**: remove(detach) a piece of shared memory from the calling process's address space
  

You can also consider the following memory mapping functions, which can be a replacement for shmget:
* **shm_open**: create a file for shared memory use OR give a handle (called a file descriptor) to access it
* **mmap**: map a file descriptor taken from shm_open to a piece of memory
* **munmap**: unmap the same file from the process's memory

You may think using POSIX library is more attractive than System V library because it also has access to the potentially useful semaphore and message queue functionality. What you use is up to you (and your partner). Any other external library must be cleared by the TAs or the instructor first.
