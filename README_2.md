# MiDAS


## What is MiDAS?
MiDAS is a Migration-based Data placement technique with Adaptive group number and Size configuration for log-structured systems. 

MiDAS is implemented in both **trace-driven simulation** and a **real SSD prototype**. The simulator is used to quickly evaluate WAF of the GC techniques. Also, We used a real SSD prototype to measure I/O performance and the overheads associated with the CPU and memory for system execution.

The original paper that introduced MiDAS is currently in the revision stage of [USENIX FAST 2024](https://www.usenix.org/conference/fast24).

The archive snapshot with a detailed screencast used in the paper is available at ~~link~~.


## Trace-driven simulation
The simulation of MiDAS is implemented by C. 

You can evaluate the WAF of MiDAS with the simulator and trace files.


### Prerequisites on software
The software requirements for executing MiDAS simulator are as followed.

~
~
~

## Real SSD prototype
MiDAS is implemented on [FlashDriver](https://github.com/dgist-datalab/FlashFTLDriver), a user-space Flash Simulation Platform.

To measure I/O performance and the overheads associated with the CPU, you need special hardware (Xilinx VCU108+Customized NAND Flash).

However, you can evaluate the WAF without the specific hardware using a memory (RAM drive).

The MiDAS algorithm is implemented in the directory `algorithm/MiDAS/`.


### Prerequisites
The hardware/software requirements for executing MiDAS are as followed.


#### Hardware
* `DRAM`: Larger than device size of trace files + extra 10% of the device size for the data structures and OP region to test the trace files. For example, you need 140GB size of DRAM to run trace file with 128GB device size.


#### Software
~
~
~


### Installation & Compilation
* Clone required reopsitory (MiDAS SSD prototype) into your host machine.
```
$ git clone ~
$ cd FlashFTLDriver
```

* Download the trace file to test the prototype
```
$ ~
```


### Compile & Execution
After downloading trace file, you can test MiDAS. This experiment may be finished in %% minutes by your server environment.

* FIO Zipfian 1.0 workload (filename: ~)
   * You can test the same FIO-H workload in the paper. But you need larger than 135GB size of DRAM.

```
$ make clean; make -j
$ ./midas {trace_file}
```

* Smaller FIO Zipfian 1.0 workload (filename: ~)
   * You need 8GB size of DRAM to test this trace file.
```
$ make clean; make GIGAUNIT=4L _PPS=128 -j
$ ./midas {trace_file}
``` 


### Some statements for code structure
MiDAS algorithm is implemented in a algorithm/MiDAS/ directory.
This includes UID, MCAM, GCS algorithms.
- `midas.c`     : adaptably change group cofiguration, and check irregular pattern
- `model.c`     : UID, MCAM, GCS algorithm
- `gc.c`        : victim selection policy
- `hot.c`       : hot group separation


### Results
During the experiment, you can see that MiDAS adaptably change the group configuration.


* UID information : When you run MiDAS, You can see the parameters of the UID at the beginning. 
We sample a subset of LBA for timestamp monitoring, with a sampling rate of 0.01. 
We use a coarse-grained update interval unit of 16K blocks and epoch lengths of 4x of the storage capacity (128GB).
Following result is an example.

```
Storage Capacity: 128GiB  (LBA NUMBER: 31250000)
*** Update Interval Distribution SETTINGS ***
- LBA sampling rate: 0.01
- UID interval unit size: 1 segments (16384 pages)
- Epoch size: 512.00GB (2048 units)
```


* Throughput information

Throughput is calculated per 100GB write requests.

```
[THROUGHPUT] 347 MB/sec (current progress: 300GB)
[THROUGHPUT] 246 MB/sec (current progress: 400GB)
```


* You can see the group configuration and valid ratio of the groups per 1x write request of the storage capacity.

```
[progress: 896GB]
TOTAL WAF:	1.852, TMP WAF:	1.640
  GROUP 0[4]: 0.0000 (ERASE:287)
  GROUP 1[51]: 0.7653 (ERASE:224)
  GROUP 2[222]: 0.5629 (ERASE:102)
  GROUP 3[28]: 0.4636 (ERASE:30)
  GROUP 4[12]: 0.6019 (ERASE:2)
  GROUP 5[190]: 0.4284 (ERASE:195)

```


* When an epoch is over, GCS algorithm finds the best group configuration using UID and MCAM. The group configuration is shown as follows.

```

*****MODEL PREDICTION RESULTS*****
group number: 6
*group 0: size 4, valid ratio 0.000000
*group 1: size 51, valid ratio 0.734829
*group 2: size 222, valid ratio 0.350166
*group 3: size 28, valid ratio 0.413656
*group 4: size 12, valid ratio 0.734003
*group 5: size 191, valid ratio 0.781421
calculated WAF: 1.626148
calculated Traffic: 0.570
************************************
```


* MiDAS periodically check the irregular pattern of the workload. If there is a group that its valid ratio prediction is wrong, MiDAS gives up on adjusting group sizes for all groups beyond the group and simply merges the groups.

```
==========ERR check==========
[GROUP 0] calc vr: 0.000, real vr: 0.000	(O)
[GROUP 1] calc vr: 0.735, real vr: 0.765	(O)
[GROUP 2] calc vr: 0.350, real vr: 0.559	(X)
!!!IRREGULAR GROUP: 2!!!
=> MERGE GROUP : G2 ~ G5
=> NAIVE ON!!!!
===============================
```

