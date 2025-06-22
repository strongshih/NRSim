# utils/system.py
from utils.unit import Unit

class System(object):
    def __init__(self, offchip_mem_bw=900, compute_efficiency=1,
                 memory_efficiency=1, flops=123, frequency=940,
                 bytes_per_elem=2):
        self.unit = Unit()
        self.offchip_mem_bw = self.unit.unit_to_raw(offchip_mem_bw, type='BW')
        self.compute_efficiency = compute_efficiency
        self.memory_efficiency = memory_efficiency
        self.bytes_per_elem = bytes_per_elem  # size of each tensor element in bytes
        self.flops = self.unit.unit_to_raw(flops, type='C')
        self.op_per_sec = self.flops / 2  # assuming two FLOPs per operation
        self.frequency = self.unit.unit_to_raw(frequency, type='F')

    def __str__(self):
        a = f"Accelerator OPS: {self.unit.raw_to_unit(self.flops, type='C')} TOPS, Freq = {self.unit.raw_to_unit(self.frequency, type='F')} GHz\n"
        c = f"Off-chip mem BW: {self.unit.raw_to_unit(self.offchip_mem_bw, type='BW')} GB/s\n"
        e = f"Bytes/element: {self.bytes_per_elem}\n"
        return a + c + e
