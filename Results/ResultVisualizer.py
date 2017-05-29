#!/usr/bin/python3

import sys
import math
import matplotlib.pyplot as plt
import numpy as np

def plot(ir, rr, cr, data):
    objects = ('1', '2', '4', '8', '16', '32')
    y_pos = np.arange(len(objects))
    barlist = plt.bar(y_pos, data[0], align='center', alpha=0.5)
    barlist[0].set_color('r')
    plt.ylabel("Execution Time (ms)")
    plt.xlabel("Threads")
    plt.xticks(y_pos, objects)
    plt.title(ir + "% insert, " + rr + "% remove, " + cr + "% contains")
    plt.savefig(ir + "_" + rr + "_" + cr + '.png')

def main(argv):
    data = []
    insertRatio = argv[1]
    removeRatio = argv[2]
    containsRatio = argv[3]
    for line in sys.stdin:
        data.append(list(map(float, line.split())))
    plot(insertRatio, removeRatio, containsRatio, data)

if __name__ == "__main__":
    main(sys.argv)
