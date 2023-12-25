# MiDAS


## What is MiDAS?

MiDAS is a Migration-based Data placement technique with Adaptive group number and Size configuration for log-structured systems. 

MiDAS is implemented in both **trace-driven simulation** and a **real SSD prototype**. The simulator is used to evaluate the WAF of GC techniques quickly. The real SSD prototype is used to measure I/O performance and the overhead associated with the CPU and memory for system execution.

The original paper that introduced MiDAS is currently in the revision stage of [USENIX FAST 2024](https://www.usenix.org/conference/fast24).

The archive snapshot with a detailed screencast used in the paper is available at (link) (not available until Feb 27, 2024).


## Trace-driven simulation

The simulation of MiDAS is implemented by C. 
You can evaluate the WAF of MiDAS with the simulator and trace files.
The test code for simulation is here: https://github.com/sungkyun123/MiDAS-Simulation


## Real SSD prototype

MiDAS is implemented on [FlashDriver](https://github.com/dgist-datalab/FlashFTLDriver), a user-space Flash Simulation Platform.

To measure I/O performance and the overhead associated with the CPU, you need special hardware (Xilinx VCU108+Customized NAND Flash).

However, you can evaluate WAF without the specific hardware using memory (RAM drive).

The MiDAS algorithm is implemented in the directory `algorithm/MiDAS/`.


### Hardware Prerequisites

The hardware/software requirements for executing MiDAS are as follows.


* `DRAM`: Larger than the device size of trace files + an extra 10% of the device size for the data structures and OP region to test the trace files. For example, you need 140GB size of DRAM to run the trace file with a 128GB device size.


### Installation & Compilation

* Clone the required repository (MiDAS SSD prototype) into your host machine.

```
$ git clone https://github.com/dgist-datalab/MiDAS.git
$ cd MiDAS
```


### Trace file format

If you want to test your own trace file, you have to set the format of the trace file like this: 

```
<Time Stamp (s)> <Request Type> <LBA (unit: 4KB)> <Request Size>

- Time Stamp (in seconds): This is the time when the request is made to a block device.
- Request Type: This is categorized into four types: 0 for READ, 1 for WRITE, 2 for NOT USED, and 3 for TRIM.
- LBA (Logical Block Address, unit: 4KB): This represents the logical block address on the block device, with each unit of LBA being 4KB.
- Request Size: This indicates the size of the request being processed.
```


The example of the trace file is as follows:

```
0.146559796 1 0 4096 
0.146609386 1 1 4096 
0.146619996 1 2 4096 
0.146626626 1 3 4096 
0.146632166 1 4 4096 
0.146637836 0 5 8182 
0.146643556 0 6 8192 
0.146648946 0 7 8192 
0.146654316 0 8 8192 
```


* You can also download the trace file to test the prototype. We upload a FIO zipfian 1.0 workload with a device size of 8GB. Because the trace file we used in paper is too large to upload to the cloud, we created a smaller file for the test. The device size of this trace file is 8GB, and the file size is 1.4GB. The SHA256 hash value of the trace file matches the following: `2171a00ff770a7279383522cb5961b55d1976feeda79fea4607d1146e4fa1c69`

```
$ wget https://zenodo.org/record/10409599/files/test-fio-small
$ sha256sum test-fio-small
2171a00ff770a7279383522cb5961b55d1976feeda79fea4607d1146e4fa1c69
```


### Compile & Execution

When you want to test MiDAS, please follow the instructions below.
In our testing environment, the following steps finished in 650 ~ 680 seconds.
If you want to test MiDAS on the simulation environment for fast evaluation, go to the following link: https://github.com/sungkyun123/MiDAS-Simulation

```
$ sudo apt install git build-essential
$ git clone https://github.com/dgist-datalab/MiDAS.git
$ cd MiDAS
$ git submodule update --init --recursive
$ wget https://zenodo.org/record/10409599/files/test-fio-small # trace file
$ make clean; make GIGAUNIT=8L _PPS=128 -j # Device side = 8GB, PPS = Pages Per Segment
$ ./midas test-fio-small
``` 


### Some statements for code structure

MiDAS algorithm is implemented in the algorithm/MiDAS/ directory.
This includes UID, MCAM, and GCS algorithms.
- `midas.c`     : adaptably change group configuration and check irregular pattern
- `model.c`     : UID, MCAM, GCS algorithm
- `gc.c`        : victim selection policy
- `hot.c`       : hot group separation


### Results

During the experiment, you can see that MiDAS adaptably changes the group configuration.


* UID information : Upon running MiDAS, the parameters of UID will be displayed at the beginning.
We sample a subset of LBA for timestamp monitoring with a sampling rate of 0.01.
We use a coarse-grained update interval unit of 16K blocks and epoch lengths of 4x of the storage capacity (8GB).

The following result is from an example run:

```
Storage Capacity: 8GiB  (LBA NUMBER: 1953125)
*** Update Interval Distribution SETTINGS ***
- LBA sampling rate: 0.01
- UID interval unit size: 1 segment (1024 pages)
- Epoch size: 32.00GB (2048 units)
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


* When an epoch is over, the GCS algorithm finds the best group configuration using UID and MCAM. The group configuration is shown as follows.

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


* MiDAS periodically checks the irregular pattern of the workload. If there is a group whose valid ratio prediction is wrong, MiDAS gives up on adjusting group sizes for all groups beyond the group and simply merges the groups.

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


* When the execution is over, You can check the total runtime, read traffic, write traffic, and WAF of the MiDAS.
```
runtime: 950 sec
Total Read Traffic : 23531283
Total Write Traffic: 31225469

Total WAF: 2.52

TRIM 239862
DATAR 0
DATAW 12384205
GCDR 23531283
GCDW 18841264
```
