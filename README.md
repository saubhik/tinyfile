# Project 2: Inter-process Communication Services

**Release Date: Friday Feb. 12, 2021**

**Due Date: Friday Mar. 07, 2021 @ 11:59pm**

---

* You can choose to work in pair on this project. It effects the extra credit part in this project. 
Please check [here](#extra-credit-qos-quality-of-service).
* You may share ideas with other pairs, **but you may not share code**.
* You also may not download code or use any other external libraries without consulting the instructor or TA first.


## 0. Goal
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
What you need to implement in this project are:

1. TinyFile service

2. TinyFile library to accesing the TinyFile service

3. Sample application to test functionalities


   
### TinyFile service

* A service process/daemon that will manage the input requests and send back a response
* Your design of TinyFile service must incorporate the concept of a service process that actually executes the service.
* A TinyFile server has 2 working modes: SYNC (blocking) / ASYNC (nonblocking)

  * **Synchronous (SYNC)**
    * The library you write will need an initialization function that establishes the control structures that you may want to use in your library - queue(s), any common shared memory region you want to use in your library, and soon. 
Next, you will need functions for your library that allow blocking (synchronous) calls to the TinyFile service. 
Here, a blocking call means that the sender will block until the caller has received a response from the service

  * **Asynchronous (ASYNC)**
    * There are times when a process will send a request, but it does not want to wait for the response. 
This form of request is called asynchronous because the process does not have a specific time ordering with respect to its computations and the receipt of a response. At first glance this may seem easier than synchronous (blocking) communication because the sender does not have to wait for a response, but that is not the case. 
The service process must still potentially need to know when a response has been received back by the calling process 
- there may be structures (including the response itself) that must be cleaned up once the response has been received, 
or certain application logic may start up or continue once the recipient has acknowledged the response.

    * For this part of the project you will need to implement asynchronous communication. You have a choice as to how the service process can choose to determine that a response has been delivered. One of the design options to implement async process is to have a blocking function call in the caller that at a later time retrieves the result and initiates the cleanup. The second choice is to let the caller register a callback function that is called by the service process when a response is ready. This also requires that the callback function is called with a reference to the original response. In either case, you will need to think about how to notify the calling process that the request is complete and to deliver any data in the response.

    * The library you implement must have some way to specify a blocking call versus a non-blocking call. Ideally, it should be possible to have both uncompleted synchronous and uncompleted asynchronous calls pending completion at the same time. As before, you have a lot of latitude on your design and you must document it, its shortcomings (if any), and shortfalls in implementation.
* The service process can use one or more queues on which the requests are queued up.
  * How many you choose to use depends on your design, but must be justified.

* Note that you will need to use shared memory for all the data is shepherded back and forth.
* The service process can be started as a daemon, meaning that it is an independent process can be started from the command line.
* The service process implements a file compression functionality. Coupled with QoS functionality, this non-trivial computation kernel allows you to demonstrate the queue management of your service process.
  * Use the [snappy-c library](https://github.com/andikleen/snappy-c) to implement file compression functionality.

### TinyFile Library
* You need to implement a library for accessing the TinyFile service—a library that your application can link in.
* This library will consist of an API for sending requests and getting back a response (in a same function call)

### Sample Application
* You will need to implement a Sample Application which will be used to demonstrate the functionality of the TinyFile service.
* The sample application must exercise all the functionality of your library and service—synchronous requests, asynchronous requests and optionally QoS, must all be demonstrated.

### Design Justification
Since you have freedom in your design, you must **document the design**, any logicas to why you chose it, any shortcomings that the design may have, and any part of your design that you failed to implement.

### Extra Credit: QoS (Quality of Service)

* **Extra Credit(10pt)**: QoS implementation is optional for 6210 students working alone on this project and 4210 students. However, can get extra credit (10pts) if implemented. 
**For all 6210 students who work in pairs, it is mandatory for full credit.**
Students can also receive extra credits if they implement some more creative ring-like structure and notification mechanism, 
one that's present in Xen. 
Also, QoS feature should be explained in detail with its design in your report to receive credits.

* TinyFile service allows any process to use it, but it has no concept of fairness. For example, suppose in 1 second that process P1 first sends 100 requests, then process P2 sends 10 requests, and then process P3 sends 2 requests. If all of process P1's requests are answered first, then the service is denied to processes P2 and P3. Alternatively, it's not clear how many requests of P1 should be serviced before P2's and P3's requests are serviced.
* Here you will be developing a Quality of Service (QoS) mechanism for the TinyFile service. The mechanism must take fractional access into account. For this, the service process alternates servicing requests among processes according to the fraction of the service that they've been granted. For example,
  * if there are 4 processes that are each granted 25% of the access, then the QoS mechanism would essentially round-robin access among the 4 processes.
  * In another example, process A may be granted 1/2 of the service requests, process B may be granted 1/3 of the service requests, and process C may be granted 1/6 of the service requests. Then over the course of 6 messages, the service process will handle 6 * 1/2 = 3 of A's messages, 6 * 1/3 = 2 of B'smessages, and 6 * 1/6 = 1 of C's messages (though not necessarily in that order).
  * Designing this QoS mechanism: You may choose to implement a single queue that the service process must re-examine to determine what message to process next, or you may choose to implement multiple queues - say, one queue per communicating process. Since there is such freedom in choosing your design, you must document it, the reasons why you chose it, any shortcomings it may have, and what in your design was not implemented. 
*For extra credit, you must implement and document the QoS mechanism*.

### Dependencies
* snappy-c library: installed on course VMs

### Argument Synopsis

* The service program/application should accept the configuration filepath/arguments as a runtime parameter.
  
  * Under directory ``bin/input``, there are sample files for compression.

* TinyFile service

  * Working mode should be a configurable runtime parameter.
  * parameters:
    * ``--state``: SYNC | ASYNC
    * e.g. running TinyFile service in blocking mode: `./tinyfile --state SYNC`

* A sample Application to use the service using TinyFile Library

  * The number and size of the shared memory segments should be as configurable runtime parameters.
  * The path to indicate the file(s) to be compressed
  
  * parameters:
    * ``--n_sms``: # shared memory segments
    
    * ``--sms_size``: the size of shared memory segments
    
    * ``--file``: specify the file path to be compressed
    
    * ``--files``: specify the file containing the list of files to compressed. You will want to take advantage of this argument to test QoS.
    
    * For example, if you were to run a sample application: 
    
      * for single file request: `./sample_app --file ./aos_is_fun.txt --n_sms 5 --sms_size 32`
    
      * for multi-file request: `./sample_app --files ./filelist.txt --n_sms 5 --sms_size 32`
    
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

Library installation on course VM can be requested via Piazza.


## 4. Delivarables
Please include all reports, Makefile, etc., in a directory and compress it as a tarball. 
**If you work in a group**, one of you submits the compressed file while the other member submits a text file contained the names of members. Please make the structure inside tarball like below.

* File name: project\_2\_\<gt\_account\_id\>.tar.gz
  * Example: ``project_2_gburdell3.tar.gz``
     - src: have to have makefile here as well, and it is supposed to be built with `make` and cleaned with `make clean`.
     - bin: 
       - build results
       - input files
       - output 
     - project_2_report_<gt_account_id>.pdf

### Report
The report is expected to include (but not limited to) the design choices, reasoning behind the design selection, 
design flow diagram, the performance of your inter-process communication facility, 
result graphs and drawbacks of the implemented design.

* Your report needs to explain and show following items

  - that you have a good understanding of this project.

  * Overall Design
    - shared memory/queue(s) management—have flow charts/diagram in the report
  * SYNC/ASYNC implementation

  * How the CST varies along with segment size and the number of segments
    * **Client-side Service Time (CST)**: Time period between sending the request and receiving the response.
    * Experimental results to show, preferrably figures.

  * QoS (if implemented)
    - How and why you designed it?
    - How does QoS improve fairness comparing to non QoS version: 
put the description of experiments and the results.

  * Sample Application: what it does in order to verify which features you implmented.

* Log Requirements:

  * CST log print must be followed after each request.
  
* Figures Guide:
  * Segment Size: sms_size = {32, 64, 128, 256, 512, 1024, 2048, 4096, 8192} bytes
  * Number of Segments: n_sms = {1,3 and 5}
  * X axis = Segment Size and Y axis = CST in seconds
  * You need to show how SYNC/ASYNC mode, shared memory segment size and the number of segments affects the performance.
  * You are encourage to have more figures other than specified to justify or report your observations.
  * You can consider figures like the below, but you don't have to stick to it. It can be a barchart as well. However, you need to have some data in this plane  
 ![Graph Example](doc/simple_graph_example.png)
  * You don't necessarily have all figures of possible combinations.

    
## 5. Late Submission Policy

* A penalty of 10% per day will be applied for late submissions for up to 3 days. Submissions received more than 3 days late will receive no credits.
  
