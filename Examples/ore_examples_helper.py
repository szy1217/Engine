import os
import sys
import subprocess
import shutil
import matplotlib.pyplot as plt


class OreExample(object):

    def __init__(self, dry=False):
        self.ore_exe = ""
        self.headlinecounter = 0
        self.dry = dry
        self.ax = None
        self.plot_name=""
        self._locate_ore_exe()

    def _locate_ore_exe(self):
        ore_exe = os.getcwd()
        ore_exe = os.path.dirname(ore_exe)  # one level up
        ore_exe = os.path.dirname(ore_exe)  # one level up
        ore_exe = os.path.join(ore_exe, "App")
        ore_exe = os.path.join(ore_exe, "bin")
        ore_exe = os.path.join(ore_exe, "Win32" if sys.platform == "win32" else "x64")
        ore_exe = os.path.join(ore_exe, "Release")
        ore_exe = os.path.join(ore_exe, "ore.exe")
        self.ore_exe = ore_exe

    def print_headline(self, headline):
        self.headlinecounter += 1
        print()
        print(str(self.headlinecounter) + ") " + headline)

    def get_times(self, output):
        print("Get times from the log file:")
        logfile = open(output)
        for line in logfile.readlines():
            if "ValuationEngine completed" in line:
                times = line.split(":")[-1].strip().split(",")
                for time in times:
                    print("\t" + time.split()[0] + ": " + time.split()[1])

    def get_output_data_from_column(self, csv_name, colidx, offset=1):
        f = open(os.path.join(os.path.join(os.getcwd(), "Output"), csv_name))
        data = []
        for line in f:
            if colidx < len(line.split(',')):
                data.append(line.split(',')[colidx])
            else:
                data.append("Error")
        return data[offset:]

    def save_output_to_subdir(self, subdir, files):
        if not os.path.exists(os.path.join("Output", subdir)):
            os.makedirs(os.path.join("Output", subdir))
        for file in files:
            shutil.copy(os.path.join("Output", file), os.path.join("Output", subdir))

    def plot(self, filename, colIdxTime, colIdxVal, color, label, offset=1):
        self.ax.plot(self.get_output_data_from_column(filename, colIdxTime, offset),
                self.get_output_data_from_column(filename, colIdxVal, offset),
                color=color,
                label=label)

    def plot_npv(self, filename, colIdx, color, label):
        data = self.get_output_data_from_column(filename, colIdx)
        self.ax.plot(range(1, len(data) + 1),
                data,
                color=color,
                label=label)

    def decorate_plot(self, title, ylabel="Exposure", xlabel="Time / Years"):
        self.ax.set_title(title)
        self.ax.set_xlabel(xlabel)
        self.ax.set_ylabel(ylabel)
        self.ax.legend(loc='upper right', shadow=True)

    def plot_line(self, xvals, yvals, color, label):
        self.ax.plot(xvals, yvals, color=color, label=label)

    def setup_plot(self, filename):
        self.fig = plt.figure()
        self.ax = self.fig.add_subplot(111)
        self.plot_name = "mpl_" + filename

    def save_plot_to_file(self, subdir="Output"):
        plt.savefig(os.path.join(subdir,self.plot_name+".pdf"))

    def run(self, xml):
        if not self.dry:
            subprocess.call([self.ore_exe, xml])


