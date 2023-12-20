import os
import sys
import numpy as np
import matplotlib as mpl
if os.environ.get('DISPLAY','') == '':
    print('no display found. Using non-interactive Agg backend')
    mpl.use('Agg')
from matplotlib import pyplot as plt

#python3 ./analy... <valid path> <group number>


valid_path = sys.argv[1]
group_num = int(sys.argv[2])
fr = open(valid_path, "r")

valid_rate_list = []
group_info_list = []
group_rate_sum = np.array([0. for i in range(group_num)])
group_trim_num = np.array([0. for i in range(group_num)])
x_val = [[] for i in range(group_num)]
y_val = [[] for i in range(group_num)]
TRIM_number = 0
trim = fr.readline()
while trim:
    trim = trim.split(" ")
    #print(trim)
    valid_rate = (trim[1].split("\n"))[0]
    valid_rate = float(valid_rate)
    #print(valid_rate)
    group = int(trim[0])
    #print(trim)
    x_val[group-1].append(TRIM_number+1)
    y_val[group-1].append(valid_rate)
    group_trim_num[group-1] += 1
    group_rate_sum[group-1] += valid_rate
    valid_rate_list.append(valid_rate)
    group_info_list.append(group)
    trim = fr.readline()
    TRIM_number += 1
    if TRIM_number % 50000 == 0:
        print("TRIM_number:", TRIM_number)

group_rate_avg = group_rate_sum/group_trim_num
#print(group_rate_sum)
#print(group_rate_sum.sum(), TRIM_number)
total_rate_avg = group_rate_sum.sum()/TRIM_number

x_total = [i+1 for i in range(TRIM_number)]
color_map = ['black','green', 'gray', 'purple', 'cyan', 'saddlebrown', 'red', 'blue', 'silver', 'darkgreen', 'coral', 'olive', 'pink', 'rosybrown','moccasin', 'springgreen', 'skyblue', 'tomato', 'slategray', 'indigo', 'teal']
fig = plt.figure()
plt.scatter(x_total,valid_rate_list, marker="o", s=0.3, color='black')
plt.ylim([0, 100])
plt.xlabel("progress")
plt.ylabel("hot data percent")
plt.axhline(total_rate_avg,color='r')
plt.savefig("bps/"+valid_path+"_total.png")
plt.show()
fig.clear()
print("png 1 complete!")
"""
for i in range(group_num):
    x_val = []
    y_val = []
    for j in range(TRIM_number):
        if valid_rate_list
"""
"""
for i in range(TRIM_number):
    plt.scatter(i+1, valid_rate_list[i], marker="o", s=0.3, color=color_map[group_info_list[i]-1], label="group "+str(group_info_list[i]-1))
    print(i)
"""

'''
groupn=0;
idx = [0 for i in range(group_num)]
for i in range(TRIM_number):
    groupn = group_info_list[i]
    plt.scatter(x_val[groupn][idx[groupn]:idx[groupn]+1], y_val[groupn][idx[groupn]:idx[groupn]+1], marker="o", s=0.1, color=color_map[groupn])
    ++idx[groupn]

'''

for i in range(group_num):
    plt.scatter(x_val[i], y_val[i], marker="o", s=0.1, color=color_map[i], label="group "+str(i+1))



plt.ylim([0, 100])
plt.xlabel("progress")
plt.ylabel("hot data percent")
plt.legend(loc='upper right')
plt.savefig("bps/"+valid_path+"_group.png")
plt.show()
print("png 2 complete!\n\n")


print("===========================================")
print("              MiDA FTL Result")
print("===========================================")

print("workload:", valid_path)
print("group num:", group_num)
for i in range(group_num):
    print("group "+str(i+1)+" TRIM number:", group_trim_num[i])
    print("group "+str(i+1)+" rate avg: ", str(round(group_rate_avg[i],3))+"%")
print(" ")
print("total rate avg:", str(round(total_rate_avg,3))+"%")
print("total TRIM number:", TRIM_number)

print("\n===========================================")
    
