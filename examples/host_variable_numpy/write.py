from robot_ipc import HostVariable
import ctypes
import time
import numpy as np

if __name__ == "__main__":
    print("[*] connect to variable host_variable")
    a = HostVariable("host_variable_numpy", data_format = "numpy")
    x = np.array([1, 3, 4], dtype=np.float32)
    a.data = x
